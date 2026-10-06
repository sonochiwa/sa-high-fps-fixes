#include "vehicles/moving_parts.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/vehicles.h"

#include <array>
#include <cstdint>

namespace hff::vehicles {

namespace {

SitePatch g_movingPartStepPatch{};

// The carry of each vehicle whose part is moving. A vehicle not seen for
// longest gives up its slot to a new one.
struct MovingPartCarry {
    uintptr_t vehicle;
    uint32_t lastFrame;
    float carry;
};
std::array<MovingPartCarry, 8> g_movingPartCarries{};

float& MovingPartCarryFor(uintptr_t vehicle) {
    const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
    MovingPartCarry* oldest = &g_movingPartCarries.front();
    for (auto& slot : g_movingPartCarries) {
        if (slot.vehicle == vehicle) {
            slot.lastFrame = frame;
            return slot.carry;
        }
        if (frame - slot.lastFrame > frame - oldest->lastFrame) {
            oldest = &slot;
        }
    }
    *oldest = {vehicle, frame, 0.0f};
    return oldest->carry;
}

// Moves the part by the whole units one original frame moves it, spread over
// the frames that make up that time with the fraction carried per vehicle,
// then clamps as the game does: below zero to zero, above the limit to the
// limit. At 30 FPS and below the step is the game's own.
void __cdecl StepMovingPartAngle(uintptr_t vehicle, void* pad, float rate) {
    using GetCarGunUpDownFn = int16_t(__thiscall*)(void*);
    const int16_t upDown = reinterpret_cast<GetCarGunUpDownFn>(kPadGetCarGunUpDown)(pad);
    const float timeStep = ReadGameFloat(kTimerTimeStep, kOriginalTimeStep);
    const float ratio = timeStep / kOriginalTimeStep;
    float& carry = MovingPartCarryFor(vehicle);
    int32_t step = 0;
    if (ratio >= 1.0f) {
        step = static_cast<int16_t>(
            static_cast<float>(upDown) * timeStep * rate * kPadAxisScale);
        carry = 0.0f;
    } else {
        const auto originalStep = static_cast<int16_t>(
            static_cast<float>(upDown) * kOriginalTimeStep * rate * kPadAxisScale);
        // A reversed or released control starts from a clean fraction.
        if (originalStep == 0 || (originalStep > 0) != (carry > 0.0f)) {
            carry = 0.0f;
        }
        const float total = static_cast<float>(originalStep) * ratio + carry;
        step = static_cast<int32_t>(total);
        carry = total - static_cast<float>(step);
    }

    auto& angle = *reinterpret_cast<uint16_t*>(vehicle + kAutomobileMiscComponentAngle);
    int32_t next = static_cast<int32_t>(angle) + step;
    if (next < 0) {
        next = 0;
        carry = 0.0f;
    }
    angle = static_cast<uint16_t>(next);
    const int32_t limit = *reinterpret_cast<const int16_t*>(kMovingPartAngleLimit);
    if (static_cast<int32_t>(angle) > limit) {
        angle = static_cast<uint16_t>(limit);
        carry = 0.0f;
    }
}

// Replaces the step of a moving part from the timestep load to the clamp.
// `edi` is the vehicle, `ebx` its driver's pad and `[esp+0x1C]` the rate; the
// x87 stack is empty, and eax, ecx, edx and ebx are dead at the resume point,
// which clears ebx itself.
__declspec(naked) void MovingPartStepThunk() {
    __asm {
        push dword ptr [esp + 0x1C]
        push ebx
        push edi
        call StepMovingPartAngle
        add esp, 12
        jmp kMovingPartStepResume
    }
}

} // namespace

bool InstallMovingPartsFix() {
    if (!InstallJump(g_movingPartStepPatch, kMovingPartStep,
                     &MovingPartStepThunk, kExpectedMovingPartStep)) {
        Log("Moving parts fix skipped: CAutomobile::UpdateMovingCollision "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed forklift, dozer, dumper and ramp movement at the original "
        "rate.");
    return true;
}

} // namespace hff::vehicles
