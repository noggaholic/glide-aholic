#ifndef GLIDE_CLIENT_49284_ARCHIVE_H
#define GLIDE_CLIENT_49284_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

/* Native x86: directory selector in ECX, output in EDX, capacity on stack
 * (RET4). Cache open takes flags in ECX and filename in EDX (RET0).
 * Neither API is the main Gw2.dat publisher. */
typedef void(__fastcall* Glide49284DirectoryFn)(unsigned selector, wchar_t* output, unsigned capacity);
typedef void*(__fastcall* Glide49284CacheOpenFn)(unsigned flags, const wchar_t* path);

typedef struct Glide49284PrivateCache {
    const wchar_t* directory;
    const wchar_t* path;
    uintptr_t first_directory_return;
    uintptr_t second_directory_return;
} Glide49284PrivateCache;

/* Accept ONLY a launcher-owned working copy, not the original installation or
 * Documents. No file is opened, created or copied by this helper. */
static __inline int glide_49284_private_cache_paths(const wchar_t* exe, wchar_t* directory, size_t directory_capacity,
                                                    wchar_t* cache, size_t cache_capacity) {
    const wchar_t *leaf, *slot, *root, *digit;
    size_t length;
    int positive = 0;
    if (!exe || !directory || !cache || wcslen(exe) < 3 ||
        !((exe[0] >= L'A' && exe[0] <= L'Z') || (exe[0] >= L'a' && exe[0] <= L'z')) || exe[1] != L':' ||
        exe[2] != L'\\')
        return 0;
    leaf = wcsrchr(exe, L'\\');
    if (!leaf || wcscmp(leaf + 1, L"Gw2.exe"))
        return 0;
    slot = leaf;
    while (slot > exe && slot[-1] != L'\\')
        --slot;
    if (leaf - slot <= 7 || wcsncmp(slot, L"client-", 7))
        return 0;
    for (digit = slot + 7; digit < leaf; ++digit) {
        if (*digit < L'0' || *digit > L'9')
            return 0;
        if (*digit != L'0')
            positive = 1;
    }
    if (!positive)
        return 0;
    root = slot - 1;
    while (root > exe && root[-1] != L'\\')
        --root;
    if (slot - 1 - root != 13 || wcsncmp(root, L"runtime-49284", 13))
        return 0;
    length = (size_t)(leaf - exe);
    if (directory_capacity <= length || cache_capacity <= length + 10)
        return 0;
    wmemcpy(directory, exe, length);
    directory[length] = 0;
    wmemcpy(cache, exe, length);
    wmemcpy(cache + length, L"\\Local.dat", 11);
    return 1;
}

/* Native startup first selects two cache folders, then can MoveFileW between
 * their Local.dat paths. Route BOTH before that migration; every other caller
 * retains the stock directory API. Invalid scoped calls fail explicitly. */
static __inline int glide_49284_cache_directory(const Glide49284PrivateCache* cache, Glide49284DirectoryFn original,
                                                uintptr_t caller, unsigned selector, wchar_t* output,
                                                unsigned capacity) {
    unsigned expected = caller == cache->first_directory_return    ? 2u
                        : caller == cache->second_directory_return ? 1u
                                                                   : 0u;
    size_t length;
    if (!expected) {
        original(selector, output, capacity);
        return 1;
    }
    if (selector != expected || !output || !capacity || !cache->directory)
        return 0;
    length = wcslen(cache->directory);
    if (length >= capacity) {
        output[0] = 0;
        return 0;
    }
    wmemcpy(output, cache->directory, length + 1);
    return 1;
}

static __inline void* glide_49284_open_private_cache(const Glide49284PrivateCache* cache,
                                                     Glide49284CacheOpenFn original, unsigned flags) {
    if (!cache->path || !cache->path[0])
        return NULL;
    /* Native shareArchive bit8 selects read-only mode5. Only Local.dat clears
     * it: native mode3 opens read/write OPEN_ALWAYS and initializes a missing
     * archive through its original File3 initializer. No fabricated header. */
    return original(flags & ~8u, cache->path);
}

#endif
