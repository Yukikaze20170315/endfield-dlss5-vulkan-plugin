#pragma once

// Diagnostic-only ABI. No allocation, disk I/O, worker, or waiting in producers.
#include <Windows.h>
#include <cstdint>
#include <climits>
#include <atomic>
#include <cwchar>

namespace dlss5timing {
constexpr uint64_t kMagic = 0x31474E4D49543544ull; // D5TIMNG1
constexpr uint32_t kVersion = 1;
constexpr uint32_t kCapacity = 8192;
// Reserved recorder bit: Scope entry marker. Match completion by tid/kind/begin_qpc.
// Caller flags occupy the lower 31 bits; Record and completed Scope entries clear this bit.
constexpr uint32_t kSpanBegin = 0x80000000u;
struct alignas(8) Entry {
    LONG64 stamp;
    int64_t begin_qpc;
    int64_t end_qpc;
    uint64_t object;
    uint64_t arg;
    uint32_t tid;
    uint32_t kind;
    int32_t result;
    uint32_t flags;
    LONG64 tail;
};
static_assert(sizeof(Entry) == 64, "timing ABI");
struct alignas(8) Buffer {
    uint64_t magic;
    uint32_t version;
    uint32_t bytes;
    uint32_t capacity;
    uint32_t entry_bytes;
    LONG init_state; // 0 uninitialized, 1 initializing, 2 ready
    uint32_t pid;
    LONG64 qpc_frequency;
    LONG64 attempts;
    LONG64 drops;
    LONG64 overwritten;
    LONG64 committed;
    LONG claims[kCapacity];
    Entry entries[kCapacity];
};
static_assert(sizeof(Buffer) <= 1024 * 1024, "bounded diagnostic memory");
}

