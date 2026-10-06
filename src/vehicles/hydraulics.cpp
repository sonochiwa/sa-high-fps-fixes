#include "vehicles/hydraulics.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/vehicles.h"

namespace hff::vehicles {

namespace {

SitePatch g_hydraulicRaisePatch{};
SitePatch g_hydraulicLowerPatch{};

// Both steps of the stance happen only on the frames on which an original
// 30 FPS frame has passed. `FrameTick` may change eax, ecx and edx, and the
// raise needs ecx, so they are kept; the flags it leaves are the ones the
// lowering's `test ax,ax` would have set.
__declspec(naked) void HydraulicRaiseThunk() {
    __asm {
        push eax
        push ecx
        push edx
        push kFrameTickHydraulics
        call FrameTick
        add esp, 4
        test eax, eax
        pop edx
        pop ecx
        pop eax
        je held
        inc ecx
        mov word ptr [ebp+0x86C], cx
    held:
        ret
    }
}

__declspec(naked) void HydraulicLowerThunk() {
    __asm {
        push ecx
        push edx
        push kFrameTickHydraulics
        call FrameTick
        add esp, 4
        pop edx
        pop ecx
        test eax, eax
        lea eax, [ecx-1]
        jne lowered
        mov eax, ecx
    lowered:
        test ax, ax
        ret
    }
}

} // namespace

bool InstallHydraulicStanceFix() {
    PatchSet patches("Hydraulic stance fix");
    if (!patches.Track(InstallCall(g_hydraulicRaisePatch, kHydraulicRaise,
                                   &HydraulicRaiseThunk, kExpectedHydraulicRaise),
                       g_hydraulicRaisePatch)
        || !patches.Track(InstallCall(g_hydraulicLowerPatch, kHydraulicLower,
                                      &HydraulicLowerThunk, kExpectedHydraulicLower),
                          g_hydraulicLowerPatch)) {
        Log("Hydraulic stance fix skipped: CAutomobile::HydraulicControl bytes do "
            "not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed the hydraulic stance at the original rate.");
    return true;
}

} // namespace hff::vehicles
