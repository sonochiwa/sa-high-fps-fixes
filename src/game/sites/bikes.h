#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// CBike fields.
constexpr size_t kBikeLeanMatrixCalculated = 0x5C8;
constexpr size_t kBikeLeanMatrix = 0x5CC;
constexpr size_t kBikeFlags = 0x614;
constexpr uint8_t kBikeGettingPickedUp = 0x08;
constexpr size_t kBikeWheelRatios = 0x710;
constexpr size_t kBikeWheelContactTimers = 0x730;

// The per-entity steps an abandoned bike runs at the original rate, and the
// render calls that draw it interpolated between two of those steps.
constexpr uintptr_t kBikeProcessControl = 0x006B9250;
constexpr uintptr_t kPhysicalProcessCollision = 0x0054DFB0;
constexpr uintptr_t kPhysicalProcessShift = 0x0054DB10;
constexpr uintptr_t kEntityUpdateRwMatrix = 0x00446F90;
constexpr uintptr_t kEntityUpdateRwFrame = 0x00532B00;
constexpr uintptr_t kBikePreRender = 0x006BD090;
constexpr uintptr_t kBikeRender = 0x006BDE20;
constexpr std::array<uint8_t, 7> kExpectedBikeProcessControl{
    0x6A, 0xFF, 0x68, 0xEB, 0x82, 0x84, 0x00
};
constexpr std::array<uint8_t, 8> kExpectedPhysicalProcessCollision{
    0x6A, 0xFF, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedPhysicalProcessShift{
    0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8
};
constexpr std::array<uint8_t, 5> kExpectedEntityUpdateRwFrame{
    0x8B, 0x41, 0x18, 0x85, 0xC0
};
constexpr std::array<uint8_t, 7> kExpectedBikePreRender{
    0x6A, 0xFF, 0x68, 0x21, 0x83, 0x84, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedBikeRender{
    0x51, 0x56, 0x8D, 0x44, 0x24, 0x04
};

// Bike takeoff pitch. `CPhysical::ApplyTurnForce` changes the angular velocity
// directly, `m_vecTurnSpeed += cross(point, force) / m_fTurnMass`, with no
// timestep. The wheel contact call at 0x6D7B17 hands it a force that is not
// scaled by time either, so while a bike leaves a ramp the same impulse lands
// on every rendered frame and the bike pitches back far harder at a high
// frame rate. The call is redirected so the excess positive pitch it adds can
// be taken back.
constexpr uintptr_t kBikeWheelTurnForceCall = 0x006D7B17;
constexpr uintptr_t kApplyTurnForce = 0x00542A50;
// The BMX bunny hop launch, the knock-off check on landing, and the two
// rider-fall events a hard landing raises: for excessive turn speed and for
// excessive velocity along the bike's up axis.
constexpr uintptr_t kBmxLaunchBunnyHop = 0x006C0390;
constexpr uintptr_t kBikeDamageKnockOffRider = 0x006B5A10;
constexpr std::array<uintptr_t, 2> kBikeRiderFallEventAddCalls{
    0x006B74B4, 0x006B769B
};
constexpr uintptr_t kEventGroupAdd = 0x004AB420;
// The vehicle a rider-fall event refers to.
constexpr size_t kRiderFallEventVehicle = 0x38;
constexpr std::array<uint8_t, 8> kExpectedBmxLaunchBunnyHop{
    0x83, 0xEC, 0x0C, 0x56, 0x8B, 0x74, 0x24, 0x18
};
constexpr std::array<uint8_t, 7> kExpectedBikeDamageKnockOffRider{
    0x6A, 0xFF, 0x68, 0xD3, 0x81, 0x84, 0x00
};

// Bike lean target. `CBike::ProcessControl` aims the rider lean at
//
//     target = lateralAcceleration / (max(0.01, CTimer::GetTimeStep()) * 0.008)
//
// where the numerator is the change in move speed along the bike's right axis
// during this call. That is a numerical derivative: both parts are proportional
// to the timestep, so for smooth acceleration the quotient is the same at any
// frame rate. What is not the same is its conditioning. Any jitter in the
// numerator that does not shrink with the timestep, such as contact impulses, is
// divided by an eighteen times smaller number at 500 FPS and amplified by that
// much. Measured on a bike standing still, the physical roll oscillates by
// 0.005 radians while the rendered lean swings 0.25, and at 30 FPS the bike does
// not visibly rock at all.
//
// The derivative is therefore taken over one original frame of real time rather
// than one rendered frame. The replaced instructions are
// `fstp [esp+0x14]; fstp st(0)`.
constexpr uintptr_t kBikeLeanTarget = 0x006BBB0D;
constexpr uintptr_t kBikeLeanTargetReturn = 0x006BBB13;
constexpr uintptr_t kGravityConstant = 0x00863984;
constexpr std::array<uint8_t, 6> kExpectedBikeLeanTarget{
    0xD9, 0x5C, 0x24, 0x14, 0xDD, 0xD8
};

// `CBmx::ProcessControl` at `0x6BFA30` decays the rider's sprint lean once per
// frame with no timestep when the sprint animation stops:
//
//     m_fSprintLeanAngle *= 0.95f;
//
// The angle itself is purely cosmetic body sway, but the decay is the only
// thing that returns it to neutral, so at a high frame rate it snaps back
// instead of easing. It is the shared 0.95 decay.
constexpr uintptr_t kBmxSprintLeanDecay = 0x006BFB3B;

// `CBmx::ProcessDrivingAnims` at `0x6BFB50` settles the rider's animated lean
// once the input has gone quiet:
//
//     m_RideAnimData.AnimLeanLeft *= 0.95f;
//     m_RideAnimData.AnimLeanFwd  *= 0.95f;
//
// in two branches, four instructions in all. Twenty bytes above the first pair
// the same function decays another field with `pow(rate, GetTimeStep())`, which
// is the engine's own idiom for a frame-rate correct decay. At 500 FPS the lean
// snaps to neutral instead of easing.
//
// The `0.95` lives in a writable global, so the thunks read it at run time
// rather than baking it in. The constant is already on the stack when the
// replaced `fmul` runs, which is why these thunks do not load a base of their
// own the way the shared decay does.
constexpr uintptr_t kBmxLeanLeftDecayA = 0x006C0067;
constexpr uintptr_t kBmxLeanFwdDecayA = 0x006C0079;
constexpr uintptr_t kBmxLeanLeftDecayB = 0x006C00FA;
constexpr uintptr_t kBmxLeanFwdDecayB = 0x006C010C;
// fmul dword ptr [esi+00000654h] / fmul dword ptr [esi+00000658h]
constexpr std::array<uint8_t, 6> kExpectedBmxLeanLeftDecay{
    0xD8, 0x8E, 0x54, 0x06, 0x00, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedBmxLeanFwdDecay{
    0xD8, 0x8E, 0x58, 0x06, 0x00, 0x00
};

// `CBike::ProcessControl` coasts the front wheel down the way `CAutomobile`
// does, and one of its two copies of that code lost the timestep:
//
//     if (m_WheelCounts[0] == 0.0f && m_WheelCounts[1] == 0.0f) {
//         m_aWheelAngularVelocity[0] *= 0.95f;
//         m_aWheelPitchAngles[0] += m_aWheelAngularVelocity[0];
//     }
//
// The same five instructions appear twice in the function, once at `0x6BAC77`
// and once at `0x6BB59B`, on the two sides of a rider flag, and the second copy
// multiplies the velocity by `ms_fTimeStep` before adding it while the first
// does not, so the free front wheel spins sixteen times as fast at 500 FPS as
// it does at 30. The `0.95` decay is per frame in both copies and is the shared
// decay. Both changes are identities at 30 FPS.
//
// The two front-wheel sites are cosmetic: they update the visible wheel pitch.
// The rear-wheel rate limiter is different: it controls the physical angular
// speed used by wheel contact. Its fixed -0.1 / +0.05 steps must be scaled into
// the current frame before the contact solver sees them.
constexpr uintptr_t kBikeWheelSpinDampA = 0x006BAC7D;
constexpr uintptr_t kBikeWheelPitchIntegrate = 0x006BAC89;
constexpr uintptr_t kBikeWheelSpinDampB = 0x006BB5A1;
constexpr uintptr_t kBikeRearWheelSpeedDecel = 0x006BAFF4;
constexpr uintptr_t kBikeRearWheelSpeedAccel = 0x006BB00F;
// fadd dword ptr [esi+00000750h]
constexpr std::array<uint8_t, 6> kExpectedBikeWheelPitchIntegrate{
    0xD8, 0x86, 0x50, 0x07, 0x00, 0x00
};

} // namespace hff
