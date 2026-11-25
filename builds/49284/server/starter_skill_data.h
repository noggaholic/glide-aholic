#pragma once
#include "world_loadout.h"
#include <array>
#include <cstdint>
#include "glide/files.h"
#include "glide/crypto.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>
namespace glide {
namespace hot49284 {
struct StarterPhase {
    uint8_t index, type;
    uint16_t duration;
    std::array<float, 10> movement;
};
struct StarterAnimation {
    uint32_t skill;
    bool underwater;
    uint16_t first, count;
};
struct StarterDefinition {
    uint32_t skill, flags, kind;
    int32_t recharge;
    float maximumRange;
};
struct StarterChoice {
    uint8_t profession;
    uint32_t weapon;
    uint8_t weaponSlot, requiredEquipmentSlot, slot;
    uint32_t skill;
    uint8_t minimumLevel, professionState, modePredicate, mainHandKind, offHandKind;
    uint32_t requiredEffect, requiredTrait, predecessor;
    uint16_t chainCount;
};

struct StarterSkillData {
    std::vector<uint32_t> animationless;
    std::vector<StarterPhase> phases;
    std::vector<StarterAnimation> animations;
    std::vector<StarterDefinition> definitions;
    std::vector<StarterChoice> choices;
    std::vector<Hot49284Loadout> loadouts;

    bool ParseLoadouts(ByteView bytes, std::string& error) {
        error = "Invalid local HoT starting equipment";
        Reader reader(bytes);
        ByteView magic;
        uint32_t build = 0, count = 0;
        if (!reader.Bytes(8, magic) || std::memcmp(magic.data, "GLDLO001", 8) || !reader.U32(build) || build != 49284 ||
            !reader.U32(count) || !count || count > 1000)
            return false;
        std::vector<Hot49284Loadout> rows;
        for (uint32_t i = 0; i < count; ++i) {
            Hot49284Loadout row{};
            if (!reader.U8(row.race) || row.race < 1 || row.race > 3 || !reader.U8(row.sex) || row.sex > 1 ||
                !reader.U8(row.profession) || !row.profession || row.profession > 9)
                return false;
            for (auto& value : row.armor)
                if (!reader.U32(value))
                    return false;
            for (auto& value : row.skins)
                if (!reader.U32(value))
                    return false;
            for (auto& value : row.weapons)
                if (!reader.U32(value.item) || !reader.U8(value.slot))
                    return false;
            for (const auto& existing : rows)
                if (row.race == existing.race && row.sex == existing.sex && row.profession == existing.profession)
                    return false;
            rows.push_back(row);
        }
        if (reader.Remaining())
            return false;
        loadouts = std::move(rows);
        error.clear();
        return true;
    }

    bool Parse(ByteView bytes, std::string& error) {
        error = "Invalid local HoT skill data";
        if (bytes.size > 4 * 1024 * 1024)
            return false;
        Reader reader(bytes);
        ByteView magic;
        uint32_t build = 0;
        std::array<uint32_t, 5> counts{};
        if (!reader.Bytes(8, magic) || std::memcmp(magic.data, "GLDSK001", 8) || !reader.U32(build) || build != 49284)
            return false;
        for (auto& count : counts)
            if (!reader.U32(count) || !count || count > 100000)
                return false;
        const auto number = [&](float& value) {
            uint32_t bits = 0;
            if (!reader.U32(bits))
                return false;
            std::memcpy(&value, &bits, sizeof(value));
            return std::isfinite(value);
        };
        StarterSkillData data;
        for (uint32_t i = 0; i < counts[0]; ++i) {
            uint32_t id = 0;
            if (!reader.U32(id) || !id)
                return false;
            data.animationless.push_back(id);
        }
        for (uint32_t i = 0; i < counts[1]; ++i) {
            StarterPhase row{};
            if (!reader.U8(row.index) || !reader.U8(row.type) || !reader.U16(row.duration))
                return false;
            for (auto& value : row.movement)
                if (!number(value))
                    return false;
            data.phases.push_back(row);
        }
        for (uint32_t i = 0; i < counts[2]; ++i) {
            StarterAnimation row{};
            uint8_t water = 0;
            if (!reader.U32(row.skill) || !row.skill || !reader.U8(water) || water > 1 || !reader.U16(row.first) ||
                !reader.U16(row.count) || static_cast<size_t>(row.first) + row.count > data.phases.size())
                return false;
            row.underwater = water != 0;
            data.animations.push_back(row);
        }
        for (uint32_t i = 0; i < counts[3]; ++i) {
            StarterDefinition row{};
            uint32_t recharge = 0;
            if (!reader.U32(row.skill) || !row.skill || !reader.U32(row.flags) || !reader.U32(row.kind) ||
                !reader.U32(recharge) || !number(row.maximumRange))
                return false;
            std::memcpy(&row.recharge, &recharge, sizeof(recharge));
            data.definitions.push_back(row);
        }
        for (uint32_t i = 0; i < counts[4]; ++i) {
            StarterChoice row{};
            if (!reader.U8(row.profession) || !row.profession || row.profession > 9 || !reader.U32(row.weapon) ||
                !reader.U8(row.weaponSlot) || !reader.U8(row.requiredEquipmentSlot) || !reader.U8(row.slot) ||
                !reader.U32(row.skill) || !row.skill || !reader.U8(row.minimumLevel) ||
                !reader.U8(row.professionState) || !reader.U8(row.modePredicate) || !reader.U8(row.mainHandKind) ||
                !reader.U8(row.offHandKind) || !reader.U32(row.requiredEffect) || !reader.U32(row.requiredTrait) ||
                !reader.U32(row.predecessor) || !reader.U16(row.chainCount))
                return false;
            data.choices.push_back(row);
        }
        if (reader.Remaining())
            return false;
        *this = std::move(data);
        error.clear();
        return true;
    }
};
namespace detail {
struct StarterSkillStore {
    std::mutex mutex;
    std::atomic<bool> ready{false};
    StarterSkillData data;
    std::array<uint8_t, 32> hash{};
};
inline StarterSkillStore& SkillStore() {
    static StarterSkillStore store;
    return store;
}
} // namespace detail
inline const StarterSkillData& StarterSkillsData() {
    static const StarterSkillData empty;
    const auto& store = detail::SkillStore();
    return store.ready.load(std::memory_order_acquire) ? store.data : empty;
}
// Published once before server workers start; pointers into these tables then
// remain valid for the lifetime of the process.
inline bool LoadStarterSkills(const std::wstring& file, const std::wstring& loadoutFile, std::string& error) {
    Buffer bytes, loadouts, fingerprint;
    std::array<uint8_t, 32> hash{};
    error = "Cannot read local HoT skill data";
    if (!ReadFileBytes(file, 4 * 1024 * 1024, bytes) || !ReadFileBytes(loadoutFile, 4 * 1024 * 1024, loadouts) ||
        !fingerprint.Append(bytes.View()) || !fingerprint.Append(loadouts.View()) ||
        !Sha256(fingerprint.View(), hash.data()))
        return false;
    auto& store = detail::SkillStore();
    std::lock_guard<std::mutex> guard(store.mutex);
    if (store.ready.load(std::memory_order_relaxed)) {
        if (store.hash != hash) {
            error = "Different HoT skill data already loaded";
            return false;
        }
        error.clear();
        return true;
    }
    StarterSkillData data;
    if (!data.Parse(bytes.View(), error) || !data.ParseLoadouts(loadouts.View(), error))
        return false;
    store.data = std::move(data);
    store.hash = hash;
    store.ready.store(true, std::memory_order_release);
    return true;
}

} // namespace hot49284
} // namespace glide
