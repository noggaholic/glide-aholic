#include "glide/profile/build_profile.h"

namespace glide {
const BuildProfile& Build204489Profile();
const BuildProfile& June14505Profile();
const BuildProfile& June14422Profile();
const BuildProfile& Hot49284Profile();
const BuildProfile& July14812Profile();
const BuildProfile& April14075Profile();
const BuildProfile& China167409Profile();
namespace BuildRegistry {
const BuildProfile* Find(uint32_t build) {
    if (build == 14075)
        return &April14075Profile();
    if (build == 204489)
        return &Build204489Profile();
    if (build == 14505)
        return &June14505Profile();
    if (build == 14422)
        return &June14422Profile();
    if (build == 49284)
        return &Hot49284Profile();
    if (build == 14812)
        return &July14812Profile();
    if (build == 167409)
        return &China167409Profile();
    return nullptr;
}
} // namespace BuildRegistry
} // namespace glide
