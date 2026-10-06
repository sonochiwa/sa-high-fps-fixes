#include "weapons/tear_gas.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/weapons.h"

#include <cstdint>

namespace hff::weapons {

namespace {

using RandomInRangeFn = int32_t(__cdecl*)(int32_t, int32_t);

SitePatch g_tearGasPatch{};

// Rolled on the frames on which an original 30 FPS frame has passed, so the
// gas chokes as often per second as at 30 FPS; the top of the range keeps the
// roll above the 10 that chokes.
int32_t __cdecl GatedTearGasRoll(int32_t low, int32_t high) {
    if (!FrameTick(kFrameTickTearGas)) {
        return high;
    }
    return reinterpret_cast<RandomInRangeFn>(kRandomNumberInRange)(low, high);
}

} // namespace

bool InstallTearGasFix() {
    if (!RepointCall(g_tearGasPatch, kTearGasChokeRoll, kRandomNumberInRange,
                     &GatedTearGasRoll)) {
        Log("Tear gas fix skipped: CProjectileInfo::Update bytes do not match GTA "
            "SA 1.0 US.");
        return false;
    }
    Log("Installed tear gas choking at the original rate.");
    return true;
}

} // namespace hff::weapons
