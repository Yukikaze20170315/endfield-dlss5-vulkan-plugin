"""Validate production Synth12Begin/End on an isolated WARP device with optional local debug coverage."""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/unified-20261011/bridge/src/synth.inc'
OUT = ROOT / 'builds/unified-20261011/synth-discard-gpu-tests'
VCVARS = Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/'
              'VC/Auxiliary/Build/vcvarsall.bat')


def function(source, signature):
    """Extract a whole function without changing its body (existing fixture style)."""
    if source.count(signature) != 1:
        raise ValueError(f'Expected one definition: {signature}')
    begin = source.index(signature)
    body = source.index('{', begin)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


FIXTURE = r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks = 0;
static unsigned failures = 0;
static bool debug_enabled = false;
static ComPtr<ID3D12InfoQueue> info;

static void DumpMessages()
{
    if (!info) {
        std::puts("INFOQUEUE: unavailable; error/corruption coverage NOT RUN");
        return;
    }
    UINT64 errors = 0, corruption = 0, warnings = 0;
    const UINT64 count = info->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (UINT64 i = 0; i < count; ++i) {
        SIZE_T bytes = 0;
        HRESULT hr = info->GetMessage(i, nullptr, &bytes);
        if (FAILED(hr)) {
            ++failures;
            std::printf("FAIL InfoQueue message size hr=0x%08lX\n", (unsigned long)hr);
            continue;
        }
        std::vector<unsigned char> storage(bytes);
        auto* msg = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        hr = info->GetMessage(i, msg, &bytes);
        if (FAILED(hr)) {
            ++failures;
            std::printf("FAIL InfoQueue message read hr=0x%08lX\n", (unsigned long)hr);
            continue;
        }
        errors += msg->Severity == D3D12_MESSAGE_SEVERITY_ERROR;
        corruption += msg->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION;
        warnings += msg->Severity == D3D12_MESSAGE_SEVERITY_WARNING;
        if (msg->Severity <= D3D12_MESSAGE_SEVERITY_WARNING)
            std::printf("D3D12 severity=%u id=%u: %s\n",
                        (unsigned)msg->Severity, (unsigned)msg->ID, msg->pDescription);
    }
    const UINT64 dropped = info->GetNumMessagesDiscardedByMessageCountLimit();
    checks += 3;
    failures += errors != 0;
    failures += corruption != 0;
    failures += dropped != 0;
    std::printf("INFOQUEUE: messages=%llu errors=%llu corruption=%llu warnings=%llu dropped=%llu\n",
                count, errors, corruption, warnings, dropped);
}

static void Summary()
{
    std::printf("RESULT checks=%u failures=%u debug=%s exit=%u\n", checks, failures,
                debug_enabled ? "enabled" : "UNAVAILABLE (not covered)", failures ? 1u : 0u);
    std::fflush(stdout);
}

static void Check(bool ok, const char* name)
{
    ++checks;
    if (ok) return;
    ++failures;
    std::printf("FAIL: %s\n", name);
    DumpMessages();
    Summary();
    // Terminate the isolated process without releasing potentially in-flight storage.
    ExitProcess(1);
}

static void Hr(HRESULT hr, const char* name)
{
    if (FAILED(hr)) std::printf("HRESULT %s = 0x%08lX\n", name, (unsigned long)hr);
    Check(SUCCEEDED(hr), name);
}

struct Synth12 {
    @KFRAMES@
    ID3D12CommandQueue* queue = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12CommandAllocator* alloc[kFrames] = {};
    UINT64 alloc_fence[kFrames] = {};
    int slot = 0;
    ID3D12Fence* fence = nullptr;
    UINT64 fence_value = 0;
    HANDLE event = nullptr;
    UINT64 recording_discards = 0;
    UINT64 recording_discard_failures = 0;
};
static Synth12 g_s12;
static unsigned wait_calls = 0;
static UINT64 wait_target = 0;
static DWORD wait_budget = 0;
static unsigned log_calls = 0;

