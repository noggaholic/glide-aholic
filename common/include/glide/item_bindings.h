#pragma once
#include <cstdint>
#include <string>
namespace glide {
struct ItemBinding {
    uint32_t definition = 0;
    uint8_t onUse = 0, onAcquisition = 0;
};
bool LoadItemBindings(const std::wstring& file, unsigned build, std::string& error);
const ItemBinding* FindItemBinding(unsigned build, uint32_t definition);
} // namespace glide
