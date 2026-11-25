#include "client_startup.h"
#include "../build.h"
#include "glide/tls.h"
#include <Windows.h>
#include <cstring>

namespace glide {
namespace april14075 {
namespace {
NativeClientInputs inputs;
using HelloFn = unsigned(__fastcall*)(unsigned char*, void*, void*);
using DeviceFn = int(__fastcall*)(void*, void*, void*, unsigned, unsigned);
using ViewFn = void(__fastcall*)(void*, void*, int);
using DispatchFn = void(__fastcall*)(void*, void*, uintptr_t);
using FogFn = void(__fastcall*)(const void*, const void*, const void*, const void*, const void*, uint32_t);
HelloFn hello = nullptr;
void* oldPatcher = nullptr; // Full replacement still needs the hook engine's original slot.
DeviceFn device = nullptr;
ViewFn view = nullptr;
DispatchFn dispatch = nullptr;
FogFn fog = nullptr;
LONG currentView = -1, pendingCamera = 0, requestedAt = 0, fogDisabled = 0;
bool cameraDown = false, fogDown = false;
bool Gameplay() {
    return InterlockedCompareExchange(&currentView, 0, 0) == 16;
}
bool Foreground() {
    if (inputs.foreground)
        return inputs.foreground(inputs.platform);
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId();
}
bool Active() {
    return Foreground() && Gameplay();
}
bool Key(unsigned key) {
    return inputs.key ? inputs.key(inputs.platform, key) : (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
}
uint32_t Tick() {
    return inputs.tick ? inputs.tick(inputs.platform) : GetTickCount();
}
void Pause() {
    if (inputs.pause)
        inputs.pause(inputs.platform, 25);
    else
        Sleep(25);
}
void Report(const char* type, const char* text) {
    if (inputs.log)
        inputs.log->Write(type, text);
}
unsigned __fastcall Hello(unsigned char* client, void* unused, void* reader) {
    const auto session = LocalTlsSessionId(), secret = LocalTlsMasterSecret();
    inputs.assign(client + 0xde48, nullptr, session.data, static_cast<unsigned>(session.size));
    std::memcpy(client + 0x64, secret.data, secret.size);
    return hello(client, unused, reader);
}
void __fastcall Complete(void* self, void*) {
    if (!self)
        return;
    auto* bytes = static_cast<uint8_t*>(self);
    std::memset(bytes + 0x70, 0, 12);
    InterlockedExchange(reinterpret_cast<volatile LONG*>(bytes + 8), 3);
}
void Refresh(void* state) {
    if (!state)
        return;
    auto* bytes = static_cast<uint8_t*>(state);
    unsigned cached = 0;
    std::memcpy(&cached, bytes + 0x98, 4);
    if (cached)
        return;
    void* native = nullptr;
    std::memcpy(&native, bytes + 0xa0, sizeof(native));
    if (!native)
        return;
    auto** table = *static_cast<void***>(native);
    if (!table || !table[4])
        return;
    using Available = unsigned(__stdcall*)(void*);
    unsigned detected = reinterpret_cast<Available>(table[4])(native);
    if (!detected)
        return;
    if (detected > 512u * 1024u * 1024u)
        detected = 512u * 1024u * 1024u;
    std::memcpy(bytes + 0x98, &detected, 4);
}
int __fastcall Device(void* direct3d, void* unused, void* state, unsigned behavior, unsigned presentation) {
    const int result = device(direct3d, unused, state, behavior, presentation);
    if (result >= 0)
        Refresh(state);
    return result;
}
void __fastcall TrackView(void* context, void* unused, int state) {
    view(context, unused, state);
    int finalState = -2;
    if (context)
        std::memcpy(&finalState, static_cast<uint8_t*>(context) + 0x11c, 4);
    InterlockedExchange(&currentView, finalState);
    if (finalState != 16)
        InterlockedExchange(&pendingCamera, 0);
}
void ObserveKeys() {
    const bool active = Active(), control = Key(VK_CONTROL), camera = control && Key('F'), fogKey = control && Key('N');
    if (active && camera && !cameraDown)
        QueueCamera();
    if (active && fogKey && !fogDown)
        ToggleFog();
    cameraDown = camera;
    fogDown = fogKey;
    if (!active)
        InterlockedExchange(&pendingCamera, 0);
}
void __fastcall Dispatch(void* control, void* message, uintptr_t token) {
    ObserveKeys();
    if (InterlockedCompareExchange(&pendingCamera, 0, 0)) {
        const uint32_t elapsed = Tick() - static_cast<uint32_t>(InterlockedCompareExchange(&requestedAt, 0, 0));
        if (elapsed > 1500 || static_cast<uint32_t*>(control)[1] == 0x0c)
            InterlockedExchange(&pendingCamera, 0);
        else {
            void** holder = *reinterpret_cast<void***>(static_cast<uint8_t*>(control) + 8);
            if (holder && *holder) {
                unsigned flags = 0;
                std::memcpy(&flags, static_cast<uint8_t*>(*holder) + 0x144, 4);
                const unsigned enabled = !(flags & 1u);
                InterlockedExchange(&pendingCamera, 0);
                inputs.camera(*holder, nullptr, enabled);
                Report("camera", enabled ? "Free camera on (Ctrl+F)" : "Free camera off (Ctrl+F)");
            }
        }
    }
    dispatch(control, message, token);
}
void __fastcall Fog(const void* a, const void* b, const void* c, const void* d, const void* e, uint32_t f) {
    if (Gameplay() && InterlockedCompareExchange(&fogDisabled, 0, 0))
        inputs.fog();
    else
        fog(a, b, c, d, e, f);
}
bool CodeBytes(const PeImage& image, uint32_t rva, size_t size, ByteView& bytes) {
    for (const auto& section : image.Code())
        if (rva >= section.rva && uint64_t(rva) + size <= uint64_t(section.rva) + section.bytes.size) {
            bytes = ByteView(section.bytes.data + (rva - section.rva), size);
            return true;
        }
    return false;
}

} // namespace
bool ConfigureNativeClient(const NativeClientInputs& selected, std::string& error) {
    error = "April native startup requires own x86 helper functions and launcher slots";
#ifdef _WIN64
    (void)selected;
    return false;
#else
    if (selected.build != 14075 || !selected.assign || !selected.camera || !selected.fog || !selected.wrapper ||
        !selected.launcher)
        return false;
    inputs = selected;
    cameraDown = fogDown = false;
    InterlockedExchange(&currentView, -1);
    InterlockedExchange(&pendingCamera, 0);
    InterlockedExchange(&requestedAt, 0);
    InterlockedExchange(&fogDisabled, 0);
    error.clear();
    return true;
#endif
}
void QueueCamera() {
    if (!Active())
        return;
    InterlockedExchange(&requestedAt, static_cast<LONG>(Tick()));
    InterlockedExchange(&pendingCamera, 1);
}
void ToggleFog() {
    if (!Active())
        return;
    InterlockedExchange(&fogDisabled, !InterlockedCompareExchange(&fogDisabled, 0, 0));
}
unsigned RunAutoPlay() {
    unsigned attempt = 0;
    for (; attempt < 1200; ++attempt) {
        if (inputs.ready ? inputs.ready(inputs.platform) : NativeArchiveReady())
            break;
        Pause();
    }
    if (attempt == 1200) {
        if (inputs.log)
            Report("startup", NativeArchiveDiagnostic().c_str());
        return 0;
    }
    Report("startup", "April archive ready; requesting Play");
    bool announced = false;
    for (attempt = 0; attempt < 400 && !announced; ++attempt) {
        void* wrapper = inputs.wrapper ? *inputs.wrapper : nullptr;
        if (wrapper && wrapper != static_cast<void*>(inputs.wrapper + 1)) {
            InterlockedExchange(reinterpret_cast<volatile LONG*>(static_cast<uint8_t*>(wrapper) + 0x78), 1);
            announced = true;
        }
        void* launcher = inputs.launcher ? *inputs.launcher : nullptr;
        if (launcher) {
            InterlockedExchange(reinterpret_cast<volatile LONG*>(static_cast<uint8_t*>(launcher) + 0x78), 1);
            announced = true;
        }
        Pause();
    }
    return 0;
}
const std::vector<HookSpec>& ClientStartupHooks() {
    static const std::vector<HookSpec> hooks = [] {
        const auto& symbols = ClientStartupSymbols();
        if (symbols.size() != 13)
            return std::vector<HookSpec>();
        const size_t indices[] = {0, 2, 12, 7, 8, 10};
        void* callbacks[] = {reinterpret_cast<void*>(Hello),    reinterpret_cast<void*>(Complete),
                             reinterpret_cast<void*>(Device),   reinterpret_cast<void*>(TrackView),
                             reinterpret_cast<void*>(Dispatch), reinterpret_cast<void*>(Fog)};
        void** originals[] = {reinterpret_cast<void**>(&hello),    &oldPatcher,
                              reinterpret_cast<void**>(&device),   reinterpret_cast<void**>(&view),
                              reinterpret_cast<void**>(&dispatch), reinterpret_cast<void**>(&fog)};
        std::vector<HookSpec> result;
        for (size_t i = 0; i < 6; ++i) {
            const auto& s = symbols[indices[i]];
            result.push_back({s.name, s.symbol, callbacks[i], originals[i], true});
        }
        return result;
    }();
    return hooks;
}
bool SplashPatch(ByteView bytes, StartupPatch& out) {
    if (bytes.size != 6 || !bytes.data || bytes.data[0] != 0xe8 || bytes.data[5] != 0x90)
        return false;
    StartupPatch candidate;
    candidate.size = 5;
    std::memcpy(candidate.expected.data(), bytes.data, 5);
    candidate.replacement.fill(0x90);
    out = candidate;
    return true;
}
bool CoordinatorPatch(ByteView bytes, StartupPatch& out) {
    static const uint8_t expected[] = {0x4f, 1, 0, 0};
    if (bytes.size != 4 || !bytes.data || std::memcmp(bytes.data, expected, 4))
        return false;
    StartupPatch candidate;
    candidate.size = 1;
    candidate.expected[0] = 0x4f;
    candidate.replacement[0] = 0x40;
    out = candidate;
    return true;
}
bool PlanClientPatches(const PeImage& image, std::vector<StartupPatch>& out, std::string& error) {
    error = "April patches require matching original executable and instruction bytes";
    if (image.Machine() != Bitness::X86 || ResolveSymbol(image, kBuild14075.buildGetter).count != 1)
        return false;
    const auto& symbols = ClientStartupSymbols();
    if (symbols.size() != 13)
        return false;
    const auto splash = ResolveSymbol(image, symbols[3].symbol), coordinator = ResolveSymbol(image, symbols[4].symbol);
    if (splash.count != 1 || coordinator.count != 1 || splash.rva < 6)
        return false;
    ByteView bytes;
    StartupPatch first, second;
    if (!CodeBytes(image, splash.rva - 6, 6, bytes) || !SplashPatch(bytes, first) ||
        !CodeBytes(image, coordinator.rva + 22, 4, bytes) || !CoordinatorPatch(bytes, second))
        return false;
    first.rva = splash.rva - 6;
    second.rva = coordinator.rva + 22;
    out = {first, second};
    error.clear();
    return true;
}
} // namespace april14075
} // namespace glide