#if defined(DLSS5_TIMING_DIAGNOSTIC)
extern "C" {
__declspec(dllexport) inline dlss5timing::Buffer Dlss5TimingBuffer = {
    dlss5timing::kMagic, dlss5timing::kVersion, sizeof(dlss5timing::Buffer),
    dlss5timing::kCapacity, sizeof(dlss5timing::Entry)};
}
namespace dlss5timing {
// The selected pointer never changes after Init publishes readiness. Mapping lifetime is
// process lifetime; it contains only this diagnostic ABI, not arbitrary process memory.
inline std::atomic<Buffer*> active_buffer{&Dlss5TimingBuffer};
inline std::atomic<DWORD> shared_error{ERROR_SUCCESS};
inline HANDLE shared_mapping = nullptr;
static_assert(std::atomic<Buffer*>::is_always_lock_free, "hot path pointer must be lock-free");
inline bool SharedReady() noexcept {
    return active_buffer.load(std::memory_order_acquire) != &Dlss5TimingBuffer;
}
inline DWORD SharedError() noexcept { return shared_error.load(std::memory_order_acquire); }
inline void Init(bool shared = false) noexcept {
    const DWORD previous_error = GetLastError();
    auto& b = Dlss5TimingBuffer;
    if (InterlockedCompareExchange(&b.init_state, 1, 0) != 0) return;
    LARGE_INTEGER freq{};
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0) {
        shared_error.store(ERROR_INVALID_DATA, std::memory_order_release);
        InterlockedExchange(&b.init_state, 0);
        SetLastError(previous_error);
        return;
    }
    b.pid = GetCurrentProcessId();
    b.qpc_frequency = freq.QuadPart;
    if (shared) {
        wchar_t name[64]{};
        swprintf_s(name, L"Local\\DLSS5TimingV1-%lu", b.pid);
        HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            static_cast<DWORD>(sizeof(Buffer)), name);
        DWORD error = mapping ? ERROR_SUCCESS : GetLastError();
        // Never attach to or overwrite an unexpected pre-existing named object.
        if (mapping && GetLastError() == ERROR_ALREADY_EXISTS) {
            error = ERROR_ALREADY_EXISTS;
            CloseHandle(mapping);
            mapping = nullptr;
        }
        if (mapping) {
            auto* mapped = static_cast<Buffer*>(MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(Buffer)));
            if (mapped) {
                ZeroMemory(mapped, sizeof(Buffer));
                mapped->magic = kMagic;
                mapped->version = kVersion;
                mapped->bytes = sizeof(Buffer);
                mapped->capacity = kCapacity;
                mapped->entry_bytes = sizeof(Entry);
                mapped->pid = b.pid;
                mapped->qpc_frequency = freq.QuadPart;
                InterlockedExchange(&mapped->init_state, 2);
                shared_mapping = mapping;
                active_buffer.store(mapped, std::memory_order_release);
            } else {
                error = GetLastError();
                CloseHandle(mapping);
            }
        }
        shared_error.store(error, std::memory_order_release);
    }
    InterlockedExchange(&b.init_state, 2);
    SetLastError(previous_error);
}
inline bool Ready() noexcept {
    return InterlockedCompareExchange(&active_buffer.load(std::memory_order_acquire)->init_state, 2, 2) == 2;
}
inline int64_t Now() noexcept {
    const DWORD previous_error = GetLastError();
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    SetLastError(previous_error);
    return value.QuadPart;
}
inline void Write(uint32_t kind, uint64_t object, uint64_t arg, int32_t result,
                  uint32_t flags, int64_t begin, int64_t end) noexcept {
    auto& b = *active_buffer.load(std::memory_order_relaxed);
    const LONG64 seq = InterlockedIncrement64(&b.attempts);
    const uint32_t index = static_cast<uint32_t>(seq - 1) & (kCapacity - 1);
    if (InterlockedCompareExchange(&b.claims[index], 1, 0) != 0) {
        InterlockedIncrement64(&b.drops);
        return;
    }
    auto& e = b.entries[index];
    // A preempted producer must not replace a newer generation of this slot.
    if (e.stamp >= seq) {
        InterlockedIncrement64(&b.drops);
        InterlockedExchange(&b.claims[index], 0);
        return;
    }
    const bool overwritten = e.stamp > 0;
    InterlockedExchange64(&e.stamp, -seq);
    e.begin_qpc = begin;
    e.end_qpc = end;
    e.object = object;
    e.arg = arg;
    const DWORD previous_error = GetLastError();
    e.tid = GetCurrentThreadId();
    SetLastError(previous_error);
    e.kind = kind;
    e.result = result;
    e.flags = flags;
    InterlockedExchange64(&e.tail, seq);
    InterlockedExchange64(&e.stamp, seq);
    InterlockedIncrement64(&b.committed);
    if (overwritten) InterlockedIncrement64(&b.overwritten);
    InterlockedExchange(&b.claims[index], 0);
}
inline void Record(uint32_t kind, uint64_t object = 0, uint64_t arg = 0,
                   int32_t result = INT32_MIN, uint32_t flags = 0) noexcept {
    if (!Ready()) return;
    const auto now = Now();
    Write(kind, object, arg, result, flags & ~kSpanBegin, now, now);
}
constexpr uint32_t kSummaryCapacity = 64;
struct SpanSummary {
    uint64_t calls = 0, ticks = 0, max_ticks = 0;
    uint64_t ge50ms = 0, ge100ms = 0;
    int64_t first_begin_qpc = 0, last_end_qpc = 0;
};
struct alignas(64) SummarySlot {
    SpanSummary completed;
    std::atomic_uint64_t dropped{0}, in_flight{0};
    std::atomic_flag busy = ATOMIC_FLAG_INIT;
    uint8_t padding[128 - sizeof(SpanSummary) - 2 * sizeof(std::atomic_uint64_t) - sizeof(std::atomic_flag)]{};
};
static_assert(sizeof(SummarySlot) == 128, "summary sites must not share cache lines");
inline SummarySlot summary_slots[kSummaryCapacity];
static_assert(sizeof(summary_slots) <= 16 * 1024, "bounded span summaries");
static_assert(std::atomic_uint64_t::is_always_lock_free, "bounded producer counters");
struct SummarySnapshot {
    SpanSummary completed;
    uint64_t dropped = 0, in_flight = 0;
};
inline void SummarizeSpan(uint32_t kind, int64_t begin, int64_t end) noexcept {
    if (kind >= kSummaryCapacity) return;
    auto& slot = summary_slots[kind];
    if (slot.busy.test_and_set(std::memory_order_acquire)) {
        slot.dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const uint64_t ticks = static_cast<uint64_t>(end - begin);
    const uint64_t frequency = static_cast<uint64_t>(active_buffer.load(std::memory_order_relaxed)->qpc_frequency);
    auto& sum = slot.completed;
    ++sum.calls;
    sum.ticks += ticks;
    if (ticks > sum.max_ticks) sum.max_ticks = ticks;
    if (ticks >= (frequency + 19) / 20) ++sum.ge50ms;
    if (ticks >= (frequency + 9) / 10) ++sum.ge100ms;
    if (sum.first_begin_qpc == 0 || begin < sum.first_begin_qpc) sum.first_begin_qpc = begin;
    if (end > sum.last_end_qpc) sum.last_end_qpc = end;
    slot.busy.clear(std::memory_order_release);
}
// Completion-time buckets: spans crossing a report boundary belong to the later report.
inline bool TakeSummary(uint32_t kind, SummarySnapshot* snapshot) noexcept {
    if (kind >= kSummaryCapacity) return false;
    auto& slot = summary_slots[kind];
    if (slot.busy.test_and_set(std::memory_order_acquire)) return false;
    snapshot->completed = slot.completed;
    slot.completed = {};
    snapshot->dropped = slot.dropped.exchange(0, std::memory_order_relaxed);
    snapshot->in_flight = slot.in_flight.load(std::memory_order_relaxed);
    slot.busy.clear(std::memory_order_release);
    return true;
}
class Scope {
    uint32_t kind_;
    uint32_t flags_;
    uint64_t object_;
    uint64_t arg_;
    int64_t begin_;
    bool active_;
public:
    explicit Scope(uint32_t kind, uint64_t object = 0, uint64_t arg = 0,
                   uint32_t flags = 0) noexcept
        : kind_(kind), flags_(flags & ~kSpanBegin), object_(object), arg_(arg), begin_(0), active_(Ready()) {
        if (active_) {
            begin_ = Now();
            if (kind_ < kSummaryCapacity) summary_slots[kind_].in_flight.fetch_add(1, std::memory_order_relaxed);
            Write(kind_, object_, arg_, INT32_MIN, flags_ | kSpanBegin, begin_, begin_);
        }
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    void Finish(int32_t result) noexcept {
        if (!active_) return;
        active_ = false;
        const auto end = Now();
        Write(kind_, object_, arg_, result, flags_, begin_, end);
        if (kind_ < kSummaryCapacity) summary_slots[kind_].in_flight.fetch_sub(1, std::memory_order_relaxed);
        SummarizeSpan(kind_, begin_, end);
    }
    ~Scope() noexcept { Finish(INT32_MIN); }
};
}
#else
namespace dlss5timing {
inline void Init(bool = false) noexcept {}
inline bool SharedReady() noexcept { return false; }
inline DWORD SharedError() noexcept { return ERROR_SUCCESS; }
inline void Record(uint32_t, uint64_t = 0, uint64_t = 0,
                   int32_t = INT32_MIN, uint32_t = 0) noexcept {}
class Scope {
public:
    explicit Scope(uint32_t, uint64_t = 0, uint64_t = 0, uint32_t = 0) noexcept {}
    void Finish(int32_t) noexcept {}
};
}
#endif
