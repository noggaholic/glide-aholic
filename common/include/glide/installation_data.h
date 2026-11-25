#pragma once
#include "glide/archive.h"
#include "glide/buffer.h"
#include <array>
#include <map>
#include <string>
#include <vector>

namespace glide {
struct InstallationDataInput {
    std::string name;
    std::wstring source;
};

// Glide-only data appended after the native archive. Native resource mappings
// are unchanged; this package does not install resources opened by the game.
class InstallationData {
  public:
    bool Open(const std::wstring& archive, unsigned build, std::string& error);
    bool Read(const std::string& name, Buffer& out, std::string& error);
    bool Verify(std::string& error);
    bool Extract(const std::wstring& directory, std::string& error, const std::string& prefix = {});
    unsigned Build() const {
        return build_;
    }
    size_t Count() const {
        return entries_.size();
    }
    uint64_t OriginalSize() const {
        return originalSize_;
    }
    static bool Create(const std::wstring& original, const std::wstring& output, unsigned build,
                       const std::vector<InstallationDataInput>& inputs, std::string& error);
    // Two phases allow an interrupted append to be recovered independently.
    static bool StageAppend(const std::wstring& archive, unsigned build,
                            const std::vector<InstallationDataInput>& inputs, std::string& error, bool update = false);
    static bool CommitAppend(const std::wstring& archive, unsigned build, std::string& error);
    static bool RollbackAppend(const std::wstring& archive, unsigned build, std::string& error);
    static bool AppendChunked(const std::wstring& archive, unsigned build,
                              const std::vector<InstallationDataInput>& inputs, std::string& error);

  private:
    bool OpenImpl(const std::wstring& archive, unsigned build, std::string& error, bool allowWriter);
    struct Entry {
        uint64_t offset = 0;
        uint32_t size = 0;
        std::array<uint8_t, 32> hash{};
    };
    ArchiveReader archive_;
    std::map<std::string, Entry> entries_;
    uint64_t originalSize_ = 0;
    uint64_t baseOffset_ = 0;
    unsigned build_ = 0;
};
} // namespace glide
