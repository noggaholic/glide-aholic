
#include "protocol.h"

namespace glide {
namespace china167409 {
uint16_t ServerMessageId(uint16_t canonical) {
    switch (canonical) {
    case 0x00b6:
        return 0x00b5; /* full character core */
    case 0x00b7:
        return 0x00b6; /* remote character core */
    case 0x00aa:
        return 0x00a9; /* character finalization */
    case 0x00d8:
        return 0x00d6; /* endurance */
    case 0x00f6:
        return 0x00e8; /* equipped glider design */
    case 0x00ff:
        return 0x00f1; /* character health */
    case 0x0103:
        return 0x00f5; /* health reference */
    case 0x0119:
        return 0x010b; /* inventory create */
    case 0x011a:
        return 0x010c; /* inventory ready */
    case 0x0120:
        return 0x0112; /* inventory placement */
    case 0x0130:
        return 0x0122; /* inventory capacity */
    case 0x013c:
        return 0x012e; /* gear loadout */
    case 0x014b:
        return 0x013d; /* kennel create */
    case 0x014c:
        return 0x013e; /* kennel ready */
    case 0x0152:
        return 0x0144; /* kennel equipment slot */
    case 0x01ab:
        return 0x0199; /* player create */
    case 0x01ad:
        return 0x019b; /* player removal */
    case 0x01ba:
        return 0x01a8; /* attach owned character */
    case 0x01bb:
        return 0x01a9; /* control character */
    case 0x01d5:
        return 0x01c2; /* engineer */
    case 0x01d9:
        return 0x01c6; /* elementalist */
    case 0x01dd:
        return 0x01ca; /* guardian */
    case 0x01e1:
        return 0x01ce; /* mesmer */
    case 0x01e4:
        return 0x01d1; /* necromancer */
    case 0x01e8:
        return 0x01d5; /* ranger */
    case 0x01ed:
        return 0x01d9; /* revenant */
    case 0x01ef:
        return 0x01db; /* legend pair and shared recharge */
    case 0x01f1:
        return 0x01dd; /* active profession state */
    case 0x01f0:
        return 0x01dc; /* revenant energy */
    case 0x01f2:
        return 0x01de; /* thief */
    case 0x01f8:
        return 0x01e4; /* warrior */
    case 0x0204:
        return 0x01f0; /* PvP manager */
    case 0x021a:
        return 0x0206; /* quest manager ready */
    case 0x0235:
        return 0x0221; /* reward-track manager */
    case 0x026a:
        return 0x0256; /* skillbar create */
    case 0x026b:
        return 0x0257; /* skillbar ready */
    case 0x0292:
        return 0x027d; /* character definition state */
    case 0x040d:
        return 0x03ee; /* begin playing */
    case 0x0410:
        return 0x03f1; /* mission character name */
    case 0x041e:
        return 0x03ff; /* collision scene setup */
    case 0x0425:
        return 0x0406; /* load map */
    case 0x0426:
        return 0x0407; /* agents ready */
    case 0x0427:
        return 0x0408; /* map context ready request */
    case 0x0428:
        return 0x0409; /* map data */
    case 0x042a:
        return 0x040b; /* map transfer redirect */
    case 0x042b:
        return 0x040c; /* map load position */
    case 0x0476:
        return 0x0163; /* player discovery data */
    case 0x04bc:
        return 0x049b; /* mount association */
    case 0x04bd:
        return 0x049c; /* mount removal */
    case 0x04ce:
        return 0x04ac; /* sector discovery data */
    case 0x04dd:
        return 0x04bb; /* time of day */
    default:
        return canonical;
    }
}
bool RemapServerMessage(Buffer& payload, size_t offset) {
    if (offset > payload.Size() || payload.Size() - offset < 2)
        return false;
    auto* bytes = payload.Data() + offset;
    const auto id = static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
    const auto mapped = ServerMessageId(id);
    bytes[0] = static_cast<uint8_t>(mapped);
    bytes[1] = static_cast<uint8_t>(mapped >> 8);
    return true;
}
} // namespace china167409
} // namespace glide
