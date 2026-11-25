#pragma once
#include <cstdint>
#include <initializer_list>

namespace glide {
enum class Capability : uint32_t {
    FreeCamera,
    Fog,
    Glider,
    Mounts,
    MapReveal,
    Waypoints,
    TimeOfDay,
    LocalTeleport,
    MapTransfer,
    CharacterDelete,
    EquipmentPublish,
    Markers,
    Announce,
    Inspect,
    FacetActions,
    CombatTraitSelection,
    Count
};
class CapabilitySet {
  public:
    bool Has(Capability capability) const {
        const auto index = static_cast<uint32_t>(capability);
        return index < static_cast<uint32_t>(Capability::Count) && (bits_ & (uint64_t(1) << index)) != 0;
    }
    static CapabilitySet Of(std::initializer_list<Capability> capabilities) {
        CapabilitySet result;
        for (const auto capability : capabilities) {
            const auto index = static_cast<uint32_t>(capability);
            if (index < static_cast<uint32_t>(Capability::Count))
                result.bits_ |= uint64_t(1) << index;
        }
        return result;
    }

  private:
    uint64_t bits_ = 0;
};
} // namespace glide
