#include "player/aiming_walk.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/player.h"

namespace hff::player {

namespace {

SitePatch g_aimingRifleWalkPatch{};

// Replaces the constant walk step used while aiming a rifle.
float __cdecl GetAimingRifleWalkStep() {
    const float ratio = TimeStepRatio();
    const float base = ReadGameFloat(kAimingRifleWalkConstant, 0.07f);
    return ratio > 0.0f ? base / ratio : base;
}

__declspec(naked) void AimingRifleWalkThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetAimingRifleWalkStep
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kAimingRifleWalkReturn
    }
}

} // namespace

bool InstallAimingRifleWalkFix() {
    if (!InstallJump(g_aimingRifleWalkPatch, kAimingRifleWalkPatch,
                     &AimingRifleWalkThunk, kExpectedAimingRifleWalk)) {
        Log("Aiming rifle walk fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent aiming rifle walk speed.");
    return true;
}

} // namespace hff::player
