#pragma once
#include <atomic>
#include <cstdint>

namespace dlss5::nr_demand {
// DLSS5GetNrDemandV1 reports the live NR switch, independently of saved layer
// counts. Unknown preserves the behavior of consumers without this export.
enum class State : uint32_t { Unknown = 0, Disabled = 1, Enabled = 2 };
inline constexpr State Decode(uint32_t value) {
    return value == 1 ? State::Disabled : value == 2 ? State::Enabled : State::Unknown;
}
inline constexpr bool ServiceFrame(bool allow_new_arm, bool already_armed) {
    return allow_new_arm || already_armed;
}
class Gate {
    std::atomic_bool resume_reset_{false};
public:
    bool AllowsNewArm(State state) {
        if (state != State::Disabled) return true;
        resume_reset_.store(true, std::memory_order_release);
        return false;
    }
    // Called by the sole owner of a successfully admitted carrier, immediately
    // before it snapshots the guide contract. Skipped frames cannot consume it.
    bool TakeResumeReset() {
        return resume_reset_.exchange(false, std::memory_order_acq_rel);
    }
};
}
