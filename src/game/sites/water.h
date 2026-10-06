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

} // namespace hff
