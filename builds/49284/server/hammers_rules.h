#pragma once
#include "self_effect.h"
#include "glide/profile/modern_hammers.h"

namespace glide {
namespace hot49284 {
// Duration0 is an explicit retained-instance adaptation, removed on termination. Its state uses H2, never
// reinterprets an old H1 as the elite, and drains net3 instead of the Hammers net2.
using Hammers = modern::Hammers<49284>;
inline bool ReserveHammerInstance(SelfEffect& effects, uint32_t& instance) {
    // HoT profession9 has no admitted timed aura. Reuse its existing durable
    // allocator through the public codec; never reset an announced timed effect.
    const auto text = effects.Text();
    const auto bar = text.find('|');
    if (text.compare(0, 2, "1:") || bar == std::string::npos || bar + 1 != text.size())
        return false;
    uint64_t next = 0;
    for (size_t i = 2; i < bar; ++i) {
        if (text[i] < '0' || text[i] > '9' || next > (UINT32_MAX - unsigned(text[i] - '0')) / 10)
            return false;
        next = next * 10 + unsigned(text[i] - '0');
    }
    if (!next || next >= modern::kFirstConditionInstance)
        return false;
    Character saved;
    saved.profileFields["skill_effects"] = "1:" + std::to_string(next + 1) + '|';
    if (!effects.Load(saved, 9))
        return false;
    instance = static_cast<uint32_t>(next);
    return true;
}
} // namespace hot49284
} // namespace glide
