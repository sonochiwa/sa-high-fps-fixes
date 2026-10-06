#include "vehicles/boat_waves.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/timers.h"
#include "game/sites/water.h"

#include <windows.h>

#include <cmath>
#include <cstdint>

namespace hff::vehicles {

namespace {

SitePatch g_boatWavePatch{};

// Above 30 FPS the rise is judged once per original frame, as the rise over a
// whole original frame, and is zero on the frames in between, so a boat slams
// into a wave as hard and as often as at 30 FPS.
int32_t __cdecl BoatWaveRise(double rise) {
    __try {
        const float ratio = TimeStepRatio();
        if (std::isfinite(ratio) && ratio > 0.0f && ratio < 1.0f) {
            if (!FrameTick(kFrameTickBoatWaves)) {
                return 0;
            }
            rise /= ratio;
        }
        if (!std::isfinite(rise)) {
            return 0;
        }
        return static_cast<int32_t>(std::fmax(-1.0e9, std::fmin(rise, 1.0e9)));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// Stands in for `_ftol`, which takes the value in st(0), pops it and returns
// the integer in edx:eax. ecx is kept because `_ftol` keeps it.
__declspec(naked) void BoatWaveRiseThunk() {
    __asm {
        push ecx
        sub esp, 8
        fstp qword ptr [esp]
        call BoatWaveRise
        add esp, 8
        pop ecx
        cdq
        ret
    }
}

} // namespace

bool InstallBoatWavesFix() {
    if (!RepointCall(g_boatWavePatch, kBoatWaveHit, kFtol, &BoatWaveRiseThunk)) {
        Log("Boat wave fix skipped: CVehicle::ProcessBoatControl bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed boat wave slams at the original rate.");
    return true;
}

} // namespace hff::vehicles
