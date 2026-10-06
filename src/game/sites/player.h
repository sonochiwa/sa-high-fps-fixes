#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// Aiming rifle walk: the step constant of the walk while aiming a rifle.
constexpr uintptr_t kAimingRifleWalkPatch = 0x0061E0CA;
constexpr uintptr_t kAimingRifleWalkReturn = 0x0061E0D0;
constexpr uintptr_t kAimingRifleWalkConstant = 0x00858CA8;
constexpr std::array<uint8_t, 6> kExpectedAimingRifleWalk{
    0xD8, 0x0D, 0xA8, 0x8C, 0x85, 0x00
};

// Ped push: the vehicle side of a ped/vehicle collision impulse.
constexpr uintptr_t kPedPushCarPatch = 0x00549652;
constexpr uintptr_t kPedPushCarReturn = 0x0054965A;
constexpr std::array<uint8_t, 8> kExpectedPedPushCar{
    0x8B, 0x54, 0x24, 0x20, 0x8B, 0x44, 0x24, 0x24
};

// `CPed::PlayFootSteps` uses m_nDeathTimeMS as a bloody-footprint countdown.
// The stock code subtracts one on every rendered frame, so the effect can
// expire before the next footstep at a high frame rate.
constexpr uintptr_t kBloodyFootprintCounterPatch = 0x005E5877;
constexpr uintptr_t kBloodyFootprintCounterReturn = 0x005E5880;
constexpr uintptr_t kPlayFootStepsLandedCall = 0x005E5E64;
constexpr uintptr_t kDoFootLanded = 0x005E5380;
constexpr uintptr_t kBloodyFootprintShadowCall = 0x005E54C1;
constexpr uintptr_t kAddPermanentShadow = 0x00706F60;
constexpr std::array<uint8_t, 9> kExpectedBloodyFootprintCounter{
    0x49, 0x85, 0xC9, 0x89, 0x8E, 0x50, 0x07, 0x00, 0x00
};
constexpr std::array<uint8_t, 5> kExpectedPlayFootStepsLandedCall{
    0xE8, 0x17, 0xF5, 0xFF, 0xFF
};
constexpr std::array<uint8_t, 5> kExpectedBloodyFootprintShadowCall{
    0xE8, 0x9A, 0x1A, 0x12, 0x00
};

