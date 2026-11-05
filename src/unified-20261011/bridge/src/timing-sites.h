#pragma once
#include "bounded-timing.h"
#include <cstddef>
#include <cstring>

// Stable IDs in the diagnostic CSV. CPU spans describe API execution and do
// not claim that queued GPU work has completed or a frame has been displayed.
namespace bridge_timing {
enum Site : uint32_t {
    ReflexSleep = 1, RelayThrottle = 2, ReflexMarker = 3, ReflexSetOptions = 4,
    GamePresent = 10, GamePresentNative = 11,
    FgEvaluate = 20, FgBridgeInput = 21, FgNativeEvaluate = 22,
    FgFrame = 23,
    WorkerWake = 30, RelayWorker = 31, CopyDependency = 32,
    NrSubmit = 33, CompositeSubmit = 34, NrSignal = 35,
    NrCompleted = 36, CopyCompleted = 37,
    FenceWait = 40, FenceCompletedBefore = 41, FenceCompletedAfter = 42,
};

// ReflexSetOptions: object = complete 64-bit structure version;
// arg = (raw mode << 32) | frameLimitUs. Invalid fields are zero placeholders,
// distinguished by the flags below. Scope supplies QPC, thread and raw result.
// These are observed requests, not a readback of effective driver settings.
enum ReflexOptionsFlags : uint32_t {
    ReflexHeaderReadable = 1u << 0,
    ReflexTypeMatches = 1u << 1,
    ReflexV1FieldsReadable = 1u << 2,
    ReflexUseMarkersToOptimize = 1u << 3,
    ReflexHasNext = 1u << 4,
    ReflexNewerVersion = 1u << 5,
};

#if defined(DLSS5_TIMING_DIAGNOSTIC)
struct ReflexOptionsObservation {
    uint64_t version = 0;
    uint32_t mode = 0, frame_limit_us = 0, flags = 0;
    uint64_t Packed() const noexcept {
        return (static_cast<uint64_t>(mode) << 32) | frame_limit_us;
    }
};

// Streamline x64 BaseStructure and the v1 ReflexOptions prefix. New SDK
// versions append fields; the common prefix remains readable for version >= 1.
// Read only through useMarkersToOptimize and never follow the next pointer.
struct ReflexOptionsPrefix {
    const void* next;
    GUID type;
    uint64_t version;
    uint32_t mode, frame_limit_us;
    uint8_t use_markers;
};
static_assert(offsetof(ReflexOptionsPrefix, version) == 24 &&
              offsetof(ReflexOptionsPrefix, mode) == 32 &&
              offsetof(ReflexOptionsPrefix, frame_limit_us) == 36 &&
              offsetof(ReflexOptionsPrefix, use_markers) == 40, "ReflexOptions x64 ABI");

inline ReflexOptionsObservation ReadReflexOptions(const void* options) noexcept {
    ReflexOptionsObservation observed{};
    ReflexOptionsPrefix prefix{};
    constexpr GUID reflex_type = {0xf03af81a, 0x6d0b, 0x4902,
        {0xa6, 0x51, 0xc4, 0x96, 0x5e, 0x21, 0x54, 0x34}};
    __try {
        memcpy(&prefix, options, offsetof(ReflexOptionsPrefix, mode));
        observed.flags = ReflexHeaderReadable | (prefix.next ? ReflexHasNext : 0u);
        observed.version = prefix.version;
        if (memcmp(&prefix.type, &reflex_type, sizeof(reflex_type)) != 0) return observed;
        observed.flags |= ReflexTypeMatches;
        if (prefix.version < 1) return observed;
        if (prefix.version > 1) observed.flags |= ReflexNewerVersion;
        // Keep a successfully read header when a malformed/truncated payload
        // faults. The actual function still receives the original pointer.
        memcpy(reinterpret_cast<uint8_t*>(&prefix) + offsetof(ReflexOptionsPrefix, mode),
            static_cast<const uint8_t*>(options) + offsetof(ReflexOptionsPrefix, mode), 9);
        observed.mode = prefix.mode;
        observed.frame_limit_us = prefix.frame_limit_us;
        observed.flags |= ReflexV1FieldsReadable |
            (prefix.use_markers ? ReflexUseMarkersToOptimize : 0u);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return observed;
}
#endif
}
