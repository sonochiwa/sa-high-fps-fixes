#include "player/drunk_steering.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/player.h"

namespace hff::player {

namespace {

SitePatch g_drunkSteerPatch{};

// Replaces the shift loop's two set-up instructions. `ebx` is the pad and is
// callee-saved across the helper; eax, ecx and edx are dead here, and the two
// the loop needs are rebuilt before jumping into it.
__declspec(naked) void DrunkSteerShiftThunk() {
    __asm {
        push kFrameTickDrunkSteer
        call FrameTick
        add esp, 4
        test eax, eax
        je skipped
        lea eax, [ebx + 0x72]
        mov ecx, 9
        jmp kDrunkSteerShiftResume
    skipped:
        jmp kDrunkSteerShiftSkip
    }
}

} // namespace

bool InstallDrunkSteerDelayFix() {
    if (!InstallBranch(g_drunkSteerPatch, kDrunkSteerShift,
                       &DrunkSteerShiftThunk, kExpectedDrunkSteerShift.data(),
                       8, 0xE9)) {
        Log("Drunk steering delay fix skipped: CPad::Update bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    ResetFrameTicks();
    Log("Installed a time-based drunk driving steering delay.");
    return true;
}

} // namespace hff::player
