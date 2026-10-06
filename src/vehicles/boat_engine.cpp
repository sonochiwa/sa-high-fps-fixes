#include "vehicles/boat_engine.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/frame_steps.h"
#include "game/sites/vehicles.h"

namespace hff::vehicles {

namespace {

SitePatch g_boatEngineDampingPatch{};

} // namespace

bool InstallBoatEngineSpeedFix() {
    if (!InstallBranch(g_boatEngineDampingPatch, kBoatEngineDamping,
                       &FrameStepDecayThunk, kExpectedFrameStepDecay.data(), 6,
                       0xE8)) {
        Log("Boat engine speed fix skipped: CBoat::ProcessControl bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled boat engine coast down.");
    return true;
}

} // namespace hff::vehicles
