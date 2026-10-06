#include "vehicles/sinking.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/water.h"

#include <array>

namespace hff::vehicles {

namespace {

std::array<SitePatch, kSinkSteps.size()> g_sinkStepPatches{};

// The sink step scaled into the current frame, an identity at 30 FPS, so a
// car driven into deep water stalls and settles after the same time.
__declspec(naked) void SinkStepThunk() {
    __asm {
        fmul dword ptr ds:[0x00871244]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        ret
    }
}

} // namespace

bool InstallVehicleSinkingFix() {
    PatchSet patches("Vehicle sinking fix");
    for (size_t i = 0; i < kSinkSteps.size(); ++i) {
        if (!patches.Track(InstallCall(g_sinkStepPatches[i], kSinkSteps[i],
                                       &SinkStepThunk, kExpectedSinkStep),
                           g_sinkStepPatches[i])) {
            Log("Vehicle sinking fix skipped: buoyancy loss bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed timestep-scaled sinking for cars and burnt-out boats.");
    return true;
}

} // namespace hff::vehicles
