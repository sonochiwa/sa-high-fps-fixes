#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// Skimmer: the water resistance constant `CVehicle::ApplyBoatWaterResistance`
// uses for the Skimmer.
constexpr uintptr_t kSkimmerResistancePatch = 0x006D2771;
constexpr uintptr_t kSkimmerResistanceReturn = 0x006D2777;
constexpr uintptr_t kSkimmerResistanceConstant = 0x00871DDC;
constexpr std::array<uint8_t, 6> kExpectedSkimmerResistance{
    0xD8, 0x0D, 0xDC, 0x1D, 0x87, 0x00
};

// CBoat::ProcessControl at 0x6F1770 spins the propeller down once per frame
// with no timestep when the boat is not under player, remote or physics
// control:
//
//     } else if (m_EngineSpeed > 0.0f) {
//         m_EngineSpeed *= 0.95f;
//     }
//
// The three branches directly above it, which drive the same field while the
// boat is being controlled, all carry the timestep, and the propeller angle a
// hundred bytes below integrates with it as well. Only the coast down branch is
// bare, so an abandoned boat's propeller stops turning, and its engine note
// dies, far sooner at high frame rates. It is the shared 0.95 decay.
constexpr uintptr_t kBoatEngineDamping = 0x006F1900;

// A car under water in `CAutomobile::ProcessBuoyancy`, and a burnt-out boat in
// `CBoat::ProcessControl`, sink by losing mass * 8e-6 of their buoyancy
// constant every frame, down to mass * 0.0064; the car's engine stalls below
// mass / 125. Each step is the same instruction:
// fmul dword ptr ds:[00871244h]
constexpr std::array<uintptr_t, 2> kSinkSteps{0x006A9098, 0x006F17CC};
constexpr std::array<uint8_t, 6> kExpectedSinkStep{
    0xD8, 0x0D, 0x44, 0x12, 0x87, 0x00
};

// `CVehicle::ProcessBoatControl` makes a boat on the water slam into a wave,
// with a thud and a push up and back, when its immersion rose by more than
// 0.02 since the last frame:
//
//     waveMult = _ftol((immersion - lastImmersion) * 10000.0f);
//     if (waveMult * waveAudioMult > 200) AddAudioEvent(AE_BOAT_HIT_WAVE);
//     if (waveMult > 200) ApplyMoveForce / ApplyTurnForce(... waveMult ...);
//
// A rise per frame shrinks with the frame, so above 30 FPS the slams and
// their thud fade out. This is the `_ftol`.
constexpr uintptr_t kBoatWaveHit = 0x006DCEE3;

// Planing. `CVehicle::ProcessBoatControl` lifts a boat under throttle onto the
// plane with
//
//     lift = |moveSpeed|^2 * aqPlaneForce * timeStep * buoyancy.z * 0.5f;
//     lift = min(lift * gas, timeStep * aqPlaneLimit * mass / 125.0f);
//     ApplyMoveForce(up * lift); ApplyTurnForce(up * lift, ...);
//
// The buoyancy force `cBuoyancy::ProcessBuoyancyBoat` returns already carries
// the timestep, so the lift goes with the square of it: above 30 FPS it falls
// short of the limit, the hull stays down in the water, and the drag, which
// grows with the square of the immersion, holds a Speeder to 90 km/h at 300
// FPS against 146 at 30. The patch replaces the `fmul` by the timestep with
// one by the larger of the timestep and its 30 FPS value, which leaves the
// lift one timestep, as the limit has.
constexpr uintptr_t kBoatPlaningLift = 0x006DC23D;
constexpr uintptr_t kBoatPlaningLiftReturn = 0x006DC243;
// fmul dword ptr [0B7CB5Ch]
constexpr std::array<uint8_t, 6> kExpectedBoatPlaningLift{
    0xD8, 0x0D, 0x5C, 0xCB, 0xB7, 0x00
};

// Pitch settling. The same function damps a boat's pitch rate, the x of its
// turn speed in its own frame, once per frame with
//
//     pitch += (turnRes.x^timeStep / (pitch^2 * 1000.0f + 1.0f) - 1) * pitch;
//
// The power keeps turnRes per second, but the divisor takes its share once a
// frame, so above 30 FPS a pitching hull is held flat much harder and cannot
// rise onto the plane. The patch replaces the `fadd 1.0` and the `fdivr` by
// turnRes.x that follow `pitch^2 * 1000` with turnRes.x times
// `(1 / (pitch^2 * 1000 + 1))^ratio`, the same share per second.
constexpr uintptr_t kBoatPitchSettle = 0x006DCE04;
constexpr uintptr_t kBoatPitchSettleReturn = 0x006DCE0E;
// fadd dword ptr [858624h] / fdivr dword ptr [esp+3Ch]
constexpr std::array<uint8_t, 10> kExpectedBoatPitchSettle{
    0xD8, 0x05, 0x24, 0x86, 0x85, 0x00, 0xD8, 0x7C, 0x24, 0x3C
};

} // namespace hff
