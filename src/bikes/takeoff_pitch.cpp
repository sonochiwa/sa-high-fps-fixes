#include "bikes/takeoff_pitch.h"

#include "bikes/bmx_jump.h"
#include "bikes/process_control.h"
#include "core/config.h"
#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/bikes.h"
#include "game/vehicles.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

// The wheel-contact call at 0x6D7B17 is the first known source of the excess
// backward angular speed seen at takeoff. Preserve the complete angular-velocity
// delta produced by the stock ApplyTurnForce except for the excess positive
// pitch produced during takeoff: both front suspension lines are clear, the
// bike has meaningful positive world-Z velocity, and a rear or lingering wheel
// contact still reaches ProcessBikeWheel. This catches the phase where the rear
// wheel continues to pitch the bike after the front has left a ramp without
// changing a wheelie on level ground. Negative (nose-down) pitch and 30 FPS or
// below are exact no-ops.

namespace hff::bikes {

namespace {

SitePatch g_bikePitchExperimentPatch{};
uintptr_t g_bikePitchExperimentBike{};
float g_bikePitchExperimentBefore[3]{};
float g_bikePitchExperimentAxis[3]{};
float g_bikePitchExperimentStrength{1.0f};
float g_bikePitchExperimentFrameCorrection{};
bool g_bikePitchExperimentActive{};

void __cdecl BeginBikePitchExperiment(uintptr_t bike) {
    g_bikePitchExperimentActive = false;
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (!bike || !std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            return;
        }

        // The correction exists for the rider-generated takeoff pitch. Once
        // RemoveDriver changes the packed entity status to STATUS_ABANDONED,
        // an ordinary ground bounce can otherwise satisfy the same wheel-line
        // and vertical-speed conditions and have its stock angular response
        // suppressed. Leave every riderless bike and every bike another player
        // rides untouched.
        const uint8_t status = *reinterpret_cast<const uint8_t*>(
            bike + kEntityTypeAndStatus) >> 3;
        if (status != 0 || !IsDrivenByLocalPlayer(bike)) {
            return;
        }

        const auto* wheelRatios = reinterpret_cast<const float*>(
            bike + kBikeWheelRatios);
        const auto* contactTimers = reinterpret_cast<const float*>(
            bike + kBikeWheelContactTimers);
        bool hasLingeringWheelContact = false;
        for (size_t i = 0; i < 4; ++i) {
            if (!std::isfinite(wheelRatios[i])
                || !std::isfinite(contactTimers[i])) {
                return;
            }
            hasLingeringWheelContact |= contactTimers[i] > 0.0f;
        }
        const bool hasFrontWheelContact = wheelRatios[0] < 1.0f
                                       || wheelRatios[1] < 1.0f;
        const float verticalSpeed = *reinterpret_cast<const float*>(
            bike + kPhysicalMoveSpeed + 2 * sizeof(float));
        if (!std::isfinite(verticalSpeed) || hasFrontWheelContact
            || !hasLingeringWheelContact || verticalSpeed <= 0.02f) {
            return;
        }

        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            bike + kEntityMatrix);
        if (!matrix) {
            return;
        }

        const auto* turn = reinterpret_cast<const float*>(
            bike + kPhysicalTurnSpeed);
        const auto* right = reinterpret_cast<const float*>(
            matrix + kMatrixRight);
        float axisLengthSquared = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            if (!std::isfinite(turn[i]) || !std::isfinite(right[i])) {
                return;
            }
            g_bikePitchExperimentBefore[i] = turn[i];
            g_bikePitchExperimentAxis[i] = right[i];
            axisLengthSquared += right[i] * right[i];
        }
        if (!std::isfinite(axisLengthSquared) || axisLengthSquared < 0.25f) {
            return;
        }

        g_bikePitchExperimentBike = bike;
        g_bikePitchExperimentFrameCorrection = std::clamp(
            1.0f - timeStep / kOriginalTimeStep, 0.0f, 1.0f);
        g_bikePitchExperimentActive = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bikePitchExperimentActive = false;
    }
}

void __cdecl FinishBikePitchExperiment() {
    if (!g_bikePitchExperimentActive) {
        return;
    }
    g_bikePitchExperimentActive = false;

    __try {
        auto* turn = reinterpret_cast<float*>(
            g_bikePitchExperimentBike + kPhysicalTurnSpeed);
        float pitchDelta = 0.0f;
        float axisLengthSquared = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            const float delta = turn[i] - g_bikePitchExperimentBefore[i];
            pitchDelta += delta * g_bikePitchExperimentAxis[i];
            axisLengthSquared += g_bikePitchExperimentAxis[i]
                               * g_bikePitchExperimentAxis[i];
        }
        if (!std::isfinite(pitchDelta) || pitchDelta <= 0.0f
            || !std::isfinite(axisLengthSquared)
            || axisLengthSquared < 0.25f) {
            return;
        }

        const float projection = (pitchDelta / axisLengthSquared)
                               * g_bikePitchExperimentStrength
                               * g_bikePitchExperimentFrameCorrection;
        for (size_t i = 0; i < 3; ++i) {
            turn[i] -= g_bikePitchExperimentAxis[i] * projection;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Entered through the original five-byte call. ApplyTurnForce ends in `ret 18h`,
// so an ordinary nested call would put this wrapper's return address where the
// callee expects its first float and would then corrupt the caller's stack. Make
// a private copy of all six arguments for the nested call and reproduce the
// original `ret 18h` when the wrapper itself returns.
__declspec(naked) void BikePitchExperimentThunk() {
    __asm {
        pushfd
        pushad
        push ecx
        call BeginBikePitchExperiment
        add esp, 4
        popad
        popfd
        mov eax, esp
        push dword ptr [eax + 0x18]
        push dword ptr [eax + 0x14]
        push dword ptr [eax + 0x10]
        push dword ptr [eax + 0x0C]
        push dword ptr [eax + 0x08]
        push dword ptr [eax + 0x04]
        call kApplyTurnForce
        pushfd
        pushad
        call FinishBikePitchExperiment
        popad
        popfd
        ret 0x18
    }
}

} // namespace

bool InstallBikePitchExperiment() {
    PatchSet patches("Bike pitch experiment");
    g_bikePitchExperimentStrength = static_cast<float>(std::clamp(
        ReadNumber("vehicles", "bikePitchExperimentStrength", 100), 0, 100))
                                  / 100.0f;
    if (!patches.Track(
            RepointCall(g_bikePitchExperimentPatch, kBikeWheelTurnForceCall,
                        kApplyTurnForce, &BikePitchExperimentThunk),
            g_bikePitchExperimentPatch)) {
        Log("Bike pitch experiment skipped: wheel-contact ApplyTurnForce call "
            "does not match GTA SA 1.0 US.");
        return false;
    }
    const bool bikeHookWasInstalled = BikeProcessControlPatch().installed;
    if (!EnsureBikeProcessControlHook()
        || (!bikeHookWasInstalled
            && !patches.Track(true, BikeProcessControlPatch()))) {
        Log("Bike pitch experiment skipped: CBike::ProcessControl entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallBmxJumpHooks(patches)) {
        return false;
    }
    char installed[128];
    std::snprintf(installed, sizeof(installed),
                  "Installed experimental %.0f%% correction of positive bike "
                  "pitch while climbing off a ramp above 30 FPS.",
                  g_bikePitchExperimentStrength * 100.0f);
    patches.Commit();
    Log(installed);
    return true;
}

} // namespace hff::bikes
