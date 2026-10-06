#include "vehicles/water_cannon.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/vehicles.h"

namespace hff::vehicles {

namespace {

SitePatch g_waterCannonAdvancePatch{};

// Replaces the 150 ms age test in front of the ring advance and adds the
// 30 FPS tick to it. eax, ecx and edx are rebuilt at both continuations and
// edi, the cannon, is callee-saved across `FrameTick`. Every cannon shares one
// decision a frame.
__declspec(naked) void WaterCannonAdvanceThunk() {
    __asm {
        add eax, 0x96
        cmp ecx, eax
        jbe skipped
        push kFrameTickWaterCannon
        call FrameTick
        add esp, 4
        test eax, eax
        je skipped
        jmp kWaterCannonAdvance
    skipped:
        jmp kWaterCannonAdvanceSkip
    }
}

} // namespace

bool InstallWaterCannonFix() {
    if (!InstallJump(g_waterCannonAdvancePatch, kWaterCannonAdvanceGate,
                     &WaterCannonAdvanceThunk, kExpectedWaterCannonAdvance)) {
        Log("Water cannon fix skipped: CWaterCannon::Update_OncePerFrame bytes "
            "do not match GTA SA 1.0 US.");
        return false;
    }
    ResetFrameTicks();
    Log("Installed water cannon jets that reach as far as at 30 FPS.");
    return true;
}

} // namespace hff::vehicles
