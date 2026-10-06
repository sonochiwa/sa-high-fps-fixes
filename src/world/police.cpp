#include "world/police.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/world.h"

#include <array>

namespace hff::world {

namespace {

std::array<SitePatch, kCopSuspectSlowTests.size()> g_copSlowTestPatches{};

// The test with the 30 FPS timestep, so the suspect counts as slow below the
// same speed at any frame rate.
__declspec(naked) void CopSuspectSlowTestThunk() {
    __asm {
        fmul g_originalTimeStepValue
        ret
    }
}

} // namespace

bool InstallCopCarExitFix() {
    PatchSet patches("Cop car exit fix");
    for (size_t i = 0; i < kCopSuspectSlowTests.size(); ++i) {
        if (!patches.Track(InstallCall(g_copSlowTestPatches[i], kCopSuspectSlowTests[i],
                                       &CopSuspectSlowTestThunk,
                                       kExpectedCopSuspectSlowTest),
                           g_copSlowTestPatches[i])) {
            Log("Cop car exit fix skipped: CTaskComplexCopInCar bytes do not "
                "match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed the cops' slow-suspect test at the 30 FPS timestep.");
    return true;
}

} // namespace hff::world
