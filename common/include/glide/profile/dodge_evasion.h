#pragma once
#include <cstdint>

namespace glide {
namespace combat {
// Local compatibility policy, not a recovered original server duration.
// Begin only after an ordinary dodge and its endurance write are accepted.
class DodgeEvasion {
  public:
    static constexpr uint64_t DurationMs = 750;
    static bool CanBegin(uint64_t now) {
        return now && now <= UINT64_MAX - DurationMs;
    }
    void Begin(uint64_t now) {
        start_ = now;
        end_ = now + DurationMs;
    }
    bool Active(uint64_t now) const {
        return start_ && start_ <= now && now < end_;
    }

  private:
    uint64_t start_ = 0, end_ = 0; // Transient; reconnect never restores immunity.
};
} // namespace combat
} // namespace glide
