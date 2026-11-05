import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'src/unified-20261011'
OUT = ROOT / 'builds/unified-20261011/nr-recording-discard-tests'


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    tracker = (SRC / 'generic/src/addons/dlss5/submission_tracker.hpp').read_text(encoding='utf-8')
    lease = (SRC / 'generic/src/addons/dlss5/gpu_lease.hpp').read_text(encoding='utf-8')
    synth = (SRC / 'bridge/src/synth.inc').read_text(encoding='utf-8')
    end = function(synth, 'static UINT64 Synth12End()')
    assert end.count('ExecuteCommandLists') == 1
    assert 'alloc[g_s12.slot]->Reset' not in end
    assert 'Wait' not in end and 'Synth12Begin' not in end
    assert 'Synth12ResetIdleRecording' not in synth and 'closed_recording' not in synth
    code = r'''
#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
unsigned checks = 0;
void Check(bool value, const char* name) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL %s\n", name); ExitProcess(1); }
}
struct ID3D12CommandAllocator {
    unsigned resets = 0;
    HRESULT Reset() { ++resets; return S_OK; }
};
struct ID3D12Fence {
    uint64_t completed = 0;
    unsigned refs = 1;
    void AddRef() { ++refs; }
    void Release() { --refs; }
    uint64_t GetCompletedValue() { return completed; }
};
struct ID3D12CommandList {
    uint64_t identity = 0;
    bool closed = false, fail_reset = false, fail_close = false;
    unsigned resets = 0, closes = 0;
    ID3D12CommandAllocator* allocator = nullptr;
    HRESULT GetPrivateData(const GUID&, UINT* size, void* out) {
        if (!identity || *size < sizeof(identity)) return E_FAIL;
        *size = sizeof(identity); std::memcpy(out, &identity, sizeof(identity)); return S_OK;
    }
    HRESULT SetPrivateData(const GUID&, UINT size, const void* in) {
        Check(size == sizeof(identity), "identity size");
        std::memcpy(&identity, in, sizeof(identity)); return S_OK;
    }
    HRESULT Reset(ID3D12CommandAllocator*, void*);
    HRESULT Close() { ++closes; if (fail_close) return E_FAIL; closed = true; return S_OK; }
};
namespace renodx::utils::directx {
inline void NativeFromReShadeProxy(ID3D12CommandList**) {}
}
inline GUID ModuleScopedGuid(const GUID& guid) { return guid; }
namespace renodx::addons::dlss5 {
'''
    code += function(lease, 'struct FenceRef {') + ';\n}\n'
    code += tracker[tracker.index('namespace renodx::addons::dlss5::submission {'):]
    code += r'''
namespace sub = renodx::addons::dlss5::submission;
HRESULT ID3D12CommandList::Reset(ID3D12CommandAllocator* alloc, void*) {
    ++resets;
    if (fail_reset || !closed) return E_FAIL;
    allocator = alloc;
    closed = false;
    sub::OnCommandListReset(this);
    return S_OK;
}
struct Queue {
    ID3D12Fence proof;
    uint64_t value = 0;
    unsigned executes = 0, signals = 0;
    bool no_proof = false;
    void ExecuteCommandLists(UINT count, ID3D12CommandList* const* lists) {
        Check(count == 1 && lists[0]->closed, "submit only closed list");
        ++executes;
        auto pending = sub::BeginSubmission(count, lists);
        sub::CompleteSubmission(pending, no_proof ? nullptr : &proof, ++value);
    }
    HRESULT Signal(ID3D12Fence*, uint64_t) { ++signals; return S_OK; }
};
struct Synth12 {
    static constexpr int kFrames = 3;
    ID3D12CommandList* list;
    Queue* queue;
    ID3D12CommandAllocator* alloc[kFrames];
    ID3D12Fence* fence;
    uint64_t alloc_fence[kFrames] = {}, fence_value = 0;
    int slot = 0;
    uint64_t recording_discards = 0, recording_discard_failures = 0;
};
Synth12 g_s12{};
unsigned success_logs = 0, failure_logs = 0;
void Log(const char* format, ...) {
    if (std::strstr(format, "discard failed")) ++failure_logs;
    else ++success_logs;
}
'''
    code += end
    code += r'''
void Drain() { sub::PruneCompletedGenerations(); }
int main() {
    int resource = 0, other = 0;
    ID3D12CommandAllocator alloc[3];
    ID3D12Fence fence;
    Queue queue;
    ID3D12CommandList list;
    g_s12 = { &list, &queue, {&alloc[0], &alloc[1], &alloc[2]}, &fence };
    sub::TrackUse(&list, &resource);
    Check(!sub::UseHeldByOpenRecording(&resource), "unsubmitted is not completed replay");
    list.Close();
    ID3D12CommandList* lists[] = {&list};
    auto pending = sub::BeginSubmission(1, lists);
    Check(!sub::UseHeldByOpenRecording(&resource), "pending reservation is not completed replay");
    sub::CompleteSubmission(pending, &queue.proof, 1);
    Drain();
    Check(!sub::ResourceReleasable(&resource), "in flight held");
    Check(!sub::UseHeldByOpenRecording(&resource), "in flight is not completed replay");
    queue.proof.completed = 1;
    Drain();
    Check(sub::UseHeldByOpenRecording(&resource), "completed parked list diagnosed");
    Check(!sub::ResourceReleasable(&resource), "completion alone cannot release");
    Check(sub::RecordedWorkState() == sub::RecordedWork::kReplayable, "parked generation replayable");
    list.Reset(&alloc[0], nullptr);
    Drain();
    Check(sub::ResourceReleasable(&resource), "reset plus completion releases");
    Check(!sub::UseHeldByOpenRecording(&resource), "pruned use no longer diagnosed");
    sub::TrackUse(&list, &resource);
    queue.proof.completed = 0;
    const auto v = Synth12End();
    Drain();
    Check(v == 1 && g_s12.alloc_fence[0] == 1 && g_s12.slot == 1, "retire ring unchanged");
    Check(list.allocator == &alloc[0] && list.closed, "discard uses submitting allocator and closes empty list");
    Check(!alloc[0].resets && !alloc[1].resets && !alloc[2].resets, "no allocator reset");
    Check(queue.executes == 1 && queue.signals == 1, "one submission and original signal only");
    Check(!sub::ResourceReleasable(&resource), "post-submit Reset does not free in-flight use");
    Check(!sub::UseHeldByOpenRecording(&resource), "discarded generation not replayable");
    queue.proof.completed = queue.value;
    Drain();
    Check(sub::ResourceReleasable(&resource), "last NR frame releases without another evaluate");
    Check(sub::RecordedWorkState() == sub::RecordedWork::kComplete, "no idle replay obligation");
    list.Reset(&alloc[1], nullptr);
    sub::TrackUse(&list, &resource);
    list.fail_reset = true;
    Synth12End();
    queue.proof.completed = queue.value;
    Drain();
    Check(!sub::ResourceReleasable(&resource) && sub::UseHeldByOpenRecording(&resource), "failed Reset keeps replay protection");
    Check(g_s12.recording_discard_failures == 1, "discard failure counted");
    list.fail_reset = false;
    list.Reset(&alloc[1], nullptr);
    Drain();
    Check(sub::ResourceReleasable(&resource), "later reset recovers");
    sub::TrackUse(&list, &resource);
    queue.no_proof = true;
    Synth12End();
    Drain();
    Check(!sub::ResourceReleasable(&resource), "missing submission proof never bypassed");
    Check(!sub::UseHeldByOpenRecording(&resource), "missing proof not labelled completion");
    sub::ResetAll();
    queue.no_proof = false;
    ID3D12CommandList a, b;
    sub::TrackUse(&a, &other);
    sub::TrackUse(&b, &other);
    a.Close(); b.Close();
    ID3D12CommandList* la[] = {&a};
    ID3D12CommandList* lb[] = {&b};
    auto pa = sub::BeginSubmission(1, la);
    auto pb = sub::BeginSubmission(1, lb);
    sub::CompleteSubmission(pa, &queue.proof, 10);
    sub::CompleteSubmission(pb, &queue.proof, 20);
    queue.proof.completed = 10;
    Check(sub::UseHeldByOpenRecording(&other), "one completed replay may coexist with another pending use");
    a.Reset(&alloc[0], nullptr);
    Drain();
    Check(!sub::ResourceReleasable(&other), "other list still protects resource");
    Check(!sub::UseHeldByOpenRecording(&other), "only in-flight use remains");
    b.Reset(&alloc[1], nullptr);
    Drain();
    Check(!sub::ResourceReleasable(&other), "both reset but second GPU use still pending");
    queue.proof.completed = 20;
    Drain();
    Check(sub::ResourceReleasable(&other), "all lists and all proofs required");
    ID3D12CommandList dead;
    sub::TrackUse(&dead, &other); dead.Close();
    ID3D12CommandList* ld[] = {&dead};
    auto pd = sub::BeginSubmission(1, ld);
    sub::CompleteSubmission(pd, &queue.proof, 30);
    queue.proof.completed = UINT64_MAX;
    Check(!sub::UseHeldByOpenRecording(&other), "device removal not normal completion diagnostic");
    sub::OnCommandListDestroyed(&dead);
    Drain();
    Check(sub::ResourceReleasable(&other), "destroy plus removed-device semantics unchanged");
    sub::ResetAll();
    list.fail_reset = false;
    g_s12.recording_discards = g_s12.recording_discard_failures = 0;
    success_logs = failure_logs = 0;
    for (unsigned i=0; i<100; ++i) {
        list.closed = false;
        Synth12End();
    }
    Check(success_logs == 8 && g_s12.recording_discards == 100, "success logging bounded with counter retained");
    list.fail_reset = true;
    for (unsigned i=0; i<100; ++i) Synth12End();
    Check(failure_logs == 64 && g_s12.recording_discard_failures == 100, "failure logging bounded with counter retained");
    Check(!alloc[0].resets && !alloc[1].resets && !alloc[2].resets, "no allocator storage discarded by End");
    sub::ResetAll();
    std::printf("nr-recording-discard: %u checks, 0 failures\n", checks);
}
'''
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'fixture.cpp').write_text(code, encoding='utf-8')
    runner = OUT / 'run.cmd'
    runner.write_text('@echo off\nsetlocal\nset VSCMD_SKIP_SENDTELEMETRY=1\n'
        'call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64 >nul\n'
        'if errorlevel 1 exit /b %errorlevel%\n'
        'cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++20 /utf-8 fixture.cpp /Fe:fixture.exe\n'
        'if errorlevel 1 exit /b %errorlevel%\nfixture.exe\nexit /b %errorlevel%\n', encoding='ascii')
    env = dict(os.environ, TEMP=str(OUT), TMP=str(OUT))
    result = subprocess.run(['cmd.exe', '/d', '/c', str(runner)], cwd=OUT, env=env, capture_output=True)
    sys.stdout.buffer.write(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.returncode)
    print('Production tracker and Synth12End; fake COM/GPU, no live game or device.')


if __name__ == '__main__':
    main()
