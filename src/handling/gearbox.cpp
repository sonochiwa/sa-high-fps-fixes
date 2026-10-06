#include "handling/gearbox.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/frame_hook.h"
#include "game/sites/handling.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace hff::handling {

namespace {

std::array<SitePatch, 2> g_transmissionPatches{};

// The engine inertia term is a difference between two frames, so it is
// scaled to the difference one original frame would have seen.
float __cdecl GetTransmissionInertiaScale() {
    const float ratio = TimeStepRatio();
    return ratio > 0.0f ? 1.0f / ratio : 1.0f;
}

// The smoother keeps 0.85 of its value per frame; per rendered frame that
// is 0.85 raised to the timestep ratio.
float __cdecl GetTransmissionSmootherFrac() {
    const float frac = ReadGameFloat(kTransmissionSmootherConstant, 0.85f);
    if (!(frac > 0.0f) || frac >= 1.0f) {
        return frac;
    }
    return std::pow(frac, TimeStepRatio());
}

// Replaces `fld st(0) / fsub [ebp] / jne`. On entry st(0) is the gear band
// ratio and the flags are those of the `cmp bl,1` just before the site, so
// they are kept across the call and the original branch is re-created.
__declspec(naked) void TransmissionInertiaThunk() {
    __asm {
        fld st(0)
        fsub dword ptr [ebp]
        pushfd
        push eax
        push ecx
        push edx
        call GetTransmissionInertiaScale
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jne cheat
        jmp kTransmissionInertiaReturn
    cheat:
        jmp kTransmissionInertiaCheatSkip
    }
}

// Replaces the smoother, `s = a * (1 - 0.85) + 0.85 * s`, with the same
// blend at the frame-rate-corrected fraction. On entry st(0) is `a` and
// `[esp+28h]` the pointer to `s`; on exit st(0) is the new `s` and the rest
// of the stack is untouched, as the original left it for the code after.
__declspec(naked) void TransmissionSmootherThunk() {
    __asm {
        mov eax, dword ptr [esp + 0x28]
        pushfd
        push eax
        push ecx
        push edx
        call GetTransmissionSmootherFrac
        pop edx
        pop ecx
        pop eax
        popfd
        fld st(0)
        fmul dword ptr [eax]
        fxch st(1)
        fchs
        fld1
        faddp st(1), st
        fmulp st(2), st
        faddp st(1), st
        jmp kTransmissionSmootherReturn
    }
}

// The audio engine's wait between two acceleration loops is a frame count;
// this keeps it the same third of a second at any frame rate. Rounded to
// the nearest frame and never below one, so 30 FPS reads back the stock
// ten exactly.
void UpdateAcLoopFrameCount() {
    const float ratio = TimeStepRatio();
    if (!(ratio > 0.0f)) {
        return;
    }
    const int32_t frames = std::max(
        1L, std::lround(static_cast<float>(kStockAcLoopFrameCount) / ratio));
    __try {
        auto* value = reinterpret_cast<int32_t*>(kAcLoopFrameCount);
        if (*value != frames) {
            *value = frames;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

} // namespace

bool InstallGearChangeInertiaFix() {
    PatchSet patches("Gear change inertia fix");
    if (!patches.Track(InstallJump(g_transmissionPatches[0], kTransmissionInertia,
                                   &TransmissionInertiaThunk,
                                   kExpectedTransmissionInertia),
                       g_transmissionPatches[0])
        || !patches.Track(InstallJump(g_transmissionPatches[1], kTransmissionSmoother,
                                      &TransmissionSmootherThunk,
                                      kExpectedTransmissionSmoother),
                          g_transmissionPatches[1])) {
        Log("Gear change inertia fix skipped: cTransmission::"
            "CalculateDriveAcceleration bytes do not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed engine inertia and gear change smoothing at the original rate.");
    return true;
}

bool InstallGearChangeKickFix() {
    int32_t current = 0;
    __try {
        current = *reinterpret_cast<const int32_t*>(kAcLoopFrameCount);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        current = -1;
    }
    if (current != kStockAcLoopFrameCount) {
        Log("Gear change kick fix skipped: the audio loop frame count is not "
            "the stock value.");
        return false;
    }
    if (!InstallFrameHook("Gear change kick fix")) {
        return false;
    }
    AddFrameCallback(&UpdateAcLoopFrameCount);
    Log("Installed a real-time wait between engine acceleration loops, so an "
        "upshift kicks the car once.");
    return true;
}

} // namespace hff::handling
