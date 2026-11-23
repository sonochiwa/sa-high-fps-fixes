#include "hud/airstrip_light.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/hud.h"

#include <algorithm>
#include <cstdint>

namespace hff::hud {

namespace {

SitePatch g_airstripLightPatch{};

// While the game is paused the timestep stands still but the game's own gate
// keeps reading the pause mode clock, which runs on the pause menu map. There
// the light steps once per 1/30 s of that clock. A frame of whole milliseconds
// at 30 FPS is 33 or 34 ms; the millisecond of slack lets each of them
// through, as the 3 ms gate did, and the unspent time carries so the pace stays
// at 30 steps a second.
constexpr float kOriginalFrameMs = 1000.0f / 30.0f;
// A light that has not been drawn for this long starts afresh rather than
// catching up.
constexpr uint32_t kRestartGapMs = 250;

uint32_t g_lastPausedClock{};
bool g_pausedClockSeen{};
float g_unspentPausedMs{};

int32_t PausedLightSteps() {
    const uint32_t now =
        *reinterpret_cast<volatile const uint32_t*>(kTimerTimeInMillisecondsPauseMode);
    if (!g_pausedClockSeen || now < g_lastPausedClock
        || now - g_lastPausedClock > kRestartGapMs) {
        g_pausedClockSeen = true;
        g_lastPausedClock = now;
        g_unspentPausedMs = 0.0f;
        return 1;
    }
    g_unspentPausedMs += static_cast<float>(now - g_lastPausedClock);
    g_lastPausedClock = now;
    if (g_unspentPausedMs < kOriginalFrameMs - 1.0f) {
        return 0;
    }
    g_unspentPausedMs =
        std::min(g_unspentPausedMs - kOriginalFrameMs, kOriginalFrameMs);
    return 1;
}

// One step per original frame: on every frame at or below 30 FPS, as the
// 3 ms gate allowed, and 30 a second above it.
int32_t __cdecl AirstripLightSteps() {
    const bool paused = *reinterpret_cast<volatile const uint8_t*>(kTimerUserPause)
                     || *reinterpret_cast<volatile const uint8_t*>(kTimerCodePause);
    if (paused) {
        return PausedLightSteps();
    }
    g_pausedClockSeen = false;
    return FrameTick(kFrameTickAirstripLight);
}

// Replaces the gate with the decision above. The radar distance stays on the
// x87 stack for either path: the step compares it again and the hold pops it.
// The flags are not read past the gate.
__declspec(naked) void AirstripLightGateThunk() {
    __asm {
        push eax
        push ecx
        push edx
        call AirstripLightSteps
        test eax, eax
        pop edx
        pop ecx
        pop eax
        jz hold
        jmp kAirstripLightStep
    hold:
        jmp kAirstripLightHold
    }
}

} // namespace

bool InstallAirstripLightFix() {
    if (!InstallJump(g_airstripLightPatch, kAirstripLightGate, &AirstripLightGateThunk,
                     kExpectedAirstripLightGate)) {
        Log("Airstrip light fix skipped: CRadar::DrawEntityBlip bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed the radar airstrip light at its 30 FPS pace.");
    return true;
}

} // namespace hff::hud
