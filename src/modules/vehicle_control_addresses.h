#pragma once

#include "modules/prelude.h"

// Sites of the vehicle control fixes: the yaw damping in
// `CPhysical::ApplyAirResistance` and the steering input step. The chain they
// belong to is mapped in docs/vehicle-control-reverse.md.

namespace hff {

// Turn air resistance. `CPhysical::ApplyAirResistance` (0x544C40) takes the
// vehicle branch for every entity of type vehicle and multiplies
// `m_vecMoveSpeed` by `pow(1 - |v| * m_fAirResistance, ms_fTimeStep)`, which
// is right, then `m_vecTurnSpeed` by a flat `0.99` from 0x862CD0 once per
// rendered frame, which is not: over a second that keeps `0.99^30` of the yaw
// rate at 30 FPS and `0.99^300` at 300, so a car sheds its rotation ten times
// faster and reads as heavy and understeering. The function is reached from
// `CPhysical::ProcessControl` by every vehicle class, and `CVehicle::
// FlyingControl` (0x6D85F0) already exponentiates its own resistances, so
// correcting the site for every class hands aircraft the full 30 FPS
// response, which players at a high frame rate have never flown with. The
// thunk therefore keys on the leaf class at +0x594 and the wheel contact
// count, and leaves anything else with the stock constant: aircraft, boats,
// trains and a car or bike in the air.
//
// The 36 byte span is the three `fld / fmul [0x862CD0] / fstp` triples. `esi`
// is `this` and the x87 stack is empty on entry.
constexpr uintptr_t kTurnAirResistance = 0x00544D29;
constexpr uintptr_t kTurnAirResistanceReturn = 0x00544D4D;
constexpr uintptr_t kTurnAirResistanceConstant = 0x00862CD0;
constexpr size_t kVehicleClass = 0x590;
constexpr size_t kAutomobileContactWheels = 0x960;
constexpr uint8_t kEntityTypeVehicle = 2;
constexpr int32_t kVehicleSubClassAutomobile = 0;
constexpr int32_t kVehicleSubClassMonsterTruck = 1;
constexpr int32_t kVehicleSubClassQuad = 2;
constexpr int32_t kVehicleSubClassBike = 9;
constexpr int32_t kVehicleSubClassBmx = 10;
constexpr int32_t kVehicleSubClassTrailer = 11;

// Steering input rate. `CAutomobile::ProcessControlInputs` (0x6AD690) and
// `CBike::ProcessControlInputs` (0x6BE310) slew the raw steer angle toward the
// pad with `raw += (target - raw) * ms_fTimeStep * 0.2`: an exponential
// approach written as one linear step, so the fraction of the gap closed per
// 1/30 s is `1/3` at 30 FPS but `1 - (1 - 0.2 * ts)^(n)` above it, converging
// to `1 - e^(-1/3)`; full lock arrives about 7% later at a high frame rate.
// Each site is the ten byte pair `fmul [esp+18h]` (a copy of the timestep made
// a few instructions earlier) and `fmul [0.2]` with `(target - raw)` alone on
// the x87 stack. The thunk multiplies by `1 - pow(1 - 0.2 * (50/30), ratio)`
// instead, which is `ts * 0.2` exactly at 30 FPS. The 0.2 is read from each
// site's own operand so a retuned constant keeps working.
constexpr uintptr_t kCarSteerInputA = 0x006AD864;
constexpr uintptr_t kCarSteerInputAReturn = 0x006AD86E;
constexpr uintptr_t kCarSteerInputB = 0x006AD925;
constexpr uintptr_t kCarSteerInputBReturn = 0x006AD92F;
constexpr uintptr_t kCarSteerInputConstant = 0x00871058;
constexpr uintptr_t kBikeSteerInputA = 0x006BE4A7;
constexpr uintptr_t kBikeSteerInputAReturn = 0x006BE4B1;
constexpr uintptr_t kBikeSteerInputB = 0x006BE5F8;
constexpr uintptr_t kBikeSteerInputBReturn = 0x006BE602;
constexpr uintptr_t kBikeSteerInputConstant = 0x00871300;

// Gear change inertia. `cTransmission::CalculateDriveAcceleration` (0x6D05E0)
// models engine inertia from how far the car moved through the current
// gear's speed band since the previous call:
//
//     inertia = ratio - previousRatio;           // per rendered frame
//     acceleration = clamp(1 - inertia * m_EngineInertia, 0.1, 1);
//     smoothed = acceleration * 0.15 + smoothed * 0.85;   // per frame
//     driveAcceleration *= smoothed;
//
// Both steps are per frame. At 30 FPS the per-frame change through the band
// is large enough for the inertia to bite and the smoother needs about ten
// frames, a third of a second, to follow a change. Above it the change per
// frame shrinks with the frame, so the inertia term vanishes and the
// smoother settles in a few milliseconds: an upshift, where the smoothed
// factor steps from its in-gear value back to 1, becomes a torque step
// sharp enough to pitch the nose up and lift the front wheels. Measured on a
// flat box: 1.2 degrees of pitch at 30 FPS, 5.7 to 8.2 degrees with the
// front wheels off the ground at 1000 FPS, same car, same speed.
//
// The first site is `fld st(0) / fsub [ebp] / jne` that forms the per-frame
// difference: the thunk divides it by the timestep ratio so it is the
// difference per original frame. The second is the 28 bytes of the
// smoother: the thunk raises the 0.85 (read from its operand) to the ratio.
// Both are exact at 30 FPS. `ebp` holds the previous ratio pointer and
// `[esp+28h]` the smoothed value pointer at both sites.
constexpr uintptr_t kTransmissionInertia = 0x006D087E;
constexpr uintptr_t kTransmissionInertiaReturn = 0x006D0885;
constexpr uintptr_t kTransmissionInertiaCheatSkip = 0x006D088D;
constexpr uintptr_t kTransmissionSmoother = 0x006D08E4;
constexpr uintptr_t kTransmissionSmootherReturn = 0x006D0900;
constexpr uintptr_t kTransmissionSmootherConstant = 0x008D347C;

// Gear change kick. `CAutomobile::ProcessControl` (0x6B24D6-0x6B268E) gives
// the player's car a nose-up torque, `ApplyTurnForce(-up * 0.008 *
// min(m_fTurnMass, 2500), -forward)`, with no timestep, on every frame the
// flag `bAudioChangingGear` is set while it accelerates: the squat you feel
// on an upshift. The flag is raised by the audio engine, not the gearbox:
// `CAEVehicleAudioEntity::JustFinishedAccelerationLoop` (0x4F5E50) counts
// frames since the acceleration loop last ended, and once LoopFrameCnt
// (0x8CBCC0, 10) frames have passed it declares the loop finished again as
// soon as the sound is within LoopInterval of its end. Ten frames is a
// third of a second at 30 FPS, long enough for the sound to move on, so an
// upshift kicks once. At 1000 FPS ten frames is nine milliseconds, the
// sound is still at its end, and the kick repeats every eleventh frame for
// as long as it stays there: seven to nine kicks of 21 degrees a second,
// front wheels off the ground, measured on a flat box at 100 km/h. The
// audio gear runs away with it, so the engine note jumps to top gear early.
//
// LoopFrameCnt is a plain int in .data read at both sites that use it, so
// the fix rewrites it each frame to the number of frames that make up ten
// original ones.
constexpr uintptr_t kAcLoopFrameCount = 0x008CBCC0;
constexpr int32_t kStockAcLoopFrameCount = 10;

// Suspension damping limit. `CPhysical::ApplySpringDampening` (0x543E90)
// removes `min(timeStep * dampingLevel, DAMPING_LIMIT_IN_FRAME)` of the
// contact speed along the spring every frame, with the limit 0.25 at
// 0x8CD7A0. At 30 FPS the timestep is 1.667, so any car whose handling
// damping level is above 0.15 (the Elegy has 0.20, so 0.333) hits the
// limit and gets 0.25 per frame, 7.5 per second. Above 30 FPS the product
// falls under the limit and the full level applies: 0.20 * 50 = 10 per
// second, a third more damping than the car ever had at 30 FPS.
//
// The entry is wrapped and `dampingLevel` rewritten to the level that leaves
// exactly the 30 FPS fraction after one original frame: the capped per-frame
// fraction is turned into its per-rendered-frame equivalent,
// 1 - (1 - a)^ratio, and divided back by the timestep. Exact at 30 FPS,
// where the function then applies the stock arithmetic unchanged.
constexpr uintptr_t kApplySpringDampening = 0x00543E90;
constexpr uintptr_t kDampingLimitInFrame = 0x008CD7A0;
constexpr float kStockDampingLimitInFrame = 0.25f;

// Suspension lean under load. The order of a car's frame is gravity and
// air resistance (`CPhysical::ProcessControl`), then the springs, then the
// dampers (`CAutomobile::ProcessSuspension`), then the tyre forces
// (`ProcessCarWheelPair`), then the move. The damper removes a fraction of
// the contact point's speed along the spring as it stands at that moment,
// which is the spring's reaction to the previous frame's tyre forces: the
// tyre impulse of this frame has not been added yet. In a steady corner
// the body therefore settles where the spring cancels the tyre torque
// only after the damper has taken its bite, at `spring = tyre / (1 - d)`
// with d the per-frame fraction. At 30 FPS d is 0.25 for any car with a
// damping level of 0.15 or more, so the body leans a third further under a
// cornering or braking load than the spring rate alone would let it; at
// 1000 FPS d is under 0.01 and the lean is the plain spring answer. The
// vertical axis is exempt: gravity is applied before the springs and the
// two cancel before the damper sees them, so ride height does not move.
// Measured on the Elegy in a handbrake slide: roll 2.25 degrees at 30 FPS,
// 1.90 at 1000, pitch 1.45 against 1.15; the tyre grip in this game is
// scaled by 1 - compression per wheel, so the smaller weight transfer
// leaves the yaw rate 3 to 7 percent short in every slide.
//
// The fix reproduces the bite. `CPhysical::ProcessControl` is wrapped to
// record the move and turn speeds at the start of the entity's frame, and
// the damper wrapper takes, before the stock damping, the 30 FPS fraction
// less the fraction the stock arithmetic will take itself, of the speed
// change the frame has made so far at that contact point, with the game's
// own no-reversal clamp and spring-force cap. At 30 FPS the two fractions
// are equal and nothing is added.
constexpr uintptr_t kPhysicalProcessControl = 0x005485E0;
constexpr uintptr_t kPhysicalApplyForce = 0x00542B50;
constexpr uintptr_t kDampingLimitOfSpringForce = 0x008CD7A4;
constexpr size_t kPhysicalTurnMass = 0x90;
constexpr size_t kPhysicalCentreOfMass = 0xA4;

// Wheel slip rate. `CVehicle::ProcessWheel` (0x6D6C00) weighs the slip it
// wants to cancel this frame against `adhesion`, which it has multiplied by
// the timestep at 0x6D6C7B: a per-frame budget. Under throttle the
// longitudinal term is `thrust`, which also carries the timestep, but the
// lateral term is a velocity, `-contactSpeedRight / wheelsOnGround`, and
// off the throttle the longitudinal term is one too. A velocity compared
// with a budget that shrinks with the frame: at 30 FPS a cornering tyre's
// lateral slip sits well inside the budget and is cancelled cleanly; at
// 1000 FPS the budget is a thirtieth as large, the same slip exceeds it
// every frame, the wheel is classified as skidding and its grip is cut by
// the handling's traction loss for the whole corner. Measured: in a powered
// full-lock turn both rear wheels of an Elegy spin at 1000 FPS where only
// the unloaded inner one does at 30, and the car rotates 15% faster.
//
// The two velocity terms are multiplied by the timestep ratio where they
// are formed, so they are the slip one original frame would have cancelled
// and compare with the budget in the same units. Everything after that is
// unchanged: the saturated branch rescales to the budget exactly as
// before, and the unsaturated branch applies the per-frame share of the
// cancel rather than all of it every frame. `thrust` is left alone. Both
// sites are six bytes ending in a conditional jump whose flags the thunk
// preserves.
constexpr uintptr_t kWheelSlipRight = 0x006D6D2C;         // fst [esp+18h] / jne
constexpr uintptr_t kWheelSlipRightDriving = 0x006D6D32;
constexpr uintptr_t kWheelSlipRightCoasting = 0x006D6DAB;
constexpr uintptr_t kWheelSlipCoast = 0x006D6E3B;         // fchs / fstp [esp+10h]
constexpr uintptr_t kWheelSlipCoastReturn = 0x006D6E41;

} // namespace hff
