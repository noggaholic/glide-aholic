#pragma once
#include "glide/character.h"
#include "glide/buffer.h"
namespace glide {
namespace china167409 {

inline unsigned AttunementLevel(unsigned state) {
    static const unsigned levels[] = {0, 1, 5, 11, 15};
    return state <= 4 ? levels[state] : 81;
}
inline unsigned AttunementTrait(unsigned state) {
    static const unsigned traits[] = {0, 207, 204, 205, 206};
    return state <= 4 ? traits[state] : 0;
}
inline bool SavedAttunement(const Character& saved, uint8_t& state) {
    const auto profession = saved.profileFields.find("profession");
    state = profession != saved.profileFields.end() && profession->second == "6" ? 1 : 0;
    if (!state)
        return true;
    const auto found = saved.profileFields.find("profession_state");
    if (found == saved.profileFields.end())
        return true;
    const auto& value = found->second;
    if (value.size() != 1 || value[0] < '1' || value[0] > '4')
        return false;
    const auto selected = static_cast<uint8_t>(value[0] - '0');
    if (saved.level < AttunementLevel(selected))
        return false;
    state = selected;
    return true;
}
inline bool AttunementSelection(uint16_t agent, uint8_t state, Buffer& out) {
    return agent && state >= 1 && state <= 4 && out.U16(477) && out.VarU32(agent) && out.VarU32(state);
}
} // namespace china167409
} // namespace glide
