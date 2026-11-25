#pragma once
#include "character_catalog.h"
#include "glide/profile/create_flow.h"
namespace glide {
namespace july14812 {
const SpawnPoint& InitialSpawn();
// Shared scene/diagnostic controls only; never interprets a character submission.
DecodeStatus DecodeSceneControl(ByteView, CreateMessage&);
bool CreateCharacterFlow(CharacterCatalog, const SpawnPoint&, std::unique_ptr<CreateFlow>&, std::string& error);
bool CreateCharacterFlow(const std::wstring& dataRoot, const SpawnPoint&, std::unique_ptr<CreateFlow>&,
                         std::string& error);
} // namespace july14812
} // namespace glide
