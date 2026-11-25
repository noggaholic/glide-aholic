#pragma once
#include "glide/msgconn.h"
#include "glide/guid.h"
namespace glide {
struct GameJoinRequest {
    uint32_t association = 0, map = 0, context = 0;
    Guid user, character;
};
// Profiles opting into this interface always use identity-bound admission.
class GameInitializerCodec {
  public:
    virtual ~GameInitializerCodec() {}
    virtual bool ValidAuth(const InitHeader&) const = 0;
    virtual bool DecodeGame(const InitHeader&, GameJoinRequest&) const = 0;
    virtual bool Redirect(uint32_t association, uint16_t map, uint16_t port, Buffer& out) const = 0;
};
} // namespace glide
