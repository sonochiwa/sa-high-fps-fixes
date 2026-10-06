#include "audio/engine_revs.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/audio.h"

namespace hff::audio {

namespace {

SitePatch g_stepUpPatch{};
SitePatch g_stepDownPatch{};

// The steps scaled into the current frame, an identity at 30 FPS.
__declspec(naked) void GasRevStepUpThunk() {
    __asm {
        fld dword ptr ds:[0x008CBC24]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        ret
    }
}

__declspec(naked) void GasRevStepDownThunk() {
    __asm {
        fld dword ptr ds:[0x008CBC28]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fsubp st(1), st
        ret
    }
}

} // namespace

bool InstallEngineRevsFix() {
    PatchSet patches("Engine revs fix");
    if (!patches.Track(InstallCall(g_stepUpPatch, kGasRevStepUp, &GasRevStepUpThunk,
                                   kExpectedGasRevStepUp),
                       g_stepUpPatch)
        || !patches.Track(InstallCall(g_stepDownPatch, kGasRevStepDown,
                                      &GasRevStepDownThunk, kExpectedGasRevStepDown),
                          g_stepDownPatch)) {
        Log("Engine revs fix skipped: CAEVehicleAudioEntity::UpdateGasPedalAudio "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed the engine note following the throttle at the original rate.");
    return true;
}

} // namespace hff::audio
