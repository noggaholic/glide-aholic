#include "glide/profile/modern_support_loadout.h"
#include "aquatic_skills.h"
#include "equipment_catalog.h"
#include "profession_state.h"
#include "weapon_ranges.h"
#include "starter_item_binding.h"
#include "aquatic_equipment.h"
#include "glide/files.h"
#include "glide/profile/auth_codec.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <set>
#include <sstream>
#include <utility>

namespace glide {
const AuthCodec& Build204489Auth();
const GameBootstrap& Build204489Bootstrap();
namespace build204489 {
namespace {
unsigned Key(unsigned race, unsigned sex, unsigned profession) {
    return (race * 2 + sex) * 10 + profession;
}
uint32_t ParameterWord(const InitialSkillParameter& parameter, size_t offset) {
    const auto* bytes = parameter.raw.data() + offset;
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}
bool ParameterUnsigned(std::istream& row, unsigned maximum, uint32_t& out) {
    std::string token;
    unsigned value = 0;
    if (!(row >> token) || !ParseUnsigned(token, maximum, value))
        return false;
    out = value;
    return true;
}
bool ReadSupportParameters(const std::wstring& path, const std::set<uint32_t>* required,
                           std::map<uint32_t, std::vector<InitialSkillParameter>>& out, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(path, 1024 * 1024, bytes)) {
        error = "Cannot read original Build 204489 support skill parameters";
        return false;
    }
    std::istringstream document(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<uint32_t, std::vector<InitialSkillParameter>> parameters;
    std::set<std::pair<std::string, uint32_t>> keys;
    std::string line;
    std::vector<InitialSkillParameter>* current = nullptr;
    uint32_t expected = 0;
    size_t total = 0;
    while (std::getline(document, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        std::string kind, extra;
        if (!(row >> kind)) {
            error = "Invalid Build 204489 skill parameter row";
            return false;
        }
        if (kind == "S") {
            uint32_t skill = 0, count = 0;
            if ((current && current->size() != expected) || !ParameterUnsigned(row, 0x3fffff, skill) ||
                !ParameterUnsigned(row, 1024, count) || row >> extra || !skill ||
                (required && !required->count(skill)) || parameters.count(skill) || parameters.size() >= 1024) {
                error = "Invalid or duplicate Build 204489 skill parameter dictionary";
                return false;
            }
            current = &parameters[skill];
            expected = count;
            keys.clear();
            continue;
        }
        InitialSkillParameter parameter;
        std::string key, raw;
        std::vector<uint8_t> keyBytes, rawBytes;
        if (kind != "P" || !current || current->size() >= expected || ++total > 16384 ||
            !ParameterUnsigned(row, UINT32_MAX, parameter.nameSpace) ||
            !ParameterUnsigned(row, 65535, parameter.type) || !(row >> key >> raw) ||
            !ParameterUnsigned(row, 65535, parameter.referenceType) ||
            !ParameterUnsigned(row, 0x3fffff, parameter.referenceId) || row >> extra || key.empty() ||
            key.size() > 512 || !DecodeHex(key, 256, keyBytes) || keyBytes.empty() || raw.size() != 80 ||
            !DecodeHex(raw, 40, rawBytes) || rawBytes.size() != 40) {
            error = "Invalid original Build 204489 skill parameter value";
            return false;
        }
        for (auto byte : keyBytes)
            if (byte < 32 || byte == 127) {
                error = "Invalid Build 204489 skill parameter name";
                return false;
            }
        parameter.key.assign(reinterpret_cast<const char*>(keyBytes.data()), keyBytes.size());
        std::wstring checkedKey;
        if (!Utf8ToWide(parameter.key, checkedKey)) {
            error = "Invalid UTF-8 Build 204489 skill parameter name";
            return false;
        }
        std::copy(rawBytes.begin(), rawBytes.end(), parameter.raw.begin());
        if (!parameter.type || ParameterWord(parameter, 0) != parameter.type ||
            ((parameter.referenceType == 0) != (parameter.referenceId == 0)) ||
            (parameter.referenceType && parameter.referenceType != parameter.type) ||
            !keys.emplace(parameter.key, parameter.nameSpace).second) {
            error = "Mismatched or duplicate original Build 204489 skill parameter";
            return false;
        }
        float number = 0;
        if (parameter.type == 436 && !parameter.Float(number)) {
            error = "Nonfinite Build 204489 skill parameter";
            return false;
        }
        current->push_back(std::move(parameter));
    }
    if (!current || (current->size() != expected) || (required && parameters.size() != required->size())) {
        error = "Incomplete original Build 204489 skill parameter dictionaries";
        return false;
    }
    out = std::move(parameters);
    return true;
}
bool ReadSupportQuantities(const std::wstring& path, const std::set<uint32_t>& required,
                           std::map<uint32_t, std::vector<InitialSkillQuantity>>& out, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(path, 1024 * 1024, bytes)) {
        error = "Cannot read original Build 204489 skill quantities";
        return false;
    }
    std::istringstream document(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<uint32_t, std::vector<InitialSkillQuantity>> quantities;
    std::string line;
    std::vector<InitialSkillQuantity>* current = nullptr;
    uint32_t expected = 0;
    size_t total = 0;
    std::set<std::pair<uint32_t, uint32_t>> identities;
    while (std::getline(document, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        std::string kind, extra;
        if (!(row >> kind)) {
            error = "Invalid Build 204489 quantity row";
            return false;
        }
        if (kind == "S") {
            uint32_t skill = 0, count = 0;
            if ((current && current->size() != expected) || !ParameterUnsigned(row, 0x3fffff, skill) ||
                !ParameterUnsigned(row, 1024, count) || row >> extra || !required.count(skill) ||
                quantities.count(skill)) {
                error = "Invalid or duplicate Build 204489 quantity dictionary";
                return false;
            }
            current = &quantities[skill];
            expected = count;
            identities.clear();
            continue;
        }
        InitialSkillQuantity value;
        uint32_t trait = 0, predicate = 0;
        std::string encoded;
        std::vector<uint8_t> raw;
        if (kind != "N" || !current || current->size() >= expected || ++total > 16384 ||
            !ParameterUnsigned(row, 1024, value.group) || !ParameterUnsigned(row, 4095, value.index) ||
            !ParameterUnsigned(row, 65535, value.metadata) || !ParameterUnsigned(row, 4, value.mode) ||
            !ParameterUnsigned(row, 15, value.modeFlags) || !ParameterUnsigned(row, 1, trait) ||
            !ParameterUnsigned(row, 1, predicate) || !(row >> encoded) || row >> extra || encoded.size() != 480 ||
            !DecodeHex(encoded, 240, raw) || raw.size() != 240 ||
            !identities.emplace(value.group, value.index).second) {
            error = "Invalid original Build 204489 numeric quantity";
            return false;
        }
        InitialSkillParameter fields[6];
        const uint32_t types[] = {433, 431, 436, 436, 436, 433};
        for (size_t i = 0; i < 6; ++i) {
            fields[i].type = types[i];
            std::copy(raw.begin() + i * 40, raw.begin() + (i + 1) * 40, fields[i].raw.begin());
            if (ParameterWord(fields[i], 0) != types[i]) {
                error = "Wrong original Build 204489 numeric value type";
                return false;
            }
        }
        value.attribute = ParameterWord(fields[1], 8);
        value.requiresTrait = trait != 0;
        value.requiresPredicate = predicate != 0;
        if (!fields[0].Integer(value.base) || value.attribute > 11 || !fields[2].Float(value.attributeMultiplier) ||
            !fields[3].Float(value.levelExponent) || value.levelExponent != 2.0f ||
            !fields[4].Float(value.levelMultiplier) || !fields[5].Integer(value.count) || value.count < 1 ||
            value.count > 4096) {
            error = "Unsupported or invalid original Build 204489 numeric model";
            return false;
        }
        current->push_back(value);
    }
    if (!current || current->size() != expected || quantities.size() != required.size()) {
        error = "Incomplete original Build 204489 quantity dictionaries";
        return false;
    }
    out = std::move(quantities);
    return true;
}
bool SameParameters(const std::vector<InitialSkillParameter>& first, const std::vector<InitialSkillParameter>& second) {
    if (first.size() != second.size())
        return false;
    for (size_t i = 0; i < first.size(); ++i) {
        const auto& a = first[i];
        const auto& b = second[i];
        if (a.key != b.key || a.nameSpace != b.nameSpace || a.type != b.type || a.raw != b.raw ||
            a.referenceType != b.referenceType || a.referenceId != b.referenceId)
            return false;
    }
    return true;
}
void SkillReferences(const std::vector<InitialSkillParameter>& parameters, std::set<uint32_t>& out) {
    for (const auto& parameter : parameters)
        if (parameter.referenceType == 64)
            out.insert(parameter.referenceId);
}
bool ValidateRelatedParameters(const std::map<uint32_t, std::vector<InitialSkillParameter>>& initial,
                               const std::map<uint32_t, std::vector<InitialSkillParameter>>& related,
                               std::string& error) {
    std::set<uint32_t> pending, visited;
    for (const auto& definition : initial)
        SkillReferences(definition.second, pending);
    while (!pending.empty()) {
        const auto id = *pending.begin();
        pending.erase(pending.begin());
        if (!visited.insert(id).second)
            continue;
        const auto found = related.find(id);
        if (found == related.end()) {
            error = "Missing original Build 204489 related skill parameters";
            return false;
        }
        const auto existing = initial.find(id);
        if (existing != initial.end() && !SameParameters(found->second, existing->second)) {
            error = "Conflicting original Build 204489 related skill parameters";
            return false;
        }
        SkillReferences(found->second, pending);
    }
    if (visited.size() != related.size()) {
        error = "Unreachable Build 204489 related skill parameters";
        return false;
    }
    return true;
}
bool ReadActionTiming(std::istream& row, InitialWeaponSkill& action) {
    unsigned variable = 0, ground = 0;
    if (!(row >> action.activationMs >> action.animationMs >> action.recoveryMs >> variable >> action.rechargeMs >>
          ground >> action.minimumGroundRange >> action.maximumGroundRange) ||
        action.activationMs > 65535 || action.animationMs > 65535 || variable > 1 || ground > 1 ||
        action.rechargeMs > 86400000 ||
        (action.animationMs ? (!action.activationMs || action.recoveryMs >= action.animationMs)
                            : (action.activationMs || action.recoveryMs || variable)) ||
        !std::isfinite(action.minimumGroundRange) ||
        !std::isfinite(action.maximumGroundRange * action.maximumGroundRange) ||
        (ground ? (action.minimumGroundRange < 0 || action.maximumGroundRange <= action.minimumGroundRange)
                : (action.minimumGroundRange != 0 || action.maximumGroundRange != 0)))
        return false;
    action.variableStages = variable != 0;
    action.groundTargeted = ground != 0;
    return true;
}
bool Field(const Character& saved, const char* key, unsigned maximum, unsigned& value) {
    const auto found = saved.profileFields.find(key);
    return found != saved.profileFields.end() && ParseUnsigned(found->second, maximum, value);
}
bool Colors(const Character& saved, std::array<uint16_t, 24>& out) {
    std::array<uint16_t, 24> colors{};
    const auto at = saved.profileFields.find("creation_colors");
    if (at != saved.profileFields.end()) {
        std::vector<uint8_t> bytes;
        if (!DecodeHex(at->second, 96, bytes) || bytes.size() != 96)
            return false;
        Reader reader(ByteView(bytes.data(), bytes.size()));
        for (auto& color : colors) {
            uint32_t value = 0;
            if (!reader.U32(value) || value > UINT16_MAX)
                return false;
            color = static_cast<uint16_t>(value);
        }
    }
    out = colors;
    return true;
}
bool BindingOwner(const Guid& owner, bool bound, Buffer& out) {
    return out.U8(bound ? uint8_t(1) : uint8_t(0)) && (!bound || out.Append(ByteView(owner.bytes, 16)));
}
bool Report(uint32_t instance, uint32_t definition, uint8_t type, const uint16_t* dyes, const Guid& owner, bool bound,
            bool ownerView, Buffer& out) {
    // Reuse the working China report layout, with the own Build 204489 S960/S971
    // descriptors, including the binding owner and this viewer's ownership bit.
    const uint8_t flags = bound ? uint8_t(ownerView ? 0x21 : 0x20) : uint8_t(0);
    if (!out.U16(type == 24 ? uint16_t(971) : uint16_t(960)) || !out.VarU32(instance) || !out.VarU32(definition) ||
        !out.U8(flags))
        return false;
    if (type == 24) {
        for (unsigned field = 0; field < 5; ++field)
            if (!out.VarU32(0))
                return false;
        return out.U8(0) && out.VarU32(0) && out.U8(0) && out.U64(0) && out.U8(0) && BindingOwner(owner, bound, out);
    }
    if (!out.VarU32(0) || !out.VarU32(0))
        return false;
    for (unsigned array = 0; array < 2; ++array)
        for (unsigned channel = 0; channel < 4; ++channel)
            if (!out.U16(dyes[channel]))
                return false;
    return out.VarU32(0) && out.VarU32(0) && out.VarU32(0) && out.U8(0) && BindingOwner(owner, bound, out);
}
} // namespace
bool InitialSkillParameter::Integer(int32_t& out) const {
    if (type != 433 || ParameterWord(*this, 0) != type)
        return false;
    const uint32_t bits = ParameterWord(*this, 8);
    int32_t value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    out = value;
    return true;
}
bool InitialSkillParameter::Float(float& out) const {
    if (type != 436 || ParameterWord(*this, 0) != type)
        return false;
    const uint32_t bits = ParameterWord(*this, 8);
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value))
        return false;
    out = value;
    return true;
}
bool InitialSkillParameter::ScaledInteger(int64_t& thousandths) const {
    if (type != 443 || ParameterWord(*this, 0) != type)
        return false;
    const uint64_t bits = uint64_t(ParameterWord(*this, 8)) | (uint64_t(ParameterWord(*this, 12)) << 32);
    int64_t value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    thousandths = value;
    return true;
}
bool InitialSkillParameter::Definition(uint32_t& definitionType, uint32_t& definitionId) const {
    if (!referenceType || !referenceId || referenceId >= 0x400000 || referenceType != type ||
        ParameterWord(*this, 0) != type)
        return false;
    // These are normalized content IDs, never the encoded pointer in raw+8.
    definitionType = referenceType;
    definitionId = referenceId;
    return true;
}
bool InitialSkillQuantity::Amount(unsigned level, int32_t attributeValue, int32_t& out) const {
    if (!level || level > 80 || attribute > 11 || attributeValue < 0 || levelExponent != 2.0f || count < 1 ||
        count > 4096 || !std::isfinite(attributeMultiplier) || !std::isfinite(levelMultiplier))
        return false;
    // Original SdFact rounds the attribute and level contributions separately,
    // halfway away from zero, then adds the base and applies the multiplicity.
    const auto rounded = [](float number, int32_t& result) {
        if (!std::isfinite(number))
            return false;
        const float adjusted = number > 0 ? number + 0.5f : number - 0.5f;
        if (double(adjusted) < -2147483648.0 || double(adjusted) >= 2147483648.0)
            return false;
        result = static_cast<int32_t>(adjusted);
        return true;
    };
    int32_t attributeAmount = 0, levelAmount = 0;
    const float actualLevel = static_cast<float>(level);
    if (!rounded(static_cast<float>(attribute ? attributeValue : 0) * attributeMultiplier, attributeAmount) ||
        !rounded((actualLevel * actualLevel) * levelMultiplier, levelAmount))
        return false;
    const int64_t result = (int64_t(base) + attributeAmount + levelAmount) * count;
    if (result < INT32_MIN || result > INT32_MAX)
        return false;
    out = static_cast<int32_t>(result);
    return true;
}
bool EquipmentCatalog::Load(const std::wstring& root, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(root + L"/204489/characters/initial-equipment.txt", 1024 * 1024, bytes)) {
        error = "Cannot read original Build 204489 starter equipment";
        return false;
    }
    std::istringstream document(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<unsigned, InitialEquipment> recipes;
    std::set<uint32_t> required;
    std::string line;
    while (std::getline(document, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned race = 0, sex = 0, profession = 0, level = 0;
        InitialEquipment equipment;
        if (!(row >> race >> sex >> profession >> level) || race > 4 || sex > 1 || !profession || profession > 9 ||
            level != 1) {
            error = "Invalid original Build 204489 equipment identity";
            return false;
        }
        for (auto& id : equipment.definitions) {
            if (!(row >> id) || id >= 0x400000) {
                error = "Invalid original Build 204489 item DataId";
                return false;
            }
            if (id)
                required.insert(id);
        }
        for (size_t slot = 0; slot < equipment.types.size(); ++slot) {
            unsigned type = 0;
            if (!(row >> type) || (type != 0 && type != 24) || (!equipment.definitions[slot] && type) ||
                (equipment.definitions[slot] &&
                 ((type == 0 && slot != 0 && (slot < 2 || slot > 7)) ||
                  (type == 24 && slot != 24 && slot != 25 && (slot < 29 || slot > 32))))) {
                error = "Invalid original Build 204489 starter slot or item type";
                return false;
            }
            equipment.types[slot] = static_cast<uint8_t>(type);
        }
        std::string extra;
        if (row >> extra || !recipes.emplace(Key(race, sex, profession), equipment).second) {
            error = "Duplicate or trailing Build 204489 recipe";
            return false;
        }
    }
    if (recipes.size() != 90 || required.empty()) {
        error = "Build 204489 equipment must cover all races, sexes and professions";
        return false;
    }
    if (!ReadFileBytes(root + L"/204489/characters/initial-item-appearance.txt", 128 * 1024, bytes)) {
        error = "Cannot read original Build 204489 item appearance";
        return false;
    }
    std::istringstream appearanceText(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<uint32_t, Appearance> appearances;
    while (std::getline(appearanceText, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t id = 0, skin = 0;
        unsigned type = 0;
        std::string extra;
        if (!(row >> id >> skin >> type) || !id || id >= 0x400000 || !skin || skin >= 0x400000 ||
            (type != 0 && type != 24) || row >> extra) {
            error = "Invalid original Build 204489 item appearance";
            return false;
        }
        Appearance value;
        value.skin = skin;
        value.type = static_cast<uint8_t>(type);
        if (!appearances.emplace(id, value).second) {
            error = "Duplicate Build 204489 item appearance";
            return false;
        }
    }
    if (appearances.size() != required.size()) {
        error = "Incomplete original Build 204489 item appearance";
        return false;
    }
    for (const auto& recipe : recipes)
        for (size_t slot = 0; slot < 68; ++slot) {
            const auto id = recipe.second.definitions[slot];
            if (!id)
                continue;
            const auto found = appearances.find(id);
            if (found == appearances.end() || found->second.type != recipe.second.types[slot]) {
                error = "Build 204489 recipe appearance mismatch";
                return false;
            }
        }
    if (!ReadFileBytes(root + L"/204489/characters/initial-item-binding.txt", 4096, bytes)) {
        error = "Cannot read original Build 204489 starter-item binding";
        return false;
    }
    std::istringstream bindingText(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::set<uint32_t> bindingDefinitions;
    while (std::getline(bindingText, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t id = 0;
        unsigned kind = 0;
        std::string extra;
        if (!(row >> id >> kind) || (kind != 0 && kind != 2) || row >> extra || !bindingDefinitions.insert(id).second) {
            error = "Invalid original Build 204489 starter-item binding";
            return false;
        }
        const auto found = appearances.find(id);
        if (found == appearances.end()) {
            error = "Binding definition is absent from original Build 204489 equipment";
            return false;
        }
        found->second.bindOnEquip = kind == 2;
    }
    if (bindingDefinitions != required) {
        error = "Incomplete original Build 204489 starter-item binding";
        return false;
    }
    // Reuse China's immutable innate-trait and learned-skill catalog loaders.
    // These files contain data extracted from this Build 204489 build's own DAT.
    if (!ReadFileBytes(root + L"/204489/characters/initial-weapon-proficiencies.txt", 4096, bytes)) {
        error = "Cannot read original Build 204489 weapon proficiencies";
        return false;
    }
    std::istringstream traitText(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<unsigned, WeaponProficiency> proficiencies;
    static const unsigned professionBits[] = {0, 9, 14, 7, 12, 13, 8, 10, 11, 15};
    while (std::getline(traitText, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned profession = 0, minimum = 0;
        uint32_t restriction = 0, main = 0, off = 0, water = 0;
        WeaponProficiency proficiency;
        std::string extra;
        if (!(row >> profession >> proficiency.trait >> minimum >> restriction >> main >> off >> water) ||
            !profession || profession > 9 || !proficiency.trait || proficiency.trait >= 0x400000 || minimum != 1 ||
            restriction != (uint32_t(1) << professionBits[profession]) || !main || !off || !water ||
            main >= 0x1000000 || off >= 0x1000000 || water >= 0x1000000 || row >> extra ||
            !proficiencies.emplace(profession, proficiency).second) {
            error = "Invalid original Build 204489 weapon proficiency";
            return false;
        }
        proficiencies[profession].underwaterMask = water;
    }
    if (proficiencies.size() != 9) {
        error = "Incomplete original Build 204489 weapon proficiencies";
        return false;
    }
    if (!ReadFileBytes(root + L"/204489/characters/initial-weapon-skills.txt", 16384, bytes)) {
        error = "Cannot read original Build 204489 weapon skills";
        return false;
    }
    std::istringstream skillText(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<unsigned, std::vector<InitialWeaponSkill>> skills;
    while (std::getline(skillText, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned profession = 0;
        InitialWeaponSkill skill;
        std::string extra;
        if (!(row >> profession >> skill.weapon >> skill.slotType >> skill.skill >> skill.minimumLevel) ||
            !profession || profession > 9 || !skill.weapon || skill.weapon >= 0x400000 || skill.slotType < 3 ||
            skill.slotType > 7 || !skill.skill || skill.skill >= 0x400000 || skill.minimumLevel != 1 || row >> extra) {
            error = "Invalid original Build 204489 weapon skill";
            return false;
        }
        auto& entries = skills[profession];
        if (entries.size() >= 64) {
            error = "Too many original Build 204489 weapon skills";
            return false;
        }
        for (const auto& prior : entries)
            if (prior.skill == skill.skill || prior.weapon != skill.weapon) {
                error = "Conflicting original Build 204489 weapon skills";
                return false;
            }
        for (unsigned race = 0; race < 5; ++race)
            for (unsigned sex = 0; sex < 2; ++sex) {
                const auto recipe = recipes.find(Key(race, sex, profession));
                if (recipe == recipes.end() || recipe->second.definitions[29] != skill.weapon) {
                    error = "Original Build 204489 weapon skill does not match equipment";
                    return false;
                }
            }
        entries.push_back(skill);
    }
    if (skills.size() != 9) {
        error = "Incomplete original Build 204489 weapon skills";
        return false;
    }
    if (!ReadFileBytes(root + L"/204489/characters/initial-weapon-types.txt", 4096, bytes)) {
        error = "Cannot read original Build 204489 weapon hand requirements";
        return false;
    }
    std::istringstream handText(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<uint32_t, bool> hands;
    while (std::getline(handText, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t id = 0;
        unsigned type = 0, two = 0;
        std::string extra;
        if (!(row >> id >> type >> two) || !id || type > 25 || two > 1 || row >> extra ||
            !hands.emplace(id, two != 0).second) {
            error = "Invalid original Build 204489 weapon hand requirement";
            return false;
        }
        const auto found = appearances.find(id);
        if (found == appearances.end() || found->second.type != 24) {
            error = "Build 204489 hand requirement is not a weapon";
            return false;
        }
    }
    if (!ReadFileBytes(root + L"/204489/characters/initial-weapon-skill-actions.txt", 16384, bytes)) {
        error = "Cannot read original Build 204489 weapon skill actions";
        return false;
    }
    std::istringstream actionText(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<uint32_t, InitialWeaponSkill> actions;
    while (std::getline(actionText, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        InitialWeaponSkill action;
        std::string extra;
        if (!(row >> action.skill) || !action.skill || action.skill >= 0x400000 || !ReadActionTiming(row, action) ||
            row >> extra) {
            error = "Invalid original Build 204489 weapon skill action";
            return false;
        }
        if (!actions.emplace(action.skill, action).second) {
            error = "Duplicate original Build 204489 skill action";
            return false;
        }
    }
    size_t matched = 0;
    for (auto& profession : skills)
        for (auto& skill : profession.second) {
            const auto timing = actions.find(skill.skill);
            const auto hand = hands.find(skill.weapon);
            if (timing == actions.end() || hand == hands.end()) {
                error = "Missing original Build 204489 skill action or hand requirement";
                return false;
            }
            const auto original = skill;
            skill = timing->second;
            skill.weapon = original.weapon;
            skill.slotType = original.slotType;
            skill.minimumLevel = original.minimumLevel;
            skill.nativeSlot = original.slotType + 2;
            skill.equipmentSlot = skill.slotType >= 6 && !hand->second ? 30u : 29u;
            ++matched;
        }
    if (matched != actions.size()) {
        error = "Unmatched original Build 204489 skill action";
        return false;
    }
    std::map<uint32_t, modern::SkillPhases> phases;
    if (!modern::LoadSkillPhases(root + L"/204489/characters/initial-weapon-skill-phases.txt", phases)) {
        error = "Cannot read original Build 204489 weapon skill phases";
        return false;
    }
    size_t matchedPhases = 0;
    for (auto& profession : skills)
        for (auto& skill : profession.second) {
            const auto found = phases.find(skill.skill);
            if (!skill.animationMs) {
                if (found != phases.end()) {
                    error = "Unexpected Build 204489 instant skill phases";
                    return false;
                }
                continue;
            }
            if (found == phases.end()) {
                error = "Missing original Build 204489 weapon skill phases";
                return false;
            }
            unsigned duration = 0;
            bool moving = false;
            for (const auto& phase : found->second) {
                duration += phase.durationMs;
                moving = moving || phase.type != 0;
            }
            if (duration != skill.animationMs || moving != skill.variableStages ||
                (moving && skill.activationMs + skill.recoveryMs > duration)) {
                error = "Inconsistent original Build 204489 weapon skill phases";
                return false;
            }
            skill.phases = found->second;
            ++matchedPhases;
        }
    if (matchedPhases != phases.size()) {
        error = "Unmatched original Build 204489 weapon skill phases";
        return false;
    }
    if (!modern::LoadWeaponTargetRanges(root + L"/204489/characters/initial-weapon-target-ranges.txt", skills, error))
        return false;
    modern::SupportPalettes supportPalettes;
    modern::SupportUnlocks supportUnlocks;
    if (!modern::LoadSupportLoadout(root, 204489, supportPalettes, supportUnlocks, error))
        return false;
    if (!ReadFileBytes(root + L"/204489/characters/initial-support-skill-actions.txt", 32768, bytes)) {
        error = "Cannot read original Build 204489 support skill actions";
        return false;
    }
    std::map<unsigned, std::vector<InitialWeaponSkill>> supportSkills;
    std::set<std::pair<unsigned, uint32_t>> supportIdentities;
    std::istringstream supportRows(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    unsigned knownTimings = 0;
    while (std::getline(supportRows, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned profession = 0, known = 0;
        InitialWeaponSkill action;
        std::string extra;
        if (!(row >> profession >> action.nativeSlot >> action.skill >> action.minimumLevel >>
              action.supportEnvironments >> action.knownSupportConditions >> known) ||
            !profession || profession > 9 || action.nativeSlot > 4 || !action.skill || action.skill >= 0x400000 ||
            !action.minimumLevel || action.minimumLevel > 80 || !action.supportEnvironments ||
            action.supportEnvironments > 3 || (action.knownSupportConditions & ~action.supportEnvironments) ||
            known > 1 || !ReadActionTiming(row, action) || row >> extra ||
            (!known && (action.activationMs || action.animationMs || action.recoveryMs || action.variableStages)) ||
            !supportIdentities.emplace(profession, action.skill).second) {
            error = "Invalid original Build 204489 support skill action";
            return false;
        }
        const auto unlock = supportUnlocks[profession].find(action.skill);
        if (unlock == supportUnlocks[profession].end() || unlock->second != action.minimumLevel) {
            error = "Original Build 204489 support action has no matching unlock";
            return false;
        }
        action.requiresWeapon = false;
        action.equipmentSlot = 68;
        action.timingKnown = known != 0;
        knownTimings += known;
        supportSkills[profession].push_back(action);
    }
    size_t supportCount = 0;
    for (const auto& profession : supportUnlocks)
        supportCount += profession.second.size();
    if (supportSkills.size() != 9 || supportIdentities.size() != supportCount || knownTimings != 144) {
        error = "Incomplete original Build 204489 support skill actions";
        return false;
    }
    std::map<uint32_t, modern::SkillPhases> supportPhases;
    if (!modern::LoadSkillPhases(root + L"/204489/characters/initial-support-skill-phases.txt", supportPhases)) {
        error = "Cannot read original Build 204489 support skill phases";
        return false;
    }
    size_t matchedSupportPhases = 0;
    for (auto& profession : supportSkills)
        for (auto& skill : profession.second) {
            const auto found = supportPhases.find(skill.skill);
            if (!skill.animationMs) {
                if (found != supportPhases.end()) {
                    error = "Unexpected Build 204489 instant support phases";
                    return false;
                }
                continue;
            }
            if (found == supportPhases.end()) {
                error = "Missing original Build 204489 support phases";
                return false;
            }
            unsigned duration = 0;
            bool moving = false;
            for (const auto& phase : found->second) {
                duration += phase.durationMs;
                moving = moving || phase.type != 0;
            }
            if (duration != skill.animationMs || moving != skill.variableStages ||
                skill.activationMs + skill.recoveryMs > duration) {
                error = "Inconsistent original Build 204489 support phases";
                return false;
            }
            skill.phases = found->second;
            ++matchedSupportPhases;
        }
    if (matchedSupportPhases != supportPhases.size()) {
        error = "Unmatched original Build 204489 support phases";
        return false;
    }
    std::set<uint32_t> parameterDefinitions;
    for (const auto& identity : supportIdentities)
        parameterDefinitions.insert(identity.second);
    std::map<uint32_t, std::vector<InitialSkillParameter>> supportParameters;
    if (!ReadSupportParameters(root + L"/204489/characters/initial-support-skill-parameters.txt", &parameterDefinitions,
                               supportParameters, error))
        return false;
    std::map<uint32_t, std::vector<InitialSkillParameter>> relatedParameters;
    if (!ReadSupportParameters(root + L"/204489/characters/related-support-skill-parameters.txt", nullptr,
                               relatedParameters, error) ||
        !ValidateRelatedParameters(supportParameters, relatedParameters, error))
        return false;
    std::map<uint32_t, std::vector<InitialSkillQuantity>> supportQuantities;
    if (!ReadSupportQuantities(root + L"/204489/characters/initial-support-skill-quantities.txt", parameterDefinitions,
                               supportQuantities, error))
        return false;
    std::array<modern::AquaticEquipment, 10> aquatic;
    if (!modern::LoadAquaticEquipment(root + L"/204489/characters/aquatic-equipment.txt", aquatic, error))
        return false;
    std::map<unsigned, std::vector<InitialWeaponSkill>> aquaticSkills;
    if (!modern::LoadAquaticSkills(root + L"/204489/characters", aquatic, aquaticSkills, error))
        return false;
    std::map<unsigned, bool> added;
    for (auto& row : recipes) {
        const auto profession = row.first % 10;
        const auto& item = aquatic[profession];
        if (!item.item)
            continue;
        const auto permission = proficiencies.find(profession);
        const auto skin = appearances.find(item.item);
        const auto* binding = FindStarterItemBinding(item.item);
        if (permission == proficiencies.end() || !(permission->second.underwaterMask & (1u << item.kind)) || !binding ||
            binding->onUse || binding->onAcquisition ||
            (skin != appearances.end() && (skin->second.skin != item.skin || skin->second.type != 24)) ||
            (row.second.definitions[24] && row.second.definitions[24] != item.item)) {
            error = "Aquatic equipment does not match original Build 204489 permissions or appearance";
            return false;
        }
        Appearance value;
        value.skin = item.skin;
        value.type = 24;
        appearances[item.item] = value;
        added[row.first] = row.second.definitions[24] == 0;
        row.second.definitions[24] = item.item;
        row.second.types[24] = 24;
    }
    aquaticSkills_ = std::move(aquaticSkills);
    aquaticAdded_ = std::move(added);
    recipes_ = std::move(recipes);
    appearances_ = std::move(appearances);
    weaponProficiencies_ = std::move(proficiencies);
    weaponSkills_ = std::move(skills);
    supportPalettes_ = std::move(supportPalettes);
    supportUnlocks_ = std::move(supportUnlocks);
    supportSkills_ = std::move(supportSkills);
    supportParameters_ = std::move(supportParameters);
    relatedSupportParameters_ = std::move(relatedParameters);
    supportQuantities_ = std::move(supportQuantities);
    error.clear();
    return true;
}
bool EquipmentCatalog::HasAddedAquaticWeapon(const Character& saved) const {
    unsigned race = 0, sex = 0, profession = 0;
    if (!Field(saved, "race", 4, race) || !Field(saved, "sex", 1, sex) || !Field(saved, "profession", 9, profession))
        return false;
    const auto found = aquaticAdded_.find(Key(race, sex, profession));
    return found != aquaticAdded_.end() && found->second;
}
bool EquipmentCatalog::Resolve(const Character& saved, InitialEquipment& out) const {
    unsigned race = 0, sex = 0, profession = 0;
    if (!Field(saved, "race", 4, race) || !Field(saved, "sex", 1, sex) || !Field(saved, "profession", 9, profession) ||
        !profession)
        return false;
    const auto found = recipes_.find(Key(race, sex, profession));
    if (found == recipes_.end())
        return false;
    out = found->second;
    return true;
}
uint32_t EquipmentCatalog::Instance(uint8_t player, size_t slot) {
    // All 255 player ranges end below 20019, the first reserved peer bag.
    // The owned structural bag IDs 20..24 are also outside these ranges.
    return player && slot < 68 ? 1024u + uint32_t(player) * 68u + static_cast<uint32_t>(slot) : 0;
}
bool EquipmentCatalog::WeaponPermissionPackets(const Character& saved, Buffer& out) const {
    unsigned profession = 0;
    InitialEquipment equipment;
    if (!Resolve(saved, equipment) || !Field(saved, "profession", 9, profession) || !profession || !saved.level ||
        saved.level > 80)
        return false;
    const auto proficiency = weaponProficiencies_.find(profession);
    const auto skills = weaponSkills_.find(profession);
    if (proficiency == weaponProficiencies_.end() || skills == weaponSkills_.end())
        return false;
    Buffer next;
    // S678: player, active traits, unlocked traits, five legacy line values.
    // The native trait definition derives main-hand, off-hand and underwater permissions.
    uint8_t attunement = 0;
    if (!SavedAttunement(saved, attunement))
        return false;
    uint8_t count = 1;
    if (attunement)
        for (unsigned state = 1; state <= 4; ++state)
            if (saved.level >= AttunementLevel(state))
                ++count;
    if (!next.U16(678) || !next.VarU32(1) || !next.U8(count) || !next.VarU32(proficiency->second.trait))
        return false;
    if (attunement)
        for (unsigned state = 1; state <= 4; ++state)
            if (saved.level >= AttunementLevel(state)) {
                if (!next.VarU32(AttunementTrait(state)))
                    return false;
            }
    if (!next.U8(0) || !next.U8(5))
        return false;
    for (unsigned line = 0; line < 5; ++line)
        if (!next.U8(0))
            return false;
    if (attunement)
        for (unsigned state = 1; state <= 4; ++state)
            if (saved.level >= AttunementLevel(state)) {
                if (!next.U16(605) || !next.VarU32(5491 + state) || !next.VarU32(1))
                    return false;
            }
    if (profession == 9)
        for (uint32_t selector : {28085u, 28134u, 28419u, 28494u}) {
            if (!next.U16(605) || !next.VarU32(selector) || !next.VarU32(1))
                return false;
        }
    const auto water = aquaticSkills_.find(profession);
    if (water == aquaticSkills_.end())
        return false;
    for (const auto* group : {&skills->second, &water->second})
        for (const auto& skill : *group)
            if (saved.level >= skill.minimumLevel) {
                // S605 is skill first, then the owner player. Peers never receive it.
                if (!next.U16(605) || !next.VarU32(skill.skill) || !next.VarU32(1))
                    return false;
            }
    out = std::move(next);
    return true;
}
bool EquipmentCatalog::ResolveInitialWeaponSkills(const Character& saved,
                                                  const std::vector<InitialWeaponSkill>*& out) const {
    unsigned profession = 0;
    if (!Field(saved, "profession", 9, profession) || !profession || !saved.level || saved.level > 80)
        return false;
    const auto found = weaponSkills_.find(profession);
    if (found == weaponSkills_.end() || found->second.empty())
        return false;
    out = &found->second;
    return true;
}
bool EquipmentCatalog::ResolveAquaticWeaponSkills(const Character& saved,
                                                  const std::vector<InitialWeaponSkill>*& out) const {
    unsigned profession = 0;
    if (!Field(saved, "profession", 9, profession) || !profession || !saved.level || saved.level > 80)
        return false;
    const auto found = aquaticSkills_.find(profession);
    if (found == aquaticSkills_.end())
        return false;
    out = &found->second;
    return true;
}
bool EquipmentCatalog::ResolveInitialSupportSkills(const Character& saved,
                                                   const std::vector<InitialWeaponSkill>*& out) const {
    unsigned profession = 0;
    if (!Field(saved, "profession", 9, profession) || !profession || !saved.level || saved.level > 80)
        return false;
    const auto found = supportSkills_.find(profession);
    if (found == supportSkills_.end() || found->second.empty())
        return false;
    out = &found->second;
    return true;
}
bool EquipmentCatalog::ResolveInitialSupportParameters(const Character& saved, uint32_t skill,
                                                       const std::vector<InitialSkillParameter>*& out) const {
    out = nullptr;
    unsigned profession = 0;
    if (!Field(saved, "profession", 9, profession) || !profession || !saved.level || saved.level > 80)
        return false;
    const auto unlocks = supportUnlocks_.find(profession);
    if (unlocks == supportUnlocks_.end())
        return false;
    const auto unlock = unlocks->second.find(skill);
    if (unlock == unlocks->second.end() || saved.level < unlock->second)
        return false;
    const auto found = supportParameters_.find(skill);
    if (found == supportParameters_.end())
        return false;
    out = &found->second;
    return true;
}
bool EquipmentCatalog::ResolveInitialSupportQuantities(const Character& saved, uint32_t skill,
                                                       const std::vector<InitialSkillQuantity>*& out) const {
    const std::vector<InitialSkillParameter>* parameters = nullptr;
    if (!ResolveInitialSupportParameters(saved, skill, parameters))
        return false;
    const auto found = supportQuantities_.find(skill);
    if (found == supportQuantities_.end())
        return false;
    out = &found->second;
    return true;
}
const InitialSkillParameter* EquipmentCatalog::FindInitialSupportParameter(const Character& saved, uint32_t skill,
                                                                           const std::string& key,
                                                                           uint32_t nameSpace) const {
    const std::vector<InitialSkillParameter>* parameters = nullptr;
    if (!ResolveInitialSupportParameters(saved, skill, parameters))
        return nullptr;
    for (const auto& parameter : *parameters)
        if (parameter.nameSpace == nameSpace && parameter.key == key)
            return &parameter;
    return nullptr;
}
bool EquipmentCatalog::ResolveRelatedSupportParameters(const Character& saved, uint32_t parentSkill,
                                                       uint32_t relatedSkill,
                                                       const std::vector<InitialSkillParameter>*& out) const {
    out = nullptr;
    const std::vector<InitialSkillParameter>* parent = nullptr;
    if (!ResolveInitialSupportParameters(saved, parentSkill, parent))
        return false;
    std::set<uint32_t> pending, visited;
    SkillReferences(*parent, pending);
    while (!pending.empty()) {
        const auto id = *pending.begin();
        pending.erase(pending.begin());
        if (!visited.insert(id).second)
            continue;
        const auto found = relatedSupportParameters_.find(id);
        if (found == relatedSupportParameters_.end())
            return false;
        if (id == relatedSkill) {
            out = &found->second;
            return true;
        }
        SkillReferences(found->second, pending);
    }
    return false;
}
bool EquipmentCatalog::SupportSkillPackets(const Character& saved, Buffer& out) const {
    unsigned profession = 0;
    if (!Field(saved, "profession", 9, profession) || !profession || !saved.level || saved.level > 80)
        return false;
    return modern::SupportLoadoutPackets(204489, supportPalettes_, supportUnlocks_, profession, saved.level, out);
}
bool EquipmentCatalog::InitializeInventory(Character& character, std::string& error) const {
    Character fresh = character;
    fresh.profileFields.erase("inventory_locations");
    fresh.profileFields.erase("inventory_bound");
    InitialEquipment equipment;
    InventoryState state;
    if (!Resolve(fresh, equipment) || !Inventory(fresh, state, error)) {
        error = "Cannot resolve original Build 204489 starting inventory";
        return false;
    }
    std::array<bool, 68> bindings{};
    for (size_t i = 0; i < bindings.size(); ++i)
        if (equipment.definitions[i]) {
            const auto* rule = FindStarterItemBinding(equipment.definitions[i]);
            if (!rule || (rule->onAcquisition != 0 && rule->onAcquisition != 2) ||
                (rule->onUse != 0 && rule->onUse != 2) ||
                BindsOnEquip(equipment.definitions[i]) != (rule->onUse == 2)) {
                error = "Missing or inconsistent original Build 204489 starting binding rule";
                return false;
            }
            bindings[i] = rule->onAcquisition == 2 || rule->onUse == 2;
        }
    return state.StoreInitial(character, bindings, error);
}
bool EquipmentCatalog::Inventory(const Character& saved, InventoryState& out, std::string& error) const {
    InitialEquipment initial;
    if (!Resolve(saved, initial)) {
        error = "Cannot resolve original Build 204489 inventory";
        return false;
    }
    return modern::LoadAquaticInventory(saved, initial.definitions, 80, HasAddedAquaticWeapon(saved), out, error);
}
bool EquipmentCatalog::BindsOnEquip(uint32_t definition) const {
    const auto found = appearances_.find(definition);
    return found != appearances_.end() && found->second.bindOnEquip;
}
bool EquipmentCatalog::ItemReport(const Character& saved, size_t slot, uint32_t instance, Buffer& out, bool bound,
                                  bool ownerView) const {
    InitialEquipment equipment;
    std::array<uint16_t, 24> colors{};
    if (!instance || slot >= 68 || !Resolve(saved, equipment) || !equipment.definitions[slot] || !Colors(saved, colors))
        return false;
    const uint16_t emptyDyes[4]{};
    const auto dyes = slot >= 2 && slot <= 7 ? colors.data() + (slot - 2) * 4 : emptyDyes;
    Buffer next;
    if (!Report(instance, equipment.definitions[slot], equipment.types[slot], dyes, saved.guid, bound, ownerView, next))
        return false;
    out = std::move(next);
    return true;
}
bool EquipmentCatalog::Packets(const Character& saved, uint8_t player, uint16_t agent, Buffer& out,
                               bool includeBagItems) const {
    InitialEquipment equipment;
    std::array<uint16_t, 24> colors{};
    if (!player || !agent || !Resolve(saved, equipment) || !Colors(saved, colors))
        return false;
    InventoryState state;
    std::string error;
    if (!Inventory(saved, state, error))
        return false;
    Buffer next;
    for (size_t slot = 0; slot < 68; ++slot) {
        if (!equipment.definitions[slot] || (!includeBagItems && state.locations[slot] != -1))
            continue;
        const auto instance = Instance(player, slot);
        Buffer report;
        if (!ItemReport(saved, slot, instance, report, state.bound[slot], player == 1) || !next.Append(report.View()) ||
            !next.U16(288) || !next.VarU32(agent) || !next.VarU32(instance) ||
            !next.U8(state.locations[slot] == -1 ? uint8_t(2) : uint8_t(3)) ||
            !next.VarU32(state.locations[slot] == -1 ? static_cast<uint32_t>(slot)
                                                     : static_cast<uint32_t>(state.locations[slot])))
            return false;
    }
    out = std::move(next);
    return true;
}
bool EquipmentCatalog::Summary(const Character& saved, Buffer& out) const {
    if (!saved.map || saved.map > UINT16_MAX)
        return false;
    InitialEquipment equipment;
    std::array<uint16_t, 24> colors{};
    Buffer base;
    if (!Resolve(saved, equipment) || !Colors(saved, colors) || !Build204489Auth().Summary(saved, base) ||
        base.Size() != 159)
        return false;
    InventoryState state;
    std::string error;
    if (!Inventory(saved, state, error))
        return false;
    std::vector<uint8_t> bytes(base.Data(), base.Data() + base.Size());
    // The original v11 summary stores the last map as a little-endian uint16.
    bytes[1] = static_cast<uint8_t>(saved.map);
    bytes[2] = static_cast<uint8_t>(saved.map >> 8);
    // Reuse the original six armor records and CRC used by the China codec.
    for (size_t slot = 2; slot <= 7; ++slot) {
        if (state.locations[slot] != -1) {
            for (size_t byte = 0; byte < 12; ++byte)
                bytes[61 + (slot - 2) * 12 + byte] = 0;
            continue;
        }
        if (!equipment.definitions[slot])
            continue;
        const auto found = appearances_.find(equipment.definitions[slot]);
        if (found == appearances_.end() || found->second.type != 0)
            return false;
        const size_t record = 61 + (slot - 2) * 12;
        for (unsigned byte = 0; byte < 4; ++byte)
            bytes[record + byte] = static_cast<uint8_t>(found->second.skin >> (byte * 8));
        for (size_t channel = 0; channel < 4; ++channel) {
            const auto color = colors[(slot - 2) * 4 + channel];
            bytes[record + 4 + channel * 2] = static_cast<uint8_t>(color);
            bytes[record + 5 + channel * 2] = static_cast<uint8_t>(color >> 8);
        }
    }
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < 155; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    crc = ~crc;
    for (unsigned byte = 0; byte < 4; ++byte)
        bytes[155 + byte] = static_cast<uint8_t>(crc >> (byte * 8));
    Buffer next;
    if (!next.Append(ByteView(bytes.data(), bytes.size())))
        return false;
    out = std::move(next);
    return true;
}
} // namespace build204489
namespace {
class EquippedBuild204489Bootstrap final : public GameBootstrap {
  public:
    explicit EquippedBuild204489Bootstrap(build204489::EquipmentCatalog catalog) : catalog_(std::move(catalog)) {}
    bool Steps(const BootstrapContext& context, std::vector<BootstrapStep>& out) const override {
        return Build(context, nullptr, out);
    }
    bool PeerSteps(const BootstrapContext& context, const PeerIdentity& peer,
                   std::vector<BootstrapStep>& out) const override {
        return Build(context, &peer, out);
    }
    bool PeerSlot(unsigned slot, PeerIdentity& out) const override {
        return Build204489Bootstrap().PeerSlot(slot, out);
    }
    bool BindSession(uint64_t owner, PeerIdentity& out) const override {
        return Build204489Bootstrap().BindSession(owner, out);
    }
    bool PeerRemove(const PeerIdentity& peer, uint32_t sequence, Buffer& out) const override {
        return Build204489Bootstrap().PeerRemove(peer, sequence, out);
    }

  private:
    build204489::EquipmentCatalog catalog_;
    bool Build(const BootstrapContext& context, const PeerIdentity* peer, std::vector<BootstrapStep>& out) const {
        if (context.catalog.build != 204489)
            return false;
        std::vector<BootstrapStep> recipe;
        if (!(peer ? Build204489Bootstrap().PeerSteps(context, *peer, recipe)
                   : Build204489Bootstrap().Steps(context, recipe)))
            return false;
        uint8_t attunement = 0;
        if (!build204489::SavedAttunement(context.entry.character, attunement))
            return false;
        if (attunement) {
            BootstrapStep selected;
            selected.name = "Build 204489 initial elemental attunement";
            if (!build204489::AttunementSelection(peer ? peer->character : uint16_t(57344), attunement,
                                                  selected.payload))
                return false;
            bool inserted = false;
            const char* anchor = peer ? "character profession" : "skillbar ready";
            for (auto at = recipe.begin(); at != recipe.end(); ++at)
                if (!std::strcmp(at->name, anchor)) {
                    recipe.insert(at + 1, std::move(selected));
                    inserted = true;
                    break;
                }
            if (!inserted)
                return false;
        }
        BootstrapStep equipment;
        equipment.name = "Build 204489 original starter equipment";
        if (!catalog_.Packets(context.entry.character, peer ? peer->player : uint8_t(1),
                              peer ? peer->character : uint16_t(57344), equipment.payload, !peer))
            return false;
        const auto body = peer ? peer->character : uint16_t(57344);
        BootstrapStep equipped;
        equipped.name = "Build 204489 equipped starter weapon set";
        // Original slot 29 is set 4's main hand. Native inventory initially uses unset set 11.
        if (!equipped.payload.U16(302) || !equipped.payload.VarU32(body) || !equipped.payload.U8(0) ||
            !equipped.payload.U8(4))
            return false;
        BootstrapStep current;
        current.name = "Build 204489 current weapon set";
        // Selecting the equipped terrestrial set alone does not activate the native skillbar.
        if (!current.payload.U16(298) || !current.payload.VarU32(body) || !current.payload.U8(4))
            return false;
        BootstrapStep permissions;
        permissions.name = "Build 204489 original weapon permissions";
        if (!peer && !catalog_.WeaponPermissionPackets(context.entry.character, permissions.payload))
            return false;
        if (!peer) {
            BootstrapStep support;
            support.name = "Build 204489 original support skill loadout";
            if (!catalog_.SupportSkillPackets(context.entry.character, support.payload))
                return false;
            bool inserted = false;
            for (auto at = recipe.begin(); at != recipe.end(); ++at)
                if (!std::strcmp(at->name, "skillbar ready")) {
                    recipe.insert(at + 1, std::move(support));
                    inserted = true;
                    break;
                }
            if (!inserted)
                return false;
        }
        for (auto at = recipe.begin(); at != recipe.end(); ++at)
            if (!std::strcmp(at->name, "inventory ready")) {
                at = recipe.insert(at, std::move(current));
                at = recipe.insert(at, std::move(equipped));
                at = recipe.insert(at, std::move(equipment));
                if (!peer)
                    recipe.insert(at, std::move(permissions));
                out = std::move(recipe);
                return true;
            }
        return false;
    }
};
} // namespace
bool LoadBuild204489Bootstrap(const std::wstring& root, std::unique_ptr<GameBootstrap>& out, std::string& error) {
    build204489::EquipmentCatalog catalog;
    if (!catalog.Load(root, error))
        return false;
    std::unique_ptr<GameBootstrap> next(new EquippedBuild204489Bootstrap(std::move(catalog)));
    out = std::move(next);
    error.clear();
    return true;
}
bool LoadBuild204489CharacterCodec(const std::wstring& root, std::unique_ptr<CharacterCodec>& out, std::string& error) {
    std::unique_ptr<build204489::EquipmentCatalog> next(new build204489::EquipmentCatalog);
    if (!next->Load(root, error))
        return false;
    out = std::move(next);
    error.clear();
    return true;
}
} // namespace glide
