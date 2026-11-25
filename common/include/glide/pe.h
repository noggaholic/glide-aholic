#pragma once
#include "glide/buffer.h"
#include <array>
#include <vector>

namespace glide {
enum class Bitness { X86, X64 };
struct CodeSection {
    ByteView bytes;
    uint32_t rva;
    std::array<char, 9> name{};
    uint32_t characteristics = 0;
};

class PeImage {
  public:
    PeImage() = default;
    PeImage(PeImage&&) = default;
    PeImage& operator=(PeImage&&) = default;
    PeImage(const PeImage&) = delete;
    PeImage& operator=(const PeImage&) = delete;
    static bool FromFile(const wchar_t* path, PeImage& out);
    static bool FromBytes(ByteView file, PeImage& out);
    // Copies executable sections from a loaded Windows image; RVAs remain module-relative.
    static bool FromModule(const void* module, PeImage& out);
    Bitness Machine() const {
        return machine_;
    }
    const std::vector<CodeSection>& Code() const {
        return code_;
    }
    const std::array<uint8_t, 32>& Fingerprint() const {
        return fingerprint_;
    }
    size_t SourceSize() const {
        return sourceSize_;
    }

  private:
    bool Parse();
    Buffer file_;
    Bitness machine_ = Bitness::X86;
    std::vector<CodeSection> code_;
    std::array<uint8_t, 32> fingerprint_{};
    size_t sourceSize_ = 0;
};
} // namespace glide
