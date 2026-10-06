#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// Stunt jump camera: the two `_ftol` calls that turn the frame time into the
// whole milliseconds the unique jump camera timers count.
constexpr uintptr_t kEndTimerCall = 0x0049C505;
constexpr uintptr_t kFlightTimerCall = 0x0049C6FB;
constexpr std::array<uint8_t, 5> kExpectedEndTimerCall{
    0xE8, 0x36, 0x56, 0x38, 0x00
};
constexpr std::array<uint8_t, 5> kExpectedFlightTimerCall{
    0xE8, 0x40, 0x54, 0x38, 0x00
};

// Aim camera. The offsets are shared by CCamera and its three embedded CCam
// instances in GTA SA 1.0 US.
constexpr uintptr_t kCameraProcess = 0x0052B730;
constexpr uintptr_t kProcessAimWeapon = 0x00521500;
constexpr size_t kCameraActiveCam = 0x59;
constexpr size_t kCameraCams = 0x174;
constexpr size_t kCameraWeaponMode = 0x830;
constexpr size_t kCamSize = 0x238;
constexpr size_t kCamMode = 0x0C;
// The two aim camera entry points. MinHook decodes and relocates these
// prologues itself, but checking them first keeps the plugin's rule that no
// site is touched unless it still holds stock GTA SA 1.0 US code: if another
// ASI has already detoured either function, its jump lands here and the fix
// steps aside instead of stacking a second guard on foreign code.
constexpr std::array<uint8_t, 10> kExpectedCameraProcess{
    0x81, 0xEC, 0xA0, 0x00, 0x00, 0x00,
    0x53, 0x55, 0x56, 0x57
};
constexpr std::array<uint8_t, 5> kExpectedProcessAimWeapon{
    0xA0, 0x10, 0x01, 0xB7, 0x00
};
// Aim camera zoom. `CCam::Process_AimWeapon` walks `m_fFOV` toward the
// weapon's aim FOV by `GetTimeStep() * 1.0` degrees a frame, 50 degrees a
// second at any frame rate. The aim guard pins the timestep at 1.0 for the
// whole call, so above 50 FPS the step stops shrinking with the frame and the
// zoom runs at the frame rate instead: 144 degrees a second at 144 FPS,
// nearly three times the stock speed. The load is replaced by the real frame
// duration, which is what the original reads when no guard is in place.
constexpr uintptr_t kAimWeaponFovStep = 0x0052167A;
constexpr uintptr_t kAimWeaponFovStepReturn = 0x00521680;
// fld dword ptr ds:[00B7CB5Ch], the timestep load ahead of the FOV step.
constexpr std::array<uint8_t, 6> kExpectedAimWeaponFovStep{
    0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00
};

// Drunk camera sway. While `CMBlur::Drunkness` at 0xC73C58 is above zero,
// `CCamera::Process` sways the camera's front, up and position vectors by
// `Drunkness * amplitude * cos(phase)` and the matching sine, with the
// amplitudes at 0x85904C, 0x858C28 and 0x858EF4. The phase itself is a plain
// global at 0xB6EC30 advanced by a bare `fadd` of the 5.0 at 0x858C80 once per
// rendered frame, and the 0.01745329 at 0x8595EC converts it from degrees, so
// the sway turns 150 degrees a second at 30 FPS and 600 at 120. That is the
// shaking: the amplitude is right but the oscillation runs at the frame rate.
constexpr uintptr_t kDrunkCameraPhase = 0x0052C729;
constexpr uintptr_t kDrunkCameraPhaseReturn = 0x0052C72F;
constexpr uintptr_t kDrunkCameraPhaseStep = 0x00858C80;
// fadd dword ptr ds:[00858C80h]
constexpr std::array<uint8_t, 6> kExpectedDrunkCameraPhase{
    0xD8, 0x05, 0x80, 0x8C, 0x85, 0x00
};

// Follow cameras. `CCam::Process_FollowPed_SA` and `CCam::Process_FollowCar_SA`
// turn the gap between where the camera points and where it should point into
// an angular rate by dividing it by `max(1.0f, GetTimeStep())`. A timestep of
// 1.0 is 50 FPS, so at 30 the divisor is the real timestep and the clamp never
// binds; above 50 it sticks at 1.0 and the rate comes out short by the ratio,
// ten times short at 500 FPS. Both blocks are byte for byte identical and are
// replaced by the load of the timestep alone, which is what the original picks
// whenever the timestep is at least 1.0, so nothing changes at or below 50 FPS.
constexpr uintptr_t kFollowPedCameraRate = 0x0052381D;
constexpr uintptr_t kFollowPedCameraRateReturn = 0x0052383E;
constexpr uintptr_t kFollowCarCameraRate = 0x00524FD7;
constexpr uintptr_t kFollowCarCameraRateReturn = 0x00524FF8;

} // namespace hff
