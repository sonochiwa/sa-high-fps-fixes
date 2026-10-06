#include "framerate/radio_lock.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/scripts.h"
#include "game/sites/framerate.h"

#include <windows.h>

namespace hff::framerate {

namespace {

SitePatch g_radioFrameLockPatch{};

// Stands in for the `CAudioEngine::IsBeatInfoPresent` call that makes the
// main loop run the frame limiter whatever the menu says. Music with a beat
// track keeps the limiter only while the dance minigame, the lowrider
// minigame or its beat display runs, the scripts that play along to it.
bool __fastcall BeatTrackHoldsFrameLimit(void* audioEngine, void*) {
    using IsBeatInfoPresentFn = bool(__thiscall*)(void*);
    if (!reinterpret_cast<IsBeatInfoPresentFn>(kAudioEngineIsBeatInfoPresent)(
            audioEngine)) {
        return false;
    }
    __try {
        return FindRunningScript("DANCE") || FindRunningScript("LOWGAME")
            || FindRunningScript("BDISPLY");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
}

} // namespace

bool InstallRadioFrameLockFix() {
    if (!InstallCall(g_radioFrameLockPatch, kFrameLimiterBeatCheck,
                     &BeatTrackHoldsFrameLimit, kExpectedFrameLimiterBeatCheck)) {
        Log("Radio frame lock fix skipped: the main loop bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a frame limiter that music holds only in the dance and "
        "lowrider minigames.");
    return true;
}

} // namespace hff::framerate
