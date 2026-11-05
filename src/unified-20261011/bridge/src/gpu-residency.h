#pragma once
// This process's GPU memory residency on one adapter, read from the Windows
// "GPU Process Memory" performance counters (the kernel's per-process segment
// statistics, the same source Task Manager uses). Read-only, no injection into
// anything; pdh.dll is loaded on first use so nothing links against it.
//
//   local     bytes resident in the adapter's local (VRAM) segments
//   nonlocal  bytes resident in system-memory segments
//   committed bytes committed against local segments, resident or not
//   demoted = committed - local: VRAM-preferred allocations that the video
//             memory manager currently keeps in system memory because the
//             process is at its budget. DXGI's CurrentUsage does not show them.
//
// Validated 2026-10-26 on an RTX 4060 Laptop: demoted read 485 MiB where a WPR
// allocation rundown counted 484 MiB of VRAM-preferred allocations in the
// system-memory segment.
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <cstdio>
#include <cwchar>
#include <new>

namespace gpu_residency {

struct Sample {
    bool valid = false;
    unsigned long long local = 0, nonlocal = 0, committed = 0;  // bytes
    long long DemotedMiB() const {
        return static_cast<long long>(committed >> 20) - static_cast<long long>(local >> 20);
    }
};

class Reader {
    using OpenQueryFn = PDH_STATUS(WINAPI *)(LPCWSTR, DWORD_PTR, PDH_HQUERY *);
    using AddCounterFn = PDH_STATUS(WINAPI *)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
    using CollectFn = PDH_STATUS(WINAPI *)(PDH_HQUERY);
    using FormattedFn = PDH_STATUS(WINAPI *)(PDH_HCOUNTER, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE);
    using CloseFn = PDH_STATUS(WINAPI *)(PDH_HQUERY);
    using EnumFn = PDH_STATUS(WINAPI *)(LPCWSTR, LPCWSTR, LPCWSTR, PZZWSTR, LPDWORD, PZZWSTR, LPDWORD, DWORD, DWORD);

    HMODULE pdh_ = nullptr;
    OpenQueryFn open_ = nullptr;
    AddCounterFn add_ = nullptr;
    CollectFn collect_ = nullptr;
    FormattedFn formatted_ = nullptr;
    CloseFn close_ = nullptr;
    EnumFn enumerate_ = nullptr;
    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER local_ = nullptr, nonlocal_ = nullptr, committed_ = nullptr;
    LUID luid_ = {};
    bool failed_ = false;

