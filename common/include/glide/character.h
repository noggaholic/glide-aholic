#pragma once
#include "glide/guid.h"
#include "glide/kv.h"
#include <array>
#include <map>

namespace glide {
struct SpawnPoint {
    unsigned map;
    float x, y, z;
};
struct Character {
    unsigned slot = 0, level = 0, map = 0, glider = 0, mount = 0;
    float x = 0, y = 0, z = 0;
    Guid guid;
    std::string name;
    std::map<std::string, std::string> profileFields;
    std::vector<uint8_t> native;
};
struct Marker {
    std::string name;
    unsigned map = 0;
    float x = 0, y = 0, z = 0;
};
bool ValidMarkerName(const std::string& name);
bool MarkerNameEqual(const std::string& a, const std::string& b);
struct AccountState {
    Guid userGuid;
    unsigned coins = 0, karma = 0, homeWorld = 2102, accessLevel = 2;
    unsigned worldClass = 1, instanceType = 4, selected = 0;
    std::vector<Character> characters;
    std::vector<Marker> markers;
    std::array<uint32_t, 8> completedHints{};
    KvDocument document;
};
bool DecodeAccountState(const KvDocument& document, AccountState& out, std::string& error);
bool DecodeHex(const std::string& text, size_t maxBytes, std::vector<uint8_t>& out);
} // namespace glide
