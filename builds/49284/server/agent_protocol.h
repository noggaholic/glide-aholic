#ifndef GLIDE_AGENT_PROTOCOL49284_H
#define GLIDE_AGENT_PROTOCOL49284_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Local exploration relays validated native commands; it does not simulate authoritative physics or combat. Keep
 * optional poses and coordinate tokens in their own HoT wire representation, including absent optional vectors. */
typedef struct Glide49284AgentMessage {
    uint16_t id, agent, delay, words[2];
    uint32_t sequence, tick, token, report, input_token;
    float position[3];
    uint8_t has_position, state, mode, report_extra, input_size, input[80], tail[224];
    size_t tail_size;
} Glide49284AgentMessage;
typedef struct Glide49284AgentReader {
    const uint8_t* data;
    size_t size, at;
} Glide49284AgentReader;
static __inline int glide_49284_agent_take(Glide49284AgentReader* r, void* out, size_t n) {
    if (n > r->size - r->at)
        return 0;
    if (out)
        memcpy(out, r->data + r->at, n);
    r->at += n;
    return 1;
}
static __inline int glide_49284_agent_var(Glide49284AgentReader* r, uint32_t* out) {
    unsigned j;
    uint32_t value = 0;
    for (j = 0; j < 5; j++) {
        uint8_t b;
        if (!glide_49284_agent_take(r, &b, 1))
            return 0;
        if (j == 4 && (b & 240))
            return -1;
        value |= (uint32_t)(b & 127) << (7 * j);
        if (!(b & 128)) {
            *out = value;
            return 1;
        }
    }
    return -1;
}
static __inline int glide_49284_agent_floats(Glide49284AgentReader* r, float* out, unsigned n) {
    uint32_t bits[4];
    unsigned i;
    if (n > 4)
        return -1;
    if (!glide_49284_agent_take(r, bits, n * 4))
        return 0;
    for (i = 0; i < n; i++)
        if ((bits[i] & 0x7f800000u) == 0x7f800000u)
            return -1;
    if (out)
        memcpy(out, bits, n * 4);
    return 1;
}
#define HOT_AGENT_READ(p, n)                                                                                           \
    do {                                                                                                               \
        if (!glide_49284_agent_take(&r, (p), (n)))                                                                     \
            return 0;                                                                                                  \
    } while (0)
#define HOT_AGENT_VAR(v)                                                                                               \
    do {                                                                                                               \
        int s = glide_49284_agent_var(&r, &(v));                                                                       \
        if (s != 1)                                                                                                    \
            return s;                                                                                                  \
    } while (0)
#define HOT_AGENT_FLOATS(p, n)                                                                                         \
    do {                                                                                                               \
        int s = glide_49284_agent_floats(&r, (p), (n));                                                                \
        if (s != 1)                                                                                                    \
            return s;                                                                                                  \
    } while (0)
#define HOT_AGENT_BOOL(v)                                                                                              \
    do {                                                                                                               \
        HOT_AGENT_READ(&(v), 1);                                                                                       \
        if ((v) > 1)                                                                                                   \
            return -1;                                                                                                 \
    } while (0)
