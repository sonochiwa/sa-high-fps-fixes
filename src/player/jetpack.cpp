#include "player/jetpack.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/player.h"

#include <array>
#include <cstdint>

namespace hff::player {

namespace {

std::array<SitePatch, 2> g_jetPackFxPatches{};

// Each replaces one `fadd`/`fsub` against a constant with the same operation
// scaled into the current frame; `fchs` before the add avoids relying on
// `fsubp` operand order, which assemblers disagree about. The value being
// ramped is already on the stack and the original store follows at the return
// address.
__declspec(naked) void JetPackFxRampUpThunk() {
    __asm {
        fld dword ptr ds:[0x00858B1C]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        faddp st(1), st
        ret
    }
}

__declspec(naked) void JetPackFxRampDownThunk() {
    __asm {
        fld dword ptr ds:[0x00858B1C]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        fchs
        faddp st(1), st
        ret
    }
}

} // namespace

bool InstallJetPackFxRampFix() {
    PatchSet patches("Jetpack flame ramp fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kJetPackFxRampUp, &JetPackFxRampUpThunk,
         kExpectedJetPackRampUp.data()},
        {kJetPackFxRampDown, &JetPackFxRampDownThunk,
         kExpectedJetPackRampDown.data()},
    };
    for (size_t i = 0; i < g_jetPackFxPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_jetPackFxPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE8),
                g_jetPackFxPatches[i])) {
            Log("Jetpack flame ramp fix skipped: "
                "CTaskSimpleJetPack::DoJetPackEffect bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a time-based jetpack flame ramp.");
    return true;
}

} // namespace hff::player
