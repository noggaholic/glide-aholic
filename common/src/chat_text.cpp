#include "glide/chat_text.h"
#include "glide/files.h"

namespace glide {
bool SplitChatText(const std::string& text, ChatTextLimits limits, std::vector<std::string>& lines) {
    if (!limits.units)
        return false;
    std::wstring wide;
    if (!Utf8ToWide(text, wide))
        return false;
    std::vector<std::string> result;
    for (size_t start = 0; start < wide.size();) {
        size_t end = start, used = 0;
        while (end < wide.size()) {
            const auto unit = static_cast<unsigned>(wide[end]);
            const size_t length = unit >= 0xd800 && unit <= 0xdbff ? 2 : 1;
            const size_t cost = length + (limits.countEscapes && (unit == '[' || unit == ']' || unit == '\\') ? 1 : 0);
            if (cost > limits.units - used)
                break;
            used += cost;
            end += length;
        }
        if (end == start)
            return false;
        std::string line;
        if (!WideToUtf8(wide.substr(start, end - start), line))
            return false;
        result.push_back(std::move(line));
        start = end;
    }
    lines = std::move(result);
    return true;
}
} // namespace glide
