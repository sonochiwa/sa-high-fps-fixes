#include "weapons/gun_flash.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/timers.h"
#include "game/sites/weapons.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace hff::weapons {

namespace {

std::array<SitePatch, 4> g_gunFlashFadePatches{};

// One carry for all four sites and every ped: the frame's milliseconds are
// worked out once, with the fraction kept for the next frame, and each call
// in the frame gets the same whole number, so the compare and the subtraction
// agree and every flash loses the time the frame took.
float g_gunFlashCarry{};
uint32_t g_gunFlashFrame{0xFFFFFFFFu};
double g_gunFlashValue{};
int32_t g_gunFlashResult{};

int32_t __cdecl GunFlashMilliseconds(double value) {
    __try {
        if (!std::isfinite(value)) {
            return 0;
        }
        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        if (frame == g_gunFlashFrame && value == g_gunFlashValue) {
            return g_gunFlashResult;
        }
        const double total = value + g_gunFlashCarry;
        const double whole = std::trunc(total);
        const double remainder = total - whole;
        g_gunFlashCarry = std::isfinite(remainder) ? static_cast<float>(remainder) : 0.0f;
        g_gunFlashFrame = frame;
        g_gunFlashValue = value;
        g_gunFlashResult = static_cast<int32_t>(whole);
        return g_gunFlashResult;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int32_t>(value);
    }
}

// Stands in for `_ftol`: takes st(0), pops it and returns the integer in
// edx:eax. ecx is kept as `_ftol` keeps it.
__declspec(naked) void GunFlashFtolThunk() {
    __asm {
        push ecx
        sub esp, 8
        fstp qword ptr [esp]
        call GunFlashMilliseconds
        add esp, 8
        pop ecx
        cdq
        ret
    }
}

} // namespace

bool InstallGunFlashFix() {
    PatchSet patches("Gun flash fix");
    for (size_t i = 0; i < kGunFlashFadeFtol.size(); ++i) {
        if (!patches.Track(RepointCall(g_gunFlashFadePatches[i], kGunFlashFadeFtol[i],
                                       kFtol, &GunFlashFtolThunk),
                           g_gunFlashFadePatches[i])) {
            Log("Gun flash fix skipped: CPed::ProcessControl bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed muzzle flash fading with whole frame milliseconds.");
    return true;
}

} // namespace hff::weapons
