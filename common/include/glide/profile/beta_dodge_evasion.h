#pragma once
#include "glide/profile/dodge_evasion.h"
#include "glide/profile/endurance.h"
#include "glide/profile/beta_positive_effect_state.h"
#include "glide/profile/beta_phoenix_data.h"
#include <algorithm>
#include <utility>

namespace glide {
namespace beta {
// Call only after the complete per-build motion decoder accepts the packet.
inline bool DodgeCommand(unsigned build, uint8_t state, uint32_t control, ByteView input, uint32_t& token) {
    const uint8_t opcode = build == 14075 ? 0x17 : 0x18;
    if ((build != 14075 && build != 14422 && build != 14505 && build != 14812) || state != 3 ||
        !(control & 0x80000000u) || !(control & 0x7fffffffu) || !input.Valid() || input.size != 3 ||
        input.data[0] != opcode || input.data[1] || input.data[2] > 3)
        return false;
    token = control & 0x7fffffffu;
    return true;
}
class DodgeEvasion {
  public:
    // Beta UI restoration has no verified receiver here. Keep this ledger
    // transient, resetting both the server policy and native bar on reconnect.
    bool Advance(uint64_t utc, const std::string& effects) {
        auto next = resource_;
        const auto previous = next.SavedUtc();
        if (!next.Advance(utc))
            return false;
        if (previous && utc > previous && effects.compare(0, 3, "b1:") == 0) {
            StoredPositiveEffects saved;
            if (!ReadPositiveEffects(effects, saved))
                return false;
            std::vector<std::pair<uint64_t, uint64_t>> spans;
            for (const auto& effect : saved.effects)
                if (effect.definition == Phoenix5258::Buff) {
                    if (effect.end - effect.start > Phoenix5258::DurationMs)
                        return false;
                    const auto begin = (std::max)(previous, effect.start), end = (std::min)(utc, effect.end);
                    if (begin < end)
                        spans.emplace_back(begin, end);
                }
            std::sort(spans.begin(), spans.end());
            uint64_t begin = 0, end = 0, covered = 0;
            for (const auto& span : spans) {
                if (!end) {
                    begin = span.first;
                    end = span.second;
                } else if (span.first > end) {
                    covered += end - begin;
                    begin = span.first;
                    end = span.second;
                } else
                    end = (std::max)(end, span.second);
            }
            if (end)
                covered += end - begin;
            // EnduranceRatePercent +100: one extra base refill during the
            // union of active intervals. Overlapping applications do not multiply.
            const auto bonus = (std::min)(uint64_t(modern::EnduranceLedger::Maximum),
                                          covered * modern::EnduranceLedger::RefillPerMillisecond);
            next.GainMillipoints(static_cast<unsigned>(bonus));
        }
        resource_ = next;
        return true;
    }
    bool Accept(uint32_t token, uint64_t now, uint64_t utc, const std::string& effects = {}) {
        if (!token || token <= token_ || !combat::DodgeEvasion::CanBegin(now))
            return false;
        auto next = *this;
        if (!next.Advance(utc, effects) || !next.resource_.SpendDodge())
            return false;
        *this = next;
        token_ = token;
        window_.Begin(now);
        return true;
    }
    bool Active(uint64_t now) const {
        return window_.Active(now);
    }
    unsigned Points() const {
        return resource_.Points();
    }

  private:
    modern::EnduranceLedger resource_;
    combat::DodgeEvasion window_;
    uint32_t token_ = 0;
};
} // namespace beta
} // namespace glide
