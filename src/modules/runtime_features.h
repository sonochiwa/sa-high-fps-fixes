#pragma once

#include "modules/prelude.h"

namespace hff {

float __cdecl SampObjectRotationRemainingFraction(float elapsedDistance, float totalDistance);
extern uint8_t g_sampObjectMoveExpired;
void __cdecl EvaluateSampObjectMoveExpiry(float elapsedDistance, float totalDistance);
int32_t __cdecl ShouldTickFakePhysicsCounter();

using PadStateFn = bool(__thiscall*)(void*);
void* PadAt(int index);
uintptr_t __cdecl SelectSirenReturnAddress(uintptr_t vehicle);
void WriteFrameLimit(uint8_t value);
uint8_t ReadFrameLimit();
bool ScriptNameMatches(const char* name, const char* expected);
int PreferredScriptFpsLimit();
void __cdecl ProcessAutoFpsLimit();
void __cdecl ProcessFrameHooks();
void __cdecl OnPauseMenuBackground();

} // namespace hff
