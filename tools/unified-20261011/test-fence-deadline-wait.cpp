// Exercise FenceDeadlineWait against real D3D12 fences and Win32 auto-reset events.
// WARP provides the device/fence only; no queue, texture or game is touched.
//
// Reproduces the live sequence of 2026-10-11 21:10 on one shared event: waits time
// out under load, the GPU then catches up (leaving stale notifications), and the
// next target -- the rebuild's feature create -- completes some milliseconds after
// submission. The pre-fix wait is included as a reference and must still show the
// early false; the fixed wait must not, and must stay inside the caller's budget.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdarg>
#include <thread>

static bool g_quiet = false;
static void Log(const char *format, ...)
{
    if (g_quiet) return;
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::putchar('\n');
}
#include "fence-deadline-wait.h"

// Byte-for-byte the logic Synth12WaitFence / FgCompositeWait had before fix2.
static bool LegacyWait(ID3D12Fence *fence, HANDLE ev, UINT64 value, DWORD ms, const char *)
{
    const UINT64 completed = fence->GetCompletedValue();
    if (completed == UINT64_MAX) return false;
    if (completed >= value) return true;
    if (FAILED(fence->SetEventOnCompletion(value, ev))) return false;
    if (WaitForSingleObject(ev, ms) != WAIT_OBJECT_0) return false;
    const UINT64 after = fence->GetCompletedValue();
    return after != UINT64_MAX && after >= value;
}

using WaitFn = bool (*)(ID3D12Fence *, HANDLE, UINT64, DWORD, const char *);

struct Outcome { int false_failures; ULONGLONG min_fail_ms; bool ok; };

// seeds: timed-out waits left behind before the rebuild. completes_after_ms: how
// long after "submission" the rebuild's target is reached.
static Outcome Rebuild(ID3D12Device *device, WaitFn wait, unsigned seeds, DWORD completes_after_ms)
{
    Outcome r{0, ~0ull, true};
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return {0, 0, false};
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT64 next = 1;
    for (unsigned i = 0; i < seeds; ++i, ++next)
        if (wait(fence.Get(), ev, next, 5, "seed")) r.ok = false;   // must time out: GPU "busy"
    if (FAILED(fence->Signal(next - 1))) r.ok = false;               // GPU catches up
    Sleep(20);                                                       // stale notifications land
    for (unsigned frame = 0; frame < 8; ++frame, ++next) {
        HRESULT signalled = E_FAIL;
        std::thread gpu([&] { Sleep(completes_after_ms); signalled = fence->Signal(next); });
        const ULONGLONG t0 = GetTickCount64();
        const bool result = wait(fence.Get(), ev, next, 4000, "rebuild");
        const ULONGLONG elapsed = GetTickCount64() - t0;
        gpu.join();
        if (!result) { ++r.false_failures; if (elapsed < r.min_fail_ms) r.min_fail_ms = elapsed; }
        if (FAILED(signalled)) r.ok = false;
    }
    CloseHandle(ev);
    return r;
}

// Work that never completes inside the budget must still fail, at the budget,
// however many stale wakes arrive during it.
static bool NeverCompletes(ID3D12Device *device)
{
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return false;
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    std::thread noise([&] { for (int i = 0; i < 40; ++i) { Sleep(5); SetEvent(ev); } });
    const ULONGLONG t0 = GetTickCount64();
    const bool result = FenceDeadlineWait(fence.Get(), ev, 1, 100, "never");
    const ULONGLONG elapsed = GetTickCount64() - t0;
    noise.join();
    fence->Signal(1);
    Sleep(10);
    CloseHandle(ev);
    const bool ok = !result && elapsed >= 90 && elapsed < 250;
    std::printf("never_completes pass=%d result=%d elapsed_ms=%llu (budget 100)\n", ok, result, elapsed);
    return ok;
}

static bool AlreadyComplete(ID3D12Device *device)
{
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(5, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return false;
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    const ULONGLONG t0 = GetTickCount64();
    const bool done = FenceDeadlineWait(fence.Get(), ev, 5, 0, "done") &&
                      FenceDeadlineWait(fence.Get(), ev, 3, 0, "done");
    const bool pending = FenceDeadlineWait(fence.Get(), ev, 6, 0, "pending");
    const ULONGLONG elapsed = GetTickCount64() - t0;
    fence->Signal(6);
    Sleep(10);
    CloseHandle(ev);
    const bool ok = done && !pending && elapsed < 50;
    std::printf("already_complete pass=%d elapsed_ms=%llu\n", ok, elapsed);
    return ok;
}

int main()
{
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> warp;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) ||
        FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) ||
        FAILED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return 2;

    g_quiet = true;
    const Outcome legacy_clean = Rebuild(device.Get(), LegacyWait, 0, 18);
    const Outcome legacy_stale = Rebuild(device.Get(), LegacyWait, 3, 18);
    g_quiet = false;
    const Outcome fixed_clean  = Rebuild(device.Get(), FenceDeadlineWait, 0, 18);
    const Outcome fixed_stale  = Rebuild(device.Get(), FenceDeadlineWait, 3, 18);
    std::printf("legacy clean false=%d | legacy after 3 timeouts false=%d min_fail_ms=%llu\n",
                legacy_clean.false_failures, legacy_stale.false_failures,
                legacy_stale.false_failures ? legacy_stale.min_fail_ms : 0ull);
    std::printf("fixed  clean false=%d | fixed  after 3 timeouts false=%d\n",
                fixed_clean.false_failures, fixed_stale.false_failures);
    const bool never = NeverCompletes(device.Get());
    const bool done = AlreadyComplete(device.Get());
    // The reference must reproduce the defect, or this test proves nothing.
    const bool pass = legacy_clean.ok && legacy_stale.ok && fixed_clean.ok && fixed_stale.ok &&
                      legacy_clean.false_failures == 0 && legacy_stale.false_failures > 0 &&
                      fixed_clean.false_failures == 0 && fixed_stale.false_failures == 0 && never && done;
    std::printf("%s\n", pass ? "PASS" : "FAIL");
    std::puts("Scope: isolated real-fence mechanism; not a substitute for in-game recovery acceptance.");
    return pass ? 0 : 1;
}
