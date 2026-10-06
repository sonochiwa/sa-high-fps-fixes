#include "handling/wheel_slip.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/handling.h"

#include <array>

namespace hff::handling {

namespace {

std::array<SitePatch, 2> g_wheelSlipPatches{};

// Replaces `fst [esp+18h] / jne`: st(0) is the lateral slip velocity and the
// flags are those of the `cmp` that decides driving against coasting. The
// slip is scaled to one original frame's worth, stored where the original
// stored it, and the branch re-created.
__declspec(naked) void WheelSlipRightThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call TimeStepRatio
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        fst dword ptr [esp + 0x18]
        jne coasting
        jmp kWheelSlipRightDriving
    coasting:
        jmp kWheelSlipRightCoasting
    }
}

// Replaces `fchs / fstp [esp+10h]`: st(0) is the coasting longitudinal slip
// before its sign flip; the `jne` after the site reads the flags of a
// `test` before it, so they are preserved across the call.
__declspec(naked) void WheelSlipCoastThunk() {
    __asm {
        fchs
        pushfd
        push eax
        push ecx
        push edx
        call TimeStepRatio
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        fstp dword ptr [esp + 0x10]
        jmp kWheelSlipCoastReturn
    }
}

} // namespace

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

} // namespace hff::handling
