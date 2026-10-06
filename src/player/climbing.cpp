#include "player/climbing.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "game/sites/player.h"

#include <windows.h>

#include <cmath>

namespace hff::player {

namespace {

SitePatch g_climbSpeedPatch{};

// Clamping rather than rescaling is what reproduces the original. The ped is
// pulled onto the hold by however much it can cover in one frame, so scaling
// the speed would change where it lands; limiting it makes the approach take
// several short frames instead of one long one, which is the same wall time.
void __cdecl ClampClimbMoveSpeed(float* speed) {
    __try {
        if (!speed) {
            return;
        }
        const float limit = ReadGameFloat(kClimbSpeedLimit, 0.2f);
        const float length = std::sqrt(speed[0] * speed[0]
                                       + speed[1] * speed[1]
                                       + speed[2] * speed[2]);
        if (!std::isfinite(length) || length <= limit || length <= 0.0f) {
            return;
        }
        const float scale = limit / length;
        speed[0] *= scale;
        speed[1] *= scale;
        speed[2] *= scale;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

// The clamp goes in where the sibling branch does its own, between the divide
// and the addition of the climbed entity's speed. `edi` holds the ped and `ecx`
// the address `CVector::operator+=` is about to be called on, both preserved by
// `pushad`, and the replaced instructions are reproduced around it.
__declspec(naked) void ClimbSpeedClampThunk() {
    __asm {
        add esp, 0x0C
        pushfd
        pushad
        lea eax, [edi + 0x44]
        push eax
        call ClampClimbMoveSpeed
        add esp, 4
        popad
        popfd
        lea edx, [esp + 0x48]
        push edx
        call kVectorAddAssign
        jmp kClimbSpeedClampReturn
    }
}

} // namespace

bool InstallClimbSpeedFix() {
    if (!InstallJump(g_climbSpeedPatch, kClimbSpeedClamp, &ClimbSpeedClampThunk,
                     kExpectedClimbSpeedClamp)) {
        Log("Climb speed fix skipped: CTaskSimpleClimb bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a clamped climb move speed.");
    return true;
}

} // namespace hff::player
