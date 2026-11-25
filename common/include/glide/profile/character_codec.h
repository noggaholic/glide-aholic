#pragma once
#include "glide/character.h"

namespace glide {
// Shared by account selection and world authorization, independent of Auth dispatch.
class CharacterCodec {
  public:
    virtual ~CharacterCodec() {}
    virtual bool Summary(const Character& character, Buffer& out) const = 0;
    // Called only for a new character, before its creation is saved and published.
    // Loading an existing character must never initialize or replace its inventory.
    virtual bool InitializeInventory(Character&, std::string& error) const {
        error.clear();
        return true;
    }
};
} // namespace glide
