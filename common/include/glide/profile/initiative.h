#pragma once
#include "glide/buffer.h"
#include "initiative_costs.h"
#include "beta_initiative_costs.h"
#include <string>

namespace glide {
namespace modern {
// Local exploration policy: retain each build's existing maximum. April/June retain0.6 point/s; July and later
// use1 point/s without modifiers.
class InitiativeLedger {
  public:
    static const unsigned Maximum = 8000;
    static InitiativeLedger ForBuild(unsigned build) {
        InitiativeLedger value;
        if (build == 14075 || build == 14422 || build == 14505) {
            value.maximum_ = 12000;
            value.points_ = 12000;
            value.numerator_ = 3;
            value.denominator_ = 5;
        }
        return value;
    }
    bool Load(const std::string& text) {
        points_ = maximum_;
        utc_ = 0;
        if (text.empty())
            return true;
        if (text.size() > 40 || text.compare(0, 2, "1:"))
            return false;
        const auto colon = text.find(':', 2);
        uint64_t points = 0, utc = 0;
        if (colon == std::string::npos || !Decimal(text.substr(2, colon - 2), points) || points > maximum_ ||
            !Decimal(text.substr(colon + 1), utc) || !utc)
            return false;
        points_ = static_cast<unsigned>(points);
        utc_ = utc;
        return true;
    }
    bool Advance(uint64_t utc) {
        if (!utc)
            return false;
        if (!utc_) {
            utc_ = utc;
            return true;
        }
        if (utc <= utc_)
            return true;
        const auto elapsed = utc - utc_;
        const uint64_t saturation = (uint64_t(maximum_) * denominator_ + numerator_ - 1) / numerator_;
        // Absolute UTC credits preserve the fractional0.6/ms rate across any
        // number of saves, including tiny intervals, without multiplying UTC.
        const uint64_t phase = utc_ % denominator_;
        const unsigned refill = elapsed >= saturation
                                    ? maximum_
                                    : static_cast<unsigned>((phase + elapsed) * numerator_ / denominator_ -
                                                            phase * numerator_ / denominator_);
        points_ = refill > maximum_ - points_ ? maximum_ : points_ + refill;
        utc_ = utc;
        return true;
    }
    bool Spend(unsigned cost) {
        if (cost > MaximumPoints() || cost * 1000 > points_)
            return false;
        points_ -= cost * 1000;
        return true;
    }
    unsigned MaximumPoints() const {
        return maximum_ / 1000;
    }
    float RecoveryPerSecond() const {
        return static_cast<float>(numerator_) / static_cast<float>(denominator_);
    }
    unsigned Points() const {
        return points_ / 1000;
    }
    unsigned Millipoints() const {
        return points_;
    }
    std::string Text() const {
        return utc_ ? "1:" + std::to_string(points_) + ":" + std::to_string(utc_) : std::string();
    }

  private:
    static bool Decimal(const std::string& text, uint64_t& out) {
        if (text.empty() || (text.size() > 1 && text[0] == '0'))
            return false;
        out = 0;
        for (char c : text) {
            if (c < '0' || c > '9' || out > (UINT64_MAX - (c - '0')) / 10)
                return false;
            out = out * 10 + static_cast<unsigned>(c - '0');
        }
        return true;
    }
    unsigned points_ = Maximum, maximum_ = Maximum, numerator_ = 1, denominator_ = 1;
    uint64_t utc_ = 0;
};
inline bool InitiativeBuild(unsigned build) {
    return build == 14075 || build == 14422 || build == 14505 || build == 14812 || build == 49284 || build == 167409 ||
           build == 204489;
}
inline bool InitiativeCurrent(unsigned build, uint16_t actor, const InitiativeLedger& value, Buffer& out) {
    const uint16_t id = build == 14075                                         ? 189
                        : (build == 14422 || build == 14505 || build == 14812) ? 219
                        : build == 49284                                       ? 344
                        : build == 167409                                      ? 480
                        : build == 204489                                      ? 500
                                                                               : 0;
    const auto policy = InitiativeLedger::ForBuild(build);
    return id && actor && value.MaximumPoints() == policy.MaximumPoints() &&
           value.RecoveryPerSecond() == policy.RecoveryPerSecond() && out.U16(id) && out.VarU32(actor) &&
           out.F32(static_cast<float>(value.MaximumPoints())) && out.F32(value.RecoveryPerSecond()) &&
           out.F32(static_cast<float>(value.Millipoints()) / 1000.0f);
}
} // namespace modern
} // namespace glide
