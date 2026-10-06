#include "camera/stunt_jump_camera.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/camera.h"

namespace hff::camera {

namespace {

SitePatch g_endTimerPatch{};
SitePatch g_flightTimerPatch{};
float g_endTimerFraction{};
float g_flightTimerFraction{};
bool g_endTimerActive{};
bool g_flightTimerActive{};

// The two timers run one after the other, so each starts from a clean
// fraction when the other one was the last to run.
int __cdecl AccumulateFlightTimer(float milliseconds) {
    if (!g_flightTimerActive) {
        g_flightTimerFraction = 0.0f;
        g_flightTimerActive = true;
    }
    g_endTimerActive = false;
    return AccumulateMilliseconds(milliseconds, g_flightTimerFraction);
}

int __cdecl AccumulateEndTimer(float milliseconds) {
    if (!g_endTimerActive) {
        g_endTimerFraction = 0.0f;
        g_endTimerActive = true;
    }
    g_flightTimerActive = false;
    return AccumulateMilliseconds(milliseconds, g_endTimerFraction);
}

__declspec(naked) void FlightTimerThunk() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        call AccumulateFlightTimer
        add esp, 4
        ret
    }
}

__declspec(naked) void EndTimerThunk() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        call AccumulateEndTimer
        add esp, 4
        ret
    }
}

} // namespace

bool InstallStuntJumpCameraFix() {
    PatchSet patches("Stunt jump camera fix");
    if (!patches.Track(InstallCall(g_endTimerPatch, kEndTimerCall,
                                   &EndTimerThunk, kExpectedEndTimerCall),
                       g_endTimerPatch)) {
        Log("Stunt jump camera fix skipped: camera restore timer bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallCall(g_flightTimerPatch, kFlightTimerCall,
                                   &FlightTimerThunk, kExpectedFlightTimerCall),
                       g_flightTimerPatch)) {
        Log("Stunt jump camera fix skipped: in-flight timer bytes do not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed fraction-preserving unique stunt jump timers.");
    return true;
}

} // namespace hff::camera
