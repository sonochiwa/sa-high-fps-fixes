#include "vehicles/head_bop.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/vehicles.h"

#include <array>
#include <cstdint>

namespace hff::vehicles {

namespace {

std::array<SitePatch, 2> g_headBopPatches{};

// Each replaces one `fadd`/`fsub` of 0.05 with the same step scaled into the
// current frame; `fchs` before the add avoids relying on `fsubp` operand
// order. The value being ramped is already on the stack and the original store
// follows at the return address.
__declspec(naked) void HeadBopRampUpThunk() {
    __asm {
        fld dword ptr ds:[0x00858C28]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        faddp st(1), st
        ret
    }
}

__declspec(naked) void HeadBopRampDownThunk() {
    __asm {
        fld dword ptr ds:[0x00858C28]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        fchs
        faddp st(1), st
        ret
    }
}

} // namespace

bool InstallHeadBoppingFix() {
    PatchSet patches("Head bopping fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kHeadBopRampUp, &HeadBopRampUpThunk, kExpectedHeadBopRampUp.data()},
        {kHeadBopRampDown, &HeadBopRampDownThunk,
         kExpectedHeadBopRampDown.data()},
    };
    for (size_t i = 0; i < g_headBopPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_headBopPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE8),
                g_headBopPatches[i])) {
            Log("Head bopping fix skipped: "
                "CTaskSimpleCarDrive::ProcessHeadBopping bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a time-based driver head bop ramp.");
    return true;
}

} // namespace hff::vehicles
