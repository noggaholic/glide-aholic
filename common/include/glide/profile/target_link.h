#pragma once
#include "glide/profile/world_actions.h"
#include <cmath>
namespace glide {
template <class Rules, unsigned Create = 497, unsigned Remove = 500> class TargetLink {
    uint32_t target_ = 0;
    uint64_t due_ = 0;
    bool active_ = false;
    unsigned level_ = 0;
    static const uint32_t kInstance = Rules::Instance;
    bool Marker(uint16_t actor, uint64_t now, bool remove, Buffer& out) const {
        if (remove)
            return out.U16(Remove) && out.VarU32(actor) && out.VarU32(actor) && out.VarU32(kInstance);
        return due_ > now && due_ - now <= Rules::WindowMs && out.U16(Create) && out.VarU32(actor) &&
               out.VarU32(actor) && out.VarU32(Rules::Marker) && out.VarU32(kInstance) &&
               out.VarU32(static_cast<uint32_t>(due_ - now)) && out.U8(static_cast<uint8_t>(level_)) && out.U8(1);
    }

  public:
    uint32_t Target() const {
        return target_;
    }
    bool Active(uint64_t now) const {
        return target_ && active_ && now < due_;
    }
    static bool TargetValid(uint32_t target, const WorldActionContext& context) {
        CatalogPosition point;
        if (!target || target == context.peerAgent || !context.isVisibleCharacterTarget ||
            !context.isVisibleCharacterTarget(target) || !context.characterTargetPosition ||
            !context.characterTargetPosition(target, point))
            return false;
        const double x = double(point.x) - context.position.x, y = double(point.y) - context.position.y,
                     z = double(point.z) - context.position.z;
        const double squared = (x * x + y * y + z * z) * 1024;
        return std::isfinite(squared) && squared <= double(Rules::Range) * Rules::Range;
    }
    bool ValidTarget(const WorldActionContext& context) const {
        return TargetValid(target_, context);
    }
    bool Schedule(uint32_t target, unsigned level, const WorldActionContext& context, unsigned travelMs = 0) {
        if (target_ || level > 80 || travelMs > 60000 || context.now > UINT64_MAX - Rules::ReleaseMs - travelMs)
            return false;
        auto next = *this;
        next.target_ = target;
        next.level_ = level;
        next.due_ = context.now + Rules::ReleaseMs + travelMs;
        if (!next.ValidTarget(context))
            return false;
        *this = next;
        return true;
    }
    bool Advance(const WorldActionContext& context, bool available, Buffer& out, uint16_t actor = 0) {
        if (!target_)
            return true;
        if (!available || !ValidTarget(context) || (active_ && context.now >= due_)) {
            if (active_ && !Marker(actor ? actor : context.peerAgent, context.now, true, out))
                return false;
            *this = TargetLink();
            return true;
        }
        if (!active_ && context.now >= due_) {
            if (context.now > UINT64_MAX - Rules::WindowMs)
                return false;
            due_ = context.now + Rules::WindowMs;
            active_ = true;
            return Marker(actor ? actor : context.peerAgent, context.now, false, out);
        }
        return true;
    }
    bool Consume(const WorldActionContext& context, Buffer& out, uint16_t actor = 0) {
        if (!Active(context.now) || !ValidTarget(context) ||
            !Marker(actor ? actor : context.peerAgent, context.now, true, out))
            return false;
        *this = TargetLink();
        return true;
    }
    bool Snapshot(uint16_t actor, uint64_t now, Buffer& out) const {
        return !Active(now) || Marker(actor, now, false, out);
    }
    bool Cleanup(uint16_t actor, Buffer& out) const {
        return !active_ || Marker(actor, 0, true, out);
    }
};
} // namespace glide
