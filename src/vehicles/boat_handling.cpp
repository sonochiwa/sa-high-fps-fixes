#include "vehicles/boat_handling.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/water.h"
#include "handling/turn_air_resistance.h"

#include <windows.h>

#include <cmath>

namespace hff::vehicles {

namespace {

SitePatch g_planingLiftPatch{};
SitePatch g_pitchSettlePatch{};

// Multiplies the lift on the x87 stack by max(timeStep, 30 FPS timeStep). The
// three components of the move speed sit below it, so the stack holds six at
// most. The flags of the model check made just before are read after the
// return, so they are kept.
__declspec(naked) void BoatPlaningLiftThunk() {
    __asm {
        pushfd
        fld dword ptr ds:[0x00B7CB5C]
        fld g_originalTimeStepValue
        fcomi st, st(1)
        fcmovb st, st(1)
        fstp st(1)
        fmulp st(1), st
        popfd
        jmp kBoatPlaningLiftReturn
    }
}

// turnRes.x times the share of the pitch rate the divisor keeps, taken to the
// power of the frame's share of an original frame.
float __cdecl BoatPitchSettle(float pitchSquare1000, float turnResX) {
    const float keep = 1.0f / (pitchSquare1000 + 1.0f);
    __try {
        const float ratio = TimeStepRatio();
        if (std::isfinite(ratio) && ratio > 0.0f && ratio < 1.0f
            && std::isfinite(keep) && keep > 0.0f) {
            return turnResX * std::pow(keep, ratio);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return turnResX * keep;
}

// Takes `pitch^2 * 1000` off the x87 stack and leaves the factor the
// replaced `fadd` and `fdivr` would have. turnRes.x is the game's local at
// [esp+3Ch], read past the four saves.
__declspec(naked) void BoatPitchSettleThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        push dword ptr [esp + 0x4C]
        sub esp, 4
        fstp dword ptr [esp]
        call BoatPitchSettle
        add esp, 8
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kBoatPitchSettleReturn
    }
}

} // namespace

// A boat under way is held back by three things that act once a frame: the
// planing lift and the pitch settling here, and the `0.99` turn damping of
// `CPhysical::ApplyAirResistance`, which this fix extends to boats. All
// three together give a boat its 30 FPS speed and turn.
bool InstallBoatHandlingFix() {
    PatchSet patches("Boat handling fix");
    if (!patches.Track(InstallJump(g_planingLiftPatch, kBoatPlaningLift,
                                   &BoatPlaningLiftThunk, kExpectedBoatPlaningLift),
                       g_planingLiftPatch)
        || !patches.Track(InstallJump(g_pitchSettlePatch, kBoatPitchSettle,
                                      &BoatPitchSettleThunk, kExpectedBoatPitchSettle),
                          g_pitchSettlePatch)) {
        Log("Boat handling fix skipped: CVehicle::ProcessBoatControl bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    if (!handling::InstallBoatTurnResistance()) {
        return false;
    }
    patches.Commit();
    Log("Installed boat planing lift, pitch settling and turn damping at the "
        "original rate.");
    return true;
}

} // namespace hff::vehicles
