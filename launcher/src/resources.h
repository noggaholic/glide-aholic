#pragma once
#include "runtime.h"
#include <memory>

namespace glide {
class PreparedRuntime {
  public:
    static bool Prepare(const std::wstring& sourceExe, const std::wstring& manifestPath,
                        const std::wstring& runtimeRoot, std::unique_ptr<PreparedRuntime>& out, std::string& error,
                        const std::wstring& contentDirectory = std::wstring());
    const RuntimeSlot& Slot() const {
        return slot_;
    }
    const RuntimeManifest& Manifest() const {
        return manifest_;
    }
    const std::wstring& Executable() const {
        return executable_;
    }
    const std::wstring& Archive() const {
        return archive_;
    }

  private:
    RuntimeManifest manifest_;
    ReadOnlyResources originals_;
    RuntimeSlot slot_;
    std::wstring executable_, archive_;
};
} // namespace glide
