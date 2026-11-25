#pragma once
#include "glide/character.h"
#include "glide/buffer.h"
#include "glide/profile/legend_state.h"
namespace glide {
namespace hot49284 {
inline bool LegendSelection(uint16_t agent, const LegendState& state, uint32_t remaining, Buffer& out) {
    return agent && state.Enabled() && out.U16(339) && out.VarU32(agent) && out.VarU32(remaining) && out.U8(2) &&
           out.U8(state.First()) && out.U8(state.Second()) && out.U16(341) && out.VarU32(agent) &&
           out.VarU32(state.First());
}

inline unsigned AttunementLevel(unsigned state) {
    static const unsigned levels[] = {0, 1, 5, 13, 17};
    return state <= 4 ? levels[state] : 81;
}
inline unsigned AttunementTrait(unsigned state) {
    static const unsigned traits[] = {0, 207, 204, 205, 206};
    return state <= 4 ? traits[state] : 0;
}
inline bool SavedAttunement(const Character& saved, unsigned profession, uint8_t& state) {
    state = profession == 6 ? 1 : 0;
    const auto found = saved.profileFields.find("profession_state");
    if (found == saved.profileFields.end())
        return true;
    const auto& value = found->second;
    if (value.size() != 1 || value[0] < '0' || value[0] > '4')
        return false;
    const uint8_t selected = static_cast<uint8_t>(value[0] - '0');
    if (profession != 6)
        return selected == 0;
    if (!selected || saved.level < AttunementLevel(selected))
        return false;
    state = selected;
    return true;
}
inline bool AttunementSelection(uint16_t agent, uint8_t state, Buffer& out) {
    return agent && state >= 1 && state <= 4 && out.U16(341) && out.VarU32(agent) && out.VarU32(state);
}
} // namespace hot49284
} // namespace glide
