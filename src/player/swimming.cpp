#include "player/swimming.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/frame_steps.h"
#include "game/sites/player.h"

#include <windows.h>

#include <array>
#include <cmath>

namespace hff::player {

namespace {

SitePatch g_swimmingPatch{};
std::array<SitePatch, 3> g_swimPitchPatches{};

// The animation shift is a displacement for one frame, so turning it into a
// speed in original timestep units means dividing by the ratio. The original
// values are put back the moment the swim task is done with them: the same
// field drives walking and running, and only this one call may see it changed.
float g_swimShiftSaved[2]{};
bool g_swimShiftScaled{};

bool SwimmingMovementCodeIsUnmodified() {
    return MemoryMatches(kSwimDiveScale, kExpectedSwimDiveScale)
        && MemoryMatches(kSwimAscentBias, kExpectedSwimAscentBias)
        && MemoryMatches(kSwimVectorSetup, kExpectedSwimVectorSetup)
        && MemoryMatches(kSwimVectorTransform,
                         kExpectedSwimVectorTransform);
}

void __cdecl ScaleSwimAnimShift(uintptr_t ped) {
    g_swimShiftScaled = false;
    __try {
        // Recheck the shared function at run time as well as installation.
        // Some ASI loaders install scripts-directory plugins after this one;
        // if Tweaker or Swim FPS Fix appears later, this wrapper remains in
        // place but becomes an identity instead of double-scaling the shift.
        if (!ped || !SwimmingMovementCodeIsUnmodified()) {
            return;
        }
        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            return;
        }
        auto* shift = reinterpret_cast<float*>(ped + kPedAnimMovingShift);
        g_swimShiftSaved[0] = shift[0];
        g_swimShiftSaved[1] = shift[1];
        const float scale = kOriginalTimeStep / timeStep;
        shift[0] *= scale;
        shift[1] *= scale;
        g_swimShiftScaled = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_swimShiftScaled = false;
    }
}

void __cdecl RestoreSwimAnimShift(uintptr_t ped) {
    __try {
        if (g_swimShiftScaled && ped) {
            auto* shift = reinterpret_cast<float*>(ped + kPedAnimMovingShift);
            shift[0] = g_swimShiftSaved[0];
            shift[1] = g_swimShiftSaved[1];
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    g_swimShiftScaled = false;
}

// Wraps the single call to `CTaskSimpleSwim::ProcessSwimmingResistance`. The
// two replaced instructions set up its arguments, so they are reproduced
// between the scale and the restore. `esi` is the ped and `edi` the task, both
// preserved by `pushad`.
__declspec(naked) void SwimResistanceThunk() {
    __asm {
        pushfd
        pushad
        push esi
        call ScaleSwimAnimShift
        add esp, 4
        popad
        popfd

        push esi
        mov ecx, edi
        call kProcessSwimmingResistance

        pushfd
        pushad
        push esi
        call RestoreSwimAnimShift
        add esp, 4
        popad
        popfd
        jmp kSwimResistanceReturn
    }
}

} // namespace

bool InstallSwimmingMovementFix() {
    if (!SwimmingMovementCodeIsUnmodified()) {
        Log("Swimming movement fix skipped: another swimming FPS fix has "
            "modified CTaskSimpleSwim::ProcessSwimmingResistance.");
        return false;
    }
    if (!InstallJump(g_swimmingPatch, kSwimResistanceCall,
                     &SwimResistanceThunk, kExpectedSwimResistanceCall)) {
        Log("Swimming movement fix skipped: CTaskSimpleSwim bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent swimming speed.");
    return true;
}

bool InstallSwimPitchRateFix() {
    PatchSet patches("Swim pitch rate fix");
    const uintptr_t sites[] = {kSwimPitchDecayA, kSwimPitchDecayB,
                               kSwimPitchDecayC};
    for (size_t i = 0; i < g_swimPitchPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_swimPitchPatches[i], sites[i],
                              &FrameStepDecayThunk,
                              kExpectedFrameStepDecay.data(), 6, 0xE8),
                g_swimPitchPatches[i])) {
            Log("Swim pitch rate fix skipped: "
                "CTaskSimpleSwim::ProcessSwimmingResistance bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled swim pitch rate.");
    return true;
}

} // namespace hff::player