static void Log(const char* format, ...)
{
    ++log_calls;
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::putchar('\n');
}

static bool BoundedWait(ID3D12Fence* fence, HANDLE event, UINT64 target, DWORD ms)
{
    const ULONGLONG deadline = GetTickCount64() + ms;
    bool registered = false;
    for (;;) {
        const UINT64 done = fence->GetCompletedValue();
        if (done == std::numeric_limits<UINT64>::max()) return false;
        if (done >= target) return true;
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline) return false;
        if (!registered) {
            if (FAILED(fence->SetEventOnCompletion(target, event))) return false;
            registered = true;
        }
        const DWORD result = WaitForSingleObject(event, static_cast<DWORD>(deadline - now));
        if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT) return false;
        // Always recheck the real fence, including after a stale event wake.
    }
}

static bool Synth12WaitFence(UINT64 value, DWORD ms)
{
    ++wait_calls;
    wait_target = value;
    wait_budget = ms;
    return BoundedWait(g_s12.fence, g_s12.event, value, ms);
}

// Verbatim production functions; no COM wrappers or algorithm substitutions.
@BEGIN@

@END@

static constexpr unsigned words_per_copy = 16;
static constexpr UINT64 bytes_per_copy = words_per_copy * sizeof(uint32_t);
static constexpr UINT64 buffer_bytes = Synth12::kFrames * bytes_per_copy;
static unsigned submissions = 0, empty_submissions = 0, blocked_wraps = 0;

static ComPtr<ID3D12Resource> Buffer(ID3D12Device* device, D3D12_HEAP_TYPE type,
                                   D3D12_RESOURCE_STATES state)
{
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = type;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = buffer_bytes;
    desc.Height = 1;
    desc.DepthOrArraySize = desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> resource;
    Hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state,
                                      nullptr, IID_PPV_ARGS(&resource)), "create buffer");
    return resource;
}

static uint32_t Pattern(unsigned batch, unsigned copy, unsigned word)
{
    return 0x12340000u ^ (batch << 12) ^ (copy << 8) ^ word;
}

static void VerifyData(ID3D12Resource* readback, unsigned batch, unsigned copies)
{
    void* mapped = nullptr;
    D3D12_RANGE read = {0, static_cast<SIZE_T>(copies * bytes_per_copy)};
    Hr(readback->Map(0, &read, &mapped), "map readback after fence proof");
    const auto* values = static_cast<const uint32_t*>(mapped);
    for (unsigned copy = 0; copy < copies; ++copy) {
        bool equal = true;
        for (unsigned word = 0; word < words_per_copy; ++word)
            equal &= values[copy * words_per_copy + word] == Pattern(batch, copy, word);
        Check(equal, "readback retains original copy payload");
    }
    D3D12_RANGE written = {0, 0};
    readback->Unmap(0, &written);
}

