#ifndef GLIDE_CLIENT_49284_INSTALLED_H
#define GLIDE_CLIENT_49284_INSTALLED_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Original MFST v6, PE32 packed arrays (not relocated native pointers).
 * Shape cross-checked against native MultiManifest.cpp importer and local
 * t3d MFST V6. No file IDs, properties, flags or textures are substituted. */
typedef struct Glide49284Array {
    uint32_t count;
    size_t start;
} Glide49284Array;
typedef struct Glide49284InstalledManifest {
    const uint8_t* bytes;
    size_t size;
    Glide49284Array records, streams, properties, bindings;
} Glide49284InstalledManifest;
typedef void(__fastcall* Glide49284PublishAssetFn)(uint32_t base_id, uint32_t file_id, uint32_t flags, uint32_t size);
typedef void(__fastcall* Glide49284PublishPropertyFn)(uint32_t base_id, uint32_t type, uint32_t size,
                                                      const uint8_t* data);
typedef int(__fastcall* Glide49284QueryAssetFn)(uint32_t base_id, uint32_t* file_id, uint32_t* flags, uint32_t* size,
                                                uint32_t* cached);
/* Query returns the mutable registry bit27 in both flags and cached. Compare immutable metadata while permitting native
 * cache population. */
static __inline int glide_49284_asset_metadata_matches(uint32_t revision, uint32_t flags, uint32_t size,
                                                       uint32_t cached, uint32_t expected_revision,
                                                       uint32_t expected_size) {
    return revision == expected_revision && size == expected_size && !(flags & ~0x08000000u) &&
           cached == ((flags >> 27) & 1u);
}
static __inline uint32_t glide_49284_word(const uint8_t* p) {
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static __inline int glide_49284_array(const uint8_t* bytes, size_t size, size_t at, unsigned stride,
                                      Glide49284Array* out) {
    int32_t relative;
    int64_t start;
    uint32_t count;
    if (!bytes || !out || !stride || at > size || size - at < 8)
        return 0;
    count = glide_49284_word(bytes + at);
    relative = (int32_t)glide_49284_word(bytes + at + 4);
    start = relative ? (int64_t)at + 4 + relative : 0;
    if (start < 0 || (uint64_t)start > size || (count && !start) || count > (size - (size_t)start) / stride)
        return 0;
    out->count = count;
    out->start = (size_t)start;
    return 1;
}
static __inline int glide_49284_installed_manifest(const uint8_t* bytes, size_t size,
                                                   Glide49284InstalledManifest* out) {
    Glide49284InstalledManifest m;
    size_t root;
    uint32_t i;
    if (!bytes || !out || size < 72 || size > 4u * 1024u * 1024u || memcmp(bytes, "PF\0\0", 4) ||
        memcmp(bytes + 8, "ARMFMFST", 8) || bytes[20] != 6 || bytes[21])
        return 0;
    root = 12u + bytes[22] + ((unsigned)bytes[23] << 8);
    if (root < 28 || root > size || size - root < 44 || glide_49284_word(bytes + root) != 49284)
        return 0;
    m.bytes = bytes;
    m.size = size;
    if (!glide_49284_array(bytes, size, root + 12, 16, &m.records) ||
        !glide_49284_array(bytes, size, root + 20, 8, &m.streams) ||
        !glide_49284_array(bytes, size, root + 28, 9, &m.properties) ||
        !glide_49284_array(bytes, size, root + 36, 8, &m.bindings))
        return 0;
    if ((m.records.count && m.records.start < root + 44) || (m.streams.count && m.streams.start < root + 44) ||
        (m.properties.count && m.properties.start < root + 44) || (m.bindings.count && m.bindings.start < root + 44))
        return 0;
    for (i = 0; i < m.records.count; ++i) {
        const uint8_t* r = bytes + m.records.start + (size_t)i * 16;
        if (!glide_49284_word(r) || !glide_49284_word(r + 4) || (glide_49284_word(r + 12) & 0x08000000u))
            return 0;
    }
    for (i = 0; i < m.properties.count; ++i) {
        Glide49284Array data;
        if (!glide_49284_array(bytes, size, m.properties.start + (size_t)i * 9 + 1, 1, &data) ||
            (data.count && data.start < root + 44))
            return 0;
    }
    for (i = 0; i < m.bindings.count; ++i) {
        const uint8_t* binding = bytes + m.bindings.start + (size_t)i * 8;
        if (!glide_49284_word(binding) || glide_49284_word(binding + 4) >= m.properties.count)
            return 0;
    }
    *out = m;
    return 1; /* Failure never publishes a partially validated view. */
}
static __inline void glide_49284_publish_manifest(const Glide49284InstalledManifest* m, Glide49284PublishAssetFn asset,
                                                  Glide49284PublishPropertyFn property) {
    uint32_t i;
    for (i = 0; i < m->records.count; ++i) {
        const uint8_t* r = m->bytes + m->records.start + (size_t)i * 16;
        asset(glide_49284_word(r), glide_49284_word(r + 4), glide_49284_word(r + 12), glide_49284_word(r + 8));
    }
    for (i = 0; i < m->bindings.count; ++i) {
        const uint8_t* binding = m->bytes + m->bindings.start + (size_t)i * 8;
        size_t at = m->properties.start + (size_t)glide_49284_word(binding + 4) * 9;
        Glide49284Array data;
        glide_49284_array(m->bytes, m->size, at + 1, 1, &data);
        property(glide_49284_word(binding), m->bytes[at], data.count, m->bytes + data.start);
    }
}
/* The coordinator was allocated by the original client (0x260 bytes). The
 * manifest stage is entered AFTER native main-archive prepare, BEFORE tasks.
 * Do not force a native stage/failure flag or destroy the object in Start(). */
static __inline int glide_49284_offline_stage_clean(const uint32_t* coordinator) {
    return coordinator && coordinator[0] == 1 && coordinator[1] == 0 && coordinator[0x18 / 4] == 2 &&
           !coordinator[0x30 / 4] && !coordinator[0x34 / 4] && !coordinator[0x38 / 4];
}
#endif
