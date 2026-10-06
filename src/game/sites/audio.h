#pragma once

#include <array>
#include <cstdint>

namespace hff {

// CAEVehicleAudioEntity::UpdateGasPedalAudio moves the throttle the engine
// note follows, m_GasPedalAudioRevs, towards the pedal by 0.09 a frame up and
// 0.07 a frame down, so above 30 FPS the revs jump to the pedal at once.
// fld dword ptr ds:[008CBC24h]
constexpr uintptr_t kGasRevStepUp = 0x004F5EE8;
constexpr std::array<uint8_t, 6> kExpectedGasRevStepUp{
    0xD9, 0x05, 0x24, 0xBC, 0x8C, 0x00
};
// fsub dword ptr ds:[008CBC28h]
constexpr uintptr_t kGasRevStepDown = 0x004F5F08;
constexpr std::array<uint8_t, 6> kExpectedGasRevStepDown{
    0xD8, 0x25, 0x28, 0xBC, 0x8C, 0x00
};

// CAEGlobalWeaponAudioEntity::ServiceAmbientGunFire starts the distant gunfire
// of Los Santos and the foghorn of a foggy San Fierro on the frames where
// `CTimer::m_FrameCounter & 0x3FF` is zero, once every 1024 frames: every 34 s
// at 30 FPS and every 3.4 s at 300.
// test dword ptr ds:[00B7CB4Ch],3FFh
constexpr std::array<uintptr_t, 2> kAmbientSoundCycles{0x004DF360, 0x004DF479};
constexpr std::array<uint8_t, 10> kExpectedAmbientSoundCycle{
    0xF7, 0x05, 0x4C, 0xCB, 0xB7, 0x00, 0xFF, 0x03, 0x00, 0x00
};

} // namespace hff
