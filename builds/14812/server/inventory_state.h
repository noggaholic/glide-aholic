#pragma once
#include "character_catalog.h"
#include "../../14505/server/inventory_state.h"

namespace glide {
namespace july14812 {
using InventoryState = StarterInventoryState<12>;
using InventoryRequest = june14505::InventoryRequest;
DecodeStatus DecodeInventoryRequest(ByteView input, InventoryRequest& out);
bool InitializeInventory(const CharacterCatalog& catalog, Character& character, std::string& error);
bool LoadInventoryState(const Character&, const CharacterCatalog&, InventoryState&, std::string& error);
bool InitialInventoryContents(const Character&, const CharacterCatalog&, uint32_t itemBase, uint16_t agent,
                              Buffer& out);
bool CreateInventoryActions(const Character&, const CharacterCatalog&, uint32_t itemBase, uint16_t agent,
                            std::unique_ptr<WorldActions>& out, std::string& error);
} // namespace july14812
} // namespace glide
