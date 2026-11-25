#pragma once
#include "startup.h"
namespace glide {
using HotProfessionAvailable = int(__fastcall*)(unsigned, void*);
using HotProfessionContext = unsigned(__fastcall*)(void*, void*);
using HotMovementInput = void(__fastcall*)(void*, void*, int);
using HotMovementEligible = int(__cdecl*)();
using HotGlideApply = void(__fastcall*)(void*, void*, const uint8_t*);
using HotGlidePhysics = void(__fastcall*)(void*, void*, float);
class Log;
void ObserveHotGlideApply(void* self, void* unused, const uint8_t* state, HotGlideApply original, Log& log);
void ObserveHotGlidePhysics(void* self, void* unused, float value, HotGlidePhysics original, Log& log);
int HotLocalProfession(unsigned profession, void* unused, HotProfessionAvailable original);
unsigned HotLocalProfessionValidation(void* context, void* unused, HotProfessionContext profession,
                                      HotProfessionContext original);
void DispatchHotGliderInput(void* self, void* unused, int input, HotMovementEligible eligible,
                            HotMovementInput original);
const std::vector<HotSymbol>& HotControlSymbols();
const std::vector<HookSpec>& HotControlHooks();
bool PrepareHotControls(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace glide
