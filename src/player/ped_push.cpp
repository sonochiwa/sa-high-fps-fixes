#include "player/ped_push.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/player.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

// A ped/vehicle collision builds vecEntityMoveForce for the vehicle and applies
// it once per rendered frame. Deliver that vehicle impulse at the original
// 30 Hz cadence. The ped-side response must remain untouched: suppressing it
// lets the ped penetrate the body and makes a later vehicle impulse much larger.
// The same rate limit is required for occupied vehicles: leaving them stock
// repeats their collision impulse at the render rate and lets a ped shove them
// much harder than an unoccupied vehicle.

namespace hff::player {

namespace {

SitePatch g_pedPushCarPatch{};

// Keep all contacts in one rendered frame on the same 30 Hz phase so a
// multi-point collision remains coherent.
uint32_t g_pedPushLastFrame{};
float g_pedPushCarry{};
bool g_pedPushOriginalRateFrame{true};

// The share of an original frame this whole frame is. `CPhysical::
// ProcessCollision` steps a fast ped through its collisions with
// `ms_fTimeStep` set to the part of the frame each step has reached, so a
// sprinting ped's push read it as half a frame even at 30 FPS and was let
// through on every other frame only. The unclipped timestep is left alone by
// those steps.
float FrameRatio() {
    const float timeStep = ReadGameFloat(kTimerTimeStepNonClipped, kOriginalTimeStep);
    if (!std::isfinite(timeStep) || timeStep <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(timeStep / kOriginalTimeStep, 0.0f, 1.0f);
}

bool ShouldApplyOriginalRatePedPush() {
    if (InsideOriginalTimeStep()) {
        return true;
    }
    const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
    if (frame != g_pedPushLastFrame) {
        g_pedPushLastFrame = frame;
        const float ratio = FrameRatio();
        g_pedPushCarry += ratio;
        if (g_pedPushCarry >= 1.0f) {
            g_pedPushCarry -= std::floor(g_pedPushCarry);
            g_pedPushOriginalRateFrame = true;
        } else {
            g_pedPushOriginalRateFrame = false;
        }
    }
    return g_pedPushOriginalRateFrame;
}

void __cdecl ScalePedPushCarForce(uintptr_t stackFrame) {
    __try {
        auto* vehicleForce = reinterpret_cast<float*>(stackFrame + 0x20);
        const float scale = ShouldApplyOriginalRatePedPush() ? 1.0f : 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            vehicleForce[i] *= scale;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

__declspec(naked) void PedPushCarThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x24]
        push eax
        call ScalePedPushCarForce
        add esp, 4
        popad
        popfd
        mov edx, dword ptr [esp + 0x20]
        mov eax, dword ptr [esp + 0x24]
        jmp kPedPushCarReturn
    }
}

} // namespace

bool InstallPedPushVehicleFix() {
    g_pedPushLastFrame = 0;
    g_pedPushCarry = 0.0f;
    g_pedPushOriginalRateFrame = true;
    if (!InstallJump(g_pedPushCarPatch, kPedPushCarPatch, &PedPushCarThunk,
                     kExpectedPedPushCar)) {
        Log("Ped push vehicle fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent ped push physics for cars and bikes.");
    return true;
}

} // namespace hff::player
