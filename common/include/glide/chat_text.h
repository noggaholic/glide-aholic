#pragma once
#include <string>
#include <vector>

namespace glide {
struct ChatTextLimits {
    size_t units = 0; // UTF-16 text budget, excluding the profile's framing.
    bool countEscapes = false;
};
// Preserves all text and Unicode characters. Empty text produces no lines.
// Failure leaves lines unchanged.
bool SplitChatText(const std::string& text, ChatTextLimits limits, std::vector<std::string>& lines);
} // namespace glide
