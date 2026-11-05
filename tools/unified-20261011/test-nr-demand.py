"""Exercise production NR demand query/policy and pending-frame decisions on CPU.

Builds isolated Generic stand-ins from the actual export body, loads them through
the production Bridge query, and tests live toggles, old/absent consumers, resume
history and both worker/evaluate completion orders. No GPU or game is started.
"""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'src/unified-20261011'
OUT = ROOT / 'builds/unified-20261011/nr-demand-tests'


def function(source, signature):
    begin = source.index(signature)
    body = source.index('{', begin)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name in ('modern', 'legacy'):
        (OUT / name).mkdir(exist_ok=True)
    addon = (SRC / 'generic/src/addons/dlss5/addon.cpp').read_text(encoding='utf-8')
    relay = (SRC / 'bridge/src/fg-relay.inc').read_text(encoding='utf-8')
    demand_header = (SRC / 'common/nr-demand.hpp').as_posix()
    modern = f'#include <Windows.h>\n#include "{demand_header}"\n' + r'''
namespace renodx::addons::dlss5::internal {
std::atomic_bool hook_mode_config_ready{false}, enabled{false};
}
extern "C" __declspec(dllexport) void SetTestDemand(uint32_t ready, uint32_t on) {
    renodx::addons::dlss5::internal::enabled.store(on != 0);
    renodx::addons::dlss5::internal::hook_mode_config_ready.store(ready != 0);
}
''' + function(addon, 'extern "C" __declspec(dllexport) uint32_t __cdecl DLSS5GetNrDemandV1()')
    stage_exports = r'''
extern "C" __declspec(dllexport) uint32_t __cdecl DLSS5StageProtocolVersion() { return 1; }
extern "C" __declspec(dllexport) uint32_t __cdecl DLSS5GetStagePlan() { return 64; }
'''
    (OUT / 'modern.cpp').write_text(modern + stage_exports, encoding='utf-8')
    (OUT / 'legacy.cpp').write_text('#include <cstdint>\n' + stage_exports, encoding='utf-8')
    fixture = r'''
#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
static HMODULE g_self = nullptr;
''' + f'#include "{(SRC / "bridge/src/present-adapter-config.h").as_posix()}"\n' + r'''
struct FgrState {
    bool cs_ready = true, ready = true;
    volatile LONG arm_seq = 0, worker_seq = 0, eval_seq = 0;
};
static FgrState g_fgr;
static void FgrLock() {}
static void FgrUnlock() {}
''' + function(relay, 'static bool FgRelayHasArmedFrameForEvaluate()') + '\n' + function(relay, 'static bool FgRelayPending()') + r'''
static unsigned checks = 0;
static void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", name); ExitProcess(1); }
}
int wmain(int argc, wchar_t** argv) {
    using namespace dlss5::nr_demand;
    g_self = GetModuleHandleW(nullptr);
    Check(argc == 2 || argc == 3, "arguments");
    Check(PresentAdapterNrDemand() == State::Unknown, "consumer absent");
    Gate gate;
    Check(gate.AllowsNewArm(State::Unknown), "old/absent consumer remains conservative");
    Check(!gate.TakeResumeReset(), "no artificial startup reset");
    Check(Decode(0xFFFFFFFFu) == State::Unknown, "invalid wire value remains unknown");
    if (argc == 2) { std::printf("PASS absent: %u checks\n", checks); return 0; }
    HMODULE module = LoadLibraryW(argv[2]);
    Check(module != nullptr, "load private consumer");
    uint32_t plan = 0;
    Check(PresentAdapterStagePlan(&plan) && plan == 64, "layer plan available");
    if (wcscmp(argv[1], L"legacy") == 0) {
        Check(PresentAdapterNrDemand() == State::Unknown, "legacy export absent");
        Check(FgInputActive(), "legacy presentation ownership retained");
        std::printf("PASS legacy: %u checks\n", checks); return 0;
    }
    using Set = void(*)(uint32_t, uint32_t);
    auto set = reinterpret_cast<Set>(GetProcAddress(module, "SetTestDemand"));
    Check(set != nullptr, "state control");
    Check(PresentAdapterNrDemand() == State::Unknown, "startup state unknown");
    set(1, 1);
    Check(PresentAdapterNrDemand() == State::Enabled, "live enabled");
    Check(gate.AllowsNewArm(PresentAdapterNrDemand()), "enabled arms");
    set(1, 0);
    Check(PresentAdapterNrDemand() == State::Disabled, "cached function reads current switch");
    Check(FgInputActive() && g_adapter_owner == OWNER_NONE, "off preserves selected path and ownership");
    for (unsigned i = 0; i < 500; ++i)
        Check(!gate.AllowsNewArm(PresentAdapterNrDemand()), "disabled never admits a new frame");
    Check(!ServiceFrame(false, FgRelayHasArmedFrameForEvaluate()), "idle disabled evaluate is bypassed");
    // Copy is queued before the switch: evaluation and worker may finish in either order.
    for (unsigned order = 0; order < 2; ++order) {
        g_fgr.arm_seq = 1; g_fgr.worker_seq = 0; g_fgr.eval_seq = 0;
        Check(ServiceFrame(false, FgRelayHasArmedFrameForEvaluate()), "old arm reaches evaluate when off");
        Check(FgRelayPending(), "old arm reaches worker when off");
        if (order == 0) {
            g_fgr.worker_seq = 1;
            Check(!FgRelayPending() && FgRelayHasArmedFrameForEvaluate(), "worker first leaves evaluate obligation");
            g_fgr.eval_seq = 1;
        } else {
            g_fgr.eval_seq = 1;
            Check(!FgRelayHasArmedFrameForEvaluate() && FgRelayPending(), "evaluate first leaves worker release obligation");
            g_fgr.worker_seq = 1;
        }
        Check(!FgRelayPending() && !ServiceFrame(false, FgRelayHasArmedFrameForEvaluate()), "drained frame creates no next arm");
    }
    set(1, 1);
    Check(gate.AllowsNewArm(PresentAdapterNrDemand()), "reenabled admits new work");
    // Observing readiness or skipping an arm must not consume its required reset.
    for (unsigned i = 0; i < 50; ++i) Check(gate.AllowsNewArm(State::Enabled), "resume observation preserves reset");
    Check(gate.TakeResumeReset(), "first resumed carrier gets reset");
    Check(!gate.TakeResumeReset(), "later carriers do not repeat reset");
    Check(!gate.AllowsNewArm(State::Disabled) && gate.AllowsNewArm(State::Unknown), "unknown compatibility after off");
    std::atomic_uint claimed{0};
    std::vector<std::thread> contenders;
    for (unsigned i = 0; i < 8; ++i) contenders.emplace_back([&] { if (gate.TakeResumeReset()) ++claimed; });
    for (auto& thread : contenders) thread.join();
    Check(claimed == 1, "resume reset claimed once across threads");
    std::printf("PASS modern: %u checks\n", checks);
}
'''
    (OUT / 'test.cpp').write_text(fixture, encoding='utf-8')
    build = '@echo off\r\nsetlocal\r\nset "VSCMD_SKIP_SENDTELEMETRY=1"\r\n'
    build += 'call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64 >nul\r\nif errorlevel 1 exit /b %errorlevel%\r\n'
    for source, output, obj, options in (
        ('modern.cpp', 'modern\\renodx-dlss5.addon64', 'modern\\addon.obj', '/LD'),
        ('legacy.cpp', 'legacy\\renodx-dlss5.addon64', 'legacy\\addon.obj', '/LD'),
        ('test.cpp', 'test.exe', 'test.obj', '/wd4505')):
        build += f'cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++17 /utf-8 {options} {source} /Fo:"{obj}" /Fe:"{output}"\r\nif errorlevel 1 exit /b %errorlevel%\r\n'
    (OUT / 'build.cmd').write_text(build, encoding='ascii')
    env = dict(os.environ, TEMP=str(OUT), TMP=str(OUT))
    subprocess.run(['cmd', '/d', '/c', str(OUT / 'build.cmd')], cwd=OUT, env=env, check=True)
    for args in (['absent'], ['legacy', str(OUT / 'legacy/renodx-dlss5.addon64')],
                 ['modern', str(OUT / 'modern/renodx-dlss5.addon64')]):
        subprocess.run([str(OUT / 'test.exe'), *args], cwd=OUT, env=env, check=True)


if __name__ == '__main__':
    main()
