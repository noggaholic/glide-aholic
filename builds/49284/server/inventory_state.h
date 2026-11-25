#pragma once
#include "character_catalog.h"
#include "world_actor.h"
#include "playable_loadouts.h"
#include "../../14505/server/inventory_state.h"

namespace glide {
namespace hot49284 {
using InventoryState = StarterInventoryState<12>;
using InventoryRequest = june14505::InventoryRequest;
DecodeStatus DecodeInventoryRequest(ByteView input, InventoryRequest& out);
const Hot49284Loadout* FindInventoryLoadout(const Character&, const CharacterCatalog&);
bool InitializeInventory(const CharacterCatalog& catalog, Character& character, std::string& error);
bool LoadInventoryState(const Character&, const CharacterCatalog&, InventoryState&, std::string& error);
bool PrepareInventoryActor(const Character&, const CharacterCatalog&, WorldActor&, std::string& error);
bool InitialInventoryContents(const Character&, const CharacterCatalog&, uint32_t itemBase, uint16_t agent,
                              Buffer& out);
bool CreateInventoryActions(const Character&, const CharacterCatalog&, uint32_t itemBase, uint16_t agent,
                            std::unique_ptr<WorldActions>& out, std::string& error);
} // namespace hot49284
} // namespace glide
