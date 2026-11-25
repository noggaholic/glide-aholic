#pragma once
#include <string>
#include <vector>

namespace glide {
class KvDocument {
  public:
    static bool Parse(const std::string& text, KvDocument& out, std::string& error);
    bool Get(const std::string& section, const std::string& key, std::string& value) const;
    size_t Count(const std::string& section, const std::string& key) const;
    bool Set(const std::string& section, const std::string& key, const std::string& value);
    bool SetValues(const std::string& section, const std::string& key, const std::vector<std::string>& values);
    bool RemoveSection(const std::string& section);
    std::string Serialize() const;
    std::vector<std::string> Sections() const;
    std::vector<std::pair<std::string, std::string>> Entries(const std::string& section) const;

  private:
    struct Line {
        std::string section, key, value, raw;
        bool entry = false;
    };
    std::vector<Line> lines_;
};
bool ParseUnsigned(const std::string& text, unsigned maximum, unsigned& out);
} // namespace glide
