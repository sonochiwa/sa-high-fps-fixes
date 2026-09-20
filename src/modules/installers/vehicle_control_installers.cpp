#include "modules/modules.h"

#include <cstdio>

// The vehicle control fixes: the yaw chain, steering input, gearbox and
// suspension damping. Sites are in vehicle_control_addresses.h and the
// chain is mapped in docs/vehicle-control-reverse.md.

namespace hff {

bool InstallTurnAirResistanceFix() {
    g_turnAirResistanceStrength = static_cast<float>(std::clamp(
        ReadNumber("vehicles", "turnAirResistanceStrength", 100), 0, 100))
                                / 100.0f;
    if (!InstallJump(g_turnAirResistancePatch, kTurnAirResistance,
                     &TurnAirResistanceThunk, kExpectedTurnAirResistance)) {
        Log("Turn air resistance fix skipped: CPhysical::ApplyAirResistance "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    char installed[160];
    std::snprintf(installed, sizeof(installed),
                  "Installed %.0f%% timestep-scaled turn air resistance for "
                  "cars and bikes with a wheel on the ground.",
                  g_turnAirResistanceStrength * 100.0f);
    Log(installed);
    return true;
}

bool InstallSteerInputRateFix() {
    PatchSet patches("Steer input rate fix");
    struct Site {
        SitePatch* patch;
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
        size_t size;
    };
    const Site sites[] = {
        {&g_steerInputPatches[0], kCarSteerInputA, &CarSteerInputAThunk,
         kExpectedCarSteerInputA.data(), kExpectedCarSteerInputA.size()},
        {&g_steerInputPatches[1], kCarSteerInputB, &CarSteerInputBThunk,
         kExpectedCarSteerInputB.data(), kExpectedCarSteerInputB.size()},
        {&g_steerInputPatches[2], kBikeSteerInputA, &BikeSteerInputAThunk,
         kExpectedBikeSteerInput.data(), kExpectedBikeSteerInput.size()},
        {&g_steerInputPatches[3], kBikeSteerInputB, &BikeSteerInputBThunk,
         kExpectedBikeSteerInput.data(), kExpectedBikeSteerInput.size()},
    };
    for (const Site& site : sites) {
        if (!patches.Track(InstallBranch(*site.patch, site.address, site.thunk,
                                         site.expected, site.size, 0xE9),
                           *site.patch)) {
            Log("Steer input rate fix skipped: ProcessControlInputs bytes do "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed an exponential car and bike steering input step.");
    return true;
}

bool InstallGearChangeInertiaFix() {
    PatchSet patches("Gear change inertia fix");
    if (!patches.Track(InstallJump(g_transmissionPatches[0], kTransmissionInertia,
                                   &TransmissionInertiaThunk,
                                   kExpectedTransmissionInertia),
                       g_transmissionPatches[0])
        || !patches.Track(InstallJump(g_transmissionPatches[1], kTransmissionSmoother,
                                      &TransmissionSmootherThunk,
                                      kExpectedTransmissionSmoother),
                          g_transmissionPatches[1])) {
        Log("Gear change inertia fix skipped: cTransmission::"
            "CalculateDriveAcceleration bytes do not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed engine inertia and gear change smoothing at the original rate.");
    return true;
}

bool InstallGearChangeKickFix() {
    int32_t current = 0;
    __try {
        current = *reinterpret_cast<const int32_t*>(kAcLoopFrameCount);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        current = -1;
    }
    if (current != kStockAcLoopFrameCount) {
        Log("Gear change kick fix skipped: the audio loop frame count is not "
            "the stock value.");
        return false;
    }
    if (!g_scriptsProcessPatch.installed
        && !InstallJump(g_scriptsProcessPatch, kScriptsProcess,
                        &ScriptsProcessThunk, kExpectedScriptsProcess)) {
        Log("Gear change kick fix skipped: CTheScripts::Process bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    g_gearChangeKick = true;
    Log("Installed a real-time wait between engine acceleration loops, so an "
        "upshift kicks the car once.");
    return true;
}

static void LogSkipped(const char* fixName, const char* reason) {
    char line[256];
    std::snprintf(line, sizeof(line), "%s skipped: %s", fixName, reason);
    Log(line);
}

// The damper wrapper serves two fixes; the first of them to install puts
// it in place, after checking the limit it depends on.
static bool EnsureSpringDampeningWrapper(const char* fixName) {
    if (g_suspensionDampingPatch.installed) {
        return true;
    }
    __try {
        if (!NearlyEqual(*reinterpret_cast<const float*>(kDampingLimitInFrame),
                         kStockDampingLimitInFrame)) {
            LogSkipped(fixName, "the damping limit does not match GTA SA 1.0 US.");
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogSkipped(fixName, "the damping limit is unreadable.");
        return false;
    }
    if (!InstallDetour(g_suspensionDampingPatch, kApplySpringDampening,
                       &HookedSpringDampening,
                       kExpectedApplySpringDampening.data(),
                       kExpectedApplySpringDampening.size())) {
        LogSkipped(fixName, "ApplySpringDampening entry does not match GTA SA 1.0 US.");
        return false;
    }
    return true;
}

bool InstallSuspensionDampingLimitFix() {
    if (!EnsureSpringDampeningWrapper("Suspension damping limit fix")) {
        return false;
    }
    g_suspensionDampingLimit = true;
    Log("Installed suspension damping at its 30 FPS strength.");
    return true;
}

bool InstallSuspensionLoadLeanFix() {
    if (!EnsureSpringDampeningWrapper("Suspension load lean fix")) {
        return false;
    }
    if (!InstallDetour(g_physicalProcessControlPatch, kPhysicalProcessControl,
                       &HookedPhysicalProcessControl,
                       kExpectedPhysicalProcessControl.data(),
                       kExpectedPhysicalProcessControl.size())) {
        Log("Suspension load lean fix skipped: CPhysical::ProcessControl entry "
            "does not match GTA SA 1.0 US.");
        return false;
    }
    g_suspensionLoadLean = true;
    Log("Installed the damper's 30 FPS bite on the frame's own suspension "
        "load.");
    return true;
}

bool InstallWheelSlipRateFix() {
    PatchSet patches("Wheel slip rate fix");
    if (!patches.Track(InstallJump(g_wheelSlipPatches[0], kWheelSlipRight,
                                   &WheelSlipRightThunk, kExpectedWheelSlipRight),
                       g_wheelSlipPatches[0])
        || !patches.Track(InstallJump(g_wheelSlipPatches[1], kWheelSlipCoast,
                                      &WheelSlipCoastThunk, kExpectedWheelSlipCoast),
                          g_wheelSlipPatches[1])) {
        Log("Wheel slip rate fix skipped: CVehicle::ProcessWheel bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed car wheel slip weighed per original frame.");
    return true;
}

} // namespace hff
