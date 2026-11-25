#pragma once
#include "hooks.h"
#include "glide/builds.h"
#include "glide/capabilities.h"

namespace glide {
struct ClientRuntime;
class ClientProfile {
  public:
    virtual ~ClientProfile() {}
    virtual const BuildInfo& Info() const = 0;
    virtual CapabilitySet Capabilities() const = 0;
    virtual const std::vector<HookSpec>& CodeHooks() const = 0;
    virtual const wchar_t* LauncherUrlToPatch() const = 0;
    // Resolve helpers, prepare private paths, export hooks and launcher URL; do not enable hooks.
    virtual bool Prepare(ClientRuntime&, const PeImage&, void* module, std::string& error) const = 0;
    virtual bool AfterHooks(ClientRuntime&, std::string& error) const = 0;
    virtual void ToggleFreeCamera(ClientRuntime&) const = 0;
    virtual void ToggleFog(ClientRuntime&) const = 0;
};
namespace ClientRegistry {
const ClientProfile* Find(uint32_t build);
}
} // namespace glide
