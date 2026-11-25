#pragma once
#include "glide/builds.h"
#include <string>

namespace glide {
struct LaunchReply {
    uint32_t pid = 0, slot = 0;
    std::string error;
    bool Encode(Buffer& out) const;
    static bool Decode(ByteView bytes, LaunchReply& out);
};
struct LaunchRequest {
    std::wstring executable;
    std::wstring host;
    uint32_t build = 0, overrideBuild = 0;
    uint16_t port = 6112;
    bool windowed = false;
    bool Normalize(LaunchRequest& out, std::string& error) const;
    bool Encode(Buffer& out, std::string& error) const;
    static bool Decode(ByteView bytes, LaunchRequest& out, std::string& error);
    bool Identity(std::wstring& out, std::string& error) const;
    bool VerifyExecutable(const BuildInfo*& out, std::string& error) const;
};
} // namespace glide
