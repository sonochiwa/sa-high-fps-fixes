#include "vehicles/jump_out.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/vehicles.h"

#include <array>
#include <cstdint>

namespace hff::vehicles {

namespace {

std::array<SitePatch, 6> g_jumpOutDampPatches{};

// The same shape as the shared decay, against the 0.9 constant that
// `CVehicle::CanPedJumpOutCar` uses for both speeds. On the move speed branch
// AL already holds the function's `false` result, so the registers `_CIpow`
// writes are kept across it.
__declspec(naked) void JumpOutDampThunk() {
    __asm {
        fld dword ptr ds:[0x00858C20]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        pushfd
        push eax
        push ecx
        push edx
        call kPow
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        ret
    }
}

} // namespace

bool InstallJumpOutCarSpeedFix() {
    PatchSet patches("Jump out car speed fix");
    const uintptr_t sites[] = {kJumpOutTurnDampX, kJumpOutTurnDampY,
                               kJumpOutTurnDampZ, kJumpOutMoveDampX,
                               kJumpOutMoveDampY, kJumpOutMoveDampZ};
    for (size_t i = 0; i < g_jumpOutDampPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_jumpOutDampPatches[i], sites[i],
                              &JumpOutDampThunk, kExpectedJumpOutDamp.data(), 6,
                              0xE8),
                g_jumpOutDampPatches[i])) {
            Log("Jump out car speed fix skipped: CVehicle::CanPedJumpOutCar "
                "bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled jump out car damping.");
    return true;
}

} // namespace hff::vehicles
