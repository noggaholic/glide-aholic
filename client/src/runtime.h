#pragma once
#include "hooks.h"
#include "string_patch.h"
#include "glide/slot_config.h"
#include "glide/runtime_manifest.h"
#include "glide/log.h"
#include "glide/resource_locks.h"

namespace glide {
class ClientProfile;
// One process-lifetime owner; callback state and patches must survive every game/browser thread.
struct ClientRuntime {
    SlotConfig config;
    RuntimeManifest manifest;
    ReadOnlyResources originals;
    Hooks hooks;
    Log log;
    RelativeStringPatch launcherUrl;
    const ClientProfile* profile = nullptr;
};
} // namespace glide
