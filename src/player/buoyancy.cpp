#include "player/buoyancy.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/player.h"

namespace hff::player {

namespace {

SitePatch g_buoyancyThresholdPatch{};
SitePatch g_buoyancyClampedStorePatch{};

// The impulse is built with the original timestep so the comparison against
// `mass * moveSpeed.z` keeps its 30 FPS meaning, and the copy written into the
// output vector is scaled back to the current frame. `esi` addresses the
// buoyancy state, `eax` the entity and `ecx` the output vector; none are
// touched here.
__declspec(naked) void BuoyancyThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xBC]
        fmul dword ptr [esi + 0x6C]
        add esp, 0x0C
        fmul g_originalTimeStepValue
        fld st(0)
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fstp dword ptr [ecx + 8]
        fld dword ptr [eax + 0x8C]
        fmul dword ptr [eax + 0x4C]
        fld st(1)
        fmul dword ptr ds:[0x00858B90]
        jmp kBuoyancyThresholdReturn
    }
}

// The reduced impulse taken when the entity is already rising fast. It is
// computed from the original timestep value above, so it is scaled here too.
// The replaced span runs to the end of the function, so this returns directly.
__declspec(naked) void BuoyancyClampedStoreThunk() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fstp dword ptr [ecx + 8]
        mov al, 1
        pop esi
        add esp, 0x0C
        ret 0x0C
    }
}

} // namespace

bool InstallWaterBuoyancyFix() {
    PatchSet patches("Water buoyancy fix");
    if (!MemoryMatches(kBuoyancyThreshold, kExpectedBuoyancyThreshold)
        || !MemoryMatches(kBuoyancyClampedStore,
                          kExpectedBuoyancyClampedStore)) {
        Log("Water buoyancy fix skipped: cBuoyancy bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallJump(g_buoyancyThresholdPatch,
                                   kBuoyancyThreshold,
                                   &BuoyancyThresholdThunk,
                                   kExpectedBuoyancyThreshold),
                       g_buoyancyThresholdPatch)) {
        Log("Water buoyancy fix failed while installing the threshold hook.");
        return false;
    }
    if (!patches.Track(InstallJump(g_buoyancyClampedStorePatch,
                                   kBuoyancyClampedStore,
                                   &BuoyancyClampedStoreThunk,
                                   kExpectedBuoyancyClampedStore),
                       g_buoyancyClampedStorePatch)) {
        Log("Water buoyancy fix failed while installing the clamped store hook.");
        return false;
    }
    patches.Commit();
    Log("Installed a timestep-normalized buoyancy cutoff.");
    return true;
}

} // namespace hff::player
