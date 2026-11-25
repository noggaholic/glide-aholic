#ifndef GLIDE_CURRENT_BUILD_49284_H
#define GLIDE_CURRENT_BUILD_49284_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct Glide49284InstalledMetadata {
    uint32_t executable_build;
    uint32_t executable_size;
    uint32_t root_build;
    uint32_t root_revision;
    uint32_t root_size;
} Glide49284InstalledMetadata;

/* ONLY the native equal-build branch, proven in audit_startup_descriptor_49284.
 * Executable revision0 expresses no executable transfer; it is NOT a recovered
 * native revision. This API cannot advertise another build or an update.
 * Caller must validate metadata against the original EXE/DAT before serving. */
static __inline int glide_49284_current_descriptor(const Glide49284InstalledMetadata* installed,
                                                   uint32_t requested_program, uint32_t running_build, char* output,
                                                   size_t capacity, size_t* size) {
    char text[128];
    int count;
    if (!installed || !output || !size || requested_program != 101 || running_build != 49284 ||
        installed->executable_build != running_build || installed->root_build != running_build ||
        !installed->executable_size || !installed->root_revision || !installed->root_size)
        return 0;
    count = snprintf(text, sizeof(text), "%u 0 %u %u %u", running_build, installed->executable_size,
                     installed->root_revision, installed->root_size);
    if (count < 0 || (size_t)count >= sizeof(text) || (size_t)count >= capacity)
        return 0;
    memcpy(output, text, (size_t)count + 1);
    *size = (size_t)count;
    return 1;
}
#endif
