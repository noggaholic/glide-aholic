#ifndef GLIDE_14812_RESOURCE_INDEX_H
#define GLIDE_14812_RESOURCE_INDEX_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Strict format for authored local metadata, not a generic historical reader. */
#define JULY_RESOURCE_PRIVATE_ID 0x00f00000u
typedef struct JulyResourceIndex {
    const uint8_t *bytes, *rows;
    size_t size;
    uint32_t count;
} JulyResourceIndex;
static __inline uint32_t july_resource_word(const uint8_t* p) {
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static __inline int july_resource_header(const uint8_t* b, size_t size, const char* tag, unsigned version) {
    return b && size >= 48 && size <= 16u * 1024u * 1024u && !memcmp(b, "PF\0\0\0\0\x0c\0ARMF", 12) &&
           !memcmp(b + 12, tag, 4) && july_resource_word(b + 16) == size - 20 && b[20] == version && !b[21] &&
           b[22] == 16 && !b[23] && july_resource_word(b + 28) == 14812;
}
static __inline int july_resource_manifest(const uint8_t* b, size_t size, JulyResourceIndex* out) {
    JulyResourceIndex v;
    uint32_t i, last = 16;
    if (!out || !july_resource_header(b, size, "MFST", 2) || size < 64)
        return 0;
    v.bytes = b;
    v.size = size;
    v.count = july_resource_word(b + 32);
    v.rows = b + 56;
    if (!v.count || v.count > 1000000 || july_resource_word(b + 36) != 20 || (size - 64) / 12 != v.count ||
        (size - 64) % 12 || july_resource_word(b + 40) || july_resource_word(b + 44) || july_resource_word(b + 48) ||
        july_resource_word(b + 52))
        return 0;
    if (july_resource_word(b + 24) != size - 36 || july_resource_word(b + size - 8) != 1 ||
        july_resource_word(b + size - 4) != 8)
        return 0;
    for (i = 0; i < v.count; ++i) {
        const uint8_t* r = v.rows + (size_t)i * 12;
        uint32_t id = july_resource_word(r), revision = july_resource_word(r + 4), length = july_resource_word(r + 8);
        if (id <= last || id >= JULY_RESOURCE_PRIVATE_ID || revision < id || revision >= JULY_RESOURCE_PRIVATE_ID ||
            !length || length > 512u * 1024u * 1024u)
            return 0;
        last = id;
    }
    *out = v;
    return 1;
}
static __inline int july_resource_root(const uint8_t* b, size_t size, size_t manifest_size, JulyResourceIndex* out) {
    JulyResourceIndex v;
    uint32_t i;
    size_t next, end;
    if (!out || !july_resource_header(b, size, "ARMF", 1))
        return 0;
    v.bytes = b;
    v.size = size;
    v.rows = b + 48;
    v.count = july_resource_word(b + 32);
    if (!v.count || v.count > 32 || july_resource_word(b + 36) != 12 || july_resource_word(b + 40) ||
        july_resource_word(b + 44) || v.count > (size - 48) / 20)
        return 0;
    if (july_resource_word(b + 24) > size - 28)
        return 0;
    end = 28 + (size_t)july_resource_word(b + 24);
    if (end < 48 + (size_t)v.count * 20 || size - end != 8 + 4 * (size_t)v.count ||
        july_resource_word(b + end) != v.count + 1 || july_resource_word(b + end + 4) != 8)
        return 0;
    for (i = 0; i < v.count; ++i)
        if (july_resource_word(b + end + 8 + i * 4) != 36 + i * 20)
            return 0;
    next = 48 + (size_t)v.count * 20;
    for (i = 0; i < v.count; ++i) {
        const uint8_t* r = v.rows + (size_t)i * 20;
        size_t at;
        uint32_t relative = july_resource_word(r + 16);
        if (july_resource_word(r) != JULY_RESOURCE_PRIVATE_ID + i ||
            july_resource_word(r + 4) != JULY_RESOURCE_PRIVATE_ID + i || july_resource_word(r + 8) != manifest_size ||
            july_resource_word(r + 12) || relative > end - (size_t)(r + 16 - b))
            return 0;
        at = (size_t)(r + 16 - b) + relative;
        if (at != next || end - at < 8 || memcmp(b + at, "M\0a\0p\0", 6))
            return 0;
        at += 6;
        while (at + 2 <= end && (b[at] || b[at + 1])) {
            uint8_t c = b[at];
            if (b[at + 1] || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
                return 0;
            at += 2;
            if (at - next > 128)
                return 0;
        }
        if (at + 2 > end || at == next + 6)
            return 0;
        next = at + 2;
    }
    if (next != end)
        return 0;
    *out = v;
    return 1;
}
#endif
