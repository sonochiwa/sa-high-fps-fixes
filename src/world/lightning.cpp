#include "world/lightning.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/world.h"

#include <array>
#include <cstdint>

// A storm rolls for lightning on the frames on which an original 30 FPS frame
// has passed, and a burst is measured in those frames, so bursts start, last
// and are followed by thunder as at 30 FPS.

namespace hff::world {

namespace {

using RandFn = int32_t(__cdecl*)();

SitePatch g_lightningEndPatch{};
SitePatch g_lightningStartPatch{};
std::array<SitePatch, kLightningFrameReads.size()> g_lightningFramePatches{};

// 0xFF keeps a burst going, and 0xFFFF starts none.
int32_t __cdecl GatedLightningEndRoll() {
    return FrameTick(kFrameTickWeather) ? reinterpret_cast<RandFn>(kRand)() : 0xFF;
}

int32_t __cdecl GatedLightningStartRoll() {
    return FrameTick(kFrameTickWeather) ? reinterpret_cast<RandFn>(kRand)() : 0xFFFF;
}

// One of the two rolls runs every frame of a storm before the burst reads the
// frame count, so the count of original frames is current here.
__declspec(naked) void LightningFrameReadThunk() {
    __asm {
        push ecx
        push edx
        push kFrameTickWeather
        call FrameTickCount
        add esp, 4
        pop edx
        pop ecx
        ret
    }
}

} // namespace

bool InstallLightningFix() {
    PatchSet patches("Lightning fix");
    bool installed =
        patches.Track(RepointCall(g_lightningEndPatch, kLightningEndRoll, kRand,
                                  &GatedLightningEndRoll),
                      g_lightningEndPatch)
        && patches.Track(RepointCall(g_lightningStartPatch, kLightningStartRoll, kRand,
                                     &GatedLightningStartRoll),
                         g_lightningStartPatch);
    for (size_t i = 0; installed && i < kLightningFrameReads.size(); ++i) {
        installed = patches.Track(InstallCall(g_lightningFramePatches[i],
                                              kLightningFrameReads[i],
                                              &LightningFrameReadThunk,
                                              kExpectedLightningFrameRead),
                                  g_lightningFramePatches[i]);
    }
    if (!installed) {
        Log("Lightning fix skipped: CWeather::Update bytes do not match GTA SA 1.0 "
            "US.");
        return false;
    }
    patches.Commit();
    Log("Installed lightning bursts at the original rate.");
    return true;
}

} // namespace hff::world
