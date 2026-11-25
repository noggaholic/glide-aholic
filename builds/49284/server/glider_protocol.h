#ifndef GLIDE_GLIDER_PROTOCOL49284_H
#define GLIDE_GLIDER_PROTOCOL49284_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

/* S66 Agent input, Character command37. This encoder does not decide when a player can deploy or certify flight. */
static __inline int glide_49284_glider_encode(uint16_t agent, uint32_t sequence, int active, float physics_value,
                                              uint8_t* out, size_t capacity, size_t* written) {
    static const uint8_t effect[16] = {0x41, 0x07, 0x0c, 0xd1, 0x3a, 0x7a, 0xad, 0x48,
                                       0xb9, 0xe2, 0x83, 0x03, 0x64, 0x22, 0xe4, 0x58};
    uint8_t bytes[34];
    size_t at = 4;
    if (!agent || !sequence || !out || !written || (active != 0 && active != 1) || !isfinite(physics_value) ||
        (active ? physics_value <= 0 : physics_value != 0))
        return 0;
    bytes[0] = 66;
    bytes[1] = 0;
    memcpy(bytes + 2, &agent, 2);
    do {
        uint8_t b = (uint8_t)(sequence & 127);
        sequence >>= 7;
        bytes[at++] = (uint8_t)(b | (sequence ? 128 : 0));
    } while (sequence);
    bytes[at++] = 0x50; /* Explicit server command, with own authoritative bit. */
    bytes[at++] = 0;
    bytes[at++] = 22; /* Native input token0 and byte array length. */
    bytes[at++] = 0x37;
    bytes[at++] = (uint8_t)active;
    memcpy(bytes + at, &physics_value, 4);
    at += 4;
    memcpy(bytes + at, effect, 16);
    at += 16;
    if (at > capacity)
        return 0;
    memcpy(out, bytes, at);
    *written = at;
    return 1;
}
#endif
