#include "world_managers.h"
#include "world_actor.h"
#include "world_protocol.h"
#include "elemental_unlock_rules.h"

namespace glide {
namespace june14505 {
namespace {
bool Header(Buffer& out, uint16_t message, uint32_t subject, unsigned build) {
    const auto native = WorldMessage(build, message);
    return native != UINT16_MAX && out.U16(native) && out.VarU32(subject);
}
bool Skillbar(unsigned build, uint16_t agent, Buffer& out) {
    return Header(out, 0x010c, agent, build) && Header(out, 0x0110, agent, build) && out.U8(5) &&
           Header(out, 0x010d, agent, build);
}
bool Inventory(unsigned build, uint16_t agent, uint32_t coins, Buffer& result, ByteView equipment,
               uint8_t bagSlots = 0) {
    if (bagSlots > 8)
        return false;
    if (!Header(result, 0x0084, agent, build) || !result.VarU32(coins) || !Header(result, 0x0087, agent, build) ||
        !result.VarU32(160) || !Header(result, 0x0080, agent, build) || !result.U8(34) ||
        !Header(result, 0x0086, agent, build) || !result.U8(1) || !result.U8(0) ||
        !Header(result, 0x0086, agent, build) || !result.U8(0) || !result.U8(4) || !result.Append(equipment) ||
        !Header(result, 0x0081, agent, build) || !result.U8(4) || !Header(result, 0x0083, agent, build) ||
        !result.U8(bagSlots) || !Header(result, 0x0085, agent, build) || !result.U8(0) ||
        !Header(result, 0x0077, agent, build))
        return false;
    return true;
}
bool Kennel(unsigned build, uint16_t agent, Buffer& result) {
    if (!Header(result, 0x008e, agent, build) || !result.U8(6) || !result.U8(0) || !result.U8(0))
        return false;
    for (uint8_t slot = 0; slot < 4; ++slot)
        if (!Header(result, 0x0093, agent, build) || !result.U8(slot) || !result.U16(0) || !result.VarU32(0) ||
            !result.VarU32(0))
            return false;
    return Header(result, 0x008f, agent, build);
}

} // namespace
bool ProfessionStateForBuild(unsigned build, uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out) {
    if (!WorldProtocolBuild(build) || !agent || !effectiveLevel || effectiveLevel > 80)
        return false;
    Buffer result;
    bool ok = false;
    switch (profession) {
    case 1:
        ok = Header(result, 0x00cc, agent, build) && result.F32(4) && result.F32(0) && result.F32(4);
        break;
    case 2:
        ok = Header(result, 0x00dd, agent, build) &&
             result.VarU32(effectiveLevel >= 6   ? 30
                           : effectiveLevel >= 3 ? 20
                                                 : 10) &&
             result.VarU32(0);
        break;
    case 3:
        ok = Header(result, 0x00c6, agent, build) && result.VarU32(1000) && result.VarU32(0);
        break;
    case 4:
        ok = Header(result, 0x00d5, agent, build) && result.F32(100) && result.F32(-3) && result.F32(0);
        break;
    case 5:
        ok = Header(result, 0x00d9, agent, build) && result.F32(12) && result.F32(0.6f) && result.F32(12);
        break;
    case 6:
        ok = Header(result, 0x00c9, agent, build) && result.VarU32(8) && result.VarU32(8) &&
             Header(result, 0x00d8, agent, build) && result.VarU32(1);
        break;
    case 7:
        ok = Header(result, 0x00cf, agent, build) && result.U8(1) && result.U8(0);
        break;
    case 8:
        ok = Header(result, 0x00d2, agent, build) && result.F32(0) && result.F32(245.0f * 0.75f);
        break;
    default:
        return false;
    }
    if (!ok)
        return false;
    out = std::move(result);
    return true;
}
bool InitialElementalTraitsForBuild(unsigned build, uint8_t profession, unsigned level, Buffer& out) {
    if ((build != 14075 && build != 14422 && build != 14505) || profession < 1 || profession > 8 || !level ||
        level > 80)
        return false;
    if (profession != 6)
        return true;
    // April activation uses S239 (S241 removes); both Junios use S292.
    Buffer pending;
    for (const auto& rule : kOwnJuneElementalUnlocks)
        if (level >= rule.minimumLevel)
            if (!pending.U16(build == 14075 ? 239 : 292) || !pending.VarU32(rule.trait))
                return false;
    return out.Append(pending.View());
}
bool LearnWeaponSkills(const std::vector<uint32_t>& skills, Buffer& out) {
    return LearnWeaponSkillsForBuild(14505, skills, out);
}
bool LearnWeaponSkillsForBuild(unsigned build, const std::vector<uint32_t>& skills, Buffer& out) {
    if (!WorldProtocolBuild(build) || skills.empty() || skills.size() > 64)
        return false;
    Buffer result;
    for (size_t i = 0; i < skills.size(); ++i) {
        if (!skills[i])
            return false;
        for (size_t j = 0; j < i; ++j)
            if (skills[j] == skills[i])
                return false;
        // Owned Player message, no agent ID. Zero suppresses the learning notification.
        if (!result.U16(build == 14075 ? 212 : 264) || !result.VarU32(skills[i]) || !result.U8(0))
            return false;
    }
    out = std::move(result);
    return true;
}
bool RefreshEmptySkillbar(uint16_t agent, Buffer& out) {
    if (!agent)
        return false;
    Buffer result;
    // Existing skillbar must be destroyed before the native constructor runs again.
    if (!Header(result, 0x010e, agent, 14505) || !Skillbar(14505, agent, result))
        return false;
    out = std::move(result);
    return true;
}
bool PeerManagersForBuild(unsigned build, uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out,
                          ByteView equipment) {
    Buffer result, professionBytes;
    if (!ProfessionStateForBuild(build, agent, profession, effectiveLevel, professionBytes) ||
        !Inventory(build, agent, 0, result, equipment) || !result.Append(professionBytes.View()) ||
        (profession == 4 && !Kennel(build, agent, result)) || !Header(result, 0x0043, agent, build) ||
        !Header(result, 0x0041, agent, build) || !result.U8(0))
        return false;
    out = std::move(result);
    return true;
}
bool OwnedManagersForBuild(unsigned build, uint8_t player, uint16_t agent, uint8_t profession, unsigned effectiveLevel,
                           uint32_t coins, const std::array<uint32_t, 3>& achievementTotals, Buffer& out,
                           ByteView equipment, uint8_t inventoryBagSlots) {
    Buffer result, professionBytes, control;
    if (!player || !ProfessionStateForBuild(build, agent, profession, effectiveLevel, professionBytes) ||
        !PlayerControlForBuild(build, player, agent, control))
        return false;
    // Final inventory state must retain the bag slots installed by equipment.
    if (!Inventory(build, agent, coins, result, equipment, inventoryBagSlots) || !Kennel(build, agent, result))
        return false;
    if (!result.Append(professionBytes.View()) || !Skillbar(build, agent, result) ||
        !Header(result, 0x00ad, player, build) || !result.U8(0) || !result.VarU32(0) ||
        !Header(result, 0x0116, player, build) || !result.U8(0) || !Header(result, 0x00bc, player, build) ||
        !result.U8(0) || !Header(result, 0x00bb, player, build) || !result.VarU32(0) ||
        !Header(result, 0x0039, player, build))
        return false;
    for (uint32_t total : achievementTotals)
        if (!result.VarU32(total))
            return false;
    if (!Header(result, 0x0054, player, build) || !result.U16(0) || !Header(result, 0x0058, player, build) ||
        !result.U8(0))
        return false;
    for (uint8_t discipline = 0; discipline < 8; ++discipline)
        if (!Header(result, 0x0059, player, build) || !result.U8(discipline) || !result.VarU32(0) || !result.VarU32(0))
            return false;
    if (!Header(result, 0x005a, player, build) || !Header(result, 0x00ea, player, build) ||
        !Header(result, 0x00ec, player, build) || !Header(result, 0x00bd, player, build) || !result.U8(2) ||
        !Header(result, 0x0043, agent, build) || !Header(result, 0x0041, agent, build) || !result.U8(0) ||
        !result.Append(control.View()))
        return false;
    out = std::move(result);
    return true;
}
bool ProfessionState(uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out) {
    return ProfessionStateForBuild(14505, agent, profession, effectiveLevel, out);
}
bool PeerManagers(uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out, ByteView equipment) {
    return PeerManagersForBuild(14505, agent, profession, effectiveLevel, out, equipment);
}
bool OwnedManagers(uint8_t player, uint16_t agent, uint8_t profession, unsigned effectiveLevel, uint32_t coins,
                   const std::array<uint32_t, 3>& totals, Buffer& out, ByteView equipment) {
    return OwnedManagersForBuild(14505, player, agent, profession, effectiveLevel, coins, totals, out, equipment);
}
} // namespace june14505
} // namespace glide
