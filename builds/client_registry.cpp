#include "../client/src/profile.h"

namespace glide {
const ClientProfile& Build204489Client();
const ClientProfile& June14505Client();
const ClientProfile& June14422Client();
const ClientProfile& Hot49284Client();
const ClientProfile& July14812Client();
namespace april14075 {
const ClientProfile& ClientProfile14075();
}
namespace china167409 {
const ClientProfile& ClientProfile167409();
}
namespace ClientRegistry {
const ClientProfile* Find(uint32_t build) {
    if (build == 167409)
        return &china167409::ClientProfile167409();
    if (build == 14075)
        return &april14075::ClientProfile14075();
    if (build == 204489)
        return &Build204489Client();
    if (build == 49284)
        return &Hot49284Client();
    if (build == 14505)
        return &June14505Client();
    if (build == 14422)
        return &June14422Client();
    if (build == 14812)
        return &July14812Client();
    return nullptr;
}
} // namespace ClientRegistry
} // namespace glide
