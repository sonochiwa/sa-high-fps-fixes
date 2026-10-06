#include "audio/ambience.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/audio.h"

#include <array>

namespace hff::audio {

namespace {

std::array<SitePatch, kAmbientSoundCycles.size()> g_cyclePatches{};

// Leaves the zero flag the test would have: set once every 1024 original
// 30 FPS frames, on the frame that completes them, and clear otherwise.
__declspec(naked) void AmbientSoundCycleThunk() {
    __asm {
        push eax
        push ecx
        push edx
        push kFrameTickAmbience
        call FrameTick
        add esp, 4
        test eax, eax
        je not_due
        push kFrameTickAmbience
        call FrameTickCount
        add esp, 4
        test eax, 0x3FF
        jmp done
    not_due:
        or eax, 1
    done:
        pop edx
        pop ecx
        pop eax
        ret
    }
}

} // namespace

bool InstallAmbientSoundsFix() {
    PatchSet patches("Ambient sounds fix");
    for (size_t i = 0; i < kAmbientSoundCycles.size(); ++i) {
        if (!patches.Track(InstallCall(g_cyclePatches[i], kAmbientSoundCycles[i],
                                       &AmbientSoundCycleThunk,
                                       kExpectedAmbientSoundCycle),
                           g_cyclePatches[i])) {
            Log("Ambient sounds fix skipped: "
                "CAEGlobalWeaponAudioEntity::ServiceAmbientGunFire bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed distant gunfire and the foghorn at the original rate.");
    return true;
}

} // namespace hff::audio
