#include "player/object_pickup.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/player.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace hff::player {

namespace {

std::array<SitePatch, 2> g_pickUpAlignPatches{};

// Returns the offset over the divisor that makes the `* 0.1` after it cover
// the share of the offset one original frame covers, compounded over the
// part of an original frame this one lasts. At 30 FPS and below the divisor
// is the timestep, as in the game.
float __cdecl PickUpAlignStep(float offset) {
    const float ratio = TimeStepRatio();
    if (ratio >= 1.0f) {
        return offset / (ratio * kOriginalTimeStep);
    }
    constexpr float kStockShare = kPickUpAlignFactor / kOriginalTimeStep;
    const float share = 1.0f - std::pow(1.0f - kStockShare, ratio);
    return offset * share / kPickUpAlignFactor;
}

// Stands in for `fdiv dword ptr [CTimer::ms_fTimeStep]` in the pickup
// approach, with the offset in st(0) and nothing else on the x87 stack; the
// quotient goes back there for the `fmul 0.1` that follows. `eax` and `ecx`
// point into the ped's matrix and the flags feed a later branch, so all three
// are kept.
__declspec(naked) void PickUpAlignThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        sub esp, 4
        fstp dword ptr [esp]
        call PickUpAlignStep
        add esp, 4
        pop edx
        pop ecx
        pop eax
        popfd
        ret
    }
}

} // namespace

bool InstallObjectPickUpFix() {
    PatchSet patches("Object pickup fix");
    const std::array<uintptr_t, 2> sites{kPickUpAlignAcross, kPickUpAlignAlong};
    for (size_t i = 0; i < sites.size(); ++i) {
        if (!patches.Track(InstallCall(g_pickUpAlignPatches[i], sites[i],
                                       &PickUpAlignThunk, kExpectedPickUpAlign),
                           g_pickUpAlignPatches[i])) {
            Log("Object pickup fix skipped: CTaskSimpleHoldEntity::ProcessPed "
                "bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a pickup approach at the original speed.");
    return true;
}

} // namespace hff::player