static __inline int glide_49284_agent_decode(const uint8_t* data, size_t size, Glide49284AgentMessage* out,
                                             size_t* consumed) {
    Glide49284AgentReader r = {data, size, 0};
    Glide49284AgentMessage next = {0};
    uint32_t value;
    uint8_t present, n;
    size_t tail = 0;
    float pose[4];
    if (!data || !out || !consumed)
        return -1;
    HOT_AGENT_READ(&next.id, 2);
    if (next.id < 3 || next.id > 20 || next.id == 10)
        return -1;
    if (next.id == 15) {
        HOT_AGENT_READ(&next.delay, 2);
        goto complete;
    }
    if (next.id == 16) {
        HOT_AGENT_VAR(next.sequence);
        HOT_AGENT_VAR(next.tick);
        HOT_AGENT_FLOATS(next.position, 3);
        HOT_AGENT_VAR(next.token);
        HOT_AGENT_READ(&next.state, 1);
        HOT_AGENT_READ(&next.mode, 1);
        HOT_AGENT_READ(&next.report_extra, 1);
        next.has_position = 1;
        goto complete;
    }
    HOT_AGENT_READ(&next.agent, 2);
    if (!next.agent)
        return -1;
    HOT_AGENT_VAR(next.sequence);
    if (next.id == 5) {
        HOT_AGENT_VAR(next.report);
        goto complete;
    }
    tail = r.at;
    HOT_AGENT_READ(&next.state, 1);
    if (next.id == 3)
        goto complete;
    if (next.id == 14) {
        HOT_AGENT_READ(NULL, 1);
        HOT_AGENT_FLOATS(NULL, 1);
        goto complete;
    }
    if (next.id == 19) {
        HOT_AGENT_FLOATS(next.position, 3);
        HOT_AGENT_VAR(next.token);
        next.has_position = 1;
        goto complete;
    }
    if (next.id == 20) {
        HOT_AGENT_FLOATS(pose, 4);
        memcpy(next.position, pose, 12);
        next.has_position = 1;
        HOT_AGENT_VAR(next.token);
        HOT_AGENT_FLOATS(NULL, 4);
        HOT_AGENT_READ(NULL, 2);
        goto complete;
    }
    if (next.id != 4) {
        HOT_AGENT_VAR(next.input_token);
        HOT_AGENT_READ(&n, 1);
        if (n > 80)
            return -1;
        next.input_size = n;
        HOT_AGENT_READ(next.input, n);
    }
    if (next.id == 4 || next.id == 7 || next.id == 8 || next.id == 9) {
        HOT_AGENT_BOOL(present);
        if (present) {
            HOT_AGENT_FLOATS(pose, 4);
            memcpy(next.position, pose, 12);
            next.has_position = 1;
            HOT_AGENT_VAR(next.token);
            HOT_AGENT_BOOL(present);
            if (present) {
                HOT_AGENT_FLOATS(NULL, 4);
            }
            HOT_AGENT_READ(NULL, 2);
        }
    }
    switch (next.id) {
    case 4:
    case 8:
    case 18:
        break;
    case 6:
        HOT_AGENT_READ(NULL, 2);
        break;
    case 7:
    case 12:
        HOT_AGENT_READ(NULL, 6);
        break;
    case 9:
        HOT_AGENT_FLOATS(NULL, 3);
        HOT_AGENT_READ(next.words, 4);
        HOT_AGENT_BOOL(present);
        if (present) {
            HOT_AGENT_VAR(value);
            HOT_AGENT_VAR(value);
            HOT_AGENT_BOOL(present);
            if (present) {
                HOT_AGENT_FLOATS(NULL, 3);
                HOT_AGENT_VAR(value);
            }
        }
        break;
    case 11:
        HOT_AGENT_FLOATS(NULL, 1);
        HOT_AGENT_READ(NULL, 4);
        break;
    case 13:
        HOT_AGENT_FLOATS(next.position, 3);
        HOT_AGENT_VAR(next.token);
        next.has_position = 1;
        HOT_AGENT_READ(NULL, 5);
        break;
    case 17:
        HOT_AGENT_READ(NULL, 4);
        break;
    default:
        return -1;
    }
complete:
    if (tail) {
        next.tail_size = r.at - tail;
        if (next.tail_size > sizeof(next.tail))
            return -1;
        memcpy(next.tail, data + tail, next.tail_size);
    }
    *out = next;
    *consumed = r.at;
    return 1;
}
#undef HOT_AGENT_READ
#undef HOT_AGENT_VAR
#undef HOT_AGENT_FLOATS
#undef HOT_AGENT_BOOL
static __inline uint16_t glide_49284_agent_response(uint16_t id) {
    switch (id) {
    case 3:
        return 21;
    case 4:
        return 22;
    case 6:
        return 23;
    case 7:
        return 24;
    case 8:
        return 41;
    case 9:
        return 43;
    case 11:
        return 47;
    case 12:
        return 48;
    case 13:
        return 51;
    case 14:
        return 56;
    case 17:
        return 62;
    case 18:
        return 66;
    case 19:
        return 67;
    case 20:
        return 68;
    default:
        return 0;
    }
}
static __inline int glide_49284_agent_encode(const Glide49284AgentMessage* message, uint16_t agent, uint32_t sequence,
                                             uint8_t* out, size_t capacity, size_t* written) {
    uint8_t bytes[240];
    uint16_t id;
    size_t at = 4;
    if (!message || !agent || !out || !written || !message->tail_size || message->tail_size > sizeof(message->tail))
        return 0;
    id = glide_49284_agent_response(message->id);
    if (!id)
        return 0;
    memcpy(bytes, &id, 2);
    memcpy(bytes + 2, &agent, 2);
    do {
        uint8_t b = (uint8_t)(sequence & 127);
        sequence >>= 7;
        bytes[at++] = (uint8_t)(b | (sequence ? 128 : 0));
    } while (sequence);
    memcpy(bytes + at, message->tail, message->tail_size);
    at += message->tail_size;
    if (at > capacity)
        return 0;
    memcpy(out, bytes, at);
    *written = at;
    return 1;
}
#endif
