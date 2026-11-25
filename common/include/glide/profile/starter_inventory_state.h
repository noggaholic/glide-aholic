#pragma once
#include "glide/profile/auth_codec.h"
#include <array>
#include <string>

namespace glide {
enum class InventoryChangeResult { Changed, Unchanged, Missing, WrongLocation, InvalidSlot, Occupied };

// Persistent locations and binding of original starter objects. Native packet
// formats, object ownership and compatible equipment slots belong to the profile.
template <size_t N> struct StarterInventoryState {
    static_assert(N > 0, "An inventory must have object slots");
    std::array<int, N> locations{}; // -2 absent, -1 equipped, nonnegative bag cell.
    std::array<bool, N> bound{};
    StarterInventoryState() {
        locations.fill(-2);
    }

    template <class DefinitionId>
    static bool Load(const Character& saved, const std::array<DefinitionId, N>& definitions, unsigned capacity,
                     StarterInventoryState& out, std::string& error) {
        if (!capacity || capacity > 480) {
            error = "Invalid inventory capacity";
            return false;
        }
        StarterInventoryState next;
        next.capacity_ = capacity;
        for (size_t i = 0; i < N; ++i)
            next.locations[i] = definitions[i] ? -1 : -2;
        const auto field = saved.profileFields.find("inventory_locations");
        if (field != saved.profileFields.end()) {
            if (field->second.size() > N * 4 - 1) {
                error = "Invalid inventory locations";
                return false;
            }
            size_t start = 0;
            std::array<bool, 480> used{};
            for (size_t i = 0; i < N; ++i) {
                const size_t end = field->second.find(',', start);
                if ((i + 1 < N) == (end == std::string::npos)) {
                    error = "Invalid inventory locations";
                    return false;
                }
                const std::string token =
                    field->second.substr(start, end == std::string::npos ? std::string::npos : end - start);
                int location = -2;
                if (token == "e")
                    location = -1;
                else if (token != "x") {
                    unsigned value = 0;
                    if (!ParseUnsigned(token, capacity - 1, value) || std::to_string(value) != token || used[value]) {
                        error = "Invalid or repeated inventory slot";
                        return false;
                    }
                    used[value] = true;
                    location = static_cast<int>(value);
                }
                if ((definitions[i] == 0) != (location == -2)) {
                    error = "Inventory object is absent from its original recipe";
                    return false;
                }
                next.locations[i] = location;
                start = end == std::string::npos ? field->second.size() : end + 1;
            }
        }
        const auto binding = saved.profileFields.find("inventory_bound");
        if (binding != saved.profileFields.end()) {
            const std::string& text = binding->second;
            if (text.size() != N * 2 - 1) {
                error = "Invalid inventory binding";
                return false;
            }
            for (size_t i = 0; i < N; ++i) {
                if ((text[i * 2] != '0' && text[i * 2] != '1') || (i + 1 < N && text[i * 2 + 1] != ',') ||
                    (text[i * 2] == '1' && !definitions[i])) {
                    error = "Invalid inventory binding";
                    return false;
                }
                next.bound[i] = text[i * 2] == '1';
            }
        }
        out = next;
        error.clear();
        return true;
    }
    // The caller resolves this new character's own, initially equipped recipe.
    // Binding rules are supplied by its build; saved inventories use Load instead.
    bool StoreInitial(Character& created, const std::array<bool, N>& bindings, std::string& error) const {
        if (!capacity_) {
            error = "Initial inventory has no validated capacity";
            return false;
        }
        StarterInventoryState next = *this;
        for (size_t i = 0; i < N; ++i) {
            if ((locations[i] != -2 && locations[i] != -1) || (locations[i] == -2 && bindings[i])) {
                error = "Invalid initial equipped inventory";
                return false;
            }
            next.bound[i] = bindings[i];
        }
        auto fields = created.profileFields;
        fields["inventory_locations"] = next.Text();
        fields["inventory_bound"] = next.BindingText();
        created.profileFields.swap(fields);
        error.clear();
        return true;
    }
    std::string Text() const {
        std::string result;
        for (int location : locations) {
            if (!result.empty())
                result += ',';
            result += location == -2 ? "x" : location == -1 ? "e" : std::to_string(location);
        }
        return result;
    }
    std::string BindingText() const {
        std::string result;
        for (bool value : bound) {
            if (!result.empty())
                result += ',';
            result += value ? '1' : '0';
        }
        return result;
    }
    InventoryChangeResult Move(size_t item, unsigned destination, StarterInventoryState& out) const {
        if (item >= N || locations[item] == -2)
            return InventoryChangeResult::Missing;
        if (destination >= capacity_)
            return InventoryChangeResult::InvalidSlot;
        if (locations[item] < 0)
            return InventoryChangeResult::WrongLocation;
        if (locations[item] == static_cast<int>(destination))
            return InventoryChangeResult::Unchanged;
        StarterInventoryState next = *this;
        for (size_t other = 0; other < N; ++other)
            if (next.locations[other] == static_cast<int>(destination))
                next.locations[other] = locations[item];
        next.locations[item] = static_cast<int>(destination);
        out = next;
        return InventoryChangeResult::Changed;
    }
    InventoryChangeResult Equip(size_t item, StarterInventoryState& out, bool bindOnEquip = true) const {
        if (item >= N || locations[item] == -2)
            return InventoryChangeResult::Missing;
        if (locations[item] == -1)
            return InventoryChangeResult::Unchanged;
        StarterInventoryState next = *this;
        next.locations[item] = -1;
        next.bound[item] = next.bound[item] || bindOnEquip;
        out = next;
        return InventoryChangeResult::Changed;
    }
    InventoryChangeResult Unequip(size_t item, unsigned destination, StarterInventoryState& out) const {
        if (item >= N || locations[item] == -2)
            return InventoryChangeResult::Missing;
        if (destination >= capacity_)
            return InventoryChangeResult::InvalidSlot;
        if (locations[item] != -1)
            return InventoryChangeResult::WrongLocation;
        for (int location : locations)
            if (location == static_cast<int>(destination))
                return InventoryChangeResult::Occupied;
        StarterInventoryState next = *this;
        next.locations[item] = static_cast<int>(destination);
        out = next;
        return InventoryChangeResult::Changed;
    }

  private:
    unsigned capacity_ = 0;
};
} // namespace glide