static void Batch(ID3D12Device* device, ID3D12Fence* latch,
                  ID3D12Fence* replay_done, HANDLE replay_event,
                  ID3D12Resource* upload, ID3D12Resource* readback,
                  unsigned batch, unsigned copies)
{
    void* mapped = nullptr;
    D3D12_RANGE no_read = {0, 0};
    Hr(upload->Map(0, &no_read, &mapped), "map upload");
    auto* values = static_cast<uint32_t*>(mapped);
    for (unsigned copy = 0; copy < copies; ++copy)
        for (unsigned word = 0; word < words_per_copy; ++word)
            values[copy * words_per_copy + word] = Pattern(batch, copy, word);
    D3D12_RANGE written = {0, static_cast<SIZE_T>(copies * bytes_per_copy)};
    upload->Unmap(0, &written);

    const UINT64 completed_before = g_s12.fence->GetCompletedValue();
    Check(completed_before == g_s12.fence_value, "previous original submissions completed");
    Check(latch->GetCompletedValue() == batch - 1, "latch remains below queue wait target");
    Hr(g_s12.queue->Wait(latch, batch), "queue wait on CPU-controlled latch");
    UINT64 last = 0;
    for (unsigned copy = 0; copy < copies; ++copy) {
        const int slot = g_s12.slot;
        const unsigned waits_before = wait_calls;
        Check(slot == static_cast<int>(submissions % Synth12::kFrames), "allocator ring slot");
        Check(g_s12.alloc_fence[slot] <= completed_before, "allocator retire proof before Begin");
        Check(Synth12Begin(0), "verbatim Begin resets retired allocator and command list");
        Check(wait_calls == waits_before, "Begin needs no wait for retired slot");
        g_s12.list->CopyBufferRegion(readback, copy * bytes_per_copy,
                                    upload, copy * bytes_per_copy, bytes_per_copy);
        const UINT64 discards_before = g_s12.recording_discards;
        last = Synth12End();
        ++submissions;
        Check(last == submissions && g_s12.fence_value == last, "original submission fence sequence");
        Check(g_s12.recording_discards == discards_before + 1,
              "End Reset/Close both succeeded (production success branch)");
        Check(g_s12.recording_discard_failures == 0, "no discard Reset/Close failures");
        Check(g_s12.alloc_fence[slot] == last, "allocator keeps original submission fence");
        Check(g_s12.slot == (slot + 1) % Synth12::kFrames, "End advances allocator slot");
        Check(g_s12.fence->GetCompletedValue() == completed_before,
              "original fence NOT reached while GPU is blocked");
        Check(latch->GetCompletedValue() == batch - 1, "End did not release CPU latch");
        Hr(device->GetDeviceRemovedReason(), "device healthy while original work is in flight");
        if (submissions == 1) Check(g_s12.recording_discards == 1, "first discard count exactly one");
    }

    if (copies == Synth12::kFrames) {
        const int slot = g_s12.slot;
        const UINT64 retire = g_s12.alloc_fence[slot];
        const unsigned waits_before = wait_calls;
        const auto saved_fences = std::array<UINT64, Synth12::kFrames>{
            g_s12.alloc_fence[0], g_s12.alloc_fence[1], g_s12.alloc_fence[2]};
        Check(retire > g_s12.fence->GetCompletedValue(), "wrapped allocator is genuinely in flight");
        Check(!Synth12Begin(0), "Begin rejects in-flight allocator with zero wait budget");
        Check(wait_calls == waits_before + 1 && wait_target == retire && wait_budget == 0,
              "Begin waits on the correct original allocator fence");
        Check(g_s12.slot == slot && g_s12.fence_value == last,
              "failed Begin leaves ring and submission state unchanged");
        Check(std::memcmp(saved_fences.data(), g_s12.alloc_fence, sizeof(g_s12.alloc_fence)) == 0,
              "failed Begin preserves all allocator retirement proofs");
        ++blocked_wraps;
    }

    Hr(latch->Signal(batch), "CPU releases GPU latch");
    Check(BoundedWait(g_s12.fence, g_s12.event, last, 3000),
          "original submission fence completes within 3 seconds");
    Check(g_s12.fence->GetCompletedValue() == last, "original fence proves completed copies");
    Hr(device->GetDeviceRemovedReason(), "device healthy after original completion");
    VerifyData(readback, batch, copies);

    Hr(upload->Map(0, &no_read, &mapped), "map upload for replay poison");
    values = static_cast<uint32_t*>(mapped);
    for (unsigned copy = 0; copy < copies; ++copy)
        for (unsigned word = 0; word < words_per_copy; ++word)
            values[copy * words_per_copy + word] = ~Pattern(batch, copy, word);
    upload->Unmap(0, &written);

    // A separate fence proves empty replay without altering production retirement tokens.
    ID3D12CommandList* empty[] = {g_s12.list};
    g_s12.queue->ExecuteCommandLists(1, empty);
    ++empty_submissions;
    Hr(g_s12.queue->Signal(replay_done, batch), "signal separate empty-execution proof");
    Check(BoundedWait(replay_done, replay_event, batch, 3000), "empty execution completes within 3 seconds");
    VerifyData(readback, batch, copies);
    Check(g_s12.fence->GetCompletedValue() == last && g_s12.fence_value == last,
          "empty replay test does not replace the original completion proof");
    Hr(device->GetDeviceRemovedReason(), "device healthy after empty replay");
    std::printf("BATCH %u: copies=%u original_fence=%llu discards=%llu blocked_wrap=%s; copy/replay PASS\n",
                batch, copies, last, g_s12.recording_discards,
                copies == Synth12::kFrames ? "PASS" : "not applicable");
}

