#include "glide/runtime_field.h"
// Reused from builds/204489/client/exploration.cpp and the verified old China controls.
// China has its own signatures and camera flag offset 0x3b0; the other native contracts are retained.
#include "controls.h"
#include "control_symbols.h"
#include "../build.h"
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <float.h>

namespace glide {
namespace china167409 {
namespace {
static Log* clientLog = nullptr;
static void log_line(const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf_s(text, sizeof(text), _TRUNCATE, format, args);
    va_end(args);
    if (clientLog)
        clientLog->Write("client", text);
}
typedef unsigned long long(__fastcall* MovementTransitionFn)(void*);
typedef unsigned int(__fastcall* MovementPredicateFn)(void*);
typedef unsigned int(__fastcall* MovementPredicateModeFn)(void*, int);
typedef void*(__fastcall* CharacterCollisionCreateFn)(void*, void*, void*, unsigned int, float, float, float, float,
                                                      float*);
typedef void(__fastcall* CharacterCollisionMoveFn)(void*, float*);
typedef void*(__fastcall* CubeMapQualityLookupFn)(int, int, int);
typedef void(__fastcall* PartyRootFn)(long long);
typedef BOOL(__fastcall* GameModeType8Fn)(void);
typedef void*(__fastcall* UiServiceRootFn)(void);
typedef void(__fastcall* GameplayDispatchFn)(unsigned int*, float*, unsigned long long);
typedef void(__fastcall* DebugCameraToggleFn)(void*, unsigned int);
typedef void(__fastcall* EnvironmentFogSubmitFn)(const float*, const float*, const float*, const float*, const float*,
                                                 float);
typedef void(__fastcall* EnvironmentFogDisableFn)(void);
static MovementTransitionFn original_movement_transition;
static MovementPredicateFn original_movement_policy;
static MovementPredicateModeFn original_glider_permission;
static CharacterCollisionCreateFn original_character_collision_create;
static CharacterCollisionMoveFn original_character_collision_move;
static CubeMapQualityLookupFn original_cube_map_quality_lookup;
static void** g_cube_map_quality_table;
static PartyRootFn original_party_root;
static GameModeType8Fn original_game_mode_type8;
static UiServiceRootFn ui_service_root;
static GameplayDispatchFn original_gameplay_dispatch;
static DebugCameraToggleFn debug_camera_toggle;
static EnvironmentFogSubmitFn original_environment_fog_submit;
static EnvironmentFogDisableFn environment_fog_disable;
static HANDLE g_glider_thread;
static volatile LONG g_glider_stop;
static volatile LONG g_glider_key_pending;
alignas(8) static volatile LONG64 g_glider_key_time;
static volatile LONG g_freecam_key_down;
static volatile LONG g_freecam_key_pending;
alignas(8) static volatile LONG64 g_freecam_key_time;
static volatile LONG g_fog_key_down;
static volatile LONG g_fog_disabled;
static thread_local LONG g_movement_transition_depth;
static thread_local LONG g_movement_policy_seen;
static thread_local LONG g_movement_policy_result;
static thread_local unsigned int g_movement_state;
static thread_local LONG g_force_party_path;

/* Some legacy and test maps request a cube-map descriptor whose mip count differs from the two
   descriptors shipped by the current client. Keep the stock exact lookup first. A missing result may reuse a
   client-owned descriptor only when dimension and format are still exact; an unknown size or format remains an error.
 */
static void* __fastcall detour_cube_map_quality_lookup(int dimension, int mip_levels, int format) {
    void* result = original_cube_map_quality_lookup(dimension, mip_levels, format);
    unsigned int index;
    if (result || !g_cube_map_quality_table)
        return result;
    __try {
        for (index = 0; index < 2; ++index) {
            unsigned char* descriptor = static_cast<unsigned char*>(g_cube_map_quality_table[index]);
            int descriptor_format;
            int descriptor_dimension;
            int descriptor_mip_levels;
            if (!descriptor)
                continue;
            descriptor_format = glide::ReadRuntimeField<int>(descriptor, 0x04);
            descriptor_dimension = glide::ReadRuntimeField<int>(descriptor, 0x08);
            descriptor_mip_levels = glide::ReadRuntimeField<int>(descriptor, 0x0c);
            if (descriptor_dimension != dimension || descriptor_format != format)
                continue;
            log_line("cube-map compatibility: requested=(%d,%d,%d) descriptor=%u mip=%d", dimension, mip_levels, format,
                     index, descriptor_mip_levels);
            return descriptor;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("cube-map compatibility refused an unreadable descriptor table");
    }
    return NULL;
}

static BOOL party_team_service_available(BOOL* known) {
    void* root = NULL;
    void* team = NULL;
    if (known)
        *known = FALSE;
    if (!ui_service_root)
        return FALSE;
    __try {
        root = ui_service_root();
        if (root) {
            void** vtable = *(void***)root;
            typedef void*(__fastcall * GetTeamServiceFn)(void*);
            GetTeamServiceFn get_team = vtable ? (GetTeamServiceFn)vtable[0x118 / sizeof(void*)] : NULL;
            if (get_team)
                team = get_team(root);
        }
        if (known)
            *known = TRUE;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
    return team != NULL;
}

static BOOL __fastcall detour_game_mode_type8(void) {
    if (g_force_party_path > 0)
        return TRUE;
    return original_game_mode_type8();
}

/* Some PvP-style maps have no team service in the minimal authoritative session. PartyRoot's
   team-only branch then dereferences NULL. For only that synchronous build call, select its
   existing party/squad branch, which already supports both channels being absent. */
static void __fastcall detour_party_root(long long context) {
    BOOL known = FALSE;
    BOOL has_team = TRUE;
    BOOL type8 = TRUE;
    __try {
        type8 = original_game_mode_type8();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        type8 = TRUE;
    }
    if (!type8)
        has_team = party_team_service_available(&known);
    if (!type8 && known && !has_team) {
        log_line("PartyRoot: missing team service; using party/squad path");
        ++g_force_party_path;
        __try {
            original_party_root(context);
        } __finally {
            --g_force_party_path;
        }
        return;
    }
    original_party_root(context);
}

/* Changes one decision only: the final natural glider permission result. The
   override is active solely during the client's own movement transition, after its earlier
   policy predicate has passed, while the airborne state bit is present, and immediately after
   a physical Space press in this process. */
static unsigned int __fastcall detour_movement_policy(void* object) {
    unsigned int result = original_movement_policy(object);
    if (g_movement_transition_depth > 0) {
        ++g_movement_policy_seen;
        g_movement_policy_result = (LONG)result;
    }
    return result;
}

static unsigned int __fastcall detour_glider_permission(void* object, int mode) {
    unsigned int result = original_glider_permission(object, mode);
    if (g_movement_transition_depth > 0 && result == 0 && mode == 0 && g_movement_policy_seen > 0 &&
        g_movement_policy_result == 0 && (g_movement_state & 0x20000) != 0 &&
        InterlockedCompareExchange(&g_glider_key_pending, 0, 0) != 0) {
        result = 1;
        InterlockedExchange(&g_glider_key_pending, 0);
        log_line("glider permission accepted at the native movement gate");
    }
    return result;
}

static int foreground_key_down(int key) {
    DWORD foreground_pid = 0;
    HWND foreground = GetForegroundWindow();
    if (foreground)
        GetWindowThreadProcessId(foreground, &foreground_pid);
    return foreground_pid == GetCurrentProcessId() && (GetAsyncKeyState(key) & 0x8000) != 0;
}

static void observe_freecam_key(void) {
    int down = foreground_key_down(VK_CONTROL) && foreground_key_down('F');
    if (!down) {
        InterlockedExchange(&g_freecam_key_down, 0);
        return;
    }
    if (InterlockedCompareExchange(&g_freecam_key_down, 1, 0) == 0) {
        QueueCamera();
    }
}

static void observe_fog_key(void) {
    int down = foreground_key_down(VK_CONTROL) && foreground_key_down('N');
    if (!down) {
        InterlockedExchange(&g_fog_key_down, 0);
        return;
    }
    if (InterlockedCompareExchange(&g_fog_key_down, 1, 0) == 0) {
        ToggleFog();
    }
}

/* EnvContext calls this after calculating its haze and immediately before the values are written
   into the active graphics device. Suppressing only this submit would retain the previous frame's
   fog, so use GrDev's own disable operation while Ctrl+N is active. */
static void __fastcall detour_environment_fog_submit(const float* near_color, const float* far_color,
                                                     const float* distance_range, const float* height_color,
                                                     const float* height_range, float depth_cue) {
    if (InterlockedCompareExchange(&g_fog_disabled, 0, 0) != 0) {
        environment_fog_disable();
        return;
    }
    original_environment_fog_submit(near_color, far_color, distance_range, height_color, height_range, depth_cue);
}

static unsigned long long __fastcall detour_movement_transition(void* context) {
    unsigned long long result;
    int outer = ++g_movement_transition_depth == 1;
    if (outer) {
        g_movement_policy_seen = 0;
        g_movement_policy_result = -1;
        g_movement_state = 0;
        __try {
            g_movement_state = glide::ReadRuntimeField<unsigned int>(context, 0x178);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    result = original_movement_transition(context);
    --g_movement_transition_depth;
    return result;
}

static DWORD WINAPI glider_key_worker(void* unused) {
    int was_down = 0;
    (void)unused;
    while (InterlockedCompareExchange(&g_glider_stop, 0, 0) == 0) {
        int down;
        ULONGLONG now = GetTickCount64();
        down = foreground_key_down(VK_SPACE);
        observe_freecam_key();
        observe_fog_key();
        if (down && !was_down) {
            SpacePressed();
        }
        was_down = down;
        if (InterlockedCompareExchange(&g_glider_key_pending, 0, 0) != 0) {
            ULONGLONG started = (ULONGLONG)InterlockedCompareExchange64(&g_glider_key_time, 0, 0);
            if (now >= started && now - started > 1500)
                InterlockedExchange(&g_glider_key_pending, 0);
        }
        Sleep(8);
    }
    return 0;
}

/* Normal accounts do not register the hidden GM camera action, so its action id never reaches
   this dispatcher. A foreground physical-Ctrl+F edge is queued by the input poller and consumed here.
   The control-instance header owns the live GameplayContext at *(control+8). Calling the complete
   native toggle on this gameplay/UI thread preserves camera input setup and map-leave cleanup
   without granting the account any other GM capability. */
static void __fastcall detour_gameplay_dispatch(unsigned int* control, float* event, unsigned long long argument) {
    void* gameplay = NULL;
    unsigned int enable = 0;
    int toggle = 0;
    ULONGLONG now, started;
    observe_freecam_key();
    if (InterlockedCompareExchange(&g_freecam_key_pending, 0, 0) != 0) {
        now = GetTickCount64();
        started = (ULONGLONG)InterlockedCompareExchange64(&g_freecam_key_time, 0, 0);
        if (now >= started && now - started <= 1500) {
            __try {
                if (control) {
                    void** holder = glide::ReadRuntimeField<void**>(control, 8);
                    if (holder && *holder) {
                        /* CHINA's original Gameplay.cpp F branch and native
                         * toggle both use +0x3b0; modern uses +0x3b8. */
                        unsigned int flags_offset = kCameraFlagsOffset;
                        gameplay = *holder;
                        enable = ((glide::ReadRuntimeField<unsigned int>(gameplay, flags_offset) >> 1) & 1) ^ 1;
                        toggle = 1;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                gameplay = NULL;
                toggle = 0;
            }
        }
        InterlockedExchange(&g_freecam_key_pending, 0);
    }
    if (toggle) {
        debug_camera_toggle(gameplay, enable);
        log_line("debug camera %s from Ctrl+F", enable ? "enabled" : "disabled");
        return;
    }
    original_gameplay_dispatch(control, event, argument);
}

static void clamp_position_to_scene(void* scene, float* raw_position) {
    float minimum[3], maximum[3], world[3];
    unsigned int sector = 0;
    int i;
    if (!scene || !raw_position)
        return;
    __try {
        for (i = 0; i < 3; ++i) {
            minimum[i] = glide::ReadRuntimeField<float>(scene, 0x50 + i * 4);
            maximum[i] = glide::ReadRuntimeField<float>(scene, 0x60 + i * 4);
            if (!_finite(minimum[i]) || !_finite(maximum[i]) || minimum[i] >= maximum[i])
                return;
        }
        memcpy(&sector, raw_position + 3, sizeof(sector));
        world[0] = raw_position[0] / 32.0f;
        world[1] = raw_position[1] / 32.0f;
        world[2] = -raw_position[2] / 32.0f;
        if (!sector && minimum[0] <= world[0] && world[0] <= maximum[0] && minimum[1] <= world[1] &&
            world[1] <= maximum[1] && minimum[2] <= world[2] && world[2] <= maximum[2])
            return;
        raw_position[0] = (minimum[0] + maximum[0]) * 16.0f;
        raw_position[1] = (minimum[1] + maximum[1]) * 16.0f;
        raw_position[2] = -(minimum[2] + maximum[2]) * 16.0f;
        raw_position[3] = 0.0f;
        log_line("spawn moved inside the scene bounds");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

static void* __fastcall detour_character_collision_create(void* self, void* scene, void* owner, unsigned int flags,
                                                          float radius, float height, float step_height, float angle,
                                                          float* raw_position) {
    clamp_position_to_scene(scene, raw_position);
    return original_character_collision_create(self, scene, owner, flags, radius, height, step_height, angle,
                                               raw_position);
}

static void __fastcall detour_character_collision_move(void* self, float* raw_position) {
    void* scene = NULL;
    __try {
        if (self)
            scene = *(void**)((unsigned char*)self + 0x70);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    clamp_position_to_scene(scene, raw_position);
    original_character_collision_move(self, raw_position);
}

} // namespace
void QueueCamera() {
    InterlockedExchange64(&g_freecam_key_time, static_cast<LONG64>(GetTickCount64()));
    InterlockedExchange(&g_freecam_key_pending, 1);
    log_line("Camera toggle queued for the gameplay thread");
}
void ToggleFog() {
    const LONG disabled = InterlockedCompareExchange(&g_fog_disabled, 0, 0) == 0;
    InterlockedExchange(&g_fog_disabled, disabled);
    log_line("Renderer fog %s", disabled ? "disabled" : "restored");
}
void SpacePressed() {
    InterlockedExchange64(&g_glider_key_time, static_cast<LONG64>(GetTickCount64()));
    InterlockedExchange(&g_glider_key_pending, 1);
}
const std::vector<HookSpec>& ControlHooks() {
    const auto& symbols = ControlSymbols();
    struct Detour {
        size_t index;
        void* replacement;
        void** original;
    };
    const Detour detours[] = {
        {0, reinterpret_cast<void*>(detour_movement_transition),
         reinterpret_cast<void**>(&original_movement_transition)},
        {1, reinterpret_cast<void*>(detour_movement_policy), reinterpret_cast<void**>(&original_movement_policy)},
        {2, reinterpret_cast<void*>(detour_glider_permission), reinterpret_cast<void**>(&original_glider_permission)},
        {3, reinterpret_cast<void*>(detour_character_collision_create),
         reinterpret_cast<void**>(&original_character_collision_create)},
        {4, reinterpret_cast<void*>(detour_character_collision_move),
         reinterpret_cast<void**>(&original_character_collision_move)},
        {5, reinterpret_cast<void*>(detour_cube_map_quality_lookup),
         reinterpret_cast<void**>(&original_cube_map_quality_lookup)},
        {6, reinterpret_cast<void*>(detour_party_root), reinterpret_cast<void**>(&original_party_root)},
        {7, reinterpret_cast<void*>(detour_game_mode_type8), reinterpret_cast<void**>(&original_game_mode_type8)},
        {9, reinterpret_cast<void*>(detour_gameplay_dispatch), reinterpret_cast<void**>(&original_gameplay_dispatch)},
        {11, reinterpret_cast<void*>(detour_environment_fog_submit),
         reinterpret_cast<void**>(&original_environment_fog_submit)}};
    static const std::vector<HookSpec> specs = [&] {
        std::vector<HookSpec> result;
        for (const auto& detour : detours) {
            const auto& symbol = symbols[detour.index];
            result.push_back({symbol.name, symbol.symbol, detour.replacement, detour.original, true});
        }
        return result;
    }();
    return specs;
}
bool PrepareControls(const PeImage& image, void* module, Log& log, std::string& error) {
    error = "China controls require the original x64 client and one initialization";
#ifdef _WIN64
    if (image.Machine() != Bitness::X64 || !module || clientLog ||
        ResolveSymbol(image, kBuild167409.buildGetter).count != 1)
        return false;
    const auto& symbols = ControlSymbols();
    std::vector<uint8_t*> targets;
    for (const auto& symbol : symbols) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("Required China control RVA is not verified: ") + symbol.name;
            return false;
        }
        targets.push_back(static_cast<uint8_t*>(module) + match.rva);
    }
    int32_t displacement = 0;
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), targets[5] + 6, &displacement, sizeof(displacement), &read) ||
        read != sizeof(displacement)) {
        error = "Cannot resolve cube-map descriptor table";
        return false;
    }
    g_cube_map_quality_table = reinterpret_cast<void**>(targets[5] + 10 + displacement);
    void* table[2];
    if (!ReadProcessMemory(GetCurrentProcess(), g_cube_map_quality_table, table, sizeof(table), &read) ||
        read != sizeof(table)) {
        error = "Cannot read cube-map descriptor table";
        return false;
    }
    ui_service_root = reinterpret_cast<UiServiceRootFn>(targets[8]);
    debug_camera_toggle = reinterpret_cast<DebugCameraToggleFn>(targets[10]);
    environment_fog_disable = reinterpret_cast<EnvironmentFogDisableFn>(targets[12]);
    clientLog = &log;
    error.clear();
    return true;
#else
    (void)image;
    (void)module;
    (void)log;
    return false;
#endif
}
bool StartHotkeys(std::string& error) {
    error = "Cannot start China keyboard monitoring";
    if (!clientLog || g_glider_thread)
        return false;
    InterlockedExchange(&g_glider_stop, 0);
    g_glider_thread = CreateThread(nullptr, 0, glider_key_worker, nullptr, 0, nullptr);
    if (!g_glider_thread)
        return false;
    clientLog->Write("client", "Ctrl+F camera, Ctrl+N fog and glider input monitoring started");
    error.clear();
    return true;
}
} // namespace china167409
} // namespace glide