    bool Load() {
        if (pdh_ != nullptr) return true;
        pdh_ = LoadLibraryExW(L"pdh.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (pdh_ == nullptr) return false;
        open_ = reinterpret_cast<OpenQueryFn>(GetProcAddress(pdh_, "PdhOpenQueryW"));
        add_ = reinterpret_cast<AddCounterFn>(GetProcAddress(pdh_, "PdhAddEnglishCounterW"));
        collect_ = reinterpret_cast<CollectFn>(GetProcAddress(pdh_, "PdhCollectQueryData"));
        formatted_ = reinterpret_cast<FormattedFn>(GetProcAddress(pdh_, "PdhGetFormattedCounterValue"));
        close_ = reinterpret_cast<CloseFn>(GetProcAddress(pdh_, "PdhCloseQuery"));
        enumerate_ = reinterpret_cast<EnumFn>(GetProcAddress(pdh_, "PdhEnumObjectItemsW"));
        return open_ && add_ && collect_ && formatted_ && close_;
    }
    void Close() {
        if (query_ != nullptr && close_ != nullptr) close_(query_);
        query_ = nullptr;
        local_ = nonlocal_ = committed_ = nullptr;
    }
    bool Open(LUID luid) {
        Close();
        if (!Load() || open_(nullptr, 0, &query_) != ERROR_SUCCESS) { query_ = nullptr; return false; }
        wchar_t instance[96];
        swprintf_s(instance, L"pid_%lu_luid_0x%08X_0x%08X_phys_0", GetCurrentProcessId(),
                   static_cast<unsigned>(luid.HighPart), static_cast<unsigned>(luid.LowPart));
        wchar_t path[192];
        bool ok = true;
        swprintf_s(path, L"\\GPU Process Memory(%s)\\Local Usage", instance);
        ok = ok && add_(query_, path, 0, &local_) == ERROR_SUCCESS;
        swprintf_s(path, L"\\GPU Process Memory(%s)\\Non Local Usage", instance);
        ok = ok && add_(query_, path, 0, &nonlocal_) == ERROR_SUCCESS;
        swprintf_s(path, L"\\GPU Process Memory(%s)\\Total Committed", instance);
        ok = ok && add_(query_, path, 0, &committed_) == ERROR_SUCCESS;
        if (!ok) { Close(); return false; }
        luid_ = luid;
        return true;
    }
    bool Value(PDH_HCOUNTER counter, unsigned long long *out) {
        PDH_FMT_COUNTERVALUE value = {};
        if (formatted_(counter, PDH_FMT_LARGE, nullptr, &value) != ERROR_SUCCESS ||
            (value.CStatus != PDH_CSTATUS_VALID_DATA && value.CStatus != PDH_CSTATUS_NEW_DATA))
            return false;
        *out = value.largeValue < 0 ? 0 : static_cast<unsigned long long>(value.largeValue);
        return true;
    }

public:
    ~Reader() { Close(); }
    // Opens (or re-targets) the query for `luid`. A process that has not yet
    // touched the adapter has no counter instance, so a failure is retried by
    // the caller rather than remembered forever.
    bool Read(LUID luid, Sample *out) {
        out->valid = false;
        if (failed_) return false;
        if (query_ == nullptr || luid.LowPart != luid_.LowPart || luid.HighPart != luid_.HighPart) {
            if (!Open(luid)) { if (pdh_ == nullptr || open_ == nullptr) failed_ = true; return false; }
        }
        if (collect_(query_) != ERROR_SUCCESS) { Close(); return false; }
        out->valid = Value(local_, &out->local) && Value(nonlocal_, &out->nonlocal) &&
                     Value(committed_, &out->committed);
        if (!out->valid) Close();
        return out->valid;
    }
    // No adapter known (no private D3D12 device in this process, v9): the
    // process's own "GPU Process Memory" instances name every adapter it
    // touched; the one with the most committed memory is the one rendering on.
    // Returns the adapter it settled on in `luid`, for Read to keep using.
    bool Discover(LUID *luid) {
        if (failed_ || !Load() || enumerate_ == nullptr) return false;
        DWORD counter_chars = 0, instance_chars = 0;
        PDH_STATUS status = enumerate_(nullptr, nullptr, L"GPU Process Memory", nullptr, &counter_chars,
                                       nullptr, &instance_chars, PERF_DETAIL_WIZARD, 0);
        if ((status != PDH_MORE_DATA && status != ERROR_SUCCESS) || instance_chars == 0) return false;
        // Processes come and go between the size query and the fill: give the list room.
        counter_chars += 256; instance_chars += 1024;
        wchar_t *counters = new (std::nothrow) wchar_t[counter_chars];
        wchar_t *instances = new (std::nothrow) wchar_t[instance_chars];
        bool found = false;
        if (counters != nullptr && instances != nullptr &&
            enumerate_(nullptr, nullptr, L"GPU Process Memory", counters, &counter_chars,
                       instances, &instance_chars, PERF_DETAIL_WIZARD, 0) == ERROR_SUCCESS) {
            wchar_t prefix[32];
            swprintf_s(prefix, L"pid_%lu_luid_", GetCurrentProcessId());
            const size_t prefix_length = wcslen(prefix);
            unsigned long long best = 0;
            for (const wchar_t *name = instances; *name != 0; name += wcslen(name) + 1) {
                if (wcsncmp(name, prefix, prefix_length) != 0) continue;
                unsigned high = 0, low = 0;
                if (swscanf_s(name + prefix_length, L"0x%08X_0x%08X_phys_0", &high, &low) != 2) continue;
                LUID candidate;
                candidate.HighPart = static_cast<LONG>(high);
                candidate.LowPart = low;
                Sample sample;
                if (!Read(candidate, &sample) || sample.committed < best) continue;
                best = sample.committed;
                *luid = candidate;
                found = true;
            }
            if (found) Open(*luid);
        }
        delete[] counters;
        delete[] instances;
        return found;
    }
};

}  // namespace gpu_residency
