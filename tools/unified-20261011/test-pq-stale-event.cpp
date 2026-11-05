// Exercise the production wait function against real D3D12 fences and Win32 events.
// WARP is used only to create a device/fence; no GPU queue, texture or game is touched.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdarg>
#include <thread>
#include <algorithm>
#include <cstdint>

struct PqPass { ID3D12Fence* fence; HANDLE ev; };
#ifdef PQ_AUDIT_LOGGING
static void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::putchar('\n');
}
#endif
#include "production-pq-wait.inc"

// Test-only reference: a stale wake is not a failure while the original budget remains.
static bool DeadlineWait(PqPass* p, UINT64 value, DWORD wait_ms) {
    const ULONGLONG end = GetTickCount64() + wait_ms;
    const UINT64 initial = p->fence->GetCompletedValue();
    if (initial == UINT64_MAX) return false;
    if (initial >= value) return true;
    if (FAILED(p->fence->SetEventOnCompletion(value, p->ev))) return false;
    for (;;) {
        const UINT64 completed = p->fence->GetCompletedValue();
        if (completed == UINT64_MAX) return false;
        if (completed >= value) return true;
        const ULONGLONG now = GetTickCount64();
        if (now >= end) return false;
        if (WaitForSingleObject(p->ev, static_cast<DWORD>(end - now)) != WAIT_OBJECT_0)
            return false;
    }
}

using WaitFn = bool (*)(PqPass*, UINT64, DWORD);
[[maybe_unused]] static bool DeadlineChecks(ID3D12Device* device) {
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return false;
    PqPass pass{fence.Get(), CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    if (!pass.ev) return false;
    const bool complete = PqPassWait(&pass, 0, 0);
    const bool immediate_pending = PqPassWait(&pass, 1, 0);
    std::thread old_notifications([&] {
        for (unsigned i = 0; i < 20; ++i) { Sleep(3); SetEvent(pass.ev); }
    });
    const ULONGLONG before = GetTickCount64();
    const bool pending = PqPassWait(&pass, 2, 50);
    const ULONGLONG elapsed = GetTickCount64() - before;
    old_notifications.join();
    // Complete pending registrations before disposing of the event.
    const HRESULT signal = fence->Signal(2);
    Sleep(10);
    CloseHandle(pass.ev);
    const bool ok = complete && !immediate_pending && !pending && elapsed >= 40 && elapsed < 150 && SUCCEEDED(signal);
    std::printf("deadline_checks pass=%d completed_probe=%d pending_probe=%d never_completed_result=%d elapsed_ms=%llu\n",
                ok, complete, immediate_pending, pending, elapsed);
    return ok;
}

static int Run(ID3D12Device* device, const char* name, WaitFn wait, bool seed_timeout) {
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return -1;
    PqPass pass{fence.Get(), CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    if (!pass.ev) return -1;
    UINT64 next = 1;
    if (seed_timeout) {
        const bool result = wait(&pass, next, 5);
        std::printf("%s seed_timeout result=%d completed=%llu target=%llu\n", name, result,
                    fence->GetCompletedValue(), next);
        if (result || FAILED(fence->Signal(next++))) return -1;
        // Allow the registered completion notification to arrive, without consuming it.
        Sleep(10);
    }
    int false_failures = 0;
    for (unsigned frame = 0; frame < 8; ++frame, ++next) {
        HRESULT signaled = E_FAIL;
        std::thread completion([&] { Sleep(30); signaled = fence->Signal(next); });
        const ULONGLONG before = GetTickCount64();
        const bool result = wait(&pass, next, 250);
        const ULONGLONG elapsed = GetTickCount64() - before;
        const UINT64 observed = fence->GetCompletedValue();
        completion.join();
        const UINT64 later = fence->GetCompletedValue();
        std::printf("%s frame=%u result=%d wait_ms=%llu observed=%llu target=%llu later=%llu signal_hr=%08lx\n",
                    name, frame, result, elapsed, observed, next, later, static_cast<unsigned long>(signaled));
        if (!result && elapsed < 200 && later >= next && SUCCEEDED(signaled)) ++false_failures;
        if (FAILED(signaled)) return -1;
    }
    CloseHandle(pass.ev);
    return false_failures;
}

int main() {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> warp;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) ||
        FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) ||
        FAILED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return 2;
    const int clean = Run(device.Get(), "production_clean", PqPassWait, false);
    const int stale = Run(device.Get(), "production_after_timeout", PqPassWait, true);
    const int reference = Run(device.Get(), "reference_after_timeout", DeadlineWait, true);
#ifdef PQ_FIXED_WAIT
    const bool pass = clean == 0 && stale == 0 && reference == 0 && DeadlineChecks(device.Get());
#else
    const bool pass = clean == 0 && stale == 8 && reference == 0;
#endif
    std::printf("%s clean_false=%d timeout_chain_false=%d reference_false=%d\n", pass ? "PASS" : "FAIL", clean, stale, reference);
    std::puts("Scope: isolated real-fence mechanism; this does not identify the failing API in the live game.");
    return pass ? 0 : 1;
}