// Swimming. `CTaskSimpleSwim::ProcessSwimmingResistance` blends the ped's move
// speed toward a target with `pow(0.9f, GetTimeStep())`, which is already
// frame-rate correct, so the blend needs no help. The target is the problem: it
// is built from `CPed::m_vecAnimMovingShiftLocal`, the displacement the walk
// cycle produced during this frame, which shrinks with the frame. The steady
// state of the blend is that target, so the swim speed shrinks with it.
//
// The one call site is wrapped rather than the reads inside, because the
// function also folds in plain constants and an animation progress fraction,
// neither of which may be scaled.
constexpr uintptr_t kProcessSwimmingResistance = 0x0068A1D0;
constexpr uintptr_t kSwimResistanceCall = 0x0068B4A8;
constexpr uintptr_t kSwimResistanceReturn = 0x0068B4B0;
// push esi / mov ecx,edi / call CTaskSimpleSwim::ProcessSwimmingResistance
constexpr std::array<uint8_t, 8> kExpectedSwimResistanceCall{
    0x56, 0x8B, 0xCF, 0xE8, 0x20, 0xED, 0xFF, 0xFF
};
// Compatibility probes for the sites used by Tweaker, Swim FPS Fix and
// earlier high-FPS fixes, which patch inside ProcessSwimmingResistance while
// this plugin wraps its caller, so checking only kSwimResistanceCall would
// miss them and apply the same correction twice.
constexpr uintptr_t kSwimDiveScale = 0x0068A42B;
constexpr uintptr_t kSwimAscentBias = 0x0068A4CA;
constexpr uintptr_t kSwimVectorSetup = 0x0068A4FC;
constexpr uintptr_t kSwimVectorTransform = 0x0068A50E;
constexpr std::array<uint8_t, 6> kExpectedSwimDiveScale{
    0xD8, 0x0D, 0xF4, 0x8E, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedSwimAscentBias{
    0xD8, 0x05, 0xCC, 0x08, 0x87, 0x00
};
constexpr std::array<uint8_t, 18> kExpectedSwimVectorSetup{
    0xD9, 0x5C, 0x24, 0x1C,
    0xD9, 0x44, 0x24, 0x14,
    0xD8, 0xC9,
    0xD9, 0x5C, 0x24, 0x20,
    0xD8, 0x4C, 0x24, 0x18
};
constexpr std::array<uint8_t, 6> kExpectedSwimVectorTransform{
    0xD9, 0xC1, 0xD8, 0x08, 0xD9, 0xC2
};

// `CTaskSimpleSwim::ProcessSwimmingResistance` also drives the swim pitch, and
// the rate it pitches at decays once per frame with no timestep in three
// places:
//
//     m_fStateChanger *= 0.95f;                              // no timestep
//     m_fStateChanger += GetTimeStepInSeconds() / 10.0f;     // timestep
//     m_fRotationX    += GetTimeStep() * m_fStateChanger;    // timestep
//
// The build-up and the integration both carry a timestep and the move speed
// blend at the top of the same function uses `pow(0.9f, GetTimeStep())`, so
// only the decay is bare. The decay runs while the ped is under the surface,
// so at a high frame rate the pitch rate is killed almost as fast as it builds
// and the swim angle barely responds. All three sites are the shared 0.95
// decay.
//
// This does not overlap `swimmingMovement`, which scales
// `CPed::m_vecAnimMovingShiftLocal`. That field feeds the move speed blend and
// is not read by the pitch code.
constexpr uintptr_t kSwimPitchDecayA = 0x0068A6BD;
constexpr uintptr_t kSwimPitchDecayB = 0x0068A735;
constexpr uintptr_t kSwimPitchDecayC = 0x0068A7C0;

// The same function lifts a surface swimmer towards the water line no faster
// than timestep * 0.1, a speed limit that shrinks with the frame, so above
// 30 FPS the swimmer settles to swimming depth and follows the waves many
// times more slowly. The limit takes the 30 FPS timestep instead.
// fld dword ptr ds:[00B7CB5Ch]
constexpr uintptr_t kSwimSurfaceSpeedLimit = 0x0068A7E6;
constexpr std::array<uint8_t, 6> kExpectedSwimSurfaceSpeedLimit{
    0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00
};

// `CTaskSimpleSwim::ProcessEffects` makes a splash with its stroke sound on
// every frame a hand or foot of a sprinting surface swimmer is within 0.05 of
// the ped's height, so above 30 FPS more frames fall in that window and the
// splashes multiply. Each is a `CreateFxSystem` whose null result skips both.
constexpr std::array<uintptr_t, 4> kSwimSplashEffects{
    0x0068AEBA, 0x0068AF15, 0x0068AF66, 0x0068AFB3
};

// Climbing. `CTaskSimpleClimb::ProcessPed` drags the ped onto the hand hold by
// setting `m_vecMoveSpeed` to the remaining offset divided by the timestep. The
// branch taken while that offset is still large clamps the result to the `0.2`
// at `0x858CC4`; the branch for the last part of the move, at `0x6811E8`, does
// not. Dividing by a short frame leaves a move speed above three, which the
// impact code reads as a lethal fall once the climb lets go. The clamp is the
// game's own constant from the sibling branch, and at 30 FPS the offset is
// small enough that it never binds.
constexpr uintptr_t kClimbSpeedClamp = 0x00681212;
constexpr uintptr_t kClimbSpeedClampReturn = 0x0068121F;
constexpr uintptr_t kClimbSpeedLimit = 0x00858CC4;
constexpr uintptr_t kVectorAddAssign = 0x00411A00;
// add esp,0Ch / lea edx,[esp+48h] / push edx / call CVector::operator+=
constexpr std::array<uint8_t, 13> kExpectedClimbSpeedClamp{
    0x83, 0xC4, 0x0C,
    0x8D, 0x54, 0x24, 0x48,
    0x52,
    0xE8, 0xE1, 0x07, 0xD9, 0xFF
};

// Picking up an object. While the pickup animation plays,
// `CTaskSimpleHoldEntity::ProcessPed` walks the ped onto the pickup point by
// adding `offset / CTimer::ms_fTimeStep * 0.1` to its animation shift for the
// frame, once across the ped and once along it. That covers a fixed share of
// the remaining offset each frame, 6% at 30 FPS, and the ped's speed goes up
// with the square of the frame rate: at 300 FPS the ped sets off a hundred
// times as fast, and above 500 FPS it overshoots the point. Each
// `fdiv dword ptr [CTimer::ms_fTimeStep]` is replaced by a call.
constexpr uintptr_t kPickUpAlignAcross = 0x00693F4F;
constexpr uintptr_t kPickUpAlignAlong = 0x00693F80;
constexpr float kPickUpAlignFactor = 0.1f;
// fdiv dword ptr [CTimer::ms_fTimeStep]
constexpr std::array<uint8_t, 6> kExpectedPickUpAlign{
    0xD8, 0x35, 0x5C, 0xCB, 0xB7, 0x00
};

// Buoyancy. `cBuoyancy::CalcBuoyancyForce` builds a per frame impulse,
// `immersion * buoyancy * GetTimeStep()`, then refuses to apply it when
// `mass * moveSpeed.z` exceeds four times that impulse. The left side is a
// momentum and does not follow the frame; the right side does, so the cutoff
// falls with the frame length and a swimmer rising at any speed loses buoyancy
// entirely above a few hundred FPS. The threshold is evaluated in original
// timestep units and only the stored impulse is scaled back to this frame.
constexpr uintptr_t kBuoyancyThreshold = 0x006C27A2;
constexpr uintptr_t kBuoyancyThresholdReturn = 0x006C27C8;
constexpr uintptr_t kBuoyancyClampedStore = 0x006C27EC;
// The impulse, its store, the mass times move speed product and the compare
// against the 4.0 at 0x858B90.
constexpr std::array<uint8_t, 38> kExpectedBuoyancyThreshold{
    0xD9, 0x86, 0xBC, 0x00, 0x00, 0x00,
    0xD8, 0x4E, 0x6C,
    0x83, 0xC4, 0x0C,
    0xD8, 0x0D, 0x5C, 0xCB, 0xB7, 0x00,
    0xD9, 0x51, 0x08,
    0xD9, 0x80, 0x8C, 0x00, 0x00, 0x00,
    0xD8, 0x48, 0x4C,
    0xD9, 0xC1,
    0xD8, 0x0D, 0x90, 0x8B, 0x85, 0x00
};
// fstp [ecx+8] / mov al,1 / pop esi / add esp,0Ch / ret 0Ch
constexpr std::array<uint8_t, 12> kExpectedBuoyancyClampedStore{
    0xD9, 0x59, 0x08,
    0xB0, 0x01,
    0x5E,
    0x83, 0xC4, 0x0C,
    0xC2, 0x0C, 0x00
};

// Drowning damage. `CPlayerPed::HandlePlayerBreath` computes the per frame
// damage as the timestep times three and truncates it to an integer, so above
// roughly 150 FPS every frame rounds down to zero and the player never drowns.
// The patched span covers the multiply, the two argument pushes that the
// optimizer hoisted in front of the conversion, and the `_ftol` call itself.
constexpr uintptr_t kDrowningDamage = 0x0060A92F;
constexpr uintptr_t kDrowningDamageReturn = 0x0060A93E;
// fmul dword ptr ds:[00858B3Ch] / push 0 / push 3 / call _ftol
constexpr std::array<uint8_t, 15> kExpectedDrowningDamage{
    0xD8, 0x0D, 0x3C, 0x8B, 0x85, 0x00,
    0x6A, 0x00,
    0x6A, 0x03,
    0xE8, 0x02, 0x72, 0x21, 0x00
};

// `CTaskSimpleJetPack::DoJetPackEffect` at `0x67B7F0` ramps `m_FxKeyTime` by
// 0.1 toward 1 while the thrusters are firing and back toward 0 when they are
// not, and hands it to the jetpack particle system as its constant time. Ten
// frames is a third of a second at 30 FPS and twenty milliseconds at 500, so
// the flame snaps between its two states instead of blending. `gta-reversed`
// notes on this line that it should use the frame delta time, so the defect
// is known upstream and simply never fixed.
constexpr uintptr_t kJetPackFxRampUp = 0x0067B99C;
constexpr uintptr_t kJetPackFxRampDown = 0x0067B9C8;
// fadd dword ptr ds:[00858B1Ch] / fsub dword ptr ds:[00858B1Ch]
constexpr std::array<uint8_t, 6> kExpectedJetPackRampUp{
    0xD8, 0x05, 0x1C, 0x8B, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedJetPackRampDown{
    0xD8, 0x25, 0x1C, 0x8B, 0x85, 0x00
};

// `CStats::UpdateFatAndMuscleStats` advances `m_FatCounter` by
// `milliseconds * exerciseRate / 10` in integer arithmetic, an unsigned divide
// by ten that keeps no remainder. At 500 FPS the numerator is 2 * rate, so
// every exercise rate below five rounds to zero on every frame and fat never
// burns off. The arithmetic is replaced by one call that divides in floating
// point and carries the remainder; the exercise rate is the function's only
// argument and is read back off the caller's stack.
constexpr uintptr_t kFatCounterMath = 0x0055C5C8;
constexpr uintptr_t kFatCounter = 0x00B794FC;
// mov edx,eax / imul edx,[esp+8] / mov eax,0CCCCCCCDh / mul edx /
// mov eax,ds:[00B794FCh] / shr edx,3 / add eax,edx / mov ds:[00B794FCh],eax
constexpr std::array<uint8_t, 29> kExpectedFatCounterMath{
    0x8B, 0xD0,
    0x0F, 0xAF, 0x54, 0x24, 0x08,
    0xB8, 0xCD, 0xCC, 0xCC, 0xCC,
    0xF7, 0xE2,
    0xA1, 0xFC, 0x94, 0xB7, 0x00,
    0xC1, 0xEA, 0x03,
    0x03, 0xC2,
    0xA3, 0xFC, 0x94, 0xB7, 0x00
};

// Drunk driving steering delay. `CPad::Update` keeps a ten deep FIFO of
// steering samples and shifts it by one entry every frame, and
// `CPad::GetSteeringLeftRight` returns entry `DrunkDrivingBufferUsed`, which a
// script sets when the player is drunk. At 30 FPS a delay of 9 is 300 ms of
// lag on the wheel; at 2000 FPS it is 4.5 ms. The shift is made on 30 FPS
// ticks instead, so the delay stays the same number of milliseconds. Both
// pads must shift on the same frames, so the decision is taken once per frame.
constexpr uintptr_t kDrunkSteerShift = 0x00541D2D;
constexpr uintptr_t kDrunkSteerShiftResume = 0x00541D35;
constexpr uintptr_t kDrunkSteerShiftSkip = 0x00541D42;
// lea eax,[ebx+72h] / mov ecx,9
constexpr std::array<uint8_t, 8> kExpectedDrunkSteerShift{
    0x8D, 0x43, 0x72, 0xB9, 0x09, 0x00, 0x00, 0x00
};

} // namespace hff
