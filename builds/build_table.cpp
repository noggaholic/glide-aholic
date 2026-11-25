#include "glide/builds.h"
#include "204489/build.h"
#include "14505/build.h"
#include "49284/build.h"
#include "14812/build.h"
#include "14422/build.h"
#include "14075/build.h"
#include "167409/build.h"
namespace glide {
namespace {
const BuildInfo* const entries[] = {&kBuild204489, &kBuild14505, &kBuild49284, &kBuild14812,
                                    &kBuild14422,  &kBuild14075, &kBuild167409};
}
size_t BuildTable::Count() {
    return sizeof(entries) / sizeof(entries[0]);
}
const BuildInfo* BuildTable::At(size_t i) {
    return i < Count() ? entries[i] : nullptr;
}
const BuildInfo* BuildTable::Find(uint32_t build) {
    for (const auto* entry : entries)
        if (entry->build == build)
            return entry;
    return nullptr;
}
} // namespace glide