int main()
{
    ComPtr<ID3D12Debug> debug;
    const HRESULT debug_hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
    if (SUCCEEDED(debug_hr)) {
        debug->EnableDebugLayer();
        debug_enabled = true;
        std::puts("DEBUG: EnableDebugLayer enabled for this process only; GPU-based validation not enabled");
    } else {
        std::printf("DEBUG: unavailable hr=0x%08lX; debug error/corruption coverage NOT RUN\n",
                    (unsigned long)debug_hr);
    }

    ComPtr<IDXGIFactory4> factory;
    Hr(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "create independent DXGI factory");
    ComPtr<IDXGIAdapter1> warp;
    Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "enumerate WARP adapter");
    DXGI_ADAPTER_DESC1 desc = {};
    Hr(warp->GetDesc1(&desc), "read WARP adapter description");
    Check((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0, "adapter is software WARP");
    std::printf("ADAPTER: WARP vendor=0x%04X device=0x%04X (no game or hardware device used)\n",
                desc.VendorId, desc.DeviceId);
    ComPtr<ID3D12Device> device;
    Hr(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "create WARP D3D12 device");
    if (debug_enabled) {
        Hr(device.As(&info), "obtain real ID3D12InfoQueue");
        Hr(info->SetMessageCountLimit(4096), "set process-local bounded debug message storage");
    }
    {
        ComPtr<ID3D12CommandQueue> queue;
        D3D12_COMMAND_QUEUE_DESC qd = {};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Hr(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)), "create direct queue");
        std::array<ComPtr<ID3D12CommandAllocator>, Synth12::kFrames> allocators;
        for (int i = 0; i < Synth12::kFrames; ++i) {
            Hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             IID_PPV_ARGS(&allocators[i])), "create allocator");
            g_s12.alloc[i] = allocators[i].Get();
        }
        ComPtr<ID3D12GraphicsCommandList> list;
        Hr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators[0].Get(),
                                    nullptr, IID_PPV_ARGS(&list)), "create command list");
        Hr(list->Close(), "close initial empty list");
        ComPtr<ID3D12Fence> original, latch, replay;
        Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&original)), "create original fence");
        Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&latch)), "create CPU latch fence at zero");
        Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&replay)), "create empty replay fence");
        g_s12.queue = queue.Get();
        g_s12.list = list.Get();
        g_s12.fence = original.Get();
        g_s12.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HANDLE replay_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Check(g_s12.event != nullptr && replay_event != nullptr, "create fence events");
        auto upload = Buffer(device.Get(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        auto readback = Buffer(device.Get(), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);

        Batch(device.Get(), latch.Get(), replay.Get(), replay_event, upload.Get(), readback.Get(), 1, 1);
        for (unsigned batch = 2; batch <= 5; ++batch)
            Batch(device.Get(), latch.Get(), replay.Get(), replay_event, upload.Get(), readback.Get(),
                  batch, Synth12::kFrames);
        Check(submissions == 13 && g_s12.recording_discards == 13, "13 successful in-flight discards");
        Check(empty_submissions == 5 && blocked_wraps == 4, "five non-replays and four blocked ring wraps");
        Check(log_calls == 8, "production success logging remains bounded to eight entries");
        Check(wait_calls == 4, "four actual pending-fence Begin wait-stub calls");
        Check(g_s12.recording_discard_failures == 0, "all production Reset/Close pairs succeeded");
        Hr(device->GetDeviceRemovedReason(), "final device health");
        Check(CloseHandle(replay_event) != FALSE, "close replay event");
        Check(CloseHandle(g_s12.event) != FALSE, "close original event");
        g_s12 = {};
    }
    // Include teardown messages, not just messages from the first submit.
    DumpMessages();
    Summary();
    return failures ? 1 : 0;
}
'''


def main():
    if os.name != 'nt':
        raise SystemExit('This local fixture requires Windows D3D12/WARP.')
    if not VCVARS.is_file():
        raise SystemExit(f'Existing MSVC toolchain not found: {VCVARS}')
    original = SOURCE.read_bytes()
    source = original.decode('utf-8')
    begin = function(source, 'static bool Synth12Begin(DWORD wait_ms = 4000)')
    end = function(source, 'static UINT64 Synth12End()')
    struct = function(source, 'struct Synth12\n' if 'struct Synth12\n' in source else 'struct Synth12\r\n')
    frames = re.search(r'static const int\s+kFrames\s*=\s*3;', struct)
    if frames is None:
        raise SystemExit('Allocator ring contract changed; expected production kFrames = 3.')
    fixture = FIXTURE.replace('@KFRAMES@', frames.group()).replace('@BEGIN@', begin).replace('@END@', end)
    if fixture.count(begin) != 1 or fixture.count(end) != 1:
        raise SystemExit('Verbatim extraction check failed.')
    OUT.mkdir(parents=True, exist_ok=True)
    cpp = OUT / 'synth-discard-gpu.cpp'
    cpp.write_bytes(fixture.encode('utf-8'))
    runner = OUT / 'build-synth-discard-gpu.cmd'
    runner.write_text(
        '@echo off\nsetlocal\nset "VSCMD_SKIP_SENDTELEMETRY=1"\n'
        f'call "{VCVARS}" x64 >nul\n'
        'if errorlevel 1 exit /b %errorlevel%\n'
        'cl /nologo /W4 /O2 /MT /EHsc /std:c++20 /utf-8 '
        '"%~dp0synth-discard-gpu.cpp" /Fo:"%~dp0synth-discard-gpu.obj" '
        '/Fe:"%~dp0synth-discard-gpu.exe" /link d3d12.lib dxgi.lib kernel32.lib /INCREMENTAL:NO\n'
        'exit /b %errorlevel%\n', encoding='ascii')
    print(f'SOURCE: {SOURCE}\nSHA256: {hashlib.sha256(original).hexdigest()}', flush=True)
    print('EXTRACTION: Synth12Begin and Synth12End verbatim; real COM objects, bounded wait adapter only', flush=True)
    env = os.environ.copy()
    env.update(TEMP=str(OUT), TMP=str(OUT))
    cmd = Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32/cmd.exe'
    build = subprocess.run([str(cmd), '/d', '/c', str(runner)], cwd=str(OUT), env=env,
                           capture_output=True, timeout=180)
    sys.stdout.buffer.write(build.stdout)
    sys.stdout.buffer.write(build.stderr)
    sys.stdout.flush()
    print(f'BUILD_EXIT={build.returncode}', flush=True)
    if build.returncode:
        return build.returncode
    run = subprocess.run([str(OUT / 'synth-discard-gpu.exe')], cwd=str(OUT), env=env,
                         capture_output=True, timeout=60)
    sys.stdout.buffer.write(run.stdout)
    sys.stdout.buffer.write(run.stderr)
    sys.stdout.flush()
    print(f'GPU_TEST_EXIT={run.returncode}', flush=True)
    if SOURCE.read_bytes() != original:
        print('SOURCE_CHANGED_DURING_TEST: results describe the SHA256 above, not the new source.', flush=True)
        return 1
    print('SOURCE_UNCHANGED; WARP only, no NGX/Generic/game/hardware-GPU validation.', flush=True)
    return run.returncode


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except subprocess.TimeoutExpired as exc:
        for stream in (exc.stdout, exc.stderr):
            if stream:
                sys.stdout.buffer.write(stream)
        print(f'\nTIMEOUT after {exc.timeout}s; child stopped, GPU validation incomplete.', flush=True)
        raise SystemExit(1)
