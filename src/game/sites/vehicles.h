#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// Helicopter rotor: the two per-frame spin-up steps in
// `CHeli::ProcessFlyingCarStuff`, a fraction of the final rotor speed whose
// operand is read through the instruction at 0x6C4EFE.
constexpr std::array<uintptr_t, 2> kHeliRotorSites{0x006C4F29, 0x006C4F37};
constexpr uintptr_t kHeliRotorSlowReturn = 0x006C4F2F;
constexpr uintptr_t kHeliRotorFastReturn = 0x006C4F3D;
constexpr uintptr_t kHeliRotorSpeedOperand = 0x006C4EFE;
constexpr float kHeliRotorSpeedDivisor = 220.0f;
constexpr std::array<uint8_t, 6> kExpectedHeliRotorSlow{
    0xD8, 0x05, 0xDC, 0x8C, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedHeliRotorFast{
    0xD8, 0x05, 0xD8, 0x9C, 0x85, 0x00
};
constexpr std::array<uint8_t, 2> kExpectedHeliRotorOperand{0xD8, 0x1D};

// Siren tap. `CVehicle::ProcessSirenAndHorn` tells a horn tap from a hold with
// a per-frame history buffer. At 0x006E0961 it loads the index of the newest
// entry, `movzx ecx,byte ptr [0B7356Eh]`, and reads the history from
// 0x006E0968. The Compact build has that instruction in place; the Hoodlum
// build moved it to a stub at 0x00403940 and left `nop; nop; jmp` behind.
constexpr uintptr_t kSirenPatch = 0x006E0961;
constexpr uintptr_t kSirenAnchor = 0x006E0999;
constexpr uintptr_t kSirenHistoryCheck = 0x006E0968;
constexpr uintptr_t kSirenToggleReturn = 0x006E0999;
constexpr uintptr_t kSirenHornReturn = 0x006E09E8;
constexpr uintptr_t kSirenNoHornReturn = 0x006E09F7;
constexpr uintptr_t kPadGetHorn = 0x0053FEE0;
constexpr uintptr_t kPadHornJustDown = 0x0053FF30;
constexpr uint32_t kSirenTapMilliseconds = 150;
constexpr std::array<uint8_t, 7> kExpectedSirenCompact{
    0x0F, 0xB6, 0x0D, 0x6E, 0x35, 0xB7, 0x00
};
constexpr std::array<uint8_t, 7> kExpectedSirenHoodlum{
    0x90, 0x90, 0xE9, 0xD8, 0x2F, 0xD2, 0xFF
};
constexpr std::array<uint8_t, 6> kExpectedSirenAnchor{
    0x8A, 0x86, 0x2D, 0x04, 0x00, 0x00
};

// Rest threshold. `CPhysical::m_fMovingSpeed` at +0xD4 is how far the entity
// moved during the current frame, so it shrinks with the timestep, but it is
// compared against a fixed limit right before the at-rest branch.
constexpr uintptr_t kCarRestThreshold = 0x006B1C9C;
constexpr uintptr_t kCarRestThresholdReturn = 0x006B1CA2;
constexpr uintptr_t kBikeRestThreshold = 0x006B9955;
constexpr uintptr_t kBikeRestThresholdReturn = 0x006B995B;
constexpr uintptr_t kTrailerRestThreshold = 0x006F9B92;
constexpr uintptr_t kTrailerRestThresholdReturn = 0x006F9B98;
constexpr std::array<uint8_t, 6> kExpectedRestThreshold{
    0xD9, 0x86, 0xD4, 0x00, 0x00, 0x00
};

// Physics sleep counter. `CPhysical::m_nFakePhysics` at +0xB8 is incremented
// once per rendered frame and, above 10, the entity has its move and turn speed
// reset and its physics skipped for the frame. The sites are in the
// ProcessControl of CObject, CAutomobile, CBike and CTrailer.
constexpr uintptr_t kObjectFakePhysics = 0x005A241F;
constexpr uintptr_t kObjectFakePhysicsReturn = 0x005A2427;
constexpr uintptr_t kCarFakePhysics = 0x006B1D2A;
constexpr uintptr_t kCarFakePhysicsReturn = 0x006B1D32;
constexpr uintptr_t kBikeFakePhysics = 0x006B9972;
constexpr uintptr_t kBikeFakePhysicsReturn = 0x006B997A;
constexpr uintptr_t kTrailerFakePhysics = 0x006F9BD1;
constexpr uintptr_t kTrailerFakePhysicsReturn = 0x006F9BD9;
constexpr std::array<uint8_t, 8> kExpectedObjectFakePhysics{
    0x8A, 0x8E, 0xB8, 0x00, 0x00, 0x00, 0xFE, 0xC1
};
constexpr std::array<uint8_t, 8> kExpectedCarFakePhysics{
    0x8A, 0x96, 0xB8, 0x00, 0x00, 0x00, 0xFE, 0xC2
};
constexpr std::array<uint8_t, 8> kExpectedBikeFakePhysics{
    0x8A, 0x8E, 0xB8, 0x00, 0x00, 0x00, 0xFE, 0xC1
};
constexpr std::array<uint8_t, 8> kExpectedTrailerFakePhysics{
    0x8A, 0x9E, 0xB8, 0x00, 0x00, 0x00, 0xFE, 0xC3
};

// Swinging doors, boots, bonnets, the lowrider chassis and the firetruck
// ladder. Smooth angular input comes from the difference between the current
// and previous point velocities and already follows the timestep. Suspension
// contact impulses driving a swinging chassis do not: they can arrive once per
// rendered frame and make lowrider bodies shake harder at high FPS. Normalize
// that chassis-only path. Leave the ordinary/firetruck input untouched because
// scaling it suppresses the ladder's intended movement.
constexpr uintptr_t kDoorForceChassis = 0x006F42DB;
constexpr uintptr_t kDoorForceChassisReturn = 0x006F42E0;
constexpr uintptr_t kDoorDampingFiretruck = 0x006F43A1;
constexpr uintptr_t kDoorDampingFiretruckReturn = 0x006F43A7;
constexpr uintptr_t kDoorDampingOther = 0x006F43D8;
constexpr uintptr_t kDoorDampingOtherReturn = 0x006F43DE;
constexpr uintptr_t kDoorIntegration = 0x006F4422;
constexpr uintptr_t kDoorIntegrationReturn = 0x006F4427;
constexpr uintptr_t kDoorApplyRateChassis = 0x00872328;
constexpr float kStockDoorApplyRateChassis = 0.025f;
// fmul st,st(1) / fadd dword ptr [esi+14h]
constexpr std::array<uint8_t, 5> kExpectedDoorForceChassis{
    0xD8, 0xC9, 0xD8, 0x46, 0x14
};
// fld dword ptr ds:[00872314h]
constexpr std::array<uint8_t, 6> kExpectedDoorDampingFiretruck{
    0xD9, 0x05, 0x14, 0x23, 0x87, 0x00
};
// fmul dword ptr [esi+14h] / fstp dword ptr [esi+14h]
constexpr std::array<uint8_t, 6> kExpectedDoorDampingOther{
    0xD8, 0x4E, 0x14, 0xD9, 0x5E, 0x14
};
// fld [esi+14h] / mov ecx,ebx
constexpr std::array<uint8_t, 5> kExpectedDoorIntegration{
    0xD9, 0x46, 0x14, 0x8B, 0xCB
};

// Moving parts: the forklift forks, the dozer blade, the dumper bed and the
// Packer and Andromada ramps. `CAutomobile::UpdateMovingCollision` steps
// `m_wMiscComponentAngle` by `upDown / 128 * timestep * rate` a frame and
// truncates the step to a whole unit. Above 30 FPS the truncation eats the
// step: at 300 FPS the forks rise at 300 units a second instead of 480, and
// above 500 FPS they do not rise at all. The span from the timestep load to
// the clamp is replaced; `edi` is the vehicle, `ebx` its driver's pad and
// `[esp+0x1C]` the rate, which the forklift branch sets to -10 or -20.
constexpr uintptr_t kMovingPartStep = 0x006A1691;
constexpr uintptr_t kMovingPartStepResume = 0x006A172B;
constexpr uintptr_t kPadGetCarGunUpDown = 0x0053FC10;
// `DEFAULT_COLLISION_EXTENDLIMIT`, the int16 the angle is clamped to.
constexpr uintptr_t kMovingPartAngleLimit = 0x008D314C;
constexpr size_t kAutomobileMiscComponentAngle = 0x86C;
constexpr float kPadAxisScale = 1.0f / 128.0f;
// mov ecx,dword ptr [CTimer::ms_fTimeStep]
constexpr std::array<uint8_t, 6> kExpectedMovingPartStep{
    0x8B, 0x0D, 0x5C, 0xCB, 0xB7, 0x00
};

// Water cannons of the fire truck and the SWAT van. A cannon keeps its jet as a
// ring of 32 sections; `CWaterCannon::Update_OncePerFrame` advances the ring by
// one section and clears the slot it moves to on every rendered frame once the
// cannon is 150 ms old, and the cannon's new input overwrites the current slot
// at the nozzle. A section therefore lives 31 frames: 1 s and 25 m of jet at
// 30 FPS, 5 m at 144 FPS and 2.6 m at 300, while the motion and the
// extinguishing of each section already scale by the timestep. The ring is
// advanced on original 30 FPS ticks instead, so a section lives 31 original
// frames at any frame rate.
constexpr uintptr_t kWaterCannonAdvanceGate = 0x0072A292;
constexpr uintptr_t kWaterCannonAdvance = 0x0072A29B;
constexpr uintptr_t kWaterCannonAdvanceSkip = 0x0072A2BB;
// add eax,96h / cmp ecx,eax / jbe 0072A2BB
constexpr std::array<uint8_t, 9> kExpectedWaterCannonAdvance{
    0x05, 0x96, 0x00, 0x00, 0x00, 0x3B, 0xC8, 0x76, 0x20
};

// `CVehicle::CanPedJumpOutCar` at `0x6D2030` damps both speeds once per call
// with no timestep, three components each:
//
//     m_vecTurnSpeed *= 0.9f;                     // no timestep, 0x6D2113..
//     if (moveSpeedSq / 100 > sq(ts) * sq(0.008)) {
//         m_vecMoveSpeed *= 0.9f;                 // no timestep, 0x6D217D..
//         return false;
//     }
//
// The branch only runs on a slow vehicle the player is trying to bail out of,
// and the effect is that the car is brought to a halt harder the higher the
// frame rate. Both the comparison above it and the fallthrough below it are
// timestep-correct.
constexpr uintptr_t kJumpOutTurnDampX = 0x006D2113;
constexpr uintptr_t kJumpOutTurnDampY = 0x006D211F;
constexpr uintptr_t kJumpOutTurnDampZ = 0x006D212B;
constexpr uintptr_t kJumpOutMoveDampX = 0x006D217D;
constexpr uintptr_t kJumpOutMoveDampY = 0x006D2189;
constexpr uintptr_t kJumpOutMoveDampZ = 0x006D2195;
// fmul dword ptr ds:[00858C20h]   (0.9)
constexpr std::array<uint8_t, 6> kExpectedJumpOutDamp{
    0xD8, 0x0D, 0x20, 0x8C, 0x85, 0x00
};

// `CTaskSimpleCarDrive::ProcessHeadBopping` at `0x6428C0` ramps the driver's
// head bop by 0.05 a frame, twenty frames from silent to full, and it drives
// how far the head actually moves. Cosmetic.
constexpr uintptr_t kHeadBopRampUp = 0x006429C6;
constexpr uintptr_t kHeadBopRampDown = 0x00642A02;
// fadd dword ptr ds:[00858C28h] / fsub dword ptr ds:[00858C28h]
constexpr std::array<uint8_t, 6> kExpectedHeadBopRampUp{
    0xD8, 0x05, 0x28, 0x8C, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedHeadBopRampDown{
    0xD8, 0x25, 0x28, 0x8C, 0x85, 0x00
};

// Attached entities. `CPhysical::PositionAttachedEntity` turns the distance an
// infinite-mass attached entity moved this frame into a move speed by dividing
// it by `max(1.0f, GetTimeStep())`, the same clamp the follow cameras use and
// with the same consequence: it never binds at or below 50 FPS and sticks at
// 1.0 above it, so the speed comes out short by the timestep ratio. That speed
// is then differenced against the previous one and the difference applied as a
// force to the attached entity and, negated, to whatever it hangs off, so the
// error feeds back into the carrier. Replaced by the reciprocal of the real
// timestep, which is what the original computes whenever the timestep is at
// least 1.0. `CTimer::Update` clamps the timestep to at least 0.00001, so the
// division cannot be by zero.
constexpr uintptr_t kAttachedEntitySpeed = 0x005477F4;
constexpr uintptr_t kAttachedEntitySpeedReturn = 0x00547823;
// The rate clamp, followed this time by the reciprocal and its store:
// fld 1.0 / fdiv st,st(1) / fstp [esp+0Ch] / fstp st(0)
constexpr std::array<uint8_t, 47> kExpectedAttachedSpeedClamp{
    0xD9, 0x05, 0x24, 0x86, 0x85, 0x00,
    0xD8, 0x1D, 0x5C, 0xCB, 0xB7, 0x00,
    0xDF, 0xE0,
    0xF6, 0xC4, 0x41,
    0x75, 0x08,
    0xD9, 0x05, 0x24, 0x86, 0x85, 0x00,
    0xEB, 0x06,
    0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00,
    0xD9, 0x05, 0x24, 0x86, 0x85, 0x00,
    0xD8, 0xF1,
    0xD9, 0x5C, 0x24, 0x0C,
    0xDD, 0xD8
};

// AI aircraft steering. The unnamed function at 0x423005, in the CCarCtrl block
// and operating on a CPlane or CHeli, drives an autopilot: it takes the heading
// to the target with fpatan, differences it against the angle it stored last
// frame in field_99C, and turns that difference into a rate by computing
// 30.0f / max(1.0f, GetTimeStep()). The result is a damping term in a control
// output that ends up clamped to [-1, 1] in field_990. This is another copy of
// the follow camera clamp, and it fails the same way: above 50 FPS the divisor
// sticks at 1.0 while the per-frame angle difference keeps shrinking, so the
// damping term fades out as the frame rate rises and the autopilot is left
// under-damped.
constexpr uintptr_t kAiAircraftSteerRate = 0x004235D2;
constexpr uintptr_t kAiAircraftSteerRateReturn = 0x004235F3;

// CPlane::ProcessFlyingCarStuff takes control away from a damaged plane: for
// each damaged part it multiplies one slewed control input, m_fLeftRightSkid,
// m_fSteeringUpDown or m_fSteeringLeftRight, by 1 - 0.2 * the part's damage
// status every frame, after CPlane::ProcessControlInputs has eased it towards
// the stick. Part 16 bleeds the skid, 17 and 18 the pitch, 19 and 20 the roll;
// a part counts when its node exists and its status, read with
// CDamageManager::GetAeroplaneCompStatus, is above zero. The factor is on the
// x87 stack when the input is read:
// fmul dword ptr [esi+988h] / [esi+98Ch] / [esi+990h]
constexpr uintptr_t kGetAeroplaneCompStatus = 0x006C2300;
constexpr size_t kAutomobileDamageManager = 0x5A0;
constexpr size_t kAutomobileNodes = 0x648;
constexpr uint32_t kPlaneSkidParts[] = {16};
constexpr uint32_t kPlanePitchParts[] = {17, 18};
constexpr uint32_t kPlaneRollParts[] = {19, 20};
constexpr uintptr_t kPlaneDamageSkidDecay = 0x006CBADC;
constexpr uintptr_t kPlaneDamagePitchDecay = 0x006CBBF5;
constexpr uintptr_t kPlaneDamageRollDecay = 0x006CBD1A;
constexpr std::array<uint8_t, 6> kExpectedPlaneDamageSkidDecay{
    0xD8, 0x8E, 0x88, 0x09, 0x00, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedPlaneDamagePitchDecay{
    0xD8, 0x8E, 0x8C, 0x09, 0x00, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedPlaneDamageRollDecay{
    0xD8, 0x8E, 0x90, 0x09, 0x00, 0x00
};

// CAutomobile::HydraulicControl keeps a hydraulic car's raised stance in
// m_wMiscComponentAngle: driving off sets it to 20 and steps it back up to 20
// once a frame, and standing still steps it down once a frame from at most 60
// to the idle stance at zero, so above 30 FPS a parked lowrider drops at once.
// inc ecx / mov [ebp+86Ch],cx
constexpr uintptr_t kHydraulicRaise = 0x006A0AF6;
constexpr std::array<uint8_t, 8> kExpectedHydraulicRaise{
    0x41, 0x66, 0x89, 0x8D, 0x6C, 0x08, 0x00, 0x00
};
// lea eax,[ecx-1] / test ax,ax
constexpr uintptr_t kHydraulicLower = 0x006A0B1E;
constexpr std::array<uint8_t, 6> kExpectedHydraulicLower{
    0x8D, 0x41, 0xFF, 0x66, 0x85, 0xC0
};

// CHeli::ProcessControl counts down each SWAT rope's state, m_aSwatState, once
// a frame and lays out the rope from it, so above 30 FPS the ropes are pulled
// in while the officers are still sliding down.
// dec al / mov [edi],al / mov eax,[esp+18h]
constexpr uintptr_t kSwatRopeCountdown = 0x006C782E;
constexpr std::array<uint8_t, 8> kExpectedSwatRopeCountdown{
    0xFE, 0xC8, 0x88, 0x07, 0x8B, 0x44, 0x24, 0x18
};

} // namespace hff
