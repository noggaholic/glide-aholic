#pragma once
#include "glide/profile/character_codec.h"
#include "glide/profile/world_actions.h"
#include "glide/profile/modern_condition_data.h"
#include "glide/profile/modern_conditions.h"
#include "glide/profile/modern_demon_healing_data.h"
#include "glide/profile/modern_health_maximum.h"
#include <algorithm>
#include <vector>
#include "glide/files.h"
#include <cmath>
#include <cstring>

namespace glide {
namespace modern {
// Preserve the maximum already published by this profile's accepted bootstrap.
// Complete profession, equipment and combat modifiers remain separate requirements.
inline bool ReadHealthState(const std::string& text, float& out, float maximum = kPublishedMaximumHealth) {
    if (!std::isfinite(maximum) || maximum <= 0)
        return false;
    if (text.empty()) {
        out = maximum;
        return true;
    }
    if (text.size() < 3 || text.size() > 12 || text.compare(0, 2, "1:") != 0)
        return false;
    const auto token = text.substr(2);
    if (token.size() > 1 && token[0] == '0')
        return false;
    for (char ch : token)
        if (ch < '0' || ch > '9')
            return false;
    unsigned bits = 0;
    if (!ParseUnsigned(token, UINT32_MAX, bits))
        return false;
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value) || std::signbit(value) || value > maximum)
        return false;
    out = value;
    return true;
}
inline bool ReadHealthState(const Character& saved, float& out, float maximum = kPublishedMaximumHealth) {
    const auto found = saved.profileFields.find("health_state");
    return ReadHealthState(found == saved.profileFields.end() ? std::string() : found->second, out, maximum);
}
inline bool WriteHealthState(float value, std::string& out, float maximum = kPublishedMaximumHealth) {
    if (!std::isfinite(maximum) || maximum <= 0 || !std::isfinite(value) || std::signbit(value) || value > maximum)
        return false;
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    out = "1:" + std::to_string(bits);
    return true;
}
struct PendingHeal {
    uint64_t startsAt = 0;
    uint32_t skill = 0;
    int32_t amount = 0, perCondition = 0;
};
inline bool SupportsConditionHealing(unsigned build) {
    return build == 49284 || build == 167409 || build == 204489;
}
inline unsigned DwarfConditionRemovalDue(const std::vector<PendingHeal>& pending, const WorldActionContext& context,
                                         unsigned removalLimit = kDwarfConditionRemovalLimit,
                                         float maximum = kPublishedMaximumHealth) {
    float health = 0;
    if (!ReadHealthState(context.healthState, health, maximum) || health <= 0)
        return 0;
    for (const auto& heal : pending)
        if (heal.skill == 27372 && context.now >= heal.startsAt)
            return removalLimit;
    return 0;
}
inline bool HealsDue(const std::vector<PendingHeal>& pending, uint64_t now) {
    for (const auto& heal : pending)
        if (now >= heal.startsAt)
            return true;
    return false;
}
inline void CancelPendingHeals(std::vector<PendingHeal>& pending, uint64_t now) {
    pending.erase(
        std::remove_if(pending.begin(), pending.end(), [&](const PendingHeal& heal) { return heal.startsAt > now; }),
        pending.end());
}
template <unsigned Build> bool HealthDelta(uint16_t actor, uint32_t skill, float delta, uint16_t source, Buffer& out) {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported health dialect");

    return out.U16(Build == 49284    ? 179
                   : Build == 167409 ? 240
                                     : 254) &&
           out.VarU32(actor) && out.F32(delta) && (Build == 49284 || out.F32(0)) && out.U16(0) && out.U8(0) &&
           out.VarU32(skill) && out.VarU32(source);
}
template <unsigned Build> bool HealthChange(uint16_t actor, uint32_t skill, float delta, Buffer& out) {
    return HealthDelta<Build>(actor, skill, delta, actor, out);
}
// Prepare copies; controllers persist health and effects before committing the list or packets.
template <unsigned Build>
bool PrepareSelfHeals(const std::vector<PendingHeal>& pending, const WorldActionContext& context, uint16_t peer,
                      std::vector<PendingHeal>& remaining, float& previous, float& current, WorldActionOutput& result,
                      uint16_t owner = 57344, float maximum = kPublishedMaximumHealth) {
    if (!ReadHealthState(context.healthState, previous, maximum))
        return false;
    current = previous;
    for (const auto& heal : pending) {
        if (context.now < heal.startsAt) {
            remaining.push_back(heal);
            continue;
        }
        if (current == 0)
            continue; // A heal cannot resurrect a defeated character.
        int32_t amount = heal.amount;
        if (heal.perCondition) {
            ConditionState<Build> conditions;
            // All admitted dialects 28219. HoT supplies its 3320/980
            // model through PendingHeal; no modern quantities are substituted.
            if (!SupportsConditionHealing(Build) || heal.skill != DemonHealingData<204489>::Skill ||
                heal.perCondition < 0 || !context.wallNow || !conditions.Load(context.conditionState))
                return false;
            const uint64_t total = static_cast<uint64_t>(amount) +
                                   static_cast<uint64_t>(heal.perCondition) * conditions.ActiveKinds(context.wallNow);
            if (total > INT32_MAX)
                return false;
            amount = static_cast<int32_t>(total);
        }
        const float next = (std::min)(maximum, current + static_cast<float>(amount));
        const float delta = next - current;
        if (delta > 0 && (!HealthChange<Build>(owner, heal.skill, delta, result.owned) ||
                          !HealthChange<Build>(peer, heal.skill, delta, result.peers)))
            return false;
        current = next;
    }
    return true;
}
} // namespace modern
} // namespace glide
