#pragma once
#include "glide/profile/target_link.h"
#include "glide/profile/modern_brand_rules.h"
#include <vector>

namespace glide {
const GameMessages& China167409Game();
const GameMessages& Build204489Game();
namespace modern {
template <unsigned Build> class RevenantBrand {
    using Rules = RevenantBrandRules;
    using Link = TargetLink<Rules, Build == 167409 ? 713 : 735, Build == 167409 ? 717 : 739>;
    Link link_;
    std::vector<uint8_t> motion_;
    uint32_t pendingTarget_ = 0;
    uint64_t warpDue_ = 0;
    static const GameMessages& Game() {
        return Build == 167409 ? China167409Game() : Build204489Game();
    }

  public:
    // Reuse the validated movement template required by the existing teleport.
    bool ObserveMotion(ByteView input) {
        Facing facing;
        Buffer probe;
        if (Game().DecodeFacing(input, facing) != FacingResult::Present || !Game().LocalTeleport(input, {}, 1, probe))
            return true;
        motion_.assign(input.data, input.data + input.size);
        return true;
    }
    bool Due() const {
        return link_.Target() != 0 || pendingTarget_ != 0;
    }
    uint32_t Target() const {
        return link_.Target();
    }
    bool Active(const WorldActionContext& c) const {
        return link_.Active(c.now) && link_.ValidTarget(c);
    }
    bool Schedule(uint32_t target, unsigned level, const WorldActionContext& c) {
        CatalogPosition point;
        if (motion_.empty() || pendingTarget_ || !Link::TargetValid(target, c) ||
            !c.characterTargetPosition(target, point))
            return false;
        const double x = double(point.x) - c.position.x, y = double(point.y) - c.position.y,
                     z = double(point.z) - c.position.z;
        const double distance = std::sqrt(x * x + y * y + z * z) * 32;
        if (!std::isfinite(distance) || distance <= 1 || distance > Rules::Range)
            return false;
        return link_.Schedule(target, level, c, static_cast<unsigned>(std::ceil(distance * 1000 / Rules::Speed)));
    }
    bool Consume(const WorldActionContext& c, WorldActionOutput& out) {
        if (motion_.empty() || !Active(c) || c.now > UINT64_MAX - Rules::TriggerMs)
            return false;
        auto peer = link_;
        const auto target = link_.Target();
        const auto before = out.peers.Size();
        if (!link_.Consume(c, out.owned, 57344) || !peer.Consume(c, out.peers, c.peerAgent))
            return false;
        if (out.targetPeerAgent && !out.targetView.Append({out.peers.Data() + before, out.peers.Size() - before}))
            return false;
        pendingTarget_ = target;
        warpDue_ = c.now + Rules::TriggerMs;
        return true;
    }
    bool Advance(const WorldActionContext& c, bool available, WorldActionOutput& out) {
        auto peer = link_;
        const auto before = out.peers.Size();
        if (!link_.Advance(c, available, out.owned, 57344) || !peer.Advance(c, available, out.peers, c.peerAgent))
            return false;
        if (out.targetPeerAgent && out.peers.Size() > before &&
            !out.targetView.Append({out.peers.Data() + before, out.peers.Size() - before}))
            return false;
        if (!pendingTarget_)
            return true;
        if (!available || !Link::TargetValid(pendingTarget_, c)) {
            pendingTarget_ = 0;
            return true;
        }
        if (c.now < warpDue_)
            return true;
        CatalogPosition point;
        Buffer own, remote;
        if (!c.sequence || c.sequence > UINT32_MAX - 2 || out.sequences > UINT32_MAX - c.sequence - 2 ||
            !c.characterTargetPosition(pendingTarget_, point) ||
            !Game().LocalTeleport({motion_.data(), motion_.size()}, point, c.sequence + out.sequences, own) ||
            !Game().PeerTeleport({motion_.data(), motion_.size()}, point, c.peerAgent, c.sequence + out.sequences,
                                 remote) ||
            !out.owned.Append(own.View()) || !out.peers.Append(remote.View()) ||
            (out.targetPeerAgent && !out.targetView.Append(remote.View())))
            return false;
        out.sequences += 2;
        out.relocate = true;
        out.destination = point;
        pendingTarget_ = 0;
        motion_.clear();
        return true;
    }
    void CancelWarp() {
        pendingTarget_ = 0;
    }
    bool Snapshot(uint16_t actor, uint64_t now, Buffer& out) const {
        return link_.Snapshot(actor, now, out);
    }
    bool Cleanup(uint16_t actor, Buffer& out) const {
        return link_.Cleanup(actor, out);
    }
};
} // namespace modern
} // namespace glide
