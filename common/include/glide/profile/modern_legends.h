#pragma once
#include "glide/profile/legend_state.h"
#include "glide/buffer.h"
#include "glide/profile/modern_energy.h"
#include <utility>

namespace glide {
namespace modern {
// Both modern original clients allow all four base selectors in both locales. The original protocol carries a
// locale byte; HoT's message does not. Elite states are deliberately separate.
template <unsigned Build> class Legends {
    static_assert(Build == 167409 || Build == 204489, "Unsupported legend dialect");

  public:
    using State = BasicLegendState<Build == 167409 ? 14 : 15, 15>;
    static constexpr uint16_t SelectRequest = Build == 167409 ? 180 : 192;
    static constexpr uint16_t CreateMessage = Build == 167409 ? 473 : 493;
    static constexpr uint16_t ListMessage = Build == 167409 ? 475 : 495;
    static constexpr uint16_t ActiveMessage = Build == 167409 ? 477 : 497;
    static bool Pair(const State& state, bool water, Buffer& out) {
        return out.U8(2) && out.U8(state.First(water)) && out.U8(state.Second(water));
    }
    static bool Create(uint16_t actor, const State& state, Buffer& out) {
        return state.Enabled() && out.U16(CreateMessage) && out.VarU32(actor) && Pair(state, false, out) &&
               Pair(state, true, out);
    }
    static bool List(uint16_t actor, const State& state, bool water, uint32_t remaining, Buffer& out) {
        return state.Enabled() && remaining <= 10000 && out.U16(ListMessage) && out.VarU32(actor) &&
               out.VarU32(remaining) && out.U8(water ? 1 : 0) && Pair(state, water, out);
    }
    static bool Active(uint16_t actor, const State& state, Buffer& out) {
        return state.Enabled() && out.U16(ActiveMessage) && out.VarU32(actor) && out.VarU32(state.First());
    }
    static bool Snapshot(uint16_t actor, const State& state, uint32_t remaining, Buffer& out) {
        return List(actor, state, false, remaining, out) && List(actor, state, true, remaining, out) &&
               Active(actor, state, out);
    }
};

template <unsigned Build> class LegendActions {
    using Wire = Legends<Build>;
    typename Wire::State state_;
    bool reported_ = false;
    bool hasSwapIdentity_ = false;
    uint32_t swapIdentity_ = 0;

  public:
    using Persist = std::function<bool(const std::string&, unsigned, Energy<Build>&, WorldActionOutput&)>;
    bool Load(const Character& saved) {
        const auto profession = saved.profileFields.find("profession");
        reported_ = false;
        hasSwapIdentity_ = false;
        return state_.Load(saved, profession != saved.profileFields.end() && profession->second == "9" ? 9 : 0);
    }
    bool Due() const {
        return state_.Enabled() && !reported_;
    }
    bool Enabled() const {
        return state_.Enabled();
    }
    unsigned Active() const {
        return state_.Enabled() ? state_.First() : 0;
    }
    bool Initial(const WorldActionContext& context, WorldActionOutput& out) {
        if (!Due())
            return true;
        if (!state_.Restore(context.now, context.wallNow) ||
            !Wire::Snapshot(57344, state_, state_.Remaining(context.now), out.owned))
            return false;
        reported_ = true;
        return true;
    }
    bool Medium(bool water, const WorldActionContext& context, WorldActionOutput& out) {
        if (!state_.Enabled() || (reported_ && state_.Underwater() == water))
            return true;
        if (!state_.Restore(context.now, context.wallNow))
            return false;
        state_.SetUnderwater(water);
        if (!Wire::Snapshot(57344, state_, state_.Remaining(context.now), out.owned) ||
            !Wire::Snapshot(context.peerAgent, state_, 0, out.peers))
            return false;
        reported_ = true;
        return true;
    }
    bool Peer(uint16_t actor, Buffer& out) const {
        return !state_.Enabled() || Wire::Snapshot(actor, state_, 0, out);
    }
    WorldActionResult Swap(uint32_t skill, uint8_t slot, uint32_t target, uint32_t identity,
                           const WorldActionContext& context, unsigned level, Energy<Build>& energy, uint64_t& version,
                           WorldActionOutput& out, const Persist& persist = Persist()) {
        WorldActionOutput result;
        result.sequences = 0;
        const unsigned base = Build == 167409 ? 14 : 15, second = state_.Second();
        const uint32_t selectors[] = {28085, 28134, 28419, 28494};
        if (!state_.Enabled() || level < 11 || slot != 12 || (target != 0 && target != 57344) || second < base ||
            second >= base + 4 || skill != selectors[second - base] ||
            (hasSwapIdentity_ && identity == swapIdentity_)) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (!context.peerAgent || !context.sequence || context.sequence == UINT32_MAX || version == UINT64_MAX)
            return WorldActionResult::Failed;
        auto next = *this;
        auto nextEnergy = energy;
        if (!next.state_.Restore(context.now, context.wallNow))
            return WorldActionResult::Failed;
        if (!next.state_.Swap(context.now, context.wallNow)) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (!Wire::Snapshot(57344, next.state_, next.state_.Remaining(context.now), result.owned) ||
            !Wire::Snapshot(context.peerAgent, next.state_, 0, result.peers) || !nextEnergy.Reset(context.now, result))
            return WorldActionResult::Failed;
        // Native F1 uses directed skill order 35, slot 12, the next legend's
        // selector. Acknowledge with the same channel-1 instant skill order
        // already used by modern attunements; keep weapon channel 0 intact.
        const auto acknowledge = [&](uint16_t actor, Buffer& bytes) {
            return bytes.U16(34) && bytes.U16(actor) && bytes.VarU32(context.sequence) && bytes.U8(0x30) &&
                   bytes.VarU32(identity) && bytes.U8(11) && bytes.U8(122) && bytes.U8(0) && bytes.U32(actor) &&
                   bytes.U32(skill) && bytes.U8(slot) && bytes.U16(1);
        };
        if (!acknowledge(57344, result.owned) || !acknowledge(context.peerAgent, result.peers) ||
            !context.ReserveSequences(1))
            return WorldActionResult::Failed;
        if (persist ? !persist(next.state_.Text(), next.state_.First(), nextEnergy, result)
                    : (!context.persistLegendState || !context.persistLegendState(next.state_.Text()))) {
            WorldActionOutput empty;
            empty.sequences = 0;
            out = std::move(empty);
            return WorldActionResult::Handled;
        }
        next.reported_ = true;
        next.hasSwapIdentity_ = true;
        next.swapIdentity_ = identity;
        *this = std::move(next);
        energy = std::move(nextEnergy);
        result.sequences = 1;
        result.peerEffectVersion = ++version;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Request(ByteView input, const WorldActionContext& context, unsigned level, Energy<Build>& energy,
                              uint64_t& version, WorldActionOutput& out, const Persist& persist = Persist()) {
        Reader reader(input);
        uint16_t id = 0;
        if (!reader.U16(id))
            return WorldActionResult::Failed;
        if (id != Wire::SelectRequest)
            return WorldActionResult::Ignored;
        uint8_t slot = 0, legend = 0, locale = 0;
        if (!reader.U8(slot) || !reader.U8(legend) || !reader.U8(locale) || reader.Remaining())
            return WorldActionResult::Failed;
        WorldActionOutput result;
        result.sequences = 0;
        if (!state_.Enabled() || locale > 1 || slot > 1) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (!context.peerAgent || !context.sequence || version == UINT64_MAX)
            return WorldActionResult::Failed;
        auto next = *this;
        auto nextEnergy = energy;
        if (!next.state_.Restore(context.now, context.wallNow))
            return WorldActionResult::Failed;
        const auto previous = state_.Text();
        const auto active = state_.First();
        const bool water = state_.Underwater();
        next.state_.SetUnderwater(locale == 1);
        if (!next.state_.Select(slot, legend, level, context.now)) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        next.state_.SetUnderwater(water);
        if (next.state_.Text() == previous) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (!Wire::Snapshot(57344, next.state_, next.state_.Remaining(context.now), result.owned) ||
            !Wire::Snapshot(context.peerAgent, next.state_, 0, result.peers) ||
            (active != next.state_.First() && !nextEnergy.Reset(context.now, result)))
            return WorldActionResult::Failed;
        if (!context.ReserveSequences(0))
            return WorldActionResult::Failed;
        if (persist ? !persist(next.state_.Text(), next.state_.First(), nextEnergy, result)
                    : (!context.persistLegendState || !context.persistLegendState(next.state_.Text()))) {
            WorldActionOutput empty;
            empty.sequences = 0;
            out = std::move(empty);
            return WorldActionResult::Handled;
        }
        next.reported_ = true;
        *this = std::move(next);
        energy = std::move(nextEnergy);
        result.peerEffectVersion = ++version;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
};
} // namespace modern
} // namespace glide
