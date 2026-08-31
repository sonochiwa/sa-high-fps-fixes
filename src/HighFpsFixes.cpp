#define NOMINMAX
#include <windows.h>
#include <share.h>
#include <intrin.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <string>

namespace {

// ---------------------------------------------------------------------------
// GTA San Andreas 1.0 US (Hoodlum) addresses
// ---------------------------------------------------------------------------

constexpr uintptr_t kImageBase = 0x00400000;

// Engine globals.
constexpr uintptr_t kTimerTimeStep = 0x00B7CB5C;
constexpr uintptr_t kTimerTimeInMilliseconds = 0x00B7CB84;
constexpr uintptr_t kWorldPlayers = 0x00B7CD98;
constexpr uintptr_t kPads = 0x00B73458;
constexpr uintptr_t kCutsceneRunning = 0x00B5F851;
constexpr uintptr_t kCameraWideScreenOn = 0x00B6F065;
constexpr uintptr_t kGameCurrentArea = 0x00B72914;
constexpr uintptr_t kFrameLimit = 0x00C1704C;
constexpr uintptr_t kWheelFriction = 0x00C2B9CC;
constexpr uintptr_t kAimingRifleWalkConstant = 0x00858CA8;
constexpr uintptr_t kSkimmerResistanceConstant = 0x00871DDC;
constexpr uintptr_t kBurnoutConstant = 0x00859A94;
constexpr uintptr_t kTurnAirResistanceConstant = 0x00862CD0;
constexpr uintptr_t kHeliRotorSpeedOperand = 0x006C4EFE;
constexpr uintptr_t kDoorApplyRateChassis = 0x00872328;
constexpr float kStockDoorApplyRateChassis = 0.025f;

constexpr size_t kPlayerInfoSize = 0x190;
constexpr size_t kPadSize = 0x134;
constexpr size_t kVehicleDriverOffset = 0x460;
constexpr size_t kRunningScriptNameOffset = 0x08;
constexpr size_t kRunningScriptNameSize = 8;

// Engine functions.
constexpr uintptr_t kPadGetHorn = 0x0053FEE0;
constexpr uintptr_t kPadHornJustDown = 0x0053FF30;

// Stunt jump camera.
constexpr uintptr_t kEndTimerCall = 0x0049C505;
constexpr uintptr_t kFlightTimerCall = 0x0049C6FB;

// Weapons.
constexpr uintptr_t kFxCreateParticles = 0x004A41E0;
constexpr uintptr_t kContinuousAmmoPatch = 0x007428A8;
constexpr uintptr_t kContinuousAmmoConsume = 0x007428AD;
constexpr uintptr_t kContinuousAmmoSkip = 0x007428E9;
// Chainsaw strike rate. `CTaskSimpleFight::ProcessPed` keeps the player's held
// chainsaw cutting by rewinding the moving-attack animation to `hit - 0.01`
// every time it passes `chain`, and the strike itself fires on the frame the
// animation crosses `hit`. `melee.dat` gives the chainsaw's AMOVING entry
// `hit 1.0` and `chain 1.1`, which the loader scales by 1/30 into 0.0333 s and
// 0.0367 s, so the whole loop is shorter than a single frame at 30 FPS and its
// length is decided by the frame quantisation rather than by the animation:
// the rewind and the strike cannot happen on the same frame, so the loop costs
// a near constant two to four frames whatever the frame rate. That is fifteen
// strikes a second at 30 FPS and three times as many at 144 FPS, against peds
// and vehicles alike. The patched instruction is the `fsub` that subtracts the
// 0.01: `0x858C58` holds a shared `0.01` that a few hundred other sites read,
// so the constant itself must not be touched.
constexpr uintptr_t kChainsawStrikeRewind = 0x00629F83;
// The one call to `CTaskSimpleFight::FightStrike`, i.e. the moment a melee
// attack looks for something to damage. Only hooked to count strikes when
// `traceChainsaw` is on.
constexpr uintptr_t kFightStrikeCall = 0x00629EED;
constexpr uintptr_t kFightStrike = 0x006240B0;

// Drowning damage. `CPlayerPed::HandlePlayerBreath` computes the per frame
// damage as the timestep times three and truncates it to an integer, so above
// roughly 150 FPS every frame rounds down to zero and the player never drowns.
// The patched span covers the multiply, the two argument pushes that the
// optimizer hoisted in front of the conversion, and the `_ftol` call itself.
constexpr uintptr_t kDrowningDamage = 0x0060A92F;
constexpr uintptr_t kDrowningDamageReturn = 0x0060A93E;
constexpr uintptr_t kDrowningDamageScale = 0x00858B3C;

// Aim camera.
constexpr uintptr_t kProcessAimWeapon = 0x00521500;

// Player.
constexpr uintptr_t kAimingRifleWalkPatch = 0x0061E0CA;
constexpr uintptr_t kAimingRifleWalkReturn = 0x0061E0D0;
constexpr uintptr_t kPedPushCarPatch = 0x00549652;
constexpr uintptr_t kPedPushCarReturn = 0x0054965A;
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
// Compatibility probes for the sites used by Tweaker, Swim FPS Fix and
// Framerate Vigilante. Those fixes patch inside ProcessSwimmingResistance,
// while this plugin wraps its caller, so checking only kSwimResistanceCall
// would miss them and apply the same correction twice.
constexpr uintptr_t kSwimDiveScale = 0x0068A42B;
constexpr uintptr_t kSwimAscentBias = 0x0068A4CA;
constexpr uintptr_t kSwimVectorSetup = 0x0068A4FC;
constexpr uintptr_t kSwimVectorTransform = 0x0068A50E;
constexpr size_t kPedAnimMovingShift = 0x4D8;

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

// AI aircraft steering. The unnamed function at 0x423005, in the CCarCtrl block
// and operating on a CPlane or CHeli, drives an autopilot: it takes the heading
// to the target with fpatan, differences it against the angle it stored last
// frame in field_99C, and turns that difference into a rate by computing
// 30.0f / max(1.0f, GetTimeStep()). The result is a damping term in a control
// output that ends up clamped to [-1, 1] in field_990. This is the fourth
// byte-identical copy of the follow camera clamp, and it fails the same way: at
// or below 50 FPS the divisor is the real timestep, above it the divisor sticks
// at 1.0 while the per-frame angle difference keeps shrinking, so the damping
// term fades out as the frame rate rises and the autopilot is left
// under-damped.
constexpr uintptr_t kAiAircraftSteerRate = 0x004235D2;
constexpr uintptr_t kAiAircraftSteerRateReturn = 0x004235F3;

// Skill progress. Every counter that levels a stat through use is advanced by
// `CStats::UpdateStatsWhen*` with the same three instructions:
//
//     fld ms_fTimeStep / fmul 0.02 / fmul 1000.0 / call _ftol
//
// which is milliseconds of frame time, `1000 / FPS`, truncated to an integer.
// At 30 FPS that is 33 and almost nothing is lost. At 144 FPS it is 6.94
// truncated to 6, losing 13 per cent of every second; at 400 FPS 2.5 becomes 2
// and a fifth is gone; at 600 FPS 1.67 becomes 1 and two fifths are gone; above
// 1000 FPS the value truncates to zero and the counter stops advancing
// altogether. Several of the functions truncate a second time, when they store
// `oldTotal + milliseconds * rate` back into the counter, and that store loses
// its fraction every frame as well.
//
// Twenty one call sites, all of them `call _ftol`, are repointed at one wrapper
// that carries the discarded fraction into the next frame. The wrapper
// identifies the site by its return address, so each counter gets its own
// carry.
constexpr uintptr_t kFtol = 0x00821B40;

// Every site carries the fraction and does nothing else.
//
// There used to be a second mode that also multiplied by the square root of the
// timestep ratio, for the one site at `0x55C972`. It was built on the reading
// that the cycle skill counter takes `sqrt(rate * milliseconds)` per frame, on
// the strength of the `call 0x823820` two instructions above it, and that a
// square root of a per-frame quantity cannot sum to a frame-rate independent
// total. The reasoning is sound and the premise was false: whatever `0x823820`
// is, it is not a square root at these inputs. Measured in game on 2026-08-27,
// the value arriving at that site sums to exactly 1000 a second at 30 FPS and
// to the same 1000 a second at 800 FPS, so the site is already frame-rate
// independent once the truncation below it is carried, and the correction was
// pure damage: it cut the counter to a fifth and the skill stopped levelling.
//
// The mode field is gone rather than left at zero, so nobody rebuilds the same
// correction on the same disproven premise. The lesson is in the roadmap: a
// call to an unidentified maths routine is not evidence of what it computes.
struct StatTruncSite {
    uintptr_t address;
    uint8_t group;
};

// Which fix owns a site, so the three groups can be switched on separately
// even though they share one wrapper and one carry array.
constexpr uint8_t kTruncGroupStats = 0;
constexpr uint8_t kTruncGroupStunt = 1;
constexpr uint8_t kTruncGroupUpsideDown = 2;
constexpr uint8_t kTruncGroupTask = 3;
constexpr uint8_t kTruncGroupVehicle = 4;
constexpr uint8_t kTruncGroupIdleCam = 5;
constexpr uint8_t kTruncGroupHud = 6;
constexpr uint8_t kTruncGroupBurn = 7;
constexpr uint8_t kTruncGroupWorld = 8;

// The counter each site feeds is named so the next reader does not have to
// chase the global back through the disassembly.
constexpr std::array<StatTruncSite, 103> kStatTruncSites{{
    {0x0055C5C3, kTruncGroupStats},  // m_FatCounter, milliseconds
    {0x0055C64E, kTruncGroupStats},  // m_MaxHealthCounter
    {0x0055C6DE, kTruncGroupStats},  // m_SprintStaminaCounter
    {0x0055C76E, kTruncGroupStats},  // m_RunningCounter
    {0x0055C838, kTruncGroupStats},  // m_CycleStaminaCounter
    {0x0055C94B, kTruncGroupStats},  // m_CycleSkillCounter, milliseconds
    {0x0055C972, kTruncGroupStats},  // m_CycleSkillCounter, the increment
    {0x0055CA26, kTruncGroupStats},  // m_SwimStaminaCounter
    {0x0055CAA8, kTruncGroupStats},  // m_SwimUnderWaterCounter
    {0x0055CB91, kTruncGroupStats},  // m_DrivingCounter, milliseconds
    {0x0055CBB2, kTruncGroupStats},  // m_DrivingCounter, the store
    {0x0055CBD2, kTruncGroupStats},  // m_DrivingCounter, milliseconds, second branch
    {0x0055CBF3, kTruncGroupStats},  // m_DrivingCounter, the store, second branch
    {0x0055CCE4, kTruncGroupStats},  // m_FlyingCounter, milliseconds
    {0x0055CD05, kTruncGroupStats},  // m_FlyingCounter, the store
    {0x0055CD25, kTruncGroupStats},  // m_FlyingCounter, milliseconds, second branch
    {0x0055CD46, kTruncGroupStats},  // m_FlyingCounter, the store, second branch
    {0x0055CE33, kTruncGroupStats},  // m_BikeCounter, milliseconds
    {0x0055CE54, kTruncGroupStats},  // m_BikeCounter, the store
    {0x0055CE76, kTruncGroupStats},  // m_BikeCounter, milliseconds, second branch
    {0x0055CE97, kTruncGroupStats},   // m_BikeCounter, the store, second branch

    // Stunt counters in `CPlayerInfo::Process`. Each stunt keeps a millisecond
    // counter and a grace buffer that lets the stunt survive a brief
    // interruption, and both are advanced by the same truncated frame time.
    {0x0056F9E7, kTruncGroupStunt},        // m_nCarLess3WheelCounter += ms
    {0x0056FA7B, kTruncGroupStunt},        // m_nCarTwoWheelCounter += ms
    {0x0056FAC8, kTruncGroupStunt},        // two wheels, grace buffer decay
    {0x0056FBAA, kTruncGroupStunt},        // m_nCarTwoWheelCounter, branch 2
    {0x0056FBF7, kTruncGroupStunt},        // two wheels, decay, branch 2
    {0x0056FC2D, kTruncGroupStunt},        // two wheels, the buffer store
    {0x0056FC67, kTruncGroupStunt},        // two wheels, grace buffer refill
    {0x0056FE0D, kTruncGroupStunt},        // m_nBikeRearWheelCounter += ms
    {0x0056FE5A, kTruncGroupStunt},        // wheelie, grace buffer decay
    {0x0056FE90, kTruncGroupStunt},        // wheelie, the buffer store
    {0x0056FFA4, kTruncGroupStunt},        // wheelie, grace buffer refill
    {0x00570003, kTruncGroupStunt},        // m_nBikeFrontWheelCounter += ms
    {0x00570050, kTruncGroupStunt},        // stoppie, grace buffer decay

    // `CUpsideDownCarCheck::UpdateTimers` reads the frame time once and adds it
    // to the timer of every car it is watching that is currently on its roof.
    {0x004655F9, kTruncGroupUpsideDown},

    // Ped and player task timers. Each one is `counter += frame milliseconds`
    // with an else branch that resets the counter to zero, and each is compared
    // against a threshold in milliseconds a few instructions later.
    {0x0060D3AD, kTruncGroupTask},    // CPlayerPed::EvaluateTarget, +0x8A vs 1200 ms
    {0x0060D47C, kTruncGroupTask},    // CPlayerPed::EvaluateTarget, +0x88 vs 1200 ms
    {0x006299B6, kTruncGroupTask},    // CTaskSimpleStealthKill::ManageAnim, +0x12
    {0x006809A2, kTruncGroupTask},    // CTaskSimpleInAir::ProcessPed, +0x28
    {0x0068132B, kTruncGroupTask},    // CTaskSimpleClimb::ProcessPed, +0x28 vs 1000 ms
    {0x0068790A, kTruncGroupTask},    // PlayerControlFighter, +0x10

    // Vehicle timers of the same shape.
    {0x0041F28D, kTruncGroupVehicle}, // CCarCtrl::UpdateCarAI, +0x4DC
    {0x006D9702, kTruncGroupVehicle}, // CVehicle::FlyingControl, +0x9A0

    // How long the player has been idle before the camera starts drifting.
    {0x0050A22F, kTruncGroupIdleCam},  // CIdleCam::ProcessIdleCamTicker, +0x94

    // Every timer behind the HUD's timed text and bars. All 46 are the same
    // accumulation of the truncated frame time, some into a global, some into
    // a register that is stored a few instructions later, some negated first
    // because they count down. None of them compares rather than accumulates,
    // which is why the whole block can be carried without picking through it
    // site by site.
    {0x0058AB42, kTruncGroupHud},
    {0x0058ABC4, kTruncGroupHud},
    {0x0058AC26, kTruncGroupHud},
    {0x0058ACFA, kTruncGroupHud},
    {0x0058AF5F, kTruncGroupHud},
    {0x0058AFAD, kTruncGroupHud},
    {0x0058AFD4, kTruncGroupHud},
    {0x0058B05F, kTruncGroupHud},
    {0x0058B894, kTruncGroupHud},
    {0x0058B8E6, kTruncGroupHud},
    {0x0058B938, kTruncGroupHud},
    {0x0058BA22, kTruncGroupHud},
    {0x0058C938, kTruncGroupHud},
    {0x0058C9C4, kTruncGroupHud},
    {0x0058CB5C, kTruncGroupHud},
    {0x0058CFCD, kTruncGroupHud},
    {0x0058D0B5, kTruncGroupHud},
    {0x0058D31B, kTruncGroupHud},
    {0x0058D388, kTruncGroupHud},
    {0x0058D655, kTruncGroupHud},
    {0x0058D6B1, kTruncGroupHud},
    {0x0058D713, kTruncGroupHud},
    {0x0058DA32, kTruncGroupHud},
    {0x0058DA7A, kTruncGroupHud},
    {0x0058DAF0, kTruncGroupHud},
    {0x0058DB8F, kTruncGroupHud},
    {0x0058DBC5, kTruncGroupHud},
    {0x0058DC3B, kTruncGroupHud},
    {0x0058ECB2, kTruncGroupHud},
    {0x0058ECF8, kTruncGroupHud},
    {0x0058ED52, kTruncGroupHud},
    {0x0058ED96, kTruncGroupHud},
    {0x0058EDCA, kTruncGroupHud},
    {0x0058EE24, kTruncGroupHud},
    {0x0058F237, kTruncGroupHud},
    {0x0058F293, kTruncGroupHud},
    {0x0058F2F5, kTruncGroupHud},
    {0x0058F38A, kTruncGroupHud},
    {0x0058F3D4, kTruncGroupHud},
    {0x0058F436, kTruncGroupHud},
    {0x0058F69F, kTruncGroupHud},
    {0x0058F6E7, kTruncGroupHud},
    {0x0058F75D, kTruncGroupHud},
    {0x0058F7FD, kTruncGroupHud},
    {0x0058F833, kTruncGroupHud},
    {0x0058F8A9, kTruncGroupHud},

    // Vehicle burn timers. Each adds the frame time to a float counter and
    // compares it against the threshold at 0x86CD78, which is how long a
    // burning vehicle has before it explodes. The first of the three car sites
    // scales by 0.2 first, so that state burns down more slowly.
    {0x006A7138, kTruncGroupBurn},   // CAutomobile::ProcessCarOnFireAndExplode, +0x8E4
    {0x006A7250, kTruncGroupBurn},   // the same counter, scaled by 0.2
    {0x006A7281, kTruncGroupBurn},   // the same counter, third branch
    {0x006BBD16, kTruncGroupBurn},   // CBike::ProcessControl, +0x7BC
    {0x006F1E9A, kTruncGroupBurn},   // CBoat::ProcessControl, +0x608

    // More ped task timers of the shape already covered by taskTimers.
    {0x00624C06, kTruncGroupTask},   // CTaskSimpleFightingControl::CalcMoveCommand, +0x1C
    {0x006298AA, kTruncGroupTask},   // CTaskSimpleStealthKill::ManageAnim, +0x1C vs 10000 ms
    {0x0062D855, kTruncGroupTask},   // CTaskComplexKillPedOnFootArmed, +0x16 counting down
    {0x00644595, kTruncGroupTask},   // CTaskSimpleCarDrive::ProcessPed, +0x4C vs 2000 ms
    {0x006944DA, kTruncGroupTask},   // CTaskSimpleDuck::ProcessPed, +0x0E from +0x0C

    // One more vehicle timer.
    {0x006C938F, kTruncGroupVehicle}, // CPlane::ProcessControl, +0x9FC

    // Gang war countdown, both branches of the same global at 0x96AB44.
    {0x00446BB4, kTruncGroupWorld},
    {0x00446CDC, kTruncGroupWorld}


}};

// HUD money counter. `CPlayerInfo::Process` walks `m_nDisplayMoney` toward
// `m_nMoney` by a fixed step chosen from how far apart they are, 12345, 1234,
// 123, 42 or 1, and applies it once per rendered frame with no timestep. At a
// high frame rate the counter runs through the difference that many times
// faster. The store is hooked rather than the step, so the size bands and their
// thresholds are the game's own.
constexpr uintptr_t kMoneyStepStore = 0x00570155;
constexpr uintptr_t kMoneyStepReturn = 0x0057015B;
constexpr size_t kPlayerInfoDisplayMoney = 0xBC;

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

// Vehicles.
constexpr uintptr_t kWheelFrictionCarDriveReturn = 0x006D6E6F;
constexpr uintptr_t kWheelFrictionCarBrakeReturn = 0x006D6EAE;
constexpr uintptr_t kWheelFrictionBikeBaseReturn = 0x006D7685;
constexpr uintptr_t kWheelFrictionBikeDriveReturn = 0x006D76B1;
constexpr uintptr_t kWheelFrictionBikeBrakeReturn = 0x006D76D3;
constexpr uintptr_t kPhysicalProcessCollision = 0x0054DFB0;
constexpr uintptr_t kPhysicalProcessShift = 0x0054DB10;
constexpr uintptr_t kEntityUpdateRwMatrix = 0x00446F90;
constexpr uintptr_t kEntityUpdateRwFrame = 0x00532B00;
constexpr uintptr_t kRailWheelSpinReturn0 = 0x006B5245;
constexpr uintptr_t kRailWheelSpinReturn1 = 0x006B5255;
constexpr uintptr_t kRailWheelSpinReturn2 = 0x006B5263;
constexpr uintptr_t kRailWheelSpinReturn3 = 0x006B526F;
constexpr uintptr_t kBurnoutPatch = 0x006A4FE6;
constexpr uintptr_t kBurnoutReturn = 0x006A4FEC;
constexpr uintptr_t kSkimmerResistancePatch = 0x006D2771;
constexpr uintptr_t kSkimmerResistanceReturn = 0x006D2777;
constexpr uintptr_t kHeliRotorSlowReturn = 0x006C4F2F;
constexpr uintptr_t kHeliRotorFastReturn = 0x006C4F3D;
constexpr uintptr_t kSirenPatch = 0x006E0961;
constexpr uintptr_t kSirenAnchor = 0x006E0999;
// The 1.0 US executable uses this trampoline to load the stock horn-history
// index before continuing at 0x006E0968. Network and NPC vehicles must retain
// that path because SA-MP writes their synchronized horn/siren state there.
constexpr uintptr_t kSirenOriginalReturn = 0x00403940;
constexpr uintptr_t kSirenToggleReturn = 0x006E0999;
constexpr uintptr_t kSirenHornReturn = 0x006E09E8;
constexpr uintptr_t kSirenNoHornReturn = 0x006E09F7;

// Diagnostics. Offsets used only by the optional vehicle state trace.
constexpr size_t kEntityMatrix = 0x14;
constexpr size_t kEntityFlags = 0x1C;
constexpr size_t kEntityTypeAndStatus = 0x36;
constexpr size_t kMatrixPosition = 0x30;
constexpr size_t kPhysicalMoveSpeed = 0x44;
constexpr size_t kPhysicalTurnSpeed = 0x50;
constexpr size_t kPhysicalForce = 0x74;
constexpr size_t kPhysicalTorque = 0x80;
constexpr size_t kPhysicalMass = 0x8C;
constexpr size_t kPhysicalAirResistance = 0x98;
constexpr size_t kPedVehicle = 0x58C;
constexpr size_t kPedHealth = 0x540;
constexpr size_t kPedArmour = 0x548;
constexpr size_t kVehicleSubClass = 0x594;
constexpr size_t kBikeContactWheels = 0x804;
constexpr size_t kPhysicalFlags = 0x40;
constexpr size_t kPhysicalLastCollisionTime = 0x3C;
constexpr size_t kPhysicalFrictionMoveSpeed = 0x5C;
constexpr size_t kPhysicalAttachedTo = 0xFC;
constexpr size_t kBikeBarSteerAngle = 0x644;
constexpr size_t kBikeLeanAngle = 0x648;
constexpr size_t kBikeDesiredLeanAngle = 0x64C;
constexpr size_t kBmxSprintLeanAngle = 0x81C;
constexpr size_t kVehicleBrakePedal = 0x498;
constexpr size_t kVehicleGasPedal = 0x49C;
constexpr size_t kBikeWheelAngularVelocity = 0x758;
constexpr size_t kBikeWheelRatios = 0x710;
constexpr size_t kBikeWheelContactTimers = 0x730;
constexpr uintptr_t kBikeProcessControl = 0x006B9250;
constexpr uintptr_t kPhysicalApplyGravity = 0x00542FE0;
// Diagnostic-only sites in CBike::ProcessControl. The first resumes after the
// two x87 instructions that start the balance correction; the second is the
// final balance ApplyTurnForce call, not one of the optional bikeTurnForce
// experiment sites.
constexpr uintptr_t kBikeBalanceInput = 0x006B9605;
constexpr uintptr_t kBikeBalanceInputReturn = 0x006B960D;
constexpr uintptr_t kBikeBalanceForceCall = 0x006B97A1;
// Wheel-contact call used to isolate the backward pitch acquired on a ramp.
// The narrowed experiment below now acts only during the short interval where
// the front suspension is clear and the bike is climbing off a ramp. This
// includes the rear-wheel takeoff phase and the short stale-contact tail.
constexpr uintptr_t kBikeWheelTurnForceCall = 0x006D7B17;
constexpr size_t kMatrixRight = 0x00;
constexpr size_t kMatrixUp = 0x10;
// The three writers of CRideAnimData::LeanAngle, each `fstp [esi+0x648]`.
constexpr uintptr_t kLeanWriteSmoother = 0x006B9C2F;  // CBike::ProcessControl
constexpr uintptr_t kLeanWriteSmootherReturn = 0x006B9C35;
constexpr uintptr_t kLeanWriteBike = 0x006BC681;      // CBike, later branch
constexpr uintptr_t kLeanWriteBikeReturn = 0x006BC687;
constexpr uintptr_t kLeanWriteBmx = 0x006BFB2F;       // CBmx::ProcessControl
constexpr uintptr_t kLeanWriteBmxReturn = 0x006BFB35;
constexpr uintptr_t kTimerTimeScale = 0x00B7CB64;
constexpr uint32_t kDiagnosticIntervalMs = 20;
constexpr uint32_t kDiagnosticLineLimit = 6000;

// Rest threshold. `CPhysical::m_fMovingSpeed` at +0xD4 is how far the entity
// moved during the current frame, so it shrinks with the timestep, but it is
// compared against a fixed limit right before the at-rest branch.
constexpr size_t kPhysicalMovingSpeed = 0xD4;
constexpr uintptr_t kCarRestThreshold = 0x006B1C9C;
constexpr uintptr_t kCarRestThresholdReturn = 0x006B1CA2;
constexpr uintptr_t kBikeRestThreshold = 0x006B9955;
constexpr uintptr_t kBikeRestThresholdReturn = 0x006B995B;
constexpr uintptr_t kTrailerRestThreshold = 0x006F9B92;
constexpr uintptr_t kTrailerRestThresholdReturn = 0x006F9B98;

// Move speed snap. `CAutomobile::ProcessControl` and `CBike::ProcessControl`
// each compare all three components of `m_vecMoveSpeed` against a fixed 0.005
// and, if every one of them is under it, zero the move speed outright. Move
// speed is integrated per frame, so what accumulates between two frames shrinks
// with the timestep: gravity contributes `CTimer::GetTimeStep() * 0.008`, which
// is 0.0133 at 30 FPS but only 0.0008 at 500 FPS. Above about 80 FPS a single
// frame can no longer clear 0.005, so the speed is wiped as fast as it is built
// and the entity can never start moving again. That is what pins a bike in
// mid-air at the apex of a jump, where its speed passes through zero, and what
// stops a pushed car dead between shoves.
//
// Each site is a standalone six byte `fcomp dword ptr ds:[0x00858B4C]`, and the
// thunks read that same operand back so a mod that repoints it keeps working.
constexpr std::array<uintptr_t, 6> kMoveSpeedSnapSites{
    0x006B33F6, 0x006B340C, 0x006B3422,  // CAutomobile::ProcessControl, x/y/z
    0x006BC101, 0x006BC117, 0x006BC129   // CBike::ProcessControl, x/y/z
};
constexpr uintptr_t kMoveSpeedSnapCarXReturn = 0x006B33FC;
constexpr uintptr_t kMoveSpeedSnapCarYReturn = 0x006B3412;
constexpr uintptr_t kMoveSpeedSnapCarZReturn = 0x006B3428;
constexpr uintptr_t kMoveSpeedSnapBikeXReturn = 0x006BC107;
constexpr uintptr_t kMoveSpeedSnapBikeYReturn = 0x006BC11D;
constexpr uintptr_t kMoveSpeedSnapBikeZReturn = 0x006BC12F;

// Turn speed air resistance. `CPhysical::ApplyAirResistance` raises the linear
// drag to the power of the timestep and then damps all three components of
// `m_vecTurnSpeed` by a flat 0.99 per rendered frame, two instructions apart in
// the same function. Over one second that retains `0.99^30`, about 0.74, at
// 30 FPS but `0.99^500`, about 0.0066, at 500 FPS, so angular velocity is bled
// away roughly 112 times faster.
constexpr uintptr_t kTurnAirResistance = 0x00544D29;
constexpr uintptr_t kTurnAirResistanceReturn = 0x00544D4D;

// Ground friction budget. `CPhysical::ApplyFriction(float, CColPoint&)` limits
// how much tangential speed one contact may remove, to `fFriction`. The ped
// branch of the same function computes that limit as
// `CTimer::GetTimeStep() / m_fMass * fFriction`, but the vehicle branch uses
// `fFriction` unscaled, so it is a per-rendered-frame budget.
//
// The site is `fld [esp+0x68]; fchs; fstp [esp+0x68]`, reached by a jump, so the
// thunk sees the same `esp` and the same x87 stack depth.
constexpr uintptr_t kGroundFrictionClamp = 0x00545736;
constexpr uintptr_t kGroundFrictionClampReturn = 0x00545740;

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

// NOTE: off by default. Three ways of taming these were tried and all three put
// the bike on its side, so what is here is a switch for further work rather than
// a fix. See the entry in ROADMAP.md before touching it again.
//
// Bike turn forces. `CPhysical::ApplyTurnForce` changes the angular velocity
// directly, `m_vecTurnSpeed += cross(point, force) / m_fTurnMass`, with no
// timestep anywhere in it. Four call sites in the bike and wheel code hand it a
// force that is not scaled by time either, so the same impulse lands on every
// rendered frame and the angular velocity gains sixteen times more per second
// at 500 FPS than at 30.
//
// Measured per call, with the timestep ratio between the two runs at 16.7:
// the ground contact response contributes ten times more per call at 30 FPS,
// so it follows time and is left alone, while these four contribute the same
// per call at both rates. Standing still, that is what rocks the bike by half
// a degree at high FPS and not at all at 30.
//
// Each site is a five byte `call` to `0x542A50`. The force vector is the first
// argument, so at the moment of the call it sits at `[esp]`.
constexpr uintptr_t kApplyTurnForce = 0x00542A50;
constexpr uintptr_t kApplyMoveForce = 0x005429F0;

// Roll onto wheels. When a car is nearly stationary and resting on one side,
// `CAutomobile::ProcessSuspension` pushes it back onto its wheels:
//
//     force = GetUp() * dir * ROLL_ONTO_WHEELS_FORCE * m_fTurnMass
//     ApplyTurnForce(force, GetRight() * boundingBox.max.x)
//     ApplyMoveForce(-right * ROLL_ONTO_WHEELS_FORCE * m_fMass * dir)
//
// with no timestep anywhere. `ProcessSuspension` runs once per frame from
// `ProcessControl`, so the righting impulse is applied per frame rather than
// per unit of time and the assist gets stronger in proportion to the frame
// rate. Both force vectors are scaled by the timestep ratio, which is 1.0 at
// 30 FPS and leaves the original behaviour untouched there.
constexpr uintptr_t kRollOntoWheelsTurnForce = 0x006B0603;
constexpr uintptr_t kRollOntoWheelsMoveForce = 0x006B0677;

// Swinging doors, boots, bonnets, the lowrider chassis and the firetruck
// ladder. Framerate Vigilante ed60ae8 extends the old integration/damping fix
// to the two angular-force paths and the firetruck damping path. Every changed
// operation is scaled by `timeStep / (50 / 30)`, so the patch is an identity
// at the original 30 FPS timestep.
constexpr uintptr_t kDoorForceChassis = 0x006F42DB;
constexpr uintptr_t kDoorForceChassisReturn = 0x006F42E0;
constexpr uintptr_t kDoorForceOther = 0x006F437D;
constexpr uintptr_t kDoorForceOtherReturn = 0x006F4383;
constexpr uintptr_t kDoorDampingFiretruck = 0x006F43A1;
constexpr uintptr_t kDoorDampingFiretruckReturn = 0x006F43A7;
constexpr uintptr_t kDoorDampingOther = 0x006F43D8;
constexpr uintptr_t kDoorDampingOtherReturn = 0x006F43DE;
constexpr uintptr_t kDoorIntegration = 0x006F4422;
constexpr uintptr_t kDoorIntegrationReturn = 0x006F4427;

// Suspension damping limit. `CPhysical::ApplySpringDampening` computes the
// per-frame damping as `GetTimeStep() * m_fSuspensionDampingLevel` and then
// clamps it against the constant at 0x8CD7A0, which is 0.25. The clamp is a
// per-step stability guard, which is a reasonable thing to have, but it is
// measured in frames and so it binds at 30 FPS and stops binding as the frame
// rate rises.
//
// It is not academic. Handling damping levels sit around 0.06 to 0.12 for most
// cars, which never reaches the limit, but Infernus at 0.19 and Cheetah,
// Super GT, Elegy, Benson, Washington, the kart and the Wayfarer and Freeway
// bikes at 0.20 all compute 0.317 to 0.333 per frame at 30 FPS and are clipped
// to 0.25. At 500 FPS the same vehicles compute 0.019 to 0.020 and are not
// clipped at all, so their suspension damps about a third harder per second
// than the game intends.
//
// The limit is raised or lowered with the timestep ratio so the clamp binds in
// the same proportion at any frame rate. The address is referenced three times
// and all three are inside this one clamp, so nothing else sees the change.
constexpr uintptr_t kDampingLimitInFrame = 0x008CD7A0;
constexpr float kStockDampingLimitInFrame = 0.25f;

// Free wheel spin. In `CAutomobile::ProcessCarWheelPair`, a wheel that is not
// touching the ground has its speed changed once per frame with no timestep,
// and then, two instructions later, its rotation is integrated with one:
//
//     if (driveWheels && acceleration != 0.0f) {
//         if (acceleration > 0.0f) { if (speed <  1.0f) speed -= 0.1f;  }
//         else                     { if (speed > -1.0f) speed += 0.05f; }
//     } else {
//         speed *= 0.95f;
//     }
//     m_wheelRotation[i] += CTimer::GetTimeStep() * m_wheelSpeed[i];
//
// The integration is right and the three lines above it are wrong, which is the
// sibling asymmetry this project keeps finding. At a high frame rate an
// airborne drive wheel spins up to its limit almost instantly and a free wheel
// stops almost instantly. Six sites, three operations mirrored across the left
// and right wheel, all six byte for byte identical between the two.
//
// `burnout` also patches this function, at 0x6A4FE6, where it scales the 3000.0
// burnout speed constant. That is a different site and the two do not overlap.
constexpr uintptr_t kWheelSpinDecelLeft = 0x006A5DB2;
constexpr uintptr_t kWheelSpinDecelRight = 0x006A5F1D;
constexpr uintptr_t kWheelSpinAccelLeft = 0x006A5DF7;
constexpr uintptr_t kWheelSpinAccelRight = 0x006A5F62;
constexpr uintptr_t kWheelSpinDampLeft = 0x006A5E54;
constexpr uintptr_t kWheelSpinDampRight = 0x006A5FBF;

// fsub dword ptr ds:[00858B1Ch]   (0.1)
constexpr std::array<uint8_t, 6> kExpectedWheelSpinDecel{
    0xD8, 0x25, 0x1C, 0x8B, 0x85, 0x00
};
// fadd dword ptr ds:[00858C28h]   (0.05)
constexpr std::array<uint8_t, 6> kExpectedWheelSpinAccel{
    0xD8, 0x05, 0x28, 0x8C, 0x85, 0x00
};
// fmul dword ptr ds:[00858EF0h]   (0.95)
constexpr std::array<uint8_t, 6> kExpectedWheelSpinDamp{
    0xD8, 0x0D, 0xF0, 0x8E, 0x85, 0x00
};
constexpr uintptr_t kPow = 0x00822130;

// CBoat::ProcessControl at 0x6F1770 spins the propeller down once per frame
// with no timestep when the boat is not under player, remote or physics
// control:
//
//     } else if (m_EngineSpeed > 0.0f) {
//         m_EngineSpeed *= 0.95f;
//     }
//
// The three branches directly above it, which drive the same field while the
// boat is being controlled, all read
// m_EngineSpeed += (target - m_EngineSpeed) * CTimer::GetTimeStep() * rate,
// and the propeller angle a hundred bytes below integrates with the timestep
// as well. Only the coast down branch is bare, so an abandoned boat's
// propeller stops turning, and its engine note dies, far sooner at high frame
// rates.
//
// The instruction is byte for byte the same as the free wheel damping above,
// so this reuses that thunk unchanged.
constexpr uintptr_t kBoatEngineDamping = 0x006F1900;

// `CTaskSimpleSwim::ProcessSwimmingResistance` also drives the swim pitch, and
// the rate it pitches at decays once per frame with no timestep in three
// places:
//
//     m_fStateChanger *= 0.95f;                              // no timestep
//     m_fStateChanger += GetTimeStepInSeconds() / 10.0f;     // timestep
//     m_fRotationX    += GetTimeStep() * m_fStateChanger;    // timestep
//
// The build-up and the integration both carry a timestep and the move speed
// blend at the top of the same function uses `pow(0.9f, GetTimeStep())`, so the
// author knew the idiom; only the decay is bare. In the disassembly each decay
// sits two instructions above its own correct integration, which is as close as
// this asymmetry gets.
//
// The decay runs while the ped is under the surface, so at a high frame rate
// the pitch rate is killed almost as fast as it builds and the swim angle
// barely responds. All three sites are the same instruction as the free wheel
// damping and reuse that thunk.
//
// This does not overlap `swimmingMovement`, which wraps the one call to this
// function to scale `CPed::m_vecAnimMovingShiftLocal`. That field feeds the
// move speed blend and is not read by the pitch code, so the two fixes touch
// different quantities.
constexpr uintptr_t kSwimPitchDecayA = 0x0068A6BD;
constexpr uintptr_t kSwimPitchDecayB = 0x0068A735;
constexpr uintptr_t kSwimPitchDecayC = 0x0068A7C0;

// `CBmx::ProcessControl` at `0x6BFA30` decays the rider's sprint lean once per
// frame with no timestep when the sprint animation stops:
//
//     m_fSprintLeanAngle *= 0.95f;
//
// The angle itself is purely cosmetic body sway, but the decay is the only
// thing that returns it to neutral, so at a high frame rate it snaps back
// instead of easing. Same instruction as the free wheel damping.
constexpr uintptr_t kBmxSprintLeanDecay = 0x006BFB3B;

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
// does not. The rear wheel a page below, at `0x6BB01B`, carries the timestep as
// well, so the copy at `0x6BAC77` is the only one of the three that integrates
// per frame, and the free front wheel spins sixteen times as fast at 500 FPS as
// it does at 30.
//
// The `0.95` decay is per frame in both copies, and is the same instruction as
// the car free wheel damping that `wheelSpin` already fixes, so both are raised
// to the timestep. Both changes are identities at 30 FPS. The two copies do not
// agree with each other even at 30 FPS, since one carries a timestep of 1.667
// and the other does not, and nothing here tries to make them agree: each is
// pinned to what it did at 30 FPS.
//
// The two front-wheel sites below are cosmetic: they update the visible wheel
// pitch. The rear-wheel rate limiter is different: it controls the physical
// angular speed used by wheel contact. Its fixed -0.1 / +0.05 steps must be
// scaled into the current frame before the contact solver sees them.
constexpr uintptr_t kBikeWheelSpinDampA = 0x006BAC7D;
constexpr uintptr_t kBikeWheelPitchIntegrate = 0x006BAC89;
constexpr uintptr_t kBikeWheelSpinDampB = 0x006BB5A1;
constexpr uintptr_t kBikeRearWheelSpeedDecel = 0x006BAFF4;
constexpr uintptr_t kBikeRearWheelSpeedAccel = 0x006BB00F;

// fadd dword ptr [esi+00000750h]
constexpr std::array<uint8_t, 6> kExpectedBikeWheelPitchIntegrate{
    0xD8, 0x86, 0x50, 0x07, 0x00, 0x00
};

// Two blend factors ramp by a fixed step per frame and clamp at 0 and 1, so
// each takes a fixed number of frames rather than a fixed length of time to
// cross. Same shape in both, and the same fix: the step is scaled into the
// current frame, which is an identity at 30 FPS.
//
// `CTaskSimpleJetPack::DoJetPackEffect` at `0x67B7F0` ramps `m_FxKeyTime` by
// 0.1 toward 1 while the thrusters are firing and back toward 0 when they are
// not, and hands it to the jetpack particle system as its constant time. Ten
// frames is a third of a second at 30 FPS and twenty milliseconds at 500, so
// the flame snaps between its two states instead of blending. `gta-reversed`
// carries a TODO on this line asking for frame delta time, so the defect is
// known upstream and simply never fixed.
//
// `CTaskSimpleCarDrive::ProcessHeadBopping` at `0x6428C0` ramps the driver's
// head bop by 0.05, twenty frames from silent to full, and it drives how far
// the head actually moves.
//
// Both are cosmetic.
constexpr uintptr_t kJetPackFxRampUp = 0x0067B99C;
constexpr uintptr_t kJetPackFxRampDown = 0x0067B9C8;
constexpr uintptr_t kHeadBopRampUp = 0x006429C6;
constexpr uintptr_t kHeadBopRampDown = 0x00642A02;

// fadd dword ptr ds:[00858B1Ch] / fsub dword ptr ds:[00858B1Ch]
constexpr std::array<uint8_t, 6> kExpectedJetPackRampUp{
    0xD8, 0x05, 0x1C, 0x8B, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedJetPackRampDown{
    0xD8, 0x25, 0x1C, 0x8B, 0x85, 0x00
};

// fadd dword ptr ds:[00858C28h] / fsub dword ptr ds:[00858C28h]
constexpr std::array<uint8_t, 6> kExpectedHeadBopRampUp{
    0xD8, 0x05, 0x28, 0x8C, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedHeadBopRampDown{
    0xD8, 0x25, 0x28, 0x8C, 0x85, 0x00
};

// `CBmx::ProcessDrivingAnims` at `0x6BFB50` settles the rider's animated lean
// once the input has gone quiet:
//
//     m_RideAnimData.AnimLeanLeft *= 0.95f;
//     m_RideAnimData.AnimLeanFwd  *= 0.95f;
//
// in two branches, four instructions in all. Twenty bytes above the first pair
// the same function decays another field with `pow(rate, GetTimeStep())`, which
// is the engine's own idiom for a frame-rate correct decay, so the asymmetry is
// inside one function and about as plain as it gets. At 500 FPS the lean snaps
// to neutral instead of easing.
//
// The `0.95` lives in a writable global, so the thunks read it at run time
// rather than baking it in. The constant is already on the stack when the
// replaced `fmul` runs, which is why these thunks do not load a base of their
// own the way the free wheel damping does.
//
// Cosmetic, and the sibling of `bmxSprintLean` in the same class.
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

// `CVehicle::CanPedJumpOutCar` at `0x6D2030` damps both speeds once per call
// with no timestep, three components each:
//
//     m_vecTurnSpeed *= 0.9f;                     // no timestep, 0x6D2113..
//     if (moveSpeedSq / 100 > sq(ts) * sq(0.008)) {
//         m_vecMoveSpeed *= 0.9f;                 // no timestep, 0x6D217D..
//         return false;
//     }
//     fMoveMult = GetTimeStep() / sqrt(moveSpeedSq) * 0.016f;   // timestep
//     m_vecMoveSpeed *= max(0.0f, 1.0f - fMoveMult);
//
// The reversed source flags this as bug-prone for a different reason: it is a
// query that mutates the vehicle. The branch only runs on a slow vehicle the
// player is trying to bail out of, and the effect is that the car is brought to
// a halt harder the higher the frame rate. Both the comparison above it and the
// fallthrough below it are timestep-correct.
//
// This assumes the function is reached once per frame while the exit is being
// attempted, which is what the task that calls it does. If it were ever polled
// more than once in a frame the original would be wrong in the same way and by
// the same factor, so the fix does not make that case worse.
// Rendered wheel settle. Every `PreRender` that draws wheels keeps a visual
// wheel offset — `CAutomobile::m_fWheelPosition` at `+0x838` and its
// equivalents — separate from the suspension itself. A wheel that has to move
// up is snapped there in one step, and a wheel coming back down is eased with
// `position += (target - position) * 0.75` once per rendered frame, with no
// timestep. Three frames cover the move either way, so at 30 FPS the wheel
// settles over 100 ms and the bounce is something you can watch. At 150 FPS the
// same three frames are 20 ms: the wheel is at the top for a single frame and
// already back down, which is what riding a rail looks like. Cosmetic — this is
// the drawn wheel, not the suspension that moves the car.
constexpr uintptr_t kWheelSettleCarA = 0x006AAC2E;
constexpr uintptr_t kWheelSettleCarB = 0x006AACB3;
constexpr uintptr_t kWheelSettleCarC = 0x006AAD38;
constexpr uintptr_t kWheelSettleCarD = 0x006AADBD;
constexpr uintptr_t kWheelSettleBikeA = 0x006BD150;
constexpr uintptr_t kWheelSettleBikeB = 0x006BD1D0;
constexpr uintptr_t kWheelSettleBmxA = 0x006C08D0;
constexpr uintptr_t kWheelSettleBmxB = 0x006C0950;
constexpr uintptr_t kWheelSettleHeli = 0x006C559E;
constexpr uintptr_t kWheelSettlePlane = 0x006C95AE;
constexpr uintptr_t kWheelSettleConstant = 0x00858F34;

// Penetration push-out. `CPhysical::ProcessShiftSectorList` ends by adding a
// shift straight onto the entity's matrix position: the deepest collision point
// found this frame, along the averaged contact normal, times `0.75` at
// `0x8CD7D8` on one branch and `1.5` at `0x8CD7D4` on the other. Nothing in
// that product is a timestep, so the push-out happens once per rendered frame.
// A one-off impact does not show it, because the penetration is created by the
// same frame's movement and cancels out, but a vehicle riding a shape it
// overlaps by a fixed geometric depth — a rail, a kerb, a low wall — is pushed
// out by the same fraction of the same depth every frame. At 30 FPS that is a
// steady nudge; at 150 FPS it is five times the speed for as long as the
// overlap lasts, which throws the car off the rail instead of easing it over.
// Three sites per constant, one for each component of the shift vector.
constexpr uintptr_t kPushOutScaleA = 0x00546ACA;
constexpr uintptr_t kPushOutScaleB = 0x00546ADE;
constexpr uintptr_t kPushOutScaleC = 0x00546AEC;
constexpr uintptr_t kPushOutScaleD = 0x00546B8E;
constexpr uintptr_t kPushOutScaleE = 0x00546B9A;
constexpr uintptr_t kPushOutScaleF = 0x00546BA4;
constexpr uintptr_t kPushOutConstantMain = 0x008CD7D8;
constexpr uintptr_t kPushOutConstantAlt = 0x008CD7D4;

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

// fmul dword ptr ds:[00858F34h]   (0.75)
constexpr std::array<uint8_t, 6> kExpectedWheelSettle{
    0xD8, 0x0D, 0x34, 0x8F, 0x85, 0x00
};

// fmul dword ptr ds:[008CD7D8h]   (0.75)
constexpr std::array<uint8_t, 6> kExpectedPushOutMain{
    0xD8, 0x0D, 0xD8, 0xD7, 0x8C, 0x00
};
// fmul dword ptr ds:[008CD7D4h]   (1.5)
constexpr std::array<uint8_t, 6> kExpectedPushOutAlt{
    0xD8, 0x0D, 0xD4, 0xD7, 0x8C, 0x00
};

// Pause menu map zoom. The map input block gates every zoom and pan step behind
// a 20 ms tick:
//
//     panDelayPassed = GetTimeInMSPauseMode() - m_LastActionTime > 20;
//     ...
//     if (wheelUp || PgUp || shoulder) {
//         if (panDelayPassed) { m_fMapZoom += 7.0f; if (wheelUp) += 21.0f; }
//     }
//
// For a held key that is exactly right: the repeat rate is 50 Hz whatever the
// frame rate. The mouse wheel is not a held key. `isMouseWheelMovedUp` is a
// level flag rebuilt every frame from the DirectInput wheel delta, which is the
// movement since the previous poll, so one notch sets it for exactly one frame
// and it is clear on every other frame.
//
// At 30 FPS a frame is 33 ms, the tick has always passed, and every notch lands.
// At 60 FPS half of them are already lost. At 2000 FPS the flag is up for 0.5 ms
// out of every 20 ms tick, so roughly one notch in forty does anything and the
// zoom crawls.
//
// The fix lets a wheel notch through the gate regardless of the tick, and only a
// wheel notch: held keys and the shoulder buttons keep the 50 Hz repeat they
// were designed around, and panning is untouched. The bypass fires on the rising
// edge of the flag rather than its level, so one notch is always exactly one
// step even if a free-spinning wheel keeps the delta non-zero across several
// polls. At or below 30 FPS this is a no-op, because there the tick has already
// passed on every frame the flag is up.
//
// Site A is sampled once per frame from the map bounds block, which runs before
// either branch. Note it sits between a `test al,al` at 0x57741E and a `jge` at
// 0x57747D, so the thunk has to preserve the flags as well as the registers.
constexpr uintptr_t kMapWheelSample = 0x00577445;
constexpr uintptr_t kMapWheelSampleReturn = 0x0057744B;
constexpr uintptr_t kMapZoomInGate = 0x00577656;
constexpr uintptr_t kMapZoomInProceed = 0x0057765F;
constexpr uintptr_t kMapZoomInSkip = 0x00577918;
constexpr uintptr_t kMapZoomOutGate = 0x00577989;
constexpr uintptr_t kMapZoomOutProceed = 0x0057798E;
constexpr uintptr_t kMapZoomOutSkip = 0x005779FE;
constexpr uintptr_t kMouseWheelUpFlag = 0x00B7341B;
constexpr uintptr_t kMouseWheelDownFlag = 0x00B7341C;

// fld dword ptr ds:[008653F4h]
constexpr std::array<uint8_t, 6> kExpectedMapWheelSample{
    0xD9, 0x05, 0xF4, 0x53, 0x86, 0x00
};
// cmp eax,14h / jbe 00577918
constexpr std::array<uint8_t, 9> kExpectedMapZoomInGate{
    0x83, 0xF8, 0x14, 0x0F, 0x86, 0xB9, 0x02, 0x00, 0x00
};
// cmp eax,14h / jbe 005779FE
constexpr std::array<uint8_t, 5> kExpectedMapZoomOutGate{
    0x83, 0xF8, 0x14, 0x76, 0x70
};

// Fire spread. `CFire::ProcessFire` at 0x53A570 runs once per frame per active
// fire and grows the flame correctly:
//
//     m_Strength = min(3.0f, m_Strength + CTimer::GetTimeStep() / 500.0f);
//
// but decides three of its four events by drawing a random number every frame
// and testing it against a fixed modulus, with no timestep anywhere:
//
//     if (rand() % 32  == 0) { set nearby vehicles alight }        // 0x53A8CA
//     if (rand() % 4   == 0) { damage nearby objects }             // 0x53AA0A
//     if (rand() % 128 == 0) { start a new fire nearby }           // 0x53AABB
//     if (rand() % 16  == 0) { merge with a nearby weak fire }     // 0x53AC46
//
// A per-frame probability is a rate per frame, so the events happen as many
// times more often as there are frames. At 2000 FPS that is roughly 66 times
// the 30 FPS rate: a car parked next to a fire catches almost at once instead
// of after about a second, and fires propagate explosively rather than
// creeping.
//
// The 0.02 modulus is deliberately not patched. Its body is
// `obj.ObjectFireDamage(CTimer::GetTimeStep() * 8.0f, ...)`, so the damage per
// hit already shrinks with the frame while the number of hits grows with it.
// The two cancel and the damage per second is the same at any frame rate; only
// the sampling granularity changes. Patching it would halve object burn damage.
//
// Each patched site is gated on a carry accumulator rather than a scaled
// probability, so the original draw is evaluated at an effective 30 FPS and the
// per-evaluation odds are untouched. This is the same technique the `_ftol`
// truncation fixes use. At or below 30 FPS the ratio is at least 1 and every
// frame is evaluated, which is stock behaviour exactly.
constexpr uintptr_t kFireVehicleGate = 0x0053A8CA;
constexpr uintptr_t kFireVehicleResume = 0x0053A8D2;
constexpr uintptr_t kFireVehicleSkip = 0x0053AA05;
constexpr uintptr_t kFireSpreadGate = 0x0053AABB;
constexpr uintptr_t kFireSpreadResume = 0x0053AAC3;
constexpr uintptr_t kFireSpreadSkip = 0x0053AC22;
constexpr uintptr_t kFireMergeGate = 0x0053AC46;
constexpr uintptr_t kFireMergeResume = 0x0053AC4E;
constexpr uintptr_t kFireMergeSkip = 0x0053AD72;

// test al,1Fh / jne 0053AA05
constexpr std::array<uint8_t, 8> kExpectedFireVehicleGate{
    0xA8, 0x1F, 0x0F, 0x85, 0x33, 0x01, 0x00, 0x00
};
// test al,7Fh / jne 0053AC22
constexpr std::array<uint8_t, 8> kExpectedFireSpreadGate{
    0xA8, 0x7F, 0x0F, 0x85, 0x5F, 0x01, 0x00, 0x00
};
// test al,0Fh / jne 0053AD72
constexpr std::array<uint8_t, 8> kExpectedFireMergeGate{
    0xA8, 0x0F, 0x0F, 0x85, 0x24, 0x01, 0x00, 0x00
};

// Drunk driving steering delay. `CPad::Update` at 0x541C40 keeps a ten deep
// FIFO of steering samples and shifts it by one entry every frame:
//
//     for (i = 9; i >= 1; i--) SteeringLeftRightBuffer[i] = ...[i - 1];
//
// `CPad::GetSteeringLeftRight` writes the live input into entry 0 and returns
// entry `DrunkDrivingBufferUsed`, which a script sets when the player is drunk.
// So the delay is measured in frames. At 30 FPS a delay of 9 is 300 ms of lag
// on the wheel, which is the whole point of the effect. At 2000 FPS the same 9
// entries span 4.5 ms and drunk driving is simply not drunk.
//
// The fix spends one 30 FPS tick per shift from a carry, so the delay stays the
// same number of milliseconds at any frame rate. Entry 0 keeps being
// overwritten with the live input between shifts, which means the sample that
// gets pushed is the most recent one at the moment of the shift: a 30 Hz
// sampled delay line, exactly what the original is at 30 FPS.
//
// `CPad::Update` runs once per pad and both pads must shift on the same frames,
// so the decision is taken once per frame counter value and reused.
//
// The horn history buffer twenty bytes above, `bHornHistory[5]` indexed by a
// counter that wraps every 5 frames, looks like the same bug and is not one:
// nothing in the executable ever reads it back. The horn tap it was there to
// detect is already handled by `sirenTap`, which rewrote the consumer in
// `CVehicle::ProcessSirenAndHorn` in milliseconds.
constexpr uintptr_t kDrunkSteerShift = 0x00541D2D;
constexpr uintptr_t kDrunkSteerShiftResume = 0x00541D35;
constexpr uintptr_t kDrunkSteerShiftSkip = 0x00541D42;

// lea eax,[ebx+72h] / mov ecx,9
constexpr std::array<uint8_t, 8> kExpectedDrunkSteerShift{
    0x8D, 0x43, 0x72, 0xB9, 0x09, 0x00, 0x00, 0x00
};

// `CStats::UpdateFatAndMuscleStats` advances `m_FatCounter` in integer
// arithmetic that truncates a second time, below the `_ftol` that
// `skillProgress` already repaired. The counter takes
// `milliseconds * exerciseRate / 10`, and that divide is a
// `mul 0xCCCCCCCDh / shr edx,3` unsigned divide by ten which keeps no
// remainder. At 30 FPS the numerator is 33 * rate and the quotient is
// comfortably above one; at 500 FPS it is 2 * rate, so every exercise rate
// below five produces a quotient of zero on every single frame and the counter
// never moves at all. Fat then never burns off no matter how far the player
// runs, cycles or swims.
//
// The twenty nine bytes of integer arithmetic are replaced by one call that
// does the same divide in floating point and carries the remainder into the
// next frame. The exercise rate is the function's only argument and is read
// back off the caller's stack.
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

// fmul st,st(1) / fadd dword ptr [esi+14h]
constexpr std::array<uint8_t, 5> kExpectedDoorForceChassis{
    0xD8, 0xC9, 0xD8, 0x46, 0x14
};

// fadd dword ptr [esi+14h] / fstp dword ptr [esi+14h]
constexpr std::array<uint8_t, 6> kExpectedDoorForceOther{
    0xD8, 0x46, 0x14, 0xD9, 0x5E, 0x14
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

// Physics sleep counter. `CPhysical::m_nFakePhysics` at +0xB8 is incremented
// once per rendered frame and, above 10, the entity has its move and turn speed
// reset and its physics skipped for the frame.
constexpr size_t kFakePhysicsOffset = 0xB8;
constexpr uintptr_t kObjectFakePhysics = 0x005A241F;   // CObject::ProcessControl
constexpr uintptr_t kObjectFakePhysicsReturn = 0x005A2427;
constexpr uintptr_t kCarFakePhysics = 0x006B1D2A;      // CAutomobile::ProcessControl
constexpr uintptr_t kCarFakePhysicsReturn = 0x006B1D32;
constexpr uintptr_t kBikeFakePhysics = 0x006B9972;     // CBike::ProcessControl
constexpr uintptr_t kBikeFakePhysicsReturn = 0x006B997A;
constexpr uintptr_t kTrailerFakePhysics = 0x006F9BD1;  // CTrailer::ProcessControl
constexpr uintptr_t kTrailerFakePhysicsReturn = 0x006F9BD9;

// HUD flashing. Each address is the 4 byte absolute operand of the instruction
// that reads CTimer::m_FrameCounter to decide whether the element is hidden
// this frame.
constexpr uintptr_t kFrameCounter = 0x00B7CB4C;
constexpr uintptr_t kHudArmorBarOperand = 0x005890AF;   // CHud::RenderArmorBar
constexpr uintptr_t kHudBreathBarOperand = 0x0058919F;  // CHud::RenderBreathBar
constexpr uintptr_t kHudHealthBarOperand = 0x0058927E;  // CHud::RenderHealthBar
constexpr uintptr_t kHudRadarOperand = 0x0058A363;      // CHud::DrawRadar
constexpr uintptr_t kHudWantedActiveOperand = 0x0058DDBC; // CHud::DrawWanted
constexpr uintptr_t kHudWantedEmptyOperand = 0x0058DE69;  // CHud::DrawWanted

// Optional frame limiting.
// San Andreas gates its whole frame limiter behind the menu preference
// `CMenuManager::m_bPrefsFrameLimiter`. With it off the limiter code is jumped
// over and `RsGlobal.frameLimit` is never consulted, so writing a limit alone
// does nothing. Turning the branch into an unconditional jump runs the limiter
// without touching the saved preference.
constexpr uintptr_t kFrameLimiterGate = 0x00748D68;
constexpr uintptr_t kFrameLimitStore = 0x00619620;
constexpr uintptr_t kFrameLimitStoreOperand = 0x00619626;
constexpr uintptr_t kRefreshRateCompare = 0x0074612A;
constexpr uintptr_t kRefreshRateOperand = 0x0074612C;
constexpr uintptr_t kScriptsProcess = 0x0046A000;
constexpr uintptr_t kScriptsProcessReturn = 0x0046A005;
constexpr uintptr_t kScriptQueueOperand = 0x00468D76;
// CRunningScript::ProcessCommands800To899, opcode 034E (SLIDE_OBJECT).
// The three movement rates are ScriptParams[4..6].
constexpr uintptr_t kScriptSlideObject = 0x00482342;
constexpr uintptr_t kScriptSlideObjectReturn = 0x0048234D;
// CRunningScript::ProcessCommands800To899, opcode 034D (ROTATE_OBJECT).
// Its angular rate is ScriptParams[2].
constexpr uintptr_t kScriptRotateObject = 0x00481CAE;
constexpr uintptr_t kScriptRotateObjectReturn = 0x00481CBB;
constexpr uintptr_t kScriptParams = 0x00A43C78;
constexpr uintptr_t kFallingGlassMove = 0x0071AABF;
constexpr uintptr_t kFallingGlassMoveReturn = 0x0071AAC5;
constexpr uintptr_t kFallingGlassTurnA = 0x0071AAEA;
constexpr uintptr_t kFallingGlassTurnAReturn = 0x0071AAF0;
constexpr uintptr_t kFallingGlassTurnB = 0x0071AB29;
constexpr uintptr_t kFallingGlassTurnBReturn = 0x0071AB2F;
constexpr uintptr_t kBreakObjectLifetime = 0x0059E420;
constexpr uintptr_t kBreakObjectLifetimeReturn = 0x0059E42B;
constexpr uintptr_t kMenuBackground = 0x0057C324;
constexpr uintptr_t kMenuBackgroundTarget = 0x0057B750;

constexpr float kOriginalTimeStep = 50.0f / 30.0f;
constexpr float kOriginalWeaponConsumptionRate = 30.0f;
// The animation clock is `CTimer::ms_fTimeStep / 50`, i.e. seconds.
constexpr float kAnimSecondsPerTimeStep = 0.02f;
// The rewind the chainsaw loop performs in the stock game, and the strike
// period that rewind produces at 30 FPS: the animation crosses `hit` on the
// frame after the rewind and `chain` on the one after that, so a strike lands
// on every second frame.
constexpr float kChainsawStockRewind = 0.01f;
constexpr float kChainsawStrikePeriodMs = 2000.0f / 30.0f;
// A gap this long means the player let go and started sawing again, so the
// schedule restarts rather than paying back the whole idle time at once.
constexpr uint32_t kChainsawBurstGapMs = 300;
constexpr float kChainsawParkMargin = 0.001f;
constexpr float kHeliRotorSpeedDivisor = 220.0f;
constexpr uint32_t kSirenTapMilliseconds = 150;

// The HUD flashes on `frameCounter & 8`, i.e. every 8 frames, which is 320 ms
// on and 320 ms off at the 25 FPS the game was tuned for.
constexpr unsigned kHudTicksPerFlash = 8;
constexpr unsigned kDefaultHudFlashIntervalMs = 320;

// ---------------------------------------------------------------------------
// Original instruction bytes
// ---------------------------------------------------------------------------

constexpr std::array<uint8_t, 5> kExpectedEndTimerCall{
    0xE8, 0x36, 0x56, 0x38, 0x00
};
constexpr std::array<uint8_t, 5> kExpectedFlightTimerCall{
    0xE8, 0x40, 0x54, 0x38, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedFxCreateParticles{
    0x81, 0xEC, 0x8C, 0x00, 0x00, 0x00
};

// Direct particle adds. `FxSystem_c::AddParticle` at 0x4AA440 is the one
// function every hand-written particle spawn in the game funnels through: the
// 43 call sites in the reversed source cover exhaust smoke, tyre spray, boat
// wake, water cannon, sandstorm, plane damage trails, ped splashes, gun shells
// and breaking debris. Almost all of them sit in a per-frame `ProcessControl`
// and add a fixed number of particles with no timestep anywhere, so emission
// scales with the frame rate: `CVehicle::AddExhaustParticles` alone adds two to
// eight per frame for every car in the world, and `CVehicle::DoBoatSplashes`
// adds two. At 2000 FPS that is about 66 times the density the game was drawn
// for, and the particle budget goes with it.
//
// This is deliberately not the same mechanism as
// `continuousWeaponParticles`, which patches `FxEmitter_c::CreateParticles` at
// 0x4A41E0 — the emitter path, where a system converts elapsed time into a
// whole number of particles and drops the fraction. The two paths never meet:
// `CreateParticles` calls `FxEmitter_c::AddParticle`, not this function.
//
// The gate is per call site, keyed on the return address, which is finer than
// keying on the effect system: `m_SmokeII3expand` is shared by exhaust, fire,
// the water cannon and breaking objects, and those want different treatment.
// A site that was idle for at least one original 30 Hz frame is always let
// through, so an intermittent spawn — a shell casing, a hit spark, debris —
// never loses a particle. Shorter gaps remain part of the same stream. This is
// important for exhaust smoke: its random test often skips rendered frames at
// high FPS, but those sub-33-ms gaps do not make each following particle a new
// event. A stream is opened thirty times a second, which is what it emitted
// when the game was built. At 30 FPS or below every frame is open and nothing
// changes at all.
constexpr uintptr_t kFxAddParticle = 0x004AA440;

// sub esp,8 / push esi / push edi
constexpr std::array<uint8_t, 5> kExpectedFxAddParticle{
    0x83, 0xEC, 0x08, 0x56, 0x57
};
constexpr std::array<uint8_t, 5> kExpectedContinuousAmmo{
    0x8B, 0x46, 0x08, 0x85, 0xC0
};
// fsub dword ptr ds:[00858C58h]
constexpr std::array<uint8_t, 6> kExpectedChainsawStrikeRewind{
    0xD8, 0x25, 0x58, 0x8C, 0x85, 0x00
};
// call 006240B0
constexpr std::array<uint8_t, 5> kExpectedFightStrikeCall{
    0xE8, 0xBE, 0xA1, 0xFF, 0xFF
};
// fmul dword ptr ds:[00858B3Ch] / push 0 / push 3 / call _ftol
constexpr std::array<uint8_t, 15> kExpectedDrowningDamage{
    0xD8, 0x0D, 0x3C, 0x8B, 0x85, 0x00,
    0x6A, 0x00,
    0x6A, 0x03,
    0xE8, 0x02, 0x72, 0x21, 0x00
};
constexpr std::array<uint8_t, 5> kExpectedProcessAimWeapon{
    0xA0, 0x10, 0x01, 0xB7, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedWheelFriction{
    0xD9, 0x05, 0xCC, 0xB9, 0xC2, 0x00
};
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
// push esi / mov ecx,edi / call CTaskSimpleSwim::ProcessSwimmingResistance
constexpr std::array<uint8_t, 8> kExpectedSwimResistanceCall{
    0x56, 0x8B, 0xCF, 0xE8, 0x20, 0xED, 0xFF, 0xFF
};
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
// fld 1.0 / fcomp ms_fTimeStep / fnstsw / test / jne / fld 1.0 / jmp / fld ts
constexpr std::array<uint8_t, 33> kExpectedCameraRateClamp{
    0xD9, 0x05, 0x24, 0x86, 0x85, 0x00,
    0xD8, 0x1D, 0x5C, 0xCB, 0xB7, 0x00,
    0xDF, 0xE0,
    0xF6, 0xC4, 0x41,
    0x75, 0x08,
    0xD9, 0x05, 0x24, 0x86, 0x85, 0x00,
    0xEB, 0x06,
    0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00
};

// The same clamp again, followed this time by the reciprocal and its store:
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
// mov dword ptr [esi+0BCh],edx
constexpr std::array<uint8_t, 6> kExpectedMoneyStepStore{
    0x89, 0x96, 0xBC, 0x00, 0x00, 0x00
};
// add esp,0Ch / lea edx,[esp+48h] / push edx / call CVector::operator+=
constexpr std::array<uint8_t, 13> kExpectedClimbSpeedClamp{
    0x83, 0xC4, 0x0C,
    0x8D, 0x54, 0x24, 0x48,
    0x52,
    0xE8, 0xE1, 0x07, 0xD9, 0xFF
};
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
constexpr std::array<uint8_t, 6> kExpectedAimingRifleWalk{
    0xD8, 0x0D, 0xA8, 0x8C, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedSkimmerResistance{
    0xD8, 0x0D, 0xDC, 0x1D, 0x87, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedBurnout{
    0xD9, 0x05, 0x94, 0x9A, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedHeliRotorSlow{
    0xD8, 0x05, 0xDC, 0x8C, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedHeliRotorFast{
    0xD8, 0x05, 0xD8, 0x9C, 0x85, 0x00
};
constexpr std::array<uint8_t, 2> kExpectedHeliRotorOperand{0xD8, 0x1D};
constexpr std::array<uint8_t, 8> kExpectedPedPushCar{
    0x8B, 0x54, 0x24, 0x20, 0x8B, 0x44, 0x24, 0x24
};
constexpr std::array<uint8_t, 6> kExpectedRestThreshold{
    0xD9, 0x86, 0xD4, 0x00, 0x00, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedMoveSpeedSnap{
    0xD8, 0x1D, 0x4C, 0x8B, 0x85, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedBikeLeanTarget{
    0xD9, 0x5C, 0x24, 0x14, 0xDD, 0xD8
};
constexpr std::array<uint8_t, 10> kExpectedGroundFriction{
    0xD9, 0x44, 0x24, 0x68, 0xD9, 0xE0, 0xD9, 0x5C, 0x24, 0x68
};
constexpr std::array<uint8_t, 36> kExpectedTurnAirResistance{
    0xD9, 0x46, 0x50, 0xD8, 0x0D, 0xD0, 0x2C, 0x86, 0x00, 0xD9, 0x5E, 0x50,
    0xD9, 0x46, 0x54, 0xD8, 0x0D, 0xD0, 0x2C, 0x86, 0x00, 0xD9, 0x5E, 0x54,
    0xD9, 0x46, 0x58, 0xD8, 0x0D, 0xD0, 0x2C, 0x86, 0x00, 0xD9, 0x5E, 0x58
};
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
constexpr std::array<uint8_t, 5> kExpectedSiren{
    0x90, 0x90, 0xE9, 0xD8, 0x2F
};
constexpr std::array<uint8_t, 6> kExpectedSirenAnchor{
    0x8A, 0x86, 0x2D, 0x04, 0x00, 0x00
};
constexpr std::array<uint8_t, 5> kExpectedScriptsProcess{
    0xA0, 0x88, 0x30, 0xA4, 0x00
};
constexpr std::array<uint8_t, 11> kExpectedScriptSlideObject{
    0xA1, 0x78, 0x3C, 0xA4, 0x00,
    0x8B, 0x0D, 0x9C, 0x44, 0xB7, 0x00
};
constexpr std::array<uint8_t, 13> kExpectedScriptRotateObject{
    0x8B, 0x0D, 0x78, 0x3C, 0xA4, 0x00,
    0x51,
    0x8B, 0x0D, 0x9C, 0x44, 0xB7, 0x00
};
constexpr std::array<uint8_t, 6> kExpectedFallingGlassMove{
    0xD9, 0x44, 0x24, 0x20, 0xD8, 0x06
};
constexpr std::array<uint8_t, 6> kExpectedFallingGlassTurnA{
    0x8B, 0x08, 0x89, 0x4C, 0x24, 0x2C
};
constexpr std::array<uint8_t, 6> kExpectedFallingGlassTurnB{
    0x8B, 0x10, 0x89, 0x54, 0x24, 0x38
};
constexpr std::array<uint8_t, 11> kExpectedBreakObjectLifetime{
    0x8B, 0x54, 0x07, 0x70,
    0x8D, 0x44, 0x07, 0x70,
    0x4A,
    0x89, 0x10
};
constexpr std::array<uint8_t, 5> kExpectedMenuBackground{
    0xE9, 0x27, 0xF4, 0xFF, 0xFF
};
constexpr std::array<uint8_t, 2> kExpectedFrameLimiterGate{0x75, 0x17};
constexpr std::array<uint8_t, 10> kExpectedFrameLimitStore{
    0xC7, 0x05, 0x4C, 0x70, 0xC1, 0x00, 0x1E, 0x00, 0x00, 0x00
};
constexpr std::array<uint8_t, 3> kExpectedRefreshRate{0x83, 0xF8, 0x3C};

constexpr std::array<std::array<uint8_t, 6>, 4> kExpectedRailWheelSpin{{
    {0xD8, 0x86, 0x28, 0x08, 0x00, 0x00},
    {0xD8, 0x86, 0x2C, 0x08, 0x00, 0x00},
    {0xD8, 0x86, 0x30, 0x08, 0x00, 0x00},
    {0xD8, 0x86, 0x34, 0x08, 0x00, 0x00},
}};

constexpr char kDefaultIni[] =
    "# High FPS Fixes v0.9.2\n"
    "# Created by sonochiwa\n"
    "# Source code: https://github.com/sonochiwa/sa-high-fps-fixes\n"
    "\n"
    "[camera]\n"
    "stuntJumpCamera=1\n"
    "aimCameraShake=1\n"
    "followCameraRate=1\n"
    "idleCameraTimer=1\n"
    "\n"
    "[player]\n"
    "aimingRifleWalk=1\n"
    "swimmingMovement=1\n"
    "swimPitchRate=1\n"
    "pedPushVehicle=1\n"
    "drowningDamage=1\n"
    "drunkSteerDelay=1\n"
    "jetPackFlame=1\n"
    "fatCounter=1\n"
    "waterBuoyancy=1\n"
    "climbSpeed=1\n"
    "skillProgress=1\n"
    "stuntCounters=1\n"
    "taskTimers=1\n"
    "\n"
    "[vehicles]\n"
    "bikeLeanTarget=1\n"
    "bikePitchExperiment=0\n"
    "bikePitchExperimentStrength=50\n"
    "groundFriction=1\n"
    "turnAirResistance=1\n"
    "moveSpeedSnap=1\n"
    "restThreshold=1\n"
    "physicsSleepRate=1\n"
    "wheelFriction=1\n"
    "abandonedBikePhysicsStep=0\n"
    "railWheelSpin=1\n"
    "burnout=1\n"
    "disableSwingingCompletely=0\n"
    "sirenTap=1\n"
    "heliRotorSpeed=1\n"
    "skimmerResistance=1\n"
    "attachedEntitySpeed=1\n"
    "aiAircraftSteer=1\n"
    "upsideDownTimer=1\n"
    "vehicleTimers=1\n"
    "burnTimers=1\n"
    "rollOntoWheels=1\n"
    "suspensionDampingLimit=1\n"
    "collisionPushOut=1\n"
    "wheelSettle=1\n"
    "wheelSpin=1\n"
    "boatEngineSpeed=1\n"
    "bmxSprintLean=1\n"
    "bmxLeanSettle=1\n"
    "bikeWheelSpin=1\n"
    "headBopping=1\n"
    "jumpOutCarSpeed=1\n"
    "doorSwing=1\n"
    "\n"
    "[weapons]\n"
    "continuousWeaponParticles=1\n"
    "continuousWeaponAmmo=1\n"
    "chainsawStrikeRate=1\n"
    "\n"
    "[particles]\n"
    "emissionRate=1\n"
    "\n"
    "[hud]\n"
    "hudTiming=1\n"
    "disableFlashing=0\n"
    "\n"
    "[world]\n"
    "gangWarTimer=1\n"
    "fireSpread=1\n"
    "scriptObjectSlide=1\n"
    "scriptObjectRotate=1\n"
    "fallingGlass=1\n"
    "breakableObjectLifetime=1\n"
    "\n"
    "[menu]\n"
    "mapZoomWheel=1\n"
    "\n"
    "[framerate]\n"
    "fpsLimit=0\n"
    "refreshRate=0\n"
    "\n"
    "[autoLimitFps]\n"
    "forMissions=0\n"
    "forMinigames=0\n"
    "forSchools=0\n"
    "forCutscenes=0\n"
    "forScriptedCutscenes=0\n"
    "forPauseMenu=0\n";

// ---------------------------------------------------------------------------
// Patch bookkeeping
// ---------------------------------------------------------------------------

struct SitePatch {
    uintptr_t address{};
    std::array<uint8_t, 48> original{};
    size_t size{};
    bool installed{};
};

struct BytePatch {
    uintptr_t address{};
    uint8_t original{};
    bool installed{};
};

struct DetourPatch {
    uintptr_t address{};
    std::array<uint8_t, 16> original{};
    size_t size{};
    void* gateway{};
    bool installed{};
};

struct AbsoluteOperandPatch {
    uintptr_t instruction{};
    std::array<uint8_t, 6> expected{};
    bool installed{};
};

union AutoLimitFlags {
    uint32_t value;
    struct {
        uint32_t forMissions : 1;
        uint32_t forMinigames : 1;
        uint32_t forSchools : 1;
        uint32_t forCutscenes : 1;
        uint32_t forScriptedCutscenes : 1;
        uint32_t forPauseMenu : 1;
    } flags;
};

HMODULE g_module{};

SitePatch g_endTimerPatch{};
SitePatch g_flightTimerPatch{};
SitePatch g_continuousAmmoPatch{};
SitePatch g_chainsawStrikePatch{};
SitePatch g_fightStrikeTracePatch{};
float g_chainsawRewindOffset{kChainsawStockRewind};
void* g_chainsawAnim{};
uint32_t g_chainsawLastCall{};
float g_chainsawCredit{};
bool g_traceChainsaw{};
uint64_t g_chainsawTraceLast{};
uint32_t g_chainsawCalls{};
uint32_t g_chainsawArms{};
uint32_t g_chainsawStrikes{};
int32_t g_chainsawCombo{-1};
int32_t g_chainsawMove{-1};
int32_t g_chainsawStrikeCombo{-1};
int32_t g_chainsawStrikeMove{-1};
float g_chainsawAnimStep{};
float g_chainsawAnimTime{};
SitePatch g_drowningDamagePatch{};
SitePatch g_aimingRifleWalkPatch{};
SitePatch g_pedPushCarPatch{};
SitePatch g_skimmerResistancePatch{};
SitePatch g_burnoutPatch{};
SitePatch g_sirenPatch{};
std::array<SitePatch, 4> g_fakePhysicsPatches{};
std::array<SitePatch, 3> g_restThresholdPatches{};
std::array<SitePatch, 6> g_moveSpeedSnapPatches{};
SitePatch g_turnAirResistancePatch{};
SitePatch g_groundFrictionPatch{};
SitePatch g_bikeLeanTargetPatch{};
SitePatch g_bikePitchExperimentPatch{};
// Scratch for the six move speed snap thunks. The game is single threaded
// through vehicle processing, and each thunk writes it and reads it back before
// the next instruction.
float g_scaledMoveSpeedSnap{};
SitePatch g_scriptsProcessPatch{};
SitePatch g_scriptSlideObjectPatch{};
SitePatch g_scriptRotateObjectPatch{};
std::array<SitePatch, 3> g_fallingGlassPatches{};
SitePatch g_breakObjectLifetimePatch{};
SitePatch g_menuBackgroundPatch{};
std::array<SitePatch, 5> g_wheelFrictionPatches{};
SitePatch g_swimmingPatch{};
SitePatch g_climbSpeedPatch{};
SitePatch g_moneyStepPatch{};
SitePatch g_followPedCameraPatch{};
SitePatch g_followCarCameraPatch{};
SitePatch g_attachedEntitySpeedPatch{};
SitePatch g_aiAircraftSteerPatch{};
std::array<SitePatch, kStatTruncSites.size()> g_statTruncPatches{};
SitePatch g_rollOntoWheelsTurnPatch{};
SitePatch g_rollOntoWheelsMovePatch{};
std::array<SitePatch, 5> g_doorSwingPatches{};
std::array<SitePatch, 6> g_wheelSpinPatches{};
SitePatch g_boatEngineDampingPatch{};
std::array<SitePatch, 3> g_swimPitchPatches{};
SitePatch g_bmxSprintLeanPatch{};
std::array<SitePatch, 5> g_bikeWheelSpinPatches{};
std::array<SitePatch, 2> g_jetPackFxPatches{};
std::array<SitePatch, 2> g_headBopPatches{};
std::array<SitePatch, 4> g_bmxLeanPatches{};
std::array<SitePatch, 6> g_jumpOutDampPatches{};
std::array<SitePatch, 6> g_pushOutPatches{};
std::array<SitePatch, 10> g_wheelSettlePatches{};
SitePatch g_mapWheelSamplePatch{};
SitePatch g_mapZoomInGatePatch{};
SitePatch g_mapZoomOutGatePatch{};
std::array<SitePatch, 3> g_fireGatePatches{};
SitePatch g_drunkSteerPatch{};
SitePatch g_fatCounterPatch{};
SitePatch g_buoyancyThresholdPatch{};
SitePatch g_buoyancyClampedStorePatch{};
std::array<SitePatch, 4> g_railWheelSpinPatches{};
std::array<SitePatch, 2> g_heliRotorPatches{};
BytePatch g_frameLimiterGatePatch{};
BytePatch g_frameLimitStorePatch{};
BytePatch g_refreshRatePatch{};
DetourPatch g_fxCreateParticlesPatch{};
DetourPatch g_fxAddParticlePatch{};
DetourPatch g_aimWeaponPatch{};

float g_endTimerFraction{};
float g_flightTimerFraction{};
bool g_endTimerActive{};
bool g_flightTimerActive{};
bool g_loggingEnabled{true};
float g_aimTimeStep{1.0f};
float g_originalTimeStepValue{kOriginalTimeStep};
bool g_swingingDisabled{};

uint32_t g_breakLifetimeLastFrame{0xFFFFFFFFu};
float g_breakLifetimeCarry{};
int g_breakLifetimeTicks{};

// Only the low byte is ever read by the patched instructions, but the counters
// are full dwords so that a dword read would also see a sane value.
volatile uint32_t g_hudFlashClock{};
volatile uint32_t g_hudVisibleClock{};
unsigned g_hudFlashIntervalMs{kDefaultHudFlashIntervalMs};
bool g_hudDisableFlashing{};
bool g_hudFlashActive{};
bool g_hudFlashInstalled{};

// 0 watches a stationary ridden bike, 1 watches the vehicle being pushed.
int g_watchMode = 0;
volatile uintptr_t g_watchCandidate{};
volatile float g_leanTargetRaw{};
float g_leanTargetHeld{};
volatile DWORD g_gameThreadId{};
bool g_diagnosticActive{};
std::string g_diagnosticPath;

uint32_t g_fakePhysicsLastFrame{0xFFFFFFFF};
float g_fakePhysicsCarry{};
int32_t g_fakePhysicsTick{1};

struct HornTapState {
    uint32_t pressLastTime{};
    bool hasPressed{};
};

std::array<HornTapState, 2> g_hornTapStates{};

int g_fpsLimit{};
int g_refreshRate{};
int g_lastFpsLimit{};
bool g_isOnPauseMenu{};
AutoLimitFlags g_autoLimit{};

std::array<AbsoluteOperandPatch, 13> g_aimTimeStepPatches{{
    {0x0052167A, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x00521752, {0xD8, 0x1D, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x00521765, {0xD8, 0x25, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005217C9, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005217DE, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x0052191B, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x00521F40, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x0052210A, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x0052233B, {0xD8, 0x0D, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x00522369, {0xD8, 0x0D, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005223AA, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005224E4, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005226CF, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
}};

struct EmissionCarrySlot {
    void* blueprint{};
    float intensity{};
};

struct AmmoConsumptionSlot {
    void* weapon{};
    int32_t weaponType{};
    uint32_t lastUpdate{};
    float credit{};
};

std::array<EmissionCarrySlot, 16> g_weaponFxEmissionCarry{};
std::array<AmmoConsumptionSlot, 16> g_ammoConsumptionSlots{};
std::string g_iniPath;
std::string g_logPath;

// ---------------------------------------------------------------------------
// Infrastructure
// ---------------------------------------------------------------------------

std::string ModulePathWithExtension(const char* extension) {
    std::array<char, MAX_PATH> path{};
    const DWORD length = GetModuleFileNameA(g_module, path.data(),
                                            static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return {};
    }

    std::string result(path.data(), length);
    const size_t slash = result.find_last_of("\\/");
    const size_t dot = result.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        result.resize(dot);
    }
    result += extension;
    return result;
}

void Log(const char* message) {
    if (!g_loggingEnabled || g_logPath.empty()) {
        return;
    }

    FILE* file{};
    if (fopen_s(&file, g_logPath.c_str(), "a") == 0 && file) {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        std::fprintf(file, "[%02u:%02u:%02u] %s\n", time.wHour, time.wMinute,
                     time.wSecond, message);
        std::fclose(file);
    }
}

bool CreateDefaultIniIfMissing() {
    if (g_iniPath.empty()) {
        return false;
    }
    if (GetFileAttributesA(g_iniPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }

    HANDLE file = CreateFileA(g_iniPath.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written{};
    constexpr DWORD size = static_cast<DWORD>(sizeof(kDefaultIni) - 1);
    const bool ok = WriteFile(file, kDefaultIni, size, &written, nullptr) != FALSE
                 && written == size;
    CloseHandle(file);
    return ok;
}

bool ReadSetting(const char* section, const char* key, bool defaultValue) {
    return GetPrivateProfileIntA(section, key, defaultValue ? 1 : 0,
                                 g_iniPath.c_str()) != 0;
}

int ReadNumber(const char* section, const char* key, int defaultValue) {
    return GetPrivateProfileIntA(section, key, defaultValue, g_iniPath.c_str());
}

bool WriteBytes(uintptr_t address, const uint8_t* bytes, size_t size) {
    DWORD oldProtect{};
    void* destination = reinterpret_cast<void*>(address);
    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    std::memcpy(destination, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), destination, size);

    DWORD ignored{};
    VirtualProtect(destination, size, oldProtect, &ignored);
    return true;
}

bool MemoryMatchesRaw(uintptr_t address, const uint8_t* expected, size_t size) {
    __try {
        return std::memcmp(reinterpret_cast<const void*>(address), expected,
                           size) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <size_t Size>
bool MemoryMatches(uintptr_t address,
                   const std::array<uint8_t, Size>& expected) {
    return MemoryMatchesRaw(address, expected.data(), expected.size());
}

bool SwimmingMovementCodeIsUnmodified() {
    return MemoryMatches(kSwimDiveScale, kExpectedSwimDiveScale)
        && MemoryMatches(kSwimAscentBias, kExpectedSwimAscentBias)
        && MemoryMatches(kSwimVectorSetup, kExpectedSwimVectorSetup)
        && MemoryMatches(kSwimVectorTransform,
                         kExpectedSwimVectorTransform);
}


// Every byte range this plugin has taken, so two fixes cannot claim the same
// instruction.
//
// This exists because it happened. `heliSpinUp` was written on 2026-08-26 after
// reading `CHeli::ProcessFlyingCarStuff` and finding a timer whose decay
// carried the timestep while its rise did not. That reading was correct and the
// fix was a duplicate: `heliRotorSpeed` had been patching the same two
// addresses, `0x6C4F29` and `0x6C4F37`, since long before, and doing it better.
// The byte check caught it in game, because the first fix had already replaced
// the bytes the second was matching against, and nothing shipped broken. But
// the byte check only catches an overlap when the first patch happens to change
// the bytes the second expects, which is luck rather than a guarantee.
//
// Refusing the second claim outright makes it a rule instead. The log line
// names both fixes, which is the thing that turns a silent skip into an
// obvious mistake.
struct ClaimedRange {
    uintptr_t begin;
    uintptr_t end;
};

std::array<ClaimedRange, 256> g_claimedRanges{};
size_t g_claimedRangeCount{};

bool ClaimPatchRange(uintptr_t address, size_t size) {
    const uintptr_t begin = address;
    const uintptr_t end = address + size;
    for (size_t i = 0; i < g_claimedRangeCount; ++i) {
        if (begin < g_claimedRanges[i].end
            && g_claimedRanges[i].begin < end) {
            char line[160];
            std::snprintf(line, sizeof(line),
                          "Patch site refused: 0x%08X..0x%08X overlaps "
                          "0x%08X..0x%08X, already patched by another fix.",
                          static_cast<unsigned>(begin),
                          static_cast<unsigned>(end),
                          static_cast<unsigned>(g_claimedRanges[i].begin),
                          static_cast<unsigned>(g_claimedRanges[i].end));
            Log(line);
            return false;
        }
    }
    if (g_claimedRangeCount >= g_claimedRanges.size()) {
        Log("Patch site refused: the claimed range table is full.");
        return false;
    }
    g_claimedRanges[g_claimedRangeCount++] = {begin, end};
    return true;
}

void ReleasePatchRange(uintptr_t address) {
    for (size_t i = 0; i < g_claimedRangeCount; ++i) {
        if (g_claimedRanges[i].begin == address) {
            g_claimedRanges[i] = g_claimedRanges[--g_claimedRangeCount];
            return;
        }
    }
}
// Replaces `size` original bytes with a relative branch to `target` and pads
// the remainder with NOPs. `opcode` is 0xE8 for a call or 0xE9 for a jump.
bool InstallBranch(SitePatch& patch, uintptr_t address, const void* target,
                   const uint8_t* expected, size_t size, uint8_t opcode) {
    if (size < 5 || size > patch.original.size()) {
        Log("Patch site rejected: the site is larger than a patch record.");
        return false;
    }
    if (!MemoryMatchesRaw(address, expected, size)) {
        return false;
    }

    const intptr_t displacement = reinterpret_cast<intptr_t>(target)
                                - static_cast<intptr_t>(address + 5);
    if (displacement < std::numeric_limits<int32_t>::min()
        || displacement > std::numeric_limits<int32_t>::max()) {
        return false;
    }

    if (!ClaimPatchRange(address, size)) {
        return false;
    }

    patch.address = address;
    patch.size = size;
    std::memcpy(patch.original.data(), expected, size);

    std::array<uint8_t, 48> replacement{};
    replacement.fill(0x90);
    replacement[0] = opcode;
    const int32_t relative = static_cast<int32_t>(displacement);
    std::memcpy(replacement.data() + 1, &relative, sizeof(relative));
    patch.installed = WriteBytes(address, replacement.data(), size);
    if (!patch.installed) {
        ReleasePatchRange(address);
    }
    return patch.installed;
}

template <size_t Size>
bool InstallJump(SitePatch& patch, uintptr_t address, const void* target,
                 const std::array<uint8_t, Size>& expected) {
    return InstallBranch(patch, address, target, expected.data(),
                         expected.size(), 0xE9);
}

template <size_t Size>
bool InstallCall(SitePatch& patch, uintptr_t address, const void* target,
                 const std::array<uint8_t, Size>& expected) {
    return InstallBranch(patch, address, target, expected.data(),
                         expected.size(), 0xE8);
}

void RestoreSite(SitePatch& patch) {
    if (patch.installed) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
        ReleasePatchRange(patch.address);
        patch.installed = false;
    }
}

bool InstallByte(BytePatch& patch, uintptr_t address, uint8_t value) {
    __try {
        patch.original = *reinterpret_cast<const uint8_t*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    patch.address = address;
    patch.installed = WriteBytes(address, &value, 1);
    return patch.installed;
}

void RestoreByte(BytePatch& patch) {
    if (patch.installed) {
        WriteBytes(patch.address, &patch.original, 1);
        patch.installed = false;
    }
}

void RestoreAbsoluteOperandPatches(
    std::array<AbsoluteOperandPatch, 13>& patches) {
    for (auto& patch : patches) {
        if (patch.installed) {
            WriteBytes(patch.instruction + 2, patch.expected.data() + 2, 4);
            patch.installed = false;
        }
    }
}

bool InstallAimTimeStepOperands() {
    for (const auto& patch : g_aimTimeStepPatches) {
        if (!MemoryMatches(patch.instruction, patch.expected)) {
            return false;
        }
    }

    const uintptr_t replacement = reinterpret_cast<uintptr_t>(&g_aimTimeStep);
    for (auto& patch : g_aimTimeStepPatches) {
        if (!WriteBytes(patch.instruction + 2,
                        reinterpret_cast<const uint8_t*>(&replacement),
                        sizeof(replacement))) {
            RestoreAbsoluteOperandPatches(g_aimTimeStepPatches);
            return false;
        }
        patch.installed = true;
    }
    return true;
}

bool InstallDetour(DetourPatch& patch, uintptr_t address, const void* target,
                   const uint8_t* expected, size_t size) {
    if (size < 5 || size > patch.original.size()) {
        return false;
    }
    if (!MemoryMatchesRaw(address, expected, size)) {
        return false;
    }
    if (!ClaimPatchRange(address, size)) {
        return false;
    }

    auto* gateway = static_cast<uint8_t*>(VirtualAlloc(
        nullptr, size + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!gateway) {
        ReleasePatchRange(address);
        return false;
    }
    std::memcpy(gateway, reinterpret_cast<const void*>(address), size);
    gateway[size] = 0xE9;
    const int32_t gatewayBack = static_cast<int32_t>(
        address + size - reinterpret_cast<uintptr_t>(gateway + size + 5));
    std::memcpy(gateway + size + 1, &gatewayBack, sizeof(gatewayBack));

    const intptr_t displacement = reinterpret_cast<intptr_t>(target)
                                - static_cast<intptr_t>(address + 5);
    if (displacement < std::numeric_limits<int32_t>::min()
        || displacement > std::numeric_limits<int32_t>::max()) {
        ReleasePatchRange(address);
        VirtualFree(gateway, 0, MEM_RELEASE);
        return false;
    }

    patch.address = address;
    patch.size = size;
    patch.gateway = gateway;
    std::memcpy(patch.original.data(), expected, size);
    std::array<uint8_t, 16> replacement{};
    replacement.fill(0x90);
    replacement[0] = 0xE9;
    const int32_t relative = static_cast<int32_t>(displacement);
    std::memcpy(replacement.data() + 1, &relative, sizeof(relative));
    patch.installed = WriteBytes(address, replacement.data(), size);
    if (!patch.installed) {
        ReleasePatchRange(address);
        VirtualFree(gateway, 0, MEM_RELEASE);
        patch.gateway = nullptr;
    }
    return patch.installed;
}

void RestoreDetour(DetourPatch& patch) {
    if (patch.installed) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
        ReleasePatchRange(patch.address);
        patch.installed = false;
    }
    if (patch.gateway) {
        VirtualFree(patch.gateway, 0, MEM_RELEASE);
        patch.gateway = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Shared timestep helpers
// ---------------------------------------------------------------------------

// Framerate Vigilante calls this ratio the "normalizer": it is 1.0 at the
// original 30 FPS timestep and shrinks proportionally as the frame rate rises.
float TimeStepRatio() {
    __try {
        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f) {
            return 1.0f;
        }
        return timeStep / kOriginalTimeStep;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1.0f;
    }
}

float VectorLength(const float v[3]);

float ReadGameFloat(uintptr_t address, float fallback) {
    __try {
        const float value = *reinterpret_cast<const float*>(address);
        return std::isfinite(value) ? value : fallback;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return fallback;
    }
}

// ---------------------------------------------------------------------------
// Stunt jump camera timers
// ---------------------------------------------------------------------------

int AccumulateMilliseconds(float milliseconds, float& fraction) {
    if (!std::isfinite(milliseconds) || milliseconds <= 0.0f) {
        fraction = 0.0f;
        return 0;
    }

    const float total = milliseconds + fraction;
    const int whole = static_cast<int>(total);
    fraction = total - static_cast<float>(whole);
    return whole;
}

int __cdecl AccumulateFlightTimer(float milliseconds) {
    if (!g_flightTimerActive) {
        g_flightTimerFraction = 0.0f;
        g_flightTimerActive = true;
    }
    g_endTimerActive = false;
    return AccumulateMilliseconds(milliseconds, g_flightTimerFraction);
}

int __cdecl AccumulateEndTimer(float milliseconds) {
    if (!g_endTimerActive) {
        g_endTimerFraction = 0.0f;
        g_endTimerActive = true;
    }
    g_flightTimerActive = false;
    return AccumulateMilliseconds(milliseconds, g_endTimerFraction);
}

// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------

bool IsContinuousWeapon(int32_t weaponType) {
    constexpr int32_t kFlamethrower = 37;
    constexpr int32_t kSpraycan = 41;
    constexpr int32_t kExtinguisher = 42;
    return weaponType == kFlamethrower || weaponType == kSpraycan
        || weaponType == kExtinguisher;
}

bool IsWeaponFxEmitter(void* emitter, void** systemOut = nullptr) {
    if (!emitter) {
        return false;
    }
    __try {
        void* system = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(emitter) + 0x08);
        if (systemOut) {
            *systemOut = system;
        }
        return system && (*reinterpret_cast<uint8_t*>(
            reinterpret_cast<uintptr_t>(system) + 0x62) & 0x20) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

using FxCreateParticlesFn = void(__thiscall*)(void*, float, float);

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

using AimWeaponFn = void(__thiscall*)(void*, const Vec3&, float, float, float);

void __fastcall HookedProcessAimWeapon(void* cam, void*, const Vec3* target,
                                       float orientation, float speedVar,
                                       float speedVarWanted) {
    __try {
        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        g_aimTimeStep = std::isfinite(timeStep) && timeStep > 0.0f
                     && timeStep < 1.0f
            ? 1.0f
            : timeStep;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_aimTimeStep = 1.0f;
    }

    const Vec3 zero{};
    reinterpret_cast<AimWeaponFn>(g_aimWeaponPatch.gateway)(
        cam, target ? *target : zero, orientation, speedVar, speedVarWanted);
}

EmissionCarrySlot* FindEmissionCarrySlot(void* blueprint) {
    EmissionCarrySlot* empty{};
    for (auto& slot : g_weaponFxEmissionCarry) {
        if (slot.blueprint == blueprint) {
            return &slot;
        }
        if (!slot.blueprint && !empty) {
            empty = &slot;
        }
    }
    if (empty) {
        empty->blueprint = blueprint;
    }
    return empty;
}

AmmoConsumptionSlot& FindAmmoConsumptionSlot(void* weapon) {
    AmmoConsumptionSlot* oldest = &g_ammoConsumptionSlots.front();
    for (auto& slot : g_ammoConsumptionSlots) {
        if (slot.weapon == weapon) {
            return slot;
        }
        if (!slot.weapon) {
            return slot;
        }
        if (static_cast<int32_t>(slot.lastUpdate - oldest->lastUpdate) < 0) {
            oldest = &slot;
        }
    }
    return *oldest;
}

// The fraction the truncation would have thrown away, carried into the next
// frame. At 30 FPS the damage is exactly five per frame and the carry stays at
// zero, so a capped run is bit for bit what it was. There is one accumulator
// because `HandlePlayerBreath` is a `CPlayerPed` method and single player has
// one of those.
float g_drowningDamageCarry{};

int32_t __cdecl AccumulateDrowningDamage(float damage) {
    __try {
        if (!std::isfinite(damage) || damage <= 0.0f) {
            g_drowningDamageCarry = 0.0f;
            return 0;
        }
        g_drowningDamageCarry += damage;
        const float whole = std::floor(g_drowningDamageCarry);
        g_drowningDamageCarry -= whole;
        return static_cast<int32_t>(whole);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int32_t>(damage);
    }
}


// One carry per call site, indexed the same as `kStatTruncSites`. The lookup is
// a linear scan over twenty one entries and runs at most a handful of times a
// frame, so it costs nothing worth measuring.
std::array<float, kStatTruncSites.size()> g_statTruncCarries{};

// Diagnostic for the cycle skill counter, which was reported in game as not
// levelling at all at 600 FPS with `skillProgress` on, and as levelling in
// three minutes with it off against two minutes at 30 FPS. Reading the code
// predicts the opposite: unpatched it should be several times faster at a high
// frame rate, not slower. That means the reading is wrong somewhere, and this
// logs the four numbers that separate the candidates rather than guessing
// again.
//
// `calls` is how many times a second the accumulate path is reached at all,
// which is the speed gate at `0x55C8E6`: if the bike is simply slower at a high
// frame rate the counter starves regardless of any truncation.
// `raw` against `added` separates the correction from the truncation.
constexpr uintptr_t kCycleSkillCounter = 0x00B794E0;
constexpr uintptr_t kCycleStaminaCounter = 0x00B794DC;
constexpr uintptr_t kCycleSkillLimit = 0x00B78FAC;

bool g_traceCycleSkill{};
uint64_t g_cycleTraceLast{};
uint32_t g_cycleTraceCalls{};
double g_cycleTraceRaw{};
int64_t g_cycleTraceAdded{};

void TraceCycleSkill(int32_t site, double raw, int32_t added) {
    if (!g_traceCycleSkill) {
        return;
    }
    if (site == 0) {
        ++g_cycleTraceCalls;
    } else {
        g_cycleTraceRaw += raw;
        g_cycleTraceAdded += added;
    }

    const uint64_t now = GetTickCount64();
    if (g_cycleTraceLast == 0) {
        g_cycleTraceLast = now;
        return;
    }
    if (now - g_cycleTraceLast < 1000) {
        return;
    }
    g_cycleTraceLast = now;

    const float timeStep = ReadGameFloat(kTimerTimeStep, kOriginalTimeStep);
    const float limit = ReadGameFloat(kCycleSkillLimit, 0.0f);
    char line[256];
    std::snprintf(line, sizeof(line),
                  "cycle: fps~%.0f ts=%.4f calls=%u raw=%.1f added=%lld "
                  "skill=%u stamina=%u limit=%.0f",
                  timeStep > 0.0f ? 50.0f / timeStep : 0.0f, timeStep,
                  g_cycleTraceCalls, g_cycleTraceRaw,
                  static_cast<long long>(g_cycleTraceAdded),
                  *reinterpret_cast<volatile uint32_t*>(kCycleSkillCounter),
                  *reinterpret_cast<volatile uint32_t*>(kCycleStaminaCounter),
                  limit * 1000.0f);
    Log(line);

    g_cycleTraceCalls = 0;
    g_cycleTraceRaw = 0.0;
    g_cycleTraceAdded = 0;
}

int32_t __cdecl TruncateStatWithCarry(double value, uintptr_t site) {
    __try {
        size_t index = kStatTruncSites.size();
        for (size_t i = 0; i < kStatTruncSites.size(); ++i) {
            // The return address the wrapper sees is the instruction after the
            // call, and the table holds the address of the call itself.
            if (kStatTruncSites[i].address + 5 == site) {
                index = i;
                break;
            }
        }
        if (index == kStatTruncSites.size() || !std::isfinite(value)) {
            return static_cast<int32_t>(value);
        }

        const double total = value + g_statTruncCarries[index];
        const double whole = std::trunc(total);
        const double remainder = total - whole;
        g_statTruncCarries[index] = std::isfinite(remainder)
                                  ? static_cast<float>(remainder)
                                  : 0.0f;
        const int32_t result = static_cast<int32_t>(whole);
        if (g_traceCycleSkill) {
            if (kStatTruncSites[index].address == 0x0055C94B) {
                TraceCycleSkill(0, value, result);
            } else if (kStatTruncSites[index].address == 0x0055C972) {
                TraceCycleSkill(1, value, result);
            }
        }
        return result;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int32_t>(value);
    }
}

int32_t __cdecl ShouldConsumeContinuousWeaponAmmo(uintptr_t weapon) {
    if (!weapon) {
        return true;
    }

    __try {
        const int32_t weaponType = *reinterpret_cast<int32_t*>(weapon);
        if (!IsContinuousWeapon(weaponType)) {
            return true;
        }

        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep >= kOriginalTimeStep) {
            return true;
        }

        const uint32_t now = *reinterpret_cast<uint32_t*>(
            kTimerTimeInMilliseconds);
        auto& slot = FindAmmoConsumptionSlot(reinterpret_cast<void*>(weapon));
        if (slot.weapon != reinterpret_cast<void*>(weapon)
            || slot.weaponType != weaponType) {
            slot = {reinterpret_cast<void*>(weapon), weaponType, now, 0.0f};
            return true;
        }

        const uint32_t elapsed = now - slot.lastUpdate;
        slot.lastUpdate = now;
        constexpr uint32_t kNewBurstThresholdMs = 200;
        if (elapsed > kNewBurstThresholdMs) {
            slot.credit = 0.0f;
            return true;
        }

        slot.credit += static_cast<float>(elapsed)
                     * (kOriginalWeaponConsumptionRate / 1000.0f);
        if (slot.credit < 1.0f) {
            return false;
        }

        slot.credit -= std::floor(slot.credit);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
}

void TraceChainsaw();

// Decides, on the frame the chainsaw's moving attack has run past `chain`,
// whether the animation is rewound behind `hit` so it strikes again, or parked
// on `hit` so it does not. Rewinding it further than the game does cannot work:
// `hit` is only 0.0333 s into the animation, so there is not enough animation
// in front of it to hold a whole strike period at a high frame rate. Parking is
// the half of the loop that costs nothing: the strike needs `currentTime` to
// cross `hit` from below, and a `currentTime` of exactly `hit` is not below it,
// so the next frames advance without striking and come back here.
//
// The schedule is kept on the millisecond clock rather than on frames, and the
// credit carries across frames, so a strike is armed every 66.7 ms whatever the
// frame rate. At 30 FPS and below every call arms, which is the stock 0.01
// rewind on every pass and therefore the stock behaviour exactly.
void __cdecl UpdateChainsawRewindOffset(void* anim, void* task) {
    g_chainsawRewindOffset = kChainsawStockRewind;
    if (!anim) {
        return;
    }

    __try {
        ++g_chainsawCalls;
        if (task) {
            g_chainsawCombo = *reinterpret_cast<const int8_t*>(
                reinterpret_cast<uintptr_t>(task) + 0x24);
            g_chainsawMove = *reinterpret_cast<const int8_t*>(
                reinterpret_cast<uintptr_t>(task) + 0x25);
        }
        g_chainsawAnimStep = *reinterpret_cast<const float*>(
            reinterpret_cast<uintptr_t>(anim) + 0x28);
        g_chainsawAnimTime = *reinterpret_cast<const float*>(
            reinterpret_cast<uintptr_t>(anim) + 0x20);

        const uint32_t now = *reinterpret_cast<uint32_t*>(
            kTimerTimeInMilliseconds);
        bool armed = true;
        if (anim != g_chainsawAnim
            || now - g_chainsawLastCall > kChainsawBurstGapMs) {
            g_chainsawAnim = anim;
            g_chainsawCredit = 0.0f;
        } else {
            g_chainsawCredit += static_cast<float>(now - g_chainsawLastCall);
            if (g_chainsawCredit >= kChainsawStrikePeriodMs) {
                g_chainsawCredit = std::min(
                    g_chainsawCredit - kChainsawStrikePeriodMs,
                    kChainsawStrikePeriodMs);
            } else {
                armed = false;
            }
        }
        g_chainsawLastCall = now;

        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep >= kOriginalTimeStep) {
            armed = true;
        }

        // A negative rewind parks the animation just past `hit`. The margin is
        // there so that `currentTime - m_fTimeStep`, which is how the strike
        // test reconstructs the previous frame, cannot round back below `hit`
        // and fire a strike; it stays well inside the 0.0033 s that separates
        // `hit` from `chain`.
        g_chainsawRewindOffset = armed ? kChainsawStockRewind
                                       : -kChainsawParkMargin;
        if (armed) {
            ++g_chainsawArms;
        }
        TraceChainsaw();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_chainsawRewindOffset = kChainsawStockRewind;
    }
}

void __cdecl RecordFightStrike(void* task) {
    ++g_chainsawStrikes;
    if (task) {
        __try {
            g_chainsawStrikeCombo = *reinterpret_cast<const int8_t*>(
                reinterpret_cast<uintptr_t>(task) + 0x24);
            g_chainsawStrikeMove = *reinterpret_cast<const int8_t*>(
                reinterpret_cast<uintptr_t>(task) + 0x25);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    TraceChainsaw();
}

// One line a second while `traceChainsaw` is on: how often the rewind site
// runs, how many of those passes armed a strike, how many strikes actually
// reached `FightStrike`, and which combo and move the task is in. A chainsaw
// combo is 12 and its moving attack is move 4.
void TraceChainsaw() {
    if (!g_traceChainsaw) {
        return;
    }
    const uint64_t now = GetTickCount64();
    if (g_chainsawTraceLast == 0) {
        g_chainsawTraceLast = now;
        return;
    }
    if (now - g_chainsawTraceLast < 1000) {
        return;
    }
    const double seconds = static_cast<double>(now - g_chainsawTraceLast)
                         / 1000.0;
    g_chainsawTraceLast = now;

    const float timeStep = ReadGameFloat(kTimerTimeStep, kOriginalTimeStep);
    char line[256];
    std::snprintf(line, sizeof(line),
                  "chainsaw: fps~%.0f rewinds/s=%.1f arms/s=%.1f strikes/s=%.1f "
                  "combo=%d move=%d strikeCombo=%d strikeMove=%d "
                  "animStep=%.4f animTime=%.4f",
                  timeStep > 0.0f ? 50.0f / timeStep : 0.0f,
                  g_chainsawCalls / seconds, g_chainsawArms / seconds,
                  g_chainsawStrikes / seconds, g_chainsawCombo, g_chainsawMove,
                  g_chainsawStrikeCombo, g_chainsawStrikeMove,
                  g_chainsawAnimStep, g_chainsawAnimTime);
    Log(line);

    g_chainsawCalls = 0;
    g_chainsawArms = 0;
    g_chainsawStrikes = 0;
}

void __fastcall HookedFxCreateParticles(void* emitter, void*, float currentTime,
                                        float deltaTime) {
    EmissionCarrySlot* carry{};
    float* intensity{};
    if (IsWeaponFxEmitter(emitter)) {
        __try {
            void* blueprint = *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(emitter) + 0x04);
            carry = FindEmissionCarrySlot(blueprint);
            intensity = reinterpret_cast<float*>(
                reinterpret_cast<uintptr_t>(emitter) + 0x10);
            if (carry && *intensity == 0.0f && carry->intensity > 0.0f
                && carry->intensity < 1.0f) {
                *intensity = carry->intensity;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            carry = nullptr;
            intensity = nullptr;
        }
    }

    reinterpret_cast<FxCreateParticlesFn>(g_fxCreateParticlesPatch.gateway)(
        emitter, currentTime, deltaTime);

    if (carry && intensity) {
        __try {
            carry->intensity = *intensity;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            carry->intensity = 0.0f;
        }
    }
}

// ---------------------------------------------------------------------------
// Direct particle emission rate
// ---------------------------------------------------------------------------

using FxAddParticleFn = void(__fastcall*)(void* self, void* edx, const void* pos,
                                          const void* vel, float timeSince,
                                          const void* mults, float rotZ,
                                          float lightMult, float lightMultLimit,
                                          int32_t createLocal);

bool g_particleRateGate = true;
uint32_t g_particleBudget = 0;

// One entry per call site, found by the return address of the call into
// `FxSystem_c::AddParticle`. Open addressing, and a full table fails open: a
// missed gate costs some particles, a wrong one costs the effect entirely.
constexpr uint32_t kParticleSiteSlots = 512;  // power of two
constexpr uint32_t kParticleSiteMask = kParticleSiteSlots - 1;

struct ParticleSiteState {
    uintptr_t site;
    uint32_t lastFrame;
    float carry;
    uint8_t open;
};
std::array<ParticleSiteState, kParticleSiteSlots> g_particleSites{};

void ResetParticleSites() {
    for (auto& slot : g_particleSites) {
        slot.site = 0;
        slot.lastFrame = 0;
        slot.carry = 0.0f;
        slot.open = 1;
    }
}

ParticleSiteState* FindParticleSite(uintptr_t site) {
    uint32_t key = static_cast<uint32_t>(site);
    key ^= key >> 4;
    key *= 2654435761u;
    uint32_t index = (key >> 8) & kParticleSiteMask;
    for (uint32_t probe = 0; probe < 32; ++probe) {
        ParticleSiteState& slot = g_particleSites[index];
        if (slot.site == site) {
            return &slot;
        }
        if (slot.site == 0) {
            slot.site = site;
            slot.lastFrame = 0;
            slot.carry = 0.0f;
            slot.open = 1;
            return &slot;
        }
        index = (index + 1) & kParticleSiteMask;
    }
    return nullptr;
}

// True when this call site may emit on this frame. The decision is taken once
// per frame counter value and reused, so every call the site makes within one
// frame agrees: the exhaust adds up to eight particles in its loop and either
// all of them belong to this frame or none do.
bool ParticleSiteOpen(uintptr_t site) {
    ParticleSiteState* slot = FindParticleSite(site);
    if (!slot) {
        return true;
    }
    const uint32_t frame = *reinterpret_cast<volatile uint32_t*>(kFrameCounter);
    if (slot->lastFrame == frame) {
        return slot->open != 0;
    }

    float ratio = TimeStepRatio();
    if (!std::isfinite(ratio) || ratio <= 0.0f) {
        ratio = 1.0f;
    }
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }

    const uint32_t elapsedFrames = frame - slot->lastFrame;
    if (slot->lastFrame == 0
        || static_cast<float>(elapsedFrames) * ratio >= 1.0f) {
        // The call site was idle for at least one original frame, so this is a
        // fresh event rather than a short stochastic gap in a stream. Never
        // drop the first particle of a fresh event.
        slot->carry = 0.0f;
        slot->open = 1;
    } else {
        slot->carry += ratio;
        if (slot->carry >= 1.0f) {
            slot->carry -= 1.0f;
            slot->open = 1;
        } else {
            slot->open = 0;
        }
    }
    slot->lastFrame = frame;
    return slot->open != 0;
}

// Optional hard ceiling on new particles per second, kept for parity with the
// standalone FxLimiter plugin. Off by default: the rate gate above already
// restores the density the game was drawn for, and a fixed ceiling is a
// performance knob rather than a fix. Blood has its own budget because it is
// the one effect where thinning is most visible.
double g_qpcToSeconds = 0.0;

double NowSeconds() {
    if (g_qpcToSeconds == 0.0) {
        LARGE_INTEGER frequency{};
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0) {
            return 0.0;
        }
        g_qpcToSeconds = 1.0 / static_cast<double>(frequency.QuadPart);
    }
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) * g_qpcToSeconds;
}

struct ParticleBudgetState {
    double lastRefill;
    double credit;
};
ParticleBudgetState g_generalBudget{};

bool ParticleBudgetAllows(ParticleBudgetState& state, uint32_t perSecond,
                          bool freshEvent) {
    if (perSecond == 0) {
        return true;
    }
    const double now = NowSeconds();
    if (now == 0.0) {
        return true;
    }
    if (state.lastRefill == 0.0 || now < state.lastRefill) {
        state.lastRefill = now;
        state.credit = 1.0;
    } else {
        state.credit += (now - state.lastRefill) * static_cast<double>(perSecond);
        state.lastRefill = now;
        if (state.credit > 1.0) {
            state.credit = 1.0;
        }
    }
    if (state.credit >= 1.0) {
        state.credit -= 1.0;
        return true;
    }
    return freshEvent;
}

__declspec(noinline) void __fastcall HookedFxAddParticle(
    void* self, void* edx, const void* pos, const void* vel, float timeSince,
    const void* mults, float rotZ, float lightMult, float lightMultLimit,
    int32_t createLocal) {
    const uintptr_t site = reinterpret_cast<uintptr_t>(_ReturnAddress());

    // First call this site makes in this frame. The budget below never drops
    // one of those, so an effect can go thin but never vanish.
    ParticleSiteState* slot = FindParticleSite(site);
    const bool freshEvent =
        !slot
        || slot->lastFrame != *reinterpret_cast<volatile uint32_t*>(kFrameCounter);

    // Always evaluated so the site's frame stamp stays current even when the
    // gate is switched off and only the budget or the multipliers are wanted.
    const bool open = ParticleSiteOpen(site);
    if (g_particleRateGate && !open) {
        return;
    }

    if (g_particleBudget != 0
        && !ParticleBudgetAllows(g_generalBudget, g_particleBudget,
                                 freshEvent)) {
        return;
    }

    reinterpret_cast<FxAddParticleFn>(g_fxAddParticlePatch.gateway)(
        self, edx, pos, vel, timeSince, mults, rotZ, lightMult, lightMultLimit,
        createLocal);
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------

// Replaces the constant walk step used while aiming a rifle.
float __cdecl GetAimingRifleWalkStep() {
    const float ratio = TimeStepRatio();
    const float base = ReadGameFloat(kAimingRifleWalkConstant, 0.07f);
    return ratio > 0.0f ? base / ratio : base;
}

// The step is scaled into this frame and the fraction is carried, so the
// counter covers the same ground per second as it does at 30 FPS. The carry is
// dropped when the direction reverses, otherwise a leftover from counting up
// would eat into the first step of counting down.
float g_moneyStepCarry{};

void __cdecl ApplyMoneyStep(int32_t* field, int32_t proposed) {
    __try {
        if (!field) {
            return;
        }
        const int32_t current = *field;
        const int32_t step = proposed - current;
        if (step == 0) {
            return;
        }
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            *field = proposed;
            g_moneyStepCarry = 0.0f;
            return;
        }
        if ((step < 0) != (g_moneyStepCarry < 0.0f)) {
            g_moneyStepCarry = 0.0f;
        }
        const float scaled = static_cast<float>(step)
                                 * (timeStep / kOriginalTimeStep)
                             + g_moneyStepCarry;
        const float whole = std::trunc(scaled);
        g_moneyStepCarry = scaled - whole;
        *field = current + static_cast<int32_t>(whole);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *field = proposed;
    }
}

// Clamping rather than rescaling is what reproduces the original. The ped is
// pulled onto the hold by however much it can cover in one frame, so scaling
// the speed would change where it lands; limiting it makes the approach take
// several short frames instead of one long one, which is the same wall time.
void __cdecl ClampClimbMoveSpeed(float* speed) {
    __try {
        if (!speed) {
            return;
        }
        const float limit = ReadGameFloat(kClimbSpeedLimit, 0.2f);
        const float length = std::sqrt(speed[0] * speed[0]
                                       + speed[1] * speed[1]
                                       + speed[2] * speed[2]);
        if (!std::isfinite(length) || length <= limit || length <= 0.0f) {
            return;
        }
        const float scale = limit / length;
        speed[0] *= scale;
        speed[1] *= scale;
        speed[2] *= scale;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

// The animation shift is a displacement for one frame, so turning it into a
// speed in original timestep units means dividing by the ratio. The original
// values are put back the moment the swim task is done with them: the same
// field drives walking and running, and only this one call may see it changed.
float g_swimShiftSaved[2]{};
bool g_swimShiftScaled{};

void __cdecl ScaleSwimAnimShift(uintptr_t ped) {
    g_swimShiftScaled = false;
    __try {
        // Recheck the shared function at run time as well as installation.
        // Some ASI loaders install scripts-directory plugins after this one;
        // if Tweaker or Swim FPS Fix appears later, this wrapper remains in
        // place but becomes an identity instead of double-scaling the shift.
        if (!ped || !SwimmingMovementCodeIsUnmodified()) {
            return;
        }
        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            return;
        }
        auto* shift = reinterpret_cast<float*>(ped + kPedAnimMovingShift);
        g_swimShiftSaved[0] = shift[0];
        g_swimShiftSaved[1] = shift[1];
        const float scale = kOriginalTimeStep / timeStep;
        shift[0] *= scale;
        shift[1] *= scale;
        g_swimShiftScaled = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_swimShiftScaled = false;
    }
}

void __cdecl RestoreSwimAnimShift(uintptr_t ped) {
    __try {
        if (g_swimShiftScaled && ped) {
            auto* shift = reinterpret_cast<float*>(ped + kPedAnimMovingShift);
            shift[0] = g_swimShiftSaved[0];
            shift[1] = g_swimShiftSaved[1];
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    g_swimShiftScaled = false;
}


// A ped colliding with a vehicle applies a constant per-frame push force.
// The site sits inside `CPhysical::ApplyCollision`, where `this` is the pushing
// ped and `esi` is the vehicle, and the vector being built is the
// `vecEntityMoveForce` that `entity->ApplyForce` turns straight into a velocity
// change of `force / m_fMass`. The impulse converges the vehicle onto the ped's
// contact speed by a fixed fraction of the remaining difference every rendered
// frame, so it is the same class of per-frame process as the turn damping.
//
uint32_t g_pedPushLastFrame{};
float g_pedPushCarry{};
int32_t g_pedPushTick = 1;

// Telemetry for that measurement. The trace thread reports how much velocity
// the vehicle was actually handed per unit of real time, which is the number
// that has to match between a capped and an uncapped run.
volatile uint32_t g_pushApplications{};
volatile float g_pushDeltaVSum{};
volatile float g_pushCarSpeedPeak{};
volatile float g_pushPedSpeed{};
// Distance the pushed vehicle actually covered, which is the quantity that has
// to match between frame rates: with the tangential speed cancelled every frame
// the vehicle only travels `dv * timeStep` per frame, and `timeStep * FPS` is a
// constant, so the same impulse should carry it the same distance per second.
float g_pushFirstPos[3]{};
float g_pushLastPos[3]{};
bool g_pushHavePos{};

// Individual impulses, so the per-frame convergence fraction can be read off
// their decay instead of being guessed. While the ped walks at a steady speed
// the target is constant, so the remaining difference, and with it each
// impulse, shrinks by the same factor every frame. That factor is `1 - a`, and
// `a` is the only thing needed to convert this into a real-time rate.
struct PushSample {
    uint32_t frame;
    float deltaV;
    float carSpeed;
    float timeStep;
};
constexpr size_t kPushSampleSlots = 512;
constexpr uint32_t kPushSampleLimit = 900;
PushSample g_pushSamples[kPushSampleSlots]{};
volatile uint32_t g_pushWriteIndex{};
uint32_t g_pushReadIndex{};
uint32_t g_pushSamplesLogged{};

// The impulse is not weakened; it is delivered at the rate the original engine
// delivered it. Scaling the magnitude was wrong: at low speed the tangential
// speed is cancelled by the resting contact every frame anyway, so a weakened
// impulse simply never moves the vehicle, which is why the vehicle became
// unpushable. What actually runs away with the frame rate is how often the
// impulse lands. Each one overshoots the common contact speed by the elasticity
// term, so repeating it sixteen times more often per second pumps in sixteen
// times the energy, and a walking ped drives a car to about 36 km/h.
//
// The decision is made once per game frame and shared by every contact point,
// so all of a frame's contacts either land together or are skipped together.
int32_t __cdecl ShouldApplyPedPushImpulse() {
    __try {
        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        if (frame != g_pedPushLastFrame) {
            g_pedPushLastFrame = frame;
            g_pedPushCarry += TimeStepRatio();
            if (g_pedPushCarry >= 1.0f) {
                g_pedPushCarry -= std::floor(g_pedPushCarry);
                g_pedPushTick = 1;
            } else {
                g_pedPushTick = 0;
            }
        }
        return g_pedPushTick;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

void __cdecl ScalePedPushCarForce(uintptr_t stackFrame, uintptr_t vehicle) {
    __try {
        auto* force = reinterpret_cast<float*>(stackFrame + 0x20);
        const float scale =
            ShouldApplyPedPushImpulse() != 0 ? 1.0f : 0.0f;
        force[0] *= scale;
        force[1] *= scale;
        force[2] *= scale;
        if (scale == 0.0f) {
            return;
        }

        if (!g_diagnosticActive || !vehicle) {
            return;
        }
        const float mass =
            *reinterpret_cast<const float*>(vehicle + kPhysicalMass);
        const float applied = mass > 0.0f ? VectorLength(force) / mass : 0.0f;
        g_pushDeltaVSum += applied;
        const auto* move =
            reinterpret_cast<const float*>(vehicle + kPhysicalMoveSpeed);
        const float speed = VectorLength(move);
        if (speed > g_pushCarSpeedPeak) {
            g_pushCarSpeedPeak = speed;
        }
        ++g_pushApplications;

        if (g_watchMode == 1) {
            g_watchCandidate = vehicle;
        }

        // The vehicle should end up moving at about the speed of the ped
        // pushing it, never several times faster.
        const auto player = *reinterpret_cast<const uintptr_t*>(kWorldPlayers);
        if (player) {
            g_pushPedSpeed = VectorLength(
                reinterpret_cast<const float*>(player + kPhysicalMoveSpeed));
        }

        const auto matrix = *reinterpret_cast<const uintptr_t*>(vehicle + kEntityMatrix);
        if (matrix) {
            const auto* position =
                reinterpret_cast<const float*>(matrix + kMatrixPosition);
            if (!g_pushHavePos) {
                std::memcpy(g_pushFirstPos, position, sizeof(g_pushFirstPos));
                g_pushHavePos = true;
            }
            std::memcpy(g_pushLastPos, position, sizeof(g_pushLastPos));
        }

        if (g_pushSamplesLogged < kPushSampleLimit && scale > 0.0f) {
            const uint32_t index = g_pushWriteIndex;
            PushSample& sample = g_pushSamples[index % kPushSampleSlots];
            sample.frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
            // Recorded before scaling, because the decay being measured is the
            // original engine's, not this plugin's.
            sample.deltaV = applied / scale;
            sample.carSpeed = speed;
            sample.timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
            g_pushWriteIndex = index + 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

// ---------------------------------------------------------------------------
// Vehicles
// ---------------------------------------------------------------------------

float __cdecl GetFrameIndependentWheelFriction() {
    return ReadGameFloat(kWheelFriction, 0.9f) * TimeStepRatio();
}

float __cdecl GetSkimmerResistance() {
    return ReadGameFloat(kSkimmerResistanceConstant, 30.0f) * TimeStepRatio();
}

// The 0.99 is read through the game constant so mods that repoint it keep
// working. Raising it to the timestep ratio rather than the raw timestep keeps
// the result bit-exact `0.99` at 30 FPS, where the ratio is one.
float __cdecl GetTurnAirResistanceFactor() {
    const float base = ReadGameFloat(kTurnAirResistanceConstant, 0.99f);
    return std::pow(base, TimeStepRatio());
}

float __cdecl GetBurnoutWheelSpeed() {
    return ReadGameFloat(kBurnoutConstant, 3000.0f) * TimeStepRatio();
}

// The push-out is a distance, not a decay, and the overlap it works against is
// held by the shape the vehicle is riding rather than created by the previous
// frame, so the fraction is scaled linearly: the same depth then leaves the
// entity at the same speed per second whatever the frame rate. The ratio is
// never raised above one, so at 30 FPS and below the stock constant is used
// unchanged. Both constants live in writable globals and are read through the
// original operand, so a mod that repoints or retunes them keeps working.
// A lerp weight, so what has to hold across frames is the fraction of the gap
// left over: `1 - weight` per original frame becomes `(1 - weight)` raised to
// the timestep ratio per rendered frame. The ratio is capped at one so 30 FPS
// and below keep the stock weight exactly.
float __cdecl GetWheelSettleWeight() {
    const float weight = ReadGameFloat(kWheelSettleConstant, 0.75f);
    const float ratio = std::min(TimeStepRatio(), 1.0f);
    if (!(weight > 0.0f) || weight >= 1.0f || ratio >= 1.0f) {
        return weight;
    }
    return 1.0f - std::pow(1.0f - weight, ratio);
}

float __cdecl GetPushOutScaleMain() {
    return ReadGameFloat(kPushOutConstantMain, 0.75f)
         * std::min(TimeStepRatio(), 1.0f);
}

float __cdecl GetPushOutScaleAlt() {
    return ReadGameFloat(kPushOutConstantAlt, 1.5f)
         * std::min(TimeStepRatio(), 1.0f);
}

// The rotor speed constant is reached through the original instruction operand
// so mods that repoint it keep working.
float ReadHeliRotorFinalSpeed() {
    __try {
        const auto operand = *reinterpret_cast<const uintptr_t*>(
            kHeliRotorSpeedOperand + 2);
        return ReadGameFloat(operand, 0.22f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0.22f;
    }
}

float __cdecl GetHeliRotorSlowStep() {
    return (ReadHeliRotorFinalSpeed() / kHeliRotorSpeedDivisor)
         * TimeStepRatio();
}

float __cdecl GetHeliRotorFastStep() {
    return (ReadHeliRotorFinalSpeed() / kHeliRotorSpeedDivisor) * 3.0f
         * TimeStepRatio();
}

bool NearlyEqual(float a, float b) {
    return std::fabs(a - b) < 0.002f;
}

void WriteGameFloat(uintptr_t address, float value) {
    *reinterpret_cast<float*>(address) = value;
}

bool WriteProtectedGameFloat(uintptr_t address, float value) {
    DWORD oldProtect{};
    if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(value),
                        PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    WriteGameFloat(address, value);
    DWORD ignored{};
    VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), oldProtect,
                   &ignored);
    return true;
}

bool g_dampingLimitActive{};

DWORD WINAPI SuspensionDampingLimitThread(void*) {
    float lastRatio = -1.0f;
    while (g_dampingLimitActive) {
        float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep)) {
            timeStep = kOriginalTimeStep;
        }
        timeStep = std::clamp(timeStep, 0.01f, 3.0f);
        const float ratio = std::min(1.0f, timeStep / kOriginalTimeStep);

        if (std::fabs(ratio - lastRatio) > 0.0005f) {
            lastRatio = ratio;
            WriteGameFloat(kDampingLimitInFrame,
                           kStockDampingLimitInFrame * ratio);
        }
        Sleep(1);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Experimental bike ramp pitch isolation
// ---------------------------------------------------------------------------

// The wheel-contact call at 0x6D7B17 is the first known source of the excess
// backward angular speed seen at takeoff. Preserve the complete angular-velocity
// delta produced by the stock ApplyTurnForce except for the excess positive
// pitch produced during takeoff: both front suspension lines are clear, the
// bike has meaningful positive world-Z velocity, and a rear or lingering wheel
// contact still reaches ProcessBikeWheel. This catches the phase where the rear
// wheel continues to pitch the bike after the front has left a ramp without
// changing a wheelie on level ground. Negative (nose-down) pitch and 30 FPS or
// below are exact no-ops.
uintptr_t g_bikePitchExperimentBike{};
float g_bikePitchExperimentBefore[3]{};
float g_bikePitchExperimentAxis[3]{};
float g_bikePitchExperimentStrength{0.5f};
float g_bikePitchExperimentFrameCorrection{};
bool g_bikePitchExperimentActive{};

void __cdecl BeginBikePitchExperiment(uintptr_t bike) {
    g_bikePitchExperimentActive = false;
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (!bike || !std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            return;
        }

        // The correction exists for the rider-generated takeoff pitch. Once
        // RemoveDriver changes the packed entity status to STATUS_ABANDONED,
        // an ordinary ground bounce can otherwise satisfy the same wheel-line
        // and vertical-speed conditions and have its stock angular response
        // suppressed. Leave every riderless and non-player bike completely
        // untouched.
        const uint8_t status = *reinterpret_cast<const uint8_t*>(
            bike + kEntityTypeAndStatus) >> 3;
        if (status != 0) { // STATUS_PLAYER
            return;
        }

        const auto* wheelRatios = reinterpret_cast<const float*>(
            bike + kBikeWheelRatios);
        const auto* contactTimers = reinterpret_cast<const float*>(
            bike + kBikeWheelContactTimers);
        bool hasLingeringWheelContact = false;
        for (size_t i = 0; i < 4; ++i) {
            if (!std::isfinite(wheelRatios[i])
                || !std::isfinite(contactTimers[i])) {
                return;
            }
            hasLingeringWheelContact |= contactTimers[i] > 0.0f;
        }
        const bool hasFrontWheelContact = wheelRatios[0] < 1.0f
                                       || wheelRatios[1] < 1.0f;
        const float verticalSpeed = *reinterpret_cast<const float*>(
            bike + kPhysicalMoveSpeed + 2 * sizeof(float));
        if (!std::isfinite(verticalSpeed) || hasFrontWheelContact
            || !hasLingeringWheelContact || verticalSpeed <= 0.02f) {
            return;
        }

        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            bike + kEntityMatrix);
        if (!matrix) {
            return;
        }

        const auto* turn = reinterpret_cast<const float*>(
            bike + kPhysicalTurnSpeed);
        const auto* right = reinterpret_cast<const float*>(
            matrix + kMatrixRight);
        float axisLengthSquared = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            if (!std::isfinite(turn[i]) || !std::isfinite(right[i])) {
                return;
            }
            g_bikePitchExperimentBefore[i] = turn[i];
            g_bikePitchExperimentAxis[i] = right[i];
            axisLengthSquared += right[i] * right[i];
        }
        if (!std::isfinite(axisLengthSquared) || axisLengthSquared < 0.25f) {
            return;
        }

        g_bikePitchExperimentBike = bike;
        g_bikePitchExperimentFrameCorrection = std::clamp(
            1.0f - timeStep / kOriginalTimeStep, 0.0f, 1.0f);
        g_bikePitchExperimentActive = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bikePitchExperimentActive = false;
    }
}

void __cdecl FinishBikePitchExperiment() {
    if (!g_bikePitchExperimentActive) {
        return;
    }
    g_bikePitchExperimentActive = false;

    __try {
        auto* turn = reinterpret_cast<float*>(
            g_bikePitchExperimentBike + kPhysicalTurnSpeed);
        float pitchDelta = 0.0f;
        float axisLengthSquared = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            const float delta = turn[i] - g_bikePitchExperimentBefore[i];
            pitchDelta += delta * g_bikePitchExperimentAxis[i];
            axisLengthSquared += g_bikePitchExperimentAxis[i]
                               * g_bikePitchExperimentAxis[i];
        }
        if (!std::isfinite(pitchDelta) || pitchDelta <= 0.0f
            || !std::isfinite(axisLengthSquared)
            || axisLengthSquared < 0.25f) {
            return;
        }

        const float projection = (pitchDelta / axisLengthSquared)
                               * g_bikePitchExperimentStrength
                               * g_bikePitchExperimentFrameCorrection;
        for (size_t i = 0; i < 3; ++i) {
            turn[i] -= g_bikePitchExperimentAxis[i] * projection;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Entered through the original five-byte call. ApplyTurnForce ends in `ret 18h`,
// so an ordinary nested call would put this wrapper's return address where the
// callee expects its first float and would then corrupt the caller's stack. Make
// a private copy of all six arguments for the nested call and reproduce the
// original `ret 18h` when the wrapper itself returns.
__declspec(naked) void BikePitchExperimentThunk() {
    __asm {
        pushfd
        pushad
        push ecx
        call BeginBikePitchExperiment
        add esp, 4
        popad
        popfd
        mov eax, esp
        push dword ptr [eax + 0x18]
        push dword ptr [eax + 0x14]
        push dword ptr [eax + 0x10]
        push dword ptr [eax + 0x0C]
        push dword ptr [eax + 0x08]
        push dword ptr [eax + 0x04]
        call kApplyTurnForce
        pushfd
        pushad
        call FinishBikePitchExperiment
        popad
        popfd
        ret 0x18
    }
}

// ---------------------------------------------------------------------------
// HUD flashing
// ---------------------------------------------------------------------------

// Ported from the standalone HUD Flash Rate Fix.
//
// GTA flashes HUD elements by testing a bit of the frame counter:
//
//     if (CHud::m_ItemToFlash == ITEM && CTimer::m_FrameCounter & 8) -> skip
//
// The same check hides the health bar below 10 health. The period is measured
// in frames rather than in time, so with the frame limiter off the radar,
// flashed by the tutorial scripts, and the low health bar turn into a strobe.
//
// Each flash site reads a single byte through an absolute address operand, so
// no code is rewritten: those six operands are repointed at a plugin counter
// that advances in real time at 25 ticks per second. The game then sees the
// bit pattern it would have seen at 25 FPS, and every other use of
// `CTimer::m_FrameCounter` is left alone.

constexpr std::array<uint8_t, 2> kHudTestPrefix{0xF6, 0x05}; // test byte, imm8
constexpr std::array<uint8_t, 2> kHudMovPrefix{0x8A, 0x1D};  // mov bl, byte

template <size_t Size>
bool HudFlashSiteMatches(uintptr_t operand,
                         const std::array<uint8_t, Size>& prefix) {
    __try {
        if (std::memcmp(reinterpret_cast<const void*>(operand - prefix.size()),
                        prefix.data(), prefix.size()) != 0) {
            return false;
        }
        return *reinterpret_cast<const uintptr_t*>(operand) == kFrameCounter;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool HudFlashTestSiteMatches(uintptr_t operand, uint8_t mask) {
    if (!HudFlashSiteMatches(operand, kHudTestPrefix)) {
        return false;
    }
    __try {
        return *reinterpret_cast<const uint8_t*>(operand + sizeof(uintptr_t))
            == mask;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void RepointHudFlashOperand(uintptr_t operand, const volatile uint32_t* counter) {
    const auto address = reinterpret_cast<uintptr_t>(counter);
    WriteBytes(operand, reinterpret_cast<const uint8_t*>(&address),
               sizeof(address));
}

DWORD WINAPI HudFlashThread(void*) {
    const uint32_t tickMs = g_hudFlashIntervalMs / kHudTicksPerFlash;
    while (g_hudFlashActive) {
        // Advance off the game clock rather than wall time, so flashing freezes
        // while the game is paused, exactly as the frame counter does.
        __try {
            g_hudFlashClock =
                *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds)
                / tickMs;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        Sleep(5);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Optional vehicle state trace
// ---------------------------------------------------------------------------

// Samples the vehicle the player is riding and writes one line per sample to
// `HighFpsFixes.trace.log`. This exists to diagnose frame-rate-dependent
// behavior that cannot be identified from the disassembly alone; it patches
// nothing and is disabled by default.

struct VehicleSample {
    uintptr_t vehicle;
    uint32_t time;
    float timeStep;
    float timeScale;
    float posZ;
    float move[3];
    float turn[3];
    float force[3];
    float torque[3];
    float friction[3];
    float mass;
    float airResistance;
    float barSteer;
    float lean;
    float desiredLean;
    float sprintLean;
    float brakePedal;
    float gasPedal;
    float wheelAngularVelocity[2];
    float localPitchTurnSpeed;
    uint32_t wheelTurnCalls;
    float wheelTurnPitchImpulse;
    uint32_t entityFlags;
    uint32_t physicalFlags;
    uint32_t lastCollisionTime;
    uintptr_t attachedTo;
    uint32_t processCalls;
    uint32_t gravityCalls;
    uint32_t leanWrites[3];
    float rightZ;
    float mvZAfterGravity;
    float upZ;
    uint8_t fakePhysics;
    uint8_t status;
    uint8_t subClass;
    uint8_t contactWheels;
};

// Counts CBike::ProcessControl entries for the vehicle the player is riding,
// so the trace can tell "physics ran and did nothing" from "physics never ran".
volatile uint32_t g_bikeProcessCalls{};
volatile uint32_t g_gravityCalls{};
volatile float g_moveSpeedAfterGravity{};
uint32_t g_leanWrites[3]{};
using ThisCallVoidFn = void(__thiscall*)(void*);
DetourPatch g_bikeProcessPatch{};
DetourPatch g_applyGravityPatch{};
DetourPatch g_abandonedBikeCollisionPatch{};
DetourPatch g_abandonedBikeShiftPatch{};
DetourPatch g_abandonedBikeRwFramePatch{};
std::array<SitePatch, 3> g_leanWritePatches{};

// A riderless bike is the one remaining vehicle case where mathematically
// scaling individual constants does not reproduce the 30 FPS outcome. Contact
// generation, collision retries, suspension and wheel impulses form one
// nonlinear step. This experiment therefore preserves that whole step: above
// 30 FPS an abandoned bike runs ProcessControl, ProcessCollision and
// ProcessShift at the original cadence and with the original timestep. Player
// vehicles and every other physical entity remain on the normal high-FPS path.
bool g_abandonedBikePhysicsStepEnabled{};
uint32_t g_abandonedBikePhysicsLastFrame{};
float g_abandonedBikePhysicsCredit{};
bool g_abandonedBikePhysicsTick{};

struct AbandonedBikeRenderState {
    void* bike{};
    std::array<float, 12> previous{};
    std::array<float, 12> current{};
    bool valid{};
    bool previousCaptured{};
};

std::array<AbandonedBikeRenderState, 16> g_abandonedBikeRenderStates{};

AbandonedBikeRenderState* FindAbandonedBikeRenderState(void* bike,
                                                       bool create) {
    AbandonedBikeRenderState* empty = nullptr;
    for (auto& state : g_abandonedBikeRenderStates) {
        if (state.bike == bike) {
            return &state;
        }
        if (!state.bike && !empty) {
            empty = &state;
        }
    }
    if (!create) {
        return nullptr;
    }
    auto* state = empty ? empty : &g_abandonedBikeRenderStates[0];
    *state = {};
    state->bike = bike;
    return state;
}

bool CopyBikeTransform(void* bike, std::array<float, 12>& out) {
    __try {
        const auto address = reinterpret_cast<uintptr_t>(bike);
        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            address + kEntityMatrix);
        if (!matrix) {
            return false;
        }
        constexpr std::array<size_t, 4> offsets{0x00, 0x10, 0x20, 0x30};
        for (size_t vector = 0; vector < offsets.size(); ++vector) {
            const auto* source = reinterpret_cast<const float*>(
                matrix + offsets[vector]);
            for (size_t axis = 0; axis < 3; ++axis) {
                const float value = source[axis];
                if (!std::isfinite(value)) {
                    return false;
                }
                out[vector * 3 + axis] = value;
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool WriteBikeTransform(void* bike, const std::array<float, 12>& transform) {
    __try {
        const auto address = reinterpret_cast<uintptr_t>(bike);
        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            address + kEntityMatrix);
        if (!matrix) {
            return false;
        }
        constexpr std::array<size_t, 4> offsets{0x00, 0x10, 0x20, 0x30};
        for (size_t vector = 0; vector < offsets.size(); ++vector) {
            auto* destination = reinterpret_cast<float*>(
                matrix + offsets[vector]);
            for (size_t axis = 0; axis < 3; ++axis) {
                destination[axis] = transform[vector * 3 + axis];
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void NormalizeRenderVector(std::array<float, 12>& transform, size_t base) {
    const float lengthSquared = transform[base] * transform[base]
                              + transform[base + 1] * transform[base + 1]
                              + transform[base + 2] * transform[base + 2];
    if (lengthSquared < 0.000001f || !std::isfinite(lengthSquared)) {
        return;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    transform[base] *= inverseLength;
    transform[base + 1] *= inverseLength;
    transform[base + 2] *= inverseLength;
}

std::array<float, 12> InterpolateBikeTransform(
    const AbandonedBikeRenderState& state, float alpha) {
    std::array<float, 12> out{};
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = state.previous[i]
               + (state.current[i] - state.previous[i]) * alpha;
    }

    // Nlerp the basis and remove accumulated shear. A 30 Hz step is small
    // enough that this follows the short rotation arc without a quaternion.
    NormalizeRenderVector(out, 0);
    const float projection = out[3] * out[0] + out[4] * out[1]
                           + out[5] * out[2];
    out[3] -= out[0] * projection;
    out[4] -= out[1] * projection;
    out[5] -= out[2] * projection;
    NormalizeRenderVector(out, 3);
    out[6] = out[1] * out[5] - out[2] * out[4];
    out[7] = out[2] * out[3] - out[0] * out[5];
    out[8] = out[0] * out[4] - out[1] * out[3];
    NormalizeRenderVector(out, 6);
    return out;
}

void BeginAbandonedBikePhysicsStep(void* bike) {
    auto* state = FindAbandonedBikeRenderState(bike, true);
    state->valid = false;
    state->previousCaptured = CopyBikeTransform(bike, state->previous);
}

void FinishAbandonedBikePhysicsStep(void* bike) {
    auto* state = FindAbandonedBikeRenderState(bike, true);
    state->valid = state->previousCaptured
                && CopyBikeTransform(bike, state->current);
}

bool IsAbandonedBike(const void* entity) {
    __try {
        if (!entity) {
            return false;
        }
        const auto address = reinterpret_cast<uintptr_t>(entity);
        constexpr uint8_t kTypeVehicle = 2;
        constexpr uint8_t kStatusAbandoned = 4;
        const uint8_t packed = *reinterpret_cast<const uint8_t*>(
            address + kEntityTypeAndStatus);
        if ((packed & 0x07) != kTypeVehicle) {
            return false;
        }
        const uint8_t status = packed >> 3;
        const uint8_t subClass = *reinterpret_cast<const uint8_t*>(
            address + kVehicleSubClass);
        return status == kStatusAbandoned
            && (subClass == 9 || subClass == 10);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ShouldRunAbandonedBikePhysicsStep() {
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            g_abandonedBikePhysicsTick = true;
            return true;
        }

        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        if (frame != g_abandonedBikePhysicsLastFrame) {
            uint32_t elapsedFrames = frame - g_abandonedBikePhysicsLastFrame;
            if (g_abandonedBikePhysicsLastFrame == 0 || elapsedFrames > 10000) {
                elapsedFrames = 1;
            }
            g_abandonedBikePhysicsLastFrame = frame;
            g_abandonedBikePhysicsCredit +=
                timeStep / kOriginalTimeStep * static_cast<float>(elapsedFrames);
            if (g_abandonedBikePhysicsCredit >= 1.0f) {
                g_abandonedBikePhysicsCredit -=
                    std::floor(g_abandonedBikePhysicsCredit);
                g_abandonedBikePhysicsTick = true;
            } else {
                g_abandonedBikePhysicsTick = false;
            }
        }
        return g_abandonedBikePhysicsTick;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_abandonedBikePhysicsTick = true;
        return true;
    }
}

void CallAbandonedBikePhysicsStep(DetourPatch& patch, void* entity) {
    float savedTimeStep = kOriginalTimeStep;
    bool changed = false;
    __try {
        savedTimeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        changed = std::isfinite(savedTimeStep) && savedTimeStep > 0.0f
               && savedTimeStep < kOriginalTimeStep;
        if (changed) {
            *reinterpret_cast<float*>(kTimerTimeStep) = kOriginalTimeStep;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        changed = false;
    }

    reinterpret_cast<ThisCallVoidFn>(patch.gateway)(entity);

    if (changed) {
        __try {
            *reinterpret_cast<float*>(kTimerTimeStep) = savedTimeStep;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

// One sample is published per final bike balance force. It is intentionally
// only diagnostic state: the hooks below reproduce the displaced instructions
// and pass the exact original vector to CPhysical::ApplyTurnForce.
struct BikeBalanceSample {
    uint32_t time{};
    float timeStep{};
    float local34{};
    float coefficientA{};
    float coefficientB{};
    float input68{};
    float input6C{};
    float input78{};
    float force[3]{};
    float turnY{};
    uint8_t contactWheels{};
};

struct BikeBalanceWindow {
    uint32_t time{};
    uint32_t calls{};
    float elapsed{};
    float peakInput68{};
    float sumInput68{};
    float sumInput6C{};
    float force[3]{};
};

BikeBalanceSample g_bikeBalanceSample{};
volatile LONG g_bikeBalanceSequence{};
BikeBalanceWindow g_bikeBalanceWindow{};
BikeBalanceWindow g_bikeBalanceWindowSnapshot{};
volatile LONG g_bikeBalanceWindowSequence{};
SitePatch g_bikeBalanceInputPatch{};
SitePatch g_bikeBalanceForcePatch{};
SitePatch g_bikeWheelTurnTracePatch{};

// Cumulative, exception-free telemetry for the wheel-contact turn force that
// supplies the excess backward pitch on a ramp. Unlike the hardware data
// breakpoint this adds one ordinary call at the already identified site and
// therefore does not trap every angular-velocity write in the physics engine.
volatile uint32_t g_bikeWheelTurnCalls{};
volatile float g_bikeWheelTurnPitchImpulse{};

bool IsThePlayerVehicle(const void* entity);

void __cdecl RecordBikeWheelTurnForce(uintptr_t bike, const float* arguments) {
    if (!IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
        return;
    }

    __try {
        const auto matrix = *reinterpret_cast<const uintptr_t*>(bike + kEntityMatrix);
        const float turnMass = *reinterpret_cast<const float*>(bike + kPhysicalMass + 4);
        if (!matrix || !std::isfinite(turnMass) || turnMass <= 0.0f) {
            return;
        }

        const float* force = arguments;
        float point[3]{arguments[3], arguments[4], arguments[5]};
        const auto* right = reinterpret_cast<const float*>(matrix + kMatrixRight);
        const auto* up = reinterpret_cast<const float*>(matrix + kMatrixUp);
        const auto* forward = reinterpret_cast<const float*>(matrix + 0x20);
        const auto* centre = reinterpret_cast<const float*>(bike + 0xA4);
        for (size_t i = 0; i < 3; ++i) {
            point[i] -= right[i] * centre[0]
                      + forward[i] * centre[1]
                      + up[i] * centre[2];
        }

        const float delta[3]{
            (point[1] * force[2] - point[2] * force[1]) / turnMass,
            (point[2] * force[0] - point[0] * force[2]) / turnMass,
            (point[0] * force[1] - point[1] * force[0]) / turnMass
        };
        g_bikeWheelTurnPitchImpulse += delta[0] * right[0]
                                     + delta[1] * right[1]
                                     + delta[2] * right[2];
        ++g_bikeWheelTurnCalls;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

__declspec(naked) void BikeWheelTurnTraceThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x28]
        push eax
        push ecx
        call RecordBikeWheelTurnForce
        add esp, 8
        popad
        popfd
        jmp kApplyTurnForce
    }
}

bool IsThePlayerVehicle(const void* entity) {
    __try {
        const auto ped = *reinterpret_cast<uintptr_t*>(kWorldPlayers);
        if (!ped) {
            return false;
        }
        return *reinterpret_cast<uintptr_t*>(ped + kPedVehicle)
            == reinterpret_cast<uintptr_t>(entity);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void __fastcall HookedBikeProcessControl(void* bike, void*) {
    if (IsThePlayerVehicle(bike)) {
        ++g_bikeProcessCalls;
    }
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(bike)) {
        if (!ShouldRunAbandonedBikePhysicsStep()) {
            // Physics remains at the last complete 30 Hz state, but refresh
            // the RenderWare hierarchy with an interpolated transform.
            reinterpret_cast<ThisCallVoidFn>(kEntityUpdateRwFrame)(bike);
            return;
        }
        BeginAbandonedBikePhysicsStep(bike);
        CallAbandonedBikePhysicsStep(g_bikeProcessPatch, bike);
        return;
    }
    if (g_abandonedBikePhysicsStepEnabled) {
        if (auto* state = FindAbandonedBikeRenderState(bike, false)) {
            state->valid = false;
            state->previousCaptured = false;
        }
    }
    reinterpret_cast<ThisCallVoidFn>(g_bikeProcessPatch.gateway)(bike);
}

void __fastcall HookedPhysicalProcessCollision(void* physical, void*) {
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(physical)) {
        if (!g_abandonedBikePhysicsTick) {
            __try {
                // Prevent CWorld from retrying the deliberately skipped step.
                *reinterpret_cast<uint8_t*>(
                    reinterpret_cast<uintptr_t>(physical) + kEntityFlags)
                    |= 0x20; // m_bIsInSafePosition
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
            return;
        }
        CallAbandonedBikePhysicsStep(g_abandonedBikeCollisionPatch, physical);
        FinishAbandonedBikePhysicsStep(physical);
        return;
    }
    reinterpret_cast<ThisCallVoidFn>(
        g_abandonedBikeCollisionPatch.gateway)(physical);
}

void __fastcall HookedPhysicalProcessShift(void* physical, void*) {
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(physical)) {
        if (!g_abandonedBikePhysicsTick) {
            __try {
                *reinterpret_cast<uint8_t*>(
                    reinterpret_cast<uintptr_t>(physical) + kEntityFlags)
                    |= 0x20; // m_bIsInSafePosition
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
            return;
        }
        CallAbandonedBikePhysicsStep(g_abandonedBikeShiftPatch, physical);
        FinishAbandonedBikePhysicsStep(physical);
        return;
    }
    reinterpret_cast<ThisCallVoidFn>(g_abandonedBikeShiftPatch.gateway)(
        physical);
}

void __fastcall HookedEntityUpdateRwFrame(void* entity, void*) {
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(entity)) {
        auto* state = FindAbandonedBikeRenderState(entity, false);
        if (state && state->valid) {
            std::array<float, 12> physicalTransform{};
            if (CopyBikeTransform(entity, physicalTransform)) {
                const auto renderTransform = InterpolateBikeTransform(
                    *state, g_abandonedBikePhysicsCredit);
                if (WriteBikeTransform(entity, renderTransform)) {
                    // UpdateRwMatrix is already detoured by the modpack. Call
                    // its public entry so that compatibility hook still runs.
                    reinterpret_cast<ThisCallVoidFn>(
                        kEntityUpdateRwMatrix)(entity);
                    WriteBikeTransform(entity, physicalTransform);
                }
            }
        }
    }
    reinterpret_cast<ThisCallVoidFn>(g_abandonedBikeRwFramePatch.gateway)(
        entity);
}

// Gravity is called every frame during the freeze, so the interesting value is
// what the move speed looks like the instant gravity returns. If it is non-zero
// there and zero by sample time, something downstream is wiping it.
void __fastcall HookedApplyGravity(void* physical, void*) {
    g_gameThreadId = GetCurrentThreadId();
    const bool isPlayer = IsThePlayerVehicle(physical);
    if (isPlayer) {
        ++g_gravityCalls;
    }
    reinterpret_cast<ThisCallVoidFn>(g_applyGravityPatch.gateway)(physical);
    if (isPlayer) {
        __try {
            g_moveSpeedAfterGravity = *reinterpret_cast<const float*>(
                reinterpret_cast<uintptr_t>(physical) + kPhysicalMoveSpeed + 8);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

void __cdecl RecordBikeBalanceInputs(uintptr_t bike, const float* stack) {
    if (!IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
        return;
    }

    __try {
        g_bikeBalanceSample.time = *reinterpret_cast<const uint32_t*>(
            kTimerTimeInMilliseconds);
        g_bikeBalanceSample.timeStep = *reinterpret_cast<const float*>(
            kTimerTimeStep);
        g_bikeBalanceSample.local34 = stack[0x34 / sizeof(float)];
        g_bikeBalanceSample.coefficientA = stack[0x3C / sizeof(float)];
        g_bikeBalanceSample.coefficientB = stack[0x40 / sizeof(float)];
        g_bikeBalanceSample.input68 = stack[0x68 / sizeof(float)];
        g_bikeBalanceSample.input6C = stack[0x6C / sizeof(float)];
        g_bikeBalanceSample.input78 = stack[0x78 / sizeof(float)];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void __cdecl RecordBikeBalanceForce(uintptr_t bike, const float* force) {
    if (!IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
        return;
    }

    __try {
        std::memcpy(g_bikeBalanceSample.force, force, sizeof(g_bikeBalanceSample.force));
        g_bikeBalanceSample.turnY = *reinterpret_cast<const float*>(
            bike + kPhysicalTurnSpeed + sizeof(float));
        g_bikeBalanceSample.contactWheels = *reinterpret_cast<const uint8_t*>(
            bike + kBikeContactWheels);

        const float timeStep = g_bikeBalanceSample.timeStep;
        if (std::isfinite(timeStep) && timeStep > 0.0f
            && timeStep <= kOriginalTimeStep * 1.1f) {
            g_bikeBalanceWindow.time = g_bikeBalanceSample.time;
            ++g_bikeBalanceWindow.calls;
            g_bikeBalanceWindow.elapsed += timeStep;
            g_bikeBalanceWindow.peakInput68 = std::max(
                g_bikeBalanceWindow.peakInput68,
                std::fabs(g_bikeBalanceSample.input68));
            g_bikeBalanceWindow.sumInput68 += g_bikeBalanceSample.input68;
            g_bikeBalanceWindow.sumInput6C += g_bikeBalanceSample.input6C;
            for (size_t i = 0; i < 3; ++i) {
                g_bikeBalanceWindow.force[i] += g_bikeBalanceSample.force[i];
            }
            if (g_bikeBalanceWindow.elapsed >= kOriginalTimeStep) {
                g_bikeBalanceWindowSnapshot = g_bikeBalanceWindow;
                g_bikeBalanceWindow.elapsed -= kOriginalTimeStep;
                g_bikeBalanceWindow.calls = 0;
                g_bikeBalanceWindow.peakInput68 = 0.0f;
                g_bikeBalanceWindow.sumInput68 = 0.0f;
                g_bikeBalanceWindow.sumInput6C = 0.0f;
                std::memset(g_bikeBalanceWindow.force, 0,
                            sizeof(g_bikeBalanceWindow.force));
                InterlockedIncrement(&g_bikeBalanceWindowSequence);
            }
        }
        InterlockedIncrement(&g_bikeBalanceSequence);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// `kBikeBalanceInput` starts with `fld [esp+68h]; fmul [esp+3Ch]`. The hook is
// entered with an empty x87 stack, records the completed local coefficients,
// then runs those exact instructions before continuing the stock calculation.
__declspec(naked) void BikeBalanceInputThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x24]
        push eax
        mov eax, esi
        push eax
        call RecordBikeBalanceInputs
        add esp, 8
        popad
        popfd
        fld dword ptr [esp + 0x68]
        fmul dword ptr [esp + 0x3C]
        jmp kBikeBalanceInputReturn
    }
}

// This is entered through the original five-byte call. The force argument is
// at `[esp+4]` before saving registers and `[esp+28h]` afterwards.
__declspec(naked) void BikeBalanceForceThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x28]
        push eax
        mov eax, esi
        push eax
        call RecordBikeBalanceForce
        add esp, 8
        popad
        popfd
        jmp kApplyTurnForce
    }
}

// Each site is exactly `fstp dword ptr [esi+0x648]`, so the store is reproduced
// and only a counter is added.
__declspec(naked) void LeanWriteSmootherThunk() {
    __asm {
        fstp dword ptr [esi + 0x648]
        pushfd
        inc dword ptr [g_leanWrites]
        popfd
        jmp kLeanWriteSmootherReturn
    }
}

__declspec(naked) void LeanWriteBikeThunk() {
    __asm {
        fstp dword ptr [esi + 0x648]
        pushfd
        inc dword ptr [g_leanWrites + 4]
        popfd
        jmp kLeanWriteBikeReturn
    }
}

__declspec(naked) void LeanWriteBmxThunk() {
    __asm {
        fstp dword ptr [esi + 0x648]
        pushfd
        inc dword ptr [g_leanWrites + 8]
        popfd
        jmp kLeanWriteBmxReturn
    }
}

// ---------------------------------------------------------------------------
// Move speed write watchpoint
// ---------------------------------------------------------------------------

// Three separate hypotheses for the mid-air bike freeze were wrong, so instead
// of guessing which function wipes the move speed the CPU is asked directly. A
// hardware data breakpoint is armed on `m_vecMoveSpeed.z` of the bike the
// player is riding the moment the freeze is observed, and every instruction
// that writes it is recorded. Debug registers are per-thread, so the breakpoint
// goes on the game thread, which is identified by whichever thread calls the
// gravity hook.
constexpr size_t kWatchSlots = 48;
// The executable's code, used to tell a return address from stack garbage.
constexpr uintptr_t kTextLow = 0x00401000;
constexpr uintptr_t kTextHigh = 0x00857000;
constexpr uint32_t kWatchHitLimit = 4000;
// `traceWatchHits` and `traceWatchSamples` shrink the accumulation window. A
// landing lasts a few hundred milliseconds, and at the default window it shares
// a report with the seconds of flight and standing on either side of it.
uint32_t g_watchHitLimit = kWatchHitLimit;
uint32_t g_watchSampleLimit = 100;
// Samples the candidate has to stay the same before the breakpoint goes back on.
// Every report disarms it, so this is dead time between windows: worth lowering
// when the event under study is short.
uint32_t g_watchArmDelay = 3;

// Which CPhysical field the watchpoint arms on, as an offset from the vehicle.
// Defaults to `m_vecMoveSpeed.z`; `traceWatchOffset` in `[general]` moves it, so
// one build can name the writers of any four byte field.
size_t g_watchOffset = kPhysicalMoveSpeed + 8;
volatile uintptr_t g_watchAddress{};
volatile uint32_t g_watchTotalHits{};
uintptr_t g_watchEips[kWatchSlots]{};
uintptr_t g_watchCallers[kWatchSlots]{};
// How much each caller actually moved the field. The trap fires after the
// write, so consecutive traps bracket every change exactly, and the call count
// alone cannot say which caller dominates.
float g_watchDeltaSum[kWatchSlots]{};
// Signed as well as absolute: a force that swings both ways can add a large
// absolute total while its net effect is nearly nothing, and it is the net that
// holds a bike upright.
float g_watchSignedSum[kWatchSlots]{};
float g_watchLastValue{};
bool g_watchHaveLastValue{};
uint32_t g_watchCounts[kWatchSlots]{};
volatile long g_watchLock{};
void* g_watchHandler{};
bool g_watchArmed{};
bool g_watchReported{};
uint32_t g_watchReports{};
constexpr uint32_t kWatchReportLimit = 60;
uint32_t g_watchReportLimit = kWatchReportLimit;

// The generic force helpers are shared by the whole engine, so the instruction
// that writes the field names `CPhysical::ApplyTurnForce` and says nothing
// about which system asked for the force. The stack above the trap is scanned
// for the first value that looks like a return address into the executable,
// that is, one whose preceding five bytes are a direct `call`. That is the
// caller, which is what the search is actually after.
uintptr_t FindCallerOnStack(uintptr_t esp) {
    for (size_t i = 0; i < 96; ++i) {
        const auto candidate =
            *reinterpret_cast<const uintptr_t*>(esp + i * sizeof(uintptr_t));
        if (candidate < kTextLow || candidate >= kTextHigh) {
            continue;
        }
        if (*reinterpret_cast<const uint8_t*>(candidate - 5) == 0xE8) {
            return candidate;
        }
    }
    return 0;
}

LONG CALLBACK MoveSpeedWatchHandler(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    CONTEXT* ctx = info->ContextRecord;
    if ((ctx->Dr6 & 1u) == 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    ctx->Dr6 = 0;

    const auto eip = static_cast<uintptr_t>(ctx->Eip);
    float delta = 0.0f;
    __try {
        const float value = *reinterpret_cast<const float*>(g_watchAddress);
        if (g_watchHaveLastValue) {
            delta = value - g_watchLastValue;
        }
        g_watchLastValue = value;
        g_watchHaveLastValue = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    uintptr_t caller = 0;
    __try {
        caller = FindCallerOnStack(static_cast<uintptr_t>(ctx->Esp));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        caller = 0;
    }

    while (InterlockedCompareExchange(&g_watchLock, 1, 0) != 0) {
        YieldProcessor();
    }
    ++g_watchTotalHits;
    for (size_t i = 0; i < kWatchSlots; ++i) {
        if (g_watchEips[i] == eip && g_watchCallers[i] == caller) {
            ++g_watchCounts[i];
            g_watchDeltaSum[i] += std::fabs(delta);
            g_watchSignedSum[i] += delta;
            break;
        }
        if (g_watchEips[i] == 0) {
            g_watchEips[i] = eip;
            g_watchCallers[i] = caller;
            g_watchCounts[i] = 1;
            g_watchDeltaSum[i] = std::fabs(delta);
            g_watchSignedSum[i] = delta;
            break;
        }
    }
    InterlockedExchange(&g_watchLock, 0);
    return EXCEPTION_CONTINUE_EXECUTION;
}

// `Dr7` enables the first slot locally, as a four byte write watch:
// L0, RW0 = 01 (write) and LEN0 = 11 (four bytes).
constexpr DWORD kDr7WriteFourBytes = 0x1u | (0x1u << 16) | (0x3u << 18);

bool SetMoveSpeedWatch(uintptr_t address) {
    const DWORD threadId = g_gameThreadId;
    if (!threadId) {
        return false;
    }
    const HANDLE thread = OpenThread(
        THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
        FALSE,
        threadId);
    if (!thread) {
        return false;
    }
    bool ok = false;
    if (SuspendThread(thread) != static_cast<DWORD>(-1)) {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(thread, &ctx)) {
            ctx.Dr0 = address;
            ctx.Dr6 = 0;
            ctx.Dr7 = address ? kDr7WriteFourBytes : 0;
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            ok = SetThreadContext(thread, &ctx) != FALSE;
        }
        ResumeThread(thread);
    }
    CloseHandle(thread);
    return ok;
}

// The counters are copied out and cleared under the lock, and the formatting
// happens after it is released. Printing while holding it made the game thread
// spin inside the exception handler for the length of a buffered file write,
// once per report, which is not something the render thread can afford.
void ReportMoveSpeedWatch(FILE* file) {
    uintptr_t eips[kWatchSlots];
    uintptr_t callers[kWatchSlots];
    uint32_t counts[kWatchSlots];
    float absSum[kWatchSlots];
    float netSum[kWatchSlots];
    uint32_t hits = 0;

    while (InterlockedCompareExchange(&g_watchLock, 1, 0) != 0) {
        YieldProcessor();
    }
    hits = g_watchTotalHits;
    for (size_t i = 0; i < kWatchSlots; ++i) {
        eips[i] = g_watchEips[i];
        callers[i] = g_watchCallers[i];
        counts[i] = g_watchCounts[i];
        absSum[i] = g_watchDeltaSum[i];
        netSum[i] = g_watchSignedSum[i];
        g_watchEips[i] = 0;
        g_watchCallers[i] = 0;
        g_watchCounts[i] = 0;
        g_watchDeltaSum[i] = 0.0f;
        g_watchSignedSum[i] = 0.0f;
    }
    g_watchTotalHits = 0;
    g_watchHaveLastValue = false;
    InterlockedExchange(&g_watchLock, 0);

    std::fprintf(file, "# writers of +%u, %u hits total, ts=%.5f\n",
                 static_cast<uint32_t>(g_watchOffset), hits,
                 *reinterpret_cast<const float*>(kTimerTimeStep));
    for (size_t i = 0; i < kWatchSlots && eips[i]; ++i) {
        std::fprintf(file,
                     "# writer %08X from %08X x%u abs=%.6f net=%.6f\n",
                     static_cast<uint32_t>(eips[i]),
                     static_cast<uint32_t>(callers[i]),
                     counts[i], absSum[i], netSum[i]);
    }
    std::fflush(file);
}

float VectorLength(const float v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

bool SampleThePlayerVehicle(VehicleSample& out) {
    __try {
        const auto ped = *reinterpret_cast<uintptr_t*>(kWorldPlayers);
        if (!ped) {
            return false;
        }
        const auto vehicle = *reinterpret_cast<uintptr_t*>(ped + kPedVehicle);
        if (!vehicle) {
            return false;
        }

        out.vehicle = vehicle;
        out.time = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
        out.timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        out.timeScale = *reinterpret_cast<const float*>(kTimerTimeScale);

        const auto matrix = *reinterpret_cast<uintptr_t*>(vehicle + kEntityMatrix);
        out.posZ = matrix
            ? *reinterpret_cast<const float*>(matrix + kMatrixPosition + 8)
            : 0.0f;

        std::memcpy(out.move,
                    reinterpret_cast<const void*>(vehicle + kPhysicalMoveSpeed),
                    sizeof(out.move));
        std::memcpy(out.turn,
                    reinterpret_cast<const void*>(vehicle + kPhysicalTurnSpeed),
                    sizeof(out.turn));
        std::memcpy(out.force,
                    reinterpret_cast<const void*>(vehicle + kPhysicalForce),
                    sizeof(out.force));
        std::memcpy(out.torque,
                    reinterpret_cast<const void*>(vehicle + kPhysicalTorque),
                    sizeof(out.torque));

        std::memcpy(out.friction,
                    reinterpret_cast<const void*>(vehicle
                                                  + kPhysicalFrictionMoveSpeed),
                    sizeof(out.friction));

        out.physicalFlags =
            *reinterpret_cast<const uint32_t*>(vehicle + kPhysicalFlags);
        out.lastCollisionTime = *reinterpret_cast<const uint32_t*>(
            vehicle + kPhysicalLastCollisionTime);
        out.attachedTo =
            *reinterpret_cast<const uintptr_t*>(vehicle + kPhysicalAttachedTo);
        out.processCalls = g_bikeProcessCalls;
        out.gravityCalls = g_gravityCalls;
        out.leanWrites[0] = g_leanWrites[0];
        out.leanWrites[1] = g_leanWrites[1];
        out.leanWrites[2] = g_leanWrites[2];
        out.mvZAfterGravity = g_moveSpeedAfterGravity;
        out.rightZ = matrix
            ? *reinterpret_cast<const float*>(matrix + kMatrixRight + 8)
            : 0.0f;
        out.upZ = matrix
            ? *reinterpret_cast<const float*>(matrix + kMatrixUp + 8)
            : 0.0f;
        out.barSteer = *reinterpret_cast<const float*>(vehicle + kBikeBarSteerAngle);
        out.lean = *reinterpret_cast<const float*>(vehicle + kBikeLeanAngle);
        out.desiredLean =
            *reinterpret_cast<const float*>(vehicle + kBikeDesiredLeanAngle);
        out.brakePedal =
            *reinterpret_cast<const float*>(vehicle + kVehicleBrakePedal);
        out.gasPedal =
            *reinterpret_cast<const float*>(vehicle + kVehicleGasPedal);

        out.mass = *reinterpret_cast<const float*>(vehicle + kPhysicalMass);
        out.airResistance =
            *reinterpret_cast<const float*>(vehicle + kPhysicalAirResistance);
        out.entityFlags = *reinterpret_cast<const uint32_t*>(vehicle + kEntityFlags);
        out.fakePhysics =
            *reinterpret_cast<const uint8_t*>(vehicle + kFakePhysicsOffset);
        out.status = static_cast<uint8_t>(
            (*reinterpret_cast<const uint8_t*>(vehicle + kEntityTypeAndStatus) >> 3)
            & 0x1F);
        out.subClass = *reinterpret_cast<const uint8_t*>(vehicle + kVehicleSubClass);
        // Only meaningful for the bike classes.
        const bool isBike = out.subClass == 9 || out.subClass == 10;
        out.contactWheels = isBike
            ? *reinterpret_cast<const uint8_t*>(vehicle + kBikeContactWheels)
            : 0xFF;
        if (isBike) {
            std::memcpy(out.wheelAngularVelocity,
                        reinterpret_cast<const void*>(
                            vehicle + kBikeWheelAngularVelocity),
                        sizeof(out.wheelAngularVelocity));
            if (matrix) {
                const auto* right =
                    reinterpret_cast<const float*>(matrix + kMatrixRight);
                out.localPitchTurnSpeed = out.turn[0] * right[0]
                                        + out.turn[1] * right[1]
                                        + out.turn[2] * right[2];
            }
        }
        // Only the BMX classes carry the sprint lean angle.
        out.sprintLean = out.subClass == 10
            ? *reinterpret_cast<const float*>(vehicle + kBmxSprintLeanAngle)
            : 0.0f;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The pushing ped is on foot, so this cannot ride on the vehicle sample. It
// reports how much velocity the vehicle was handed per unit of real time, which
// is the quantity that has to match between a capped and an uncapped run.
// `traceWatchMode` picks what the watchpoint is looking for: mode 0 waits for a
// ridden bike to stand nearly still, mode 1 waits for the ped to start pushing
// a vehicle. Either way the candidate has to be the same object for three
// samples running before the breakpoint is armed on it.
void MaybeArmWatch(FILE* file) {
    static uintptr_t lastCandidate = 0;
    static uint32_t stableSamples = 0;
    static uint32_t armedSamples = 0;

    if (g_watchReported) {
        return;
    }
    if (!g_watchArmed) {
        const uintptr_t candidate = g_watchCandidate;
        if (candidate != 0 && candidate == lastCandidate) {
            ++stableSamples;
        } else {
            stableSamples = candidate != 0 ? 1 : 0;
            lastCandidate = candidate;
        }
        if (stableSamples >= g_watchArmDelay) {
            g_watchAddress = candidate + g_watchOffset;
            g_watchArmed = SetMoveSpeedWatch(g_watchAddress);
            if (g_watchArmed) {
                std::fprintf(file, "# watching %08X mode=%d offset=%u\n",
                             static_cast<uint32_t>(g_watchAddress), g_watchMode,
                             static_cast<uint32_t>(g_watchOffset));
                std::fflush(file);
                armedSamples = 0;
            } else {
                stableSamples = 0;
            }
        }
        return;
    }
    // Reported repeatedly rather than once, so a single session can cover more
    // than one frame rate: the timestep goes into every report line, and the
    // counters reset after each one.
    // The breakpoint stays on between reports. Taking it down and putting it
    // back up each window meant suspending the render thread once per report,
    // and it also left the field unwatched during the re-arm gap, which is
    // exactly where a short event such as a landing falls.
    const uintptr_t candidate = g_watchCandidate;
    if (candidate != 0 && candidate + g_watchOffset != g_watchAddress) {
        g_watchAddress = candidate + g_watchOffset;
        if (SetMoveSpeedWatch(g_watchAddress)) {
            std::fprintf(file, "# rewatching %08X offset=%u\n",
                         static_cast<uint32_t>(g_watchAddress),
                         static_cast<uint32_t>(g_watchOffset));
            std::fflush(file);
        }
    }
    if (++armedSamples >= g_watchSampleLimit ||
        g_watchTotalHits >= g_watchHitLimit) {
        armedSamples = 0;
        ReportMoveSpeedWatch(file);
        if (++g_watchReports >= g_watchReportLimit) {
            SetMoveSpeedWatch(0);
            g_watchArmed = false;
            g_watchReported = true;
        }
    }
}

void DrainPushSamples(FILE* file) {
    const uint32_t head = g_pushWriteIndex;
    if (head - g_pushReadIndex > kPushSampleSlots) {
        // Overrun; skip to what is still intact rather than report garbage.
        g_pushReadIndex = head - kPushSampleSlots;
    }
    bool wrote = false;
    while (g_pushReadIndex != head && g_pushSamplesLogged < kPushSampleLimit) {
        const PushSample& s = g_pushSamples[g_pushReadIndex % kPushSampleSlots];
        std::fprintf(file, "# imp f=%u dv=%.7f v=%.6f ts=%.5f\n",
                     s.frame, s.deltaV, s.carSpeed, s.timeStep);
        ++g_pushReadIndex;
        ++g_pushSamplesLogged;
        wrote = true;
    }
    if (wrote) {
        std::fflush(file);
    }
}

void ReportPedPushTelemetry(FILE* file, uint32_t& lastReport) {
    const uint32_t now = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
    if (now - lastReport < 500) {
        return;
    }
    const uint32_t applications = g_pushApplications;
    if (applications != 0) {
        const float moved[3]{
            g_pushLastPos[0] - g_pushFirstPos[0],
            g_pushLastPos[1] - g_pushFirstPos[1],
            g_pushLastPos[2] - g_pushFirstPos[2]
        };
        std::fprintf(file,
                     "# push dt=%u n=%u dv=%.5f vmax=%.5f ped=%.5f "
                     "dist=%.5f ts=%.5f\n",
                     now - lastReport, applications, g_pushDeltaVSum,
                     g_pushCarSpeedPeak, g_pushPedSpeed, VectorLength(moved),
                     *reinterpret_cast<const float*>(kTimerTimeStep));
        std::fflush(file);
        g_pushApplications = 0;
        g_pushDeltaVSum = 0.0f;
        g_pushCarSpeedPeak = 0.0f;
        g_pushHavePos = false;
    }
    lastReport = now;
}

// The on-foot counterpart of the vehicle trace. Written for the climbing death,
// where the question is what the player's move speed is on the frame the health
// drops, so it records speed and health together and nothing else.
struct PedSample {
    uint32_t time;
    float timeStep;
    float position[3];
    float move[3];
    float health;
    float armour;
};

bool SampleThePlayerPed(PedSample& out) {
    __try {
        const auto ped = *reinterpret_cast<const uintptr_t*>(kWorldPlayers);
        if (!ped) {
            return false;
        }
        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            ped + kEntityMatrix);
        if (!matrix) {
            return false;
        }
        out.time = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
        out.timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        for (int i = 0; i < 3; ++i) {
            out.position[i] = reinterpret_cast<const float*>(matrix + 0x30)[i];
            out.move[i] = reinterpret_cast<const float*>(
                ped + kPhysicalMoveSpeed)[i];
        }
        out.health = *reinterpret_cast<const float*>(ped + kPedHealth);
        out.armour = *reinterpret_cast<const float*>(ped + kPedArmour);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

DWORD WINAPI PedTraceThread(void*) {
    FILE* file = _fsopen(g_diagnosticPath.c_str(), "a", _SH_DENYNO);
    if (!file) {
        return 0;
    }
    std::fprintf(file, "# ped session fpsLimit=%d\n", g_fpsLimit);
    std::fprintf(file, "time ts posX posY posZ mvX mvY mvZ mvLen health armour\n");

    uint32_t written = 0;
    uint32_t lastTime = 0;
    while (g_diagnosticActive && written < kDiagnosticLineLimit) {
        PedSample s{};
        if (SampleThePlayerPed(s) && s.time != lastTime) {
            lastTime = s.time;
            const float length = std::sqrt(s.move[0] * s.move[0]
                                           + s.move[1] * s.move[1]
                                           + s.move[2] * s.move[2]);
            std::fprintf(file,
                         "%u %.5f %.3f %.3f %.3f %.5f %.5f %.5f %.5f %.2f %.2f\n",
                         s.time, s.timeStep,
                         s.position[0], s.position[1], s.position[2],
                         s.move[0], s.move[1], s.move[2], length,
                         s.health, s.armour);
            if ((++written % 32) == 0) {
                std::fflush(file);
            }
        }
        Sleep(kDiagnosticIntervalMs);
    }
    std::fflush(file);
    std::fclose(file);
    return 0;
}

DWORD WINAPI VehicleTraceThread(void*) {
    FILE* file{};
    // Appended, not truncated, so a capped run and an uncapped run of the same
    // test can be compared against each other in one file. Opened with sharing
    // so the log can be read while the game is still running; the default
    // `fopen` on this toolchain locks it exclusively.
    file = _fsopen(g_diagnosticPath.c_str(), "a", _SH_DENYNO);
    if (!file) {
        return 0;
    }
    std::fprintf(file, "# session fpsLimit=%d\n", g_fpsLimit);
    std::fprintf(file,
                 "time ts scale posZ mvLen tnLen frcLen trqLen fricLen "
                 "barSteer lean desiredLean sprintLean "
                 "eFlags pFlags lastCol attached calls fake status sub wheels "
                  "grav lw0 lw1 lw2 mvZpostGrav rightZ upZ leanRaw leanHeld "
                  "brake gas wheel0 wheel1 localPitch "
                  "mvX mvY mvZ tnX tnY tnZ\n");

    uint32_t written = 0;
    uint32_t lastTime = 0;
    uint32_t lastCalls = 0;
    uint32_t lastGravity = 0;
    uint32_t lastLean[3]{};
    uint32_t lastPushReport = 0;
    LONG lastBikeBalanceSequence = 0;
    LONG lastBikeBalanceWindowSequence = 0;
    while (g_diagnosticActive && written < kDiagnosticLineLimit) {
        VehicleSample s{};
        DrainPushSamples(file);
        ReportPedPushTelemetry(file, lastPushReport);
        const LONG balanceBefore = g_bikeBalanceSequence;
        if (balanceBefore != 0 && balanceBefore != lastBikeBalanceSequence) {
            MemoryBarrier();
            BikeBalanceSample balance{};
            std::memcpy(&balance, &g_bikeBalanceSample, sizeof(balance));
            MemoryBarrier();
            if (balanceBefore == g_bikeBalanceSequence) {
                std::fprintf(file,
                             "# bike-balance t=%u ts=%.5f l34=%.6f "
                             "a=%.6f b=%.6f in68=%.6f in6c=%.6f in78=%.6f "
                             "force=%.6f,%.6f,%.6f turnY=%.6f wheels=%u\n",
                             balance.time, balance.timeStep, balance.local34,
                             balance.coefficientA, balance.coefficientB,
                             balance.input68, balance.input6C, balance.input78,
                             balance.force[0], balance.force[1], balance.force[2],
                             balance.turnY, balance.contactWheels);
                lastBikeBalanceSequence = balanceBefore;
            }
        }
        const LONG windowBefore = g_bikeBalanceWindowSequence;
        if (windowBefore != 0 && windowBefore != lastBikeBalanceWindowSequence) {
            MemoryBarrier();
            BikeBalanceWindow window{};
            std::memcpy(&window, &g_bikeBalanceWindowSnapshot, sizeof(window));
            MemoryBarrier();
            if (windowBefore == g_bikeBalanceWindowSequence) {
                std::fprintf(file,
                             "# bike-balance-window t=%u calls=%u elapsed=%.5f "
                             "peak68=%.6f sum68=%.6f sum6c=%.6f "
                             "sum-force=%.6f,%.6f,%.6f\n",
                             window.time, window.calls, window.elapsed,
                             window.peakInput68, window.sumInput68,
                             window.sumInput6C, window.force[0],
                             window.force[1], window.force[2]);
                lastBikeBalanceWindowSequence = windowBefore;
            }
        }
        if (SampleThePlayerVehicle(s) && s.time != lastTime) {
            lastTime = s.time;
            const uint32_t callDelta = s.processCalls - lastCalls;
            lastCalls = s.processCalls;
            const uint32_t gravityDelta = s.gravityCalls - lastGravity;
            lastGravity = s.gravityCalls;
            uint32_t leanDelta[3];
            for (int i = 0; i < 3; ++i) {
                leanDelta[i] = s.leanWrites[i] - lastLean[i];
                lastLean[i] = s.leanWrites[i];
            }
            std::fprintf(file,
                         "%u %.5f %.3f %.3f "
                         "%.5f %.5f %.5f %.5f %.5f "
                         "%.4f %.4f %.4f %.4f "
                         "%08X %08X %u %08X %u %u %u %u %u "
                          "%u %u %u %u %.6f %.5f %.5f "
                          "%.5f %.5f "
                          "%.4f %.4f %.6f %.6f %.6f "
                          "%.5f %.5f %.5f %.5f %.5f %.5f\n",
                         s.time, s.timeStep, s.timeScale, s.posZ,
                         VectorLength(s.move), VectorLength(s.turn),
                         VectorLength(s.force), VectorLength(s.torque),
                         VectorLength(s.friction),
                         s.barSteer, s.lean, s.desiredLean, s.sprintLean,
                         s.entityFlags, s.physicalFlags, s.lastCollisionTime,
                         static_cast<uint32_t>(s.attachedTo), callDelta,
                         s.fakePhysics, s.status, s.subClass, s.contactWheels,
                         gravityDelta, leanDelta[0], leanDelta[1], leanDelta[2],
                          s.mvZAfterGravity, s.rightZ, s.upZ,
                          g_leanTargetRaw, g_leanTargetHeld,
                          s.brakePedal, s.gasPedal,
                          s.wheelAngularVelocity[0],
                          s.wheelAngularVelocity[1],
                          s.localPitchTurnSpeed,
                          s.move[0], s.move[1], s.move[2],
                         s.turn[0], s.turn[1], s.turn[2]);
            if ((++written % 64) == 0) {
                std::fflush(file);
            }

            // Once a ridden bike has been near still for a moment, watch the
            // chosen field and record every instruction that writes it.
            const bool isBike = s.subClass == 9 || s.subClass == 10;
            if (g_watchMode == 0) {
                g_watchCandidate = isBike && VectorLength(s.move) < 0.006f
                    ? s.vehicle
                    : 0;
            } else if (g_watchMode == 2) {
                // Any ridden bike, moving or not. Mode 0 waits for it to stand
                // still, which disarms the breakpoint for the whole of a jump
                // and its landing.
                g_watchCandidate = isBike ? s.vehicle : 0;
            }
        }

        MaybeArmWatch(file);
        Sleep(kDiagnosticIntervalMs);
    }

    std::fflush(file);
    std::fclose(file);
    return 0;
}

// ---------------------------------------------------------------------------
// Physics sleep counter
// ---------------------------------------------------------------------------

// `CObject`, `CAutomobile`, `CBike` and `CTrailer` all end their per-frame
// physics with the same idea:
//
//     m_vecForce = (m_vecForce + m_vecMoveSpeed) / 2;
//     if (still moving) { m_nFakePhysics = 0; }
//     else if (++m_nFakePhysics > 10) {
//         m_nFakePhysics = 10;
//         ResetMoveSpeed(); ResetTurnSpeed(); skipPhysics = true;
//     }
//
// `m_nFakePhysics` counts rendered frames, not time. At 30 FPS an entity has to
// stay nearly still for 11 frames, about 0.37 s, before the engine parks it. At
// 300 FPS that is 0.037 s, so a bike that is momentarily slow at the apex of a
// jump has its speed zeroed and its physics skipped, and hangs in mid-air; and
// a parked car being pushed is put back to sleep between pushes, which is why
// it becomes hard to move.
//
// The counter is therefore stepped in real time at the original 30 FPS rate
// instead of once per rendered frame. The decision is made once per game frame
// and shared by every entity, so each entity keeps its own counter and the
// `> 10` comparisons are untouched. At 30 FPS and below every frame ticks, so
// the original behavior is reproduced exactly.
int32_t __cdecl ShouldTickFakePhysicsCounter() {
    __try {
        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        if (frame != g_fakePhysicsLastFrame) {
            g_fakePhysicsLastFrame = frame;
            g_fakePhysicsCarry += TimeStepRatio();
            if (g_fakePhysicsCarry >= 1.0f) {
                g_fakePhysicsCarry -= std::floor(g_fakePhysicsCarry);
                g_fakePhysicsTick = 1;
            } else {
                g_fakePhysicsTick = 0;
            }
        }
        return g_fakePhysicsTick;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

using PadStateFn = bool(__thiscall*)(void*);

void* PadAt(int index) {
    return reinterpret_cast<void*>(
        kPads + static_cast<uintptr_t>(index) * kPadSize);
}

// CVehicle::ProcessSirenAndHorn separates a horn tap from a hold using a
// per-frame history buffer, so a tap covers fewer real milliseconds as the
// frame rate rises. This replaces the buffer with a wall-clock threshold.
//
// Only local players own a CPad. In particular, a SA-MP remote driver is not
// the second local player: treating every driver other than Players[0] as pad
// 1 lets a nearby network vehicle observe the idle second pad, consume player
// 0's shared tap state and make the siren impossible to toggle. Match both
// local player slots explicitly and keep independent state for split-screen.
// All other vehicles continue through the original code: forcing its no-horn
// branch would erase the counter that SA-MP synchronizes for remote sirens,
// leaving the lights active while suppressing their sound.
uintptr_t __cdecl SelectSirenReturnAddress(uintptr_t vehicle) {
    __try {
        void* driver = *reinterpret_cast<void**>(vehicle + kVehicleDriverOffset);
        int playerIndex = -1;
        for (int i = 0; i < 2; ++i) {
            void* player = *reinterpret_cast<void**>(
                kWorldPlayers + static_cast<uintptr_t>(i) * kPlayerInfoSize);
            if (player && driver == player) {
                playerIndex = i;
                break;
            }
        }
        if (playerIndex < 0) {
            return kSirenOriginalReturn;
        }

        void* pad = PadAt(playerIndex);
        HornTapState& state = g_hornTapStates[playerIndex];

        const uint32_t now = *reinterpret_cast<uint32_t*>(
            kTimerTimeInMilliseconds);
        if (reinterpret_cast<PadStateFn>(kPadHornJustDown)(pad)) {
            state.pressLastTime = now;
            state.hasPressed = true;
        }
        const bool horn = reinterpret_cast<PadStateFn>(kPadGetHorn)(pad);

        if (horn && now - state.pressLastTime >= kSirenTapMilliseconds) {
            return kSirenHornReturn;
        }
        if (!horn && state.hasPressed) {
            state.hasPressed = false;
            if (now - state.pressLastTime < kSirenTapMilliseconds) {
                return kSirenToggleReturn;
            }
        }
        return kSirenNoHornReturn;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return kSirenOriginalReturn;
    }
}

// ---------------------------------------------------------------------------
// Optional frame limiting
// ---------------------------------------------------------------------------

void WriteFrameLimit(uint8_t value) {
    WriteBytes(kFrameLimit, &value, 1);
}

uint8_t ReadFrameLimit() {
    __try {
        return *reinterpret_cast<const uint8_t*>(kFrameLimit);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool ScriptNameMatches(const char* name, const char* expected) {
    std::array<char, kRunningScriptNameSize + 1> buffer{};
    std::memcpy(buffer.data(), name, kRunningScriptNameSize);
    return _stricmp(buffer.data(), expected) == 0;
}

int PreferredScriptFpsLimit() {
    int preferred = 0;
    const auto queueHead = *reinterpret_cast<uintptr_t*>(kScriptQueueOperand);
    if (!queueHead) {
        return 0;
    }
    for (auto script = *reinterpret_cast<uintptr_t*>(queueHead); script;
         script = *reinterpret_cast<uintptr_t*>(script)) {
        const char* name = reinterpret_cast<const char*>(
            script + kRunningScriptNameOffset);
        if (g_autoLimit.flags.forMinigames
            && (ScriptNameMatches(name, "POOL2")
                || ScriptNameMatches(name, "GFSEX"))) {
            preferred = 30;
        } else if (g_autoLimit.flags.forMissions
                   && ScriptNameMatches(name, "DRUGS1")) {
            // Big Smoke sometimes stops walking indoors, which locks the mission.
            if (*reinterpret_cast<const int32_t*>(kGameCurrentArea) != 0) {
                preferred = 50;
            }
        } else if (g_autoLimit.flags.forSchools
                   && (ScriptNameMatches(name, "DSKOOL")
                       || ScriptNameMatches(name, "BOAT")
                       || ScriptNameMatches(name, "BSKOOL"))) {
            preferred = 80;
        }
    }
    return preferred;
}

void __cdecl ProcessAutoFpsLimit() {
    __try {
        if (g_isOnPauseMenu) {
            if (g_lastFpsLimit != 0) {
                WriteFrameLimit(static_cast<uint8_t>(g_lastFpsLimit));
                g_lastFpsLimit = 0;
            }
            g_isOnPauseMenu = false;
        }

        int preferred = 0;
        if (*reinterpret_cast<const int8_t*>(kCutsceneRunning) != 0) {
            if (g_autoLimit.flags.forCutscenes) {
                preferred = 60;
            }
        } else if (*reinterpret_cast<const uint8_t*>(kCameraWideScreenOn) != 0) {
            // Letterbox borders mark scripted scenes.
            if (g_autoLimit.flags.forScriptedCutscenes) {
                preferred = 80;
            }
        } else {
            preferred = PreferredScriptFpsLimit();
        }

        if (preferred != 0) {
            if (g_lastFpsLimit == 0) {
                g_lastFpsLimit = ReadFrameLimit();
            }
            if (g_lastFpsLimit != 0 && g_lastFpsLimit < preferred) {
                preferred = g_lastFpsLimit;
            }
            WriteFrameLimit(static_cast<uint8_t>(preferred));
        } else if (g_lastFpsLimit != 0) {
            WriteFrameLimit(static_cast<uint8_t>(g_lastFpsLimit));
            g_lastFpsLimit = 0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

void __cdecl OnPauseMenuBackground() {
    g_isOnPauseMenu = true;
    if (g_lastFpsLimit == 0) {
        g_lastFpsLimit = ReadFrameLimit();
    }
    WriteFrameLimit(60);
}

// ---------------------------------------------------------------------------
// Naked thunks
// ---------------------------------------------------------------------------

__declspec(naked) void FlightTimerThunk() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        call AccumulateFlightTimer
        add esp, 4
        ret
    }
}

__declspec(naked) void EndTimerThunk() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        call AccumulateEndTimer
        add esp, 4
        ret
    }
}

// Replaces the multiply, the truncation and the two argument pushes the
// optimizer moved in front of it. The two pushes are reproduced afterwards, so
// the six arguments `CWeapon::GenerateDamageEvent` is about to receive sit in
// the original order. `eax` carries the damage into the `push eax` at the
// return address, exactly as `_ftol` left it.
__declspec(naked) void DrowningDamageThunk() {
    __asm {
        fmul dword ptr ds:[0x00858B3C]
        sub esp, 4
        fstp dword ptr [esp]
        call AccumulateDrowningDamage
        add esp, 4
        push 0
        push 3
        jmp kDrowningDamageReturn
    }
}

// `st(0)` holds `m_fHit` for the moving attack and `ecx` the animation the
// rewind is about to be written to, which is also the `this` the
// `CAnimBlendAssociation::SetCurrentTime` call four bytes later expects, so it
// has to survive the helper.
__declspec(naked) void ChainsawStrikeRewindThunk() {
    __asm {
        pushfd
        push eax
        push edx
        push ecx
        push esi
        push ecx
        call UpdateChainsawRewindOffset
        add esp, 8
        pop ecx
        pop edx
        pop eax
        popfd
        fsub g_chainsawRewindOffset
        ret
    }
}

// Counts melee strikes for `traceChainsaw`, then falls through into the
// function the replaced call was going to reach.
__declspec(naked) void FightStrikeTraceThunk() {
    __asm {
        pushfd
        pushad
        push ecx
        call RecordFightStrike
        add esp, 4
        popad
        popfd
        jmp kFightStrike
    }
}

__declspec(naked) void ContinuousWeaponAmmoThunk() {
    __asm {
        pushfd
        pushad
        push esi
        call ShouldConsumeContinuousWeaponAmmo
        add esp, 4
        mov dword ptr [esp + 28], eax
        popad
        popfd
        test eax, eax
        jz skipConsumption
        mov eax, dword ptr [esi + 8]
        test eax, eax
        jmp kContinuousAmmoConsume
    skipConsumption:
        jmp kContinuousAmmoSkip
    }
}

__declspec(naked) void WheelFrictionCarDriveThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionCarDriveReturn
    }
}

__declspec(naked) void WheelFrictionCarBrakeThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionCarBrakeReturn
    }
}

__declspec(naked) void WheelFrictionBikeBaseThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionBikeBaseReturn
    }
}

__declspec(naked) void WheelFrictionBikeDriveThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionBikeDriveReturn
    }
}

__declspec(naked) void WheelFrictionBikeBrakeThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionBikeBrakeReturn
    }
}

// Both leave exactly one value on the FPU stack, the divisor, which is what
// either arm of the replaced branch did.
__declspec(naked) void FollowPedCameraRateThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        jmp kFollowPedCameraRateReturn
    }
}

__declspec(naked) void FollowCarCameraRateThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        jmp kFollowCarCameraRateReturn
    }
}

// Leaves the FPU stack exactly as the replaced block did: the reciprocal goes
// to the same slot and the three deltas underneath it are untouched.
__declspec(naked) void AttachedEntitySpeedThunk() {
    __asm {
        fld dword ptr ds:[0x00858624]
        fdiv dword ptr ds:[0x00B7CB5C]
        fstp dword ptr [esp + 0x0C]
        jmp kAttachedEntitySpeedReturn
    }
}

// The same contract as the follow camera thunks: the replaced block left the
// divisor alone on the FPU stack and so does this.
__declspec(naked) void AiAircraftSteerRateThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        jmp kAiAircraftSteerRateReturn
    }
}

// Stands in for `_ftol` at the stat sites. `_ftol` takes the value in st(0),
// pops it and returns the integer in edx:eax; this does the same, with the
// fraction kept. The return address doubles as the site identifier, which is
// why the wrapper reads it out of the frame rather than taking a parameter.
// ecx is preserved because the compiled helper is free to clobber it.
__declspec(naked) void StatTruncCarryThunk() {
    __asm {
        push ebp
        mov ebp, esp
        push ecx
        mov eax, [ebp + 4]
        push eax
        sub esp, 8
        fstp qword ptr [esp]
        call TruncateStatWithCarry
        add esp, 12
        pop ecx
        cdq
        pop ebp
        ret
    }
}

// Replaces the store of the stepped value. `edx` carries what the original
// would have written and `esi` the player info, whose old value is still in
// place, so the helper can recover the step the game chose.
__declspec(naked) void MoneyStepThunk() {
    __asm {
        pushfd
        pushad
        push edx
        lea eax, [esi + 0xBC]
        push eax
        call ApplyMoneyStep
        add esp, 8
        popad
        popfd
        jmp kMoneyStepReturn
    }
}

// The clamp goes in where the sibling branch does its own, between the divide
// and the addition of the climbed entity's speed. `edi` holds the ped and `ecx`
// the address `CVector::operator+=` is about to be called on, both preserved by
// `pushad`, and the replaced instructions are reproduced around it.
__declspec(naked) void ClimbSpeedClampThunk() {
    __asm {
        add esp, 0x0C
        pushfd
        pushad
        lea eax, [edi + 0x44]
        push eax
        call ClampClimbMoveSpeed
        add esp, 4
        popad
        popfd
        lea edx, [esp + 0x48]
        push edx
        call kVectorAddAssign
        jmp kClimbSpeedClampReturn
    }
}

// The impulse is built with the original timestep so the comparison against
// `mass * moveSpeed.z` keeps its 30 FPS meaning, and the copy written into the
// output vector is scaled back to the current frame. `esi` addresses the
// buoyancy state, `eax` the entity and `ecx` the output vector; none are
// touched here.
__declspec(naked) void BuoyancyThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xBC]
        fmul dword ptr [esi + 0x6C]
        add esp, 0x0C
        fmul g_originalTimeStepValue
        fld st(0)
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fstp dword ptr [ecx + 8]
        fld dword ptr [eax + 0x8C]
        fmul dword ptr [eax + 0x4C]
        fld st(1)
        fmul dword ptr ds:[0x00858B90]
        jmp kBuoyancyThresholdReturn
    }
}

// The reduced impulse taken when the entity is already rising fast. It is
// computed from the original timestep value above, so it is scaled here too.
// The replaced span runs to the end of the function, so this returns directly.
__declspec(naked) void BuoyancyClampedStoreThunk() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fstp dword ptr [ecx + 8]
        mov al, 1
        pop esi
        add esp, 0x0C
        ret 0x0C
    }
}

// Wraps the single call to `CTaskSimpleSwim::ProcessSwimmingResistance`. The
// two replaced instructions set up its arguments, so they are reproduced
// between the scale and the restore. `esi` is the ped and `edi` the task, both
// preserved by `pushad`.
__declspec(naked) void SwimResistanceThunk() {
    __asm {
        pushfd
        pushad
        push esi
        call ScaleSwimAnimShift
        add esp, 4
        popad
        popfd

        push esi
        mov ecx, edi
        call kProcessSwimmingResistance

        pushfd
        pushad
        push esi
        call RestoreSwimAnimShift
        add esp, 4
        popad
        popfd
        jmp kSwimResistanceReturn
    }
}

__declspec(naked) void AimingRifleWalkThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetAimingRifleWalkStep
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kAimingRifleWalkReturn
    }
}

__declspec(naked) void SkimmerResistanceThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetSkimmerResistance
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kSkimmerResistanceReturn
    }
}

__declspec(naked) void BurnoutThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetBurnoutWheelSpeed
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kBurnoutReturn
    }
}

__declspec(naked) void HeliRotorSlowThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetHeliRotorSlowStep
        pop edx
        pop ecx
        pop eax
        popfd
        faddp st(1), st
        jmp kHeliRotorSlowReturn
    }
}

__declspec(naked) void HeliRotorFastThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetHeliRotorFastStep
        pop edx
        pop ecx
        pop eax
        popfd
        faddp st(1), st
        jmp kHeliRotorFastReturn
    }
}

__declspec(naked) void RailWheelSpinThunk0() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fadd dword ptr [esi + 0x828]
        jmp kRailWheelSpinReturn0
    }
}

__declspec(naked) void RailWheelSpinThunk1() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fadd dword ptr [esi + 0x82C]
        jmp kRailWheelSpinReturn1
    }
}

__declspec(naked) void RailWheelSpinThunk2() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fadd dword ptr [esi + 0x830]
        jmp kRailWheelSpinReturn2
    }
}

__declspec(naked) void RailWheelSpinThunk3() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fadd dword ptr [esi + 0x834]
        jmp kRailWheelSpinReturn3
    }
}

__declspec(naked) void PedPushCarThunk() {
    __asm {
        pushfd
        pushad
        push esi
        lea eax, [esp + 0x28]
        push eax
        call ScalePedPushCarForce
        add esp, 8
        popad
        popfd
        mov edx, dword ptr [esp + 0x20]
        mov eax, dword ptr [esp + 0x24]
        jmp kPedPushCarReturn
    }
}

// Each site is `mov r8, [esi+0xB8]` followed by `inc r8`, and every one of them
// overwrites AL on the very next instruction, so returning the tick flag in EAX
// is safe.
// `m_fMovingSpeed` is per-frame distance, so it is rescaled into the units the
// fixed limit was written for before the comparison. At 30 FPS this multiplies
// by exactly one.
__declspec(naked) void CarRestThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xD4]
        fmul g_originalTimeStepValue
        fdiv dword ptr ds:[0x00B7CB5C]
        jmp kCarRestThresholdReturn
    }
}

__declspec(naked) void BikeRestThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xD4]
        fmul g_originalTimeStepValue
        fdiv dword ptr ds:[0x00B7CB5C]
        jmp kBikeRestThresholdReturn
    }
}

__declspec(naked) void TrailerRestThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xD4]
        fmul g_originalTimeStepValue
        fdiv dword ptr ds:[0x00B7CB5C]
        jmp kTrailerRestThresholdReturn
    }
}

// The comparison itself is preserved exactly; only the limit it is measured
// against is rescaled into the same per-frame units as the value. The 0.005 is
// read back through the original operand, so a mod that repoints it keeps
// working. At 30 FPS the timestep and the normalizer cancel and the limit is
// bit-exact 0.005.
//
// On entry `st(0)` holds the absolute value of one move speed component, and
// the original `fcomp` pops it. Pushing the scaled limit and storing it back
// out leaves the stack exactly as the replaced instruction found it.
#define MOVE_SPEED_SNAP_THUNK(name, returnAddress)                  \
    __declspec(naked) void name() {                                 \
        __asm {                                                     \
            __asm fld dword ptr ds:[0x00858B4C]                     \
            __asm fmul dword ptr ds:[0x00B7CB5C]                    \
            __asm fdiv g_originalTimeStepValue                      \
            __asm fstp g_scaledMoveSpeedSnap                        \
            __asm fcomp g_scaledMoveSpeedSnap                       \
            __asm jmp returnAddress                                 \
        }                                                           \
    }

MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapCarXThunk, kMoveSpeedSnapCarXReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapCarYThunk, kMoveSpeedSnapCarYReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapCarZThunk, kMoveSpeedSnapCarZReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapBikeXThunk, kMoveSpeedSnapBikeXReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapBikeYThunk, kMoveSpeedSnapBikeYReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapBikeZThunk, kMoveSpeedSnapBikeZReturn)

#undef MOVE_SPEED_SNAP_THUNK

// The replaced block is three copies of `fld [esi+n]; fmul 0.99; fstp [esi+n]`,
// so the factor is computed once and reused with the same `fld st(0)` shape the
// original uses for the move speed a few instructions above. The net effect on
// the x87 stack is zero, exactly as for the block it replaces.
// The value handed in is the engine's estimate of the lateral acceleration in
// g, measured across one call as `deltaSpeed / (timeStep * gravity)`. Standing
// still it reached 0.7451 at 500 FPS against 0.0134 at 30 FPS, while the bike's
// actual roll was the same to within a factor of 1.5, so the estimate is not
// reporting real motion.
//
// It cannot be repaired by accumulating the per-call deltas over a real-time
// window. Between calls the resting contact cancels the tangential speed, so a
// delta is not an exact difference and the sum keeps the contact chatter that
// the endpoints would have cancelled. Measured, that left the amplitude at
// 0.4353, barely better than the 0.7451 it started from.
//
// What works is a plain finite difference of the state: the velocity is
// measured from how far the bike actually travelled over one original frame of
// real time, and the difference between two such measurements is the
// acceleration across that interval. Standing still, both samples are zero and
// the estimate is zero. Cornering, the difference is the real change in
// velocity over 1/30 s, which is what the engine measures at 30 FPS.
//
// The velocity kept between windows is the whole vector. Keeping only its
// component along the bike's right axis looks equivalent and is not: those axes
// turn with the bike, so a steady corner holds that component roughly constant
// and its difference is zero. That version reported no lateral acceleration
// through corners and held the bike upright at any frame rate above 30, which
// is what the projection loses — the engine's numerator is a change of world
// velocity, and the projection happens after the difference, not before.
//
// The direction dotted against is the matrix right vector, which is what the
// original numerator uses at `0x6BBAC2` through `edi`, loaded from `[esi+0x14]`.
// At or below the original frame rate the engine's own value is passed through,
// so stock behavior is reproduced by construction rather than by arithmetic.
// `CBike::ProcessControl` runs for every bike in the world, not just the one
// the player is on, so a single set of globals is torn between them: the
// "different bike" branch fires on every call, the interval never accumulates
// and the filter degenerates into a pass-through. The state is therefore kept
// per bike, in a small table that evicts whichever entry has gone longest
// without being touched.
constexpr size_t kLeanSlots = 8;

struct LeanTargetState {
    uintptr_t bike;
    float position[3];
    float velocity[3];
    float elapsed;
    uint32_t frame;
    uint32_t lastFrame;
    bool primed;
    bool sampled;
    float held;
};
LeanTargetState g_leanStates[kLeanSlots]{};

void ResetLeanStateFor(uintptr_t bike) {
    for (auto& state : g_leanStates) {
        if (state.bike == bike) {
            state = LeanTargetState{};
            return;
        }
    }
}

LeanTargetState& LeanStateFor(uintptr_t bike, uint32_t frame) {
    size_t oldest = 0;
    for (size_t i = 0; i < kLeanSlots; ++i) {
        if (g_leanStates[i].bike == bike) {
            g_leanStates[i].lastFrame = frame;
            return g_leanStates[i];
        }
        if (g_leanStates[i].bike == 0) {
            oldest = i;
            break;
        }
        if (frame - g_leanStates[i].lastFrame
            > frame - g_leanStates[oldest].lastFrame) {
            oldest = i;
        }
    }
    LeanTargetState& fresh = g_leanStates[oldest];
    fresh = LeanTargetState{};
    fresh.bike = bike;
    fresh.lastFrame = frame;
    return fresh;
}

void __cdecl FilterBikeLeanTarget(float* target, uintptr_t bike) {
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        const float gravity = ReadGameFloat(kGravityConstant, 0.008f);
        if (!std::isfinite(*target) || timeStep <= 0.0f || gravity <= 0.0f) {
            return;
        }

        // A nominal 30 FPS cap does not produce one bit-exact timestep. The old
        // hard boundary alternated between the stock target and a target held
        // over two frames when limiter jitter crossed it, making the rider snap
        // sideways. At and below the original rate the filter has zero weight;
        // above it the weight grows continuously, so there is no new FPS
        // boundary at which the behavior switches.
        if (timeStep >= kOriginalTimeStep) {
            // Do not retain a high-FPS sample across a limiter transition. The
            // next filtered frame starts from the current position and target.
            ResetLeanStateFor(bike);
            return;
        }

        const auto matrix =
            *reinterpret_cast<const uintptr_t*>(bike + kEntityMatrix);
        if (!matrix) {
            return;
        }
        const auto* position =
            reinterpret_cast<const float*>(matrix + kMatrixPosition);
        const auto* right =
            reinterpret_cast<const float*>(matrix + kMatrixRight);

        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        LeanTargetState& state = LeanStateFor(bike, frame);
        if (!state.primed) {
            state.primed = true;
            std::memcpy(state.position, position, sizeof(state.position));
            state.held = *target;
        }

        // The function runs more than once per rendered frame for a given bike,
        // so the interval advances on the frame counter, not on calls.
        if (frame != state.frame) {
            state.frame = frame;
            state.elapsed += timeStep;
        }

        if (state.elapsed >= kOriginalTimeStep) {
            // Speed is taken from how far the bike actually travelled, not from
            // `m_vecMoveSpeed`. The resting contact cancels that field partway
            // through a frame, so sampling it catches transients that never
            // moved the bike; the matrix position carries no such spikes and is
            // exactly zero for a bike standing still.
            // The whole velocity vector is carried, not its component along
            // the bike's right axis. Cornering, the bike's own axes turn with
            // it, so a steady turn holds that component almost constant and
            // differencing it gives zero: the first version of this filter
            // did exactly that and stood the bike upright through every
            // corner. What the engine measures is the change in the world
            // velocity projected onto the right axis, which in a steady turn
            // is the centripetal term and is not zero at all.
            float velocity[3];
            for (int i = 0; i < 3; ++i) {
                velocity[i] = (position[i] - state.position[i]) / state.elapsed;
            }
            if (state.sampled) {
                const float lateral =
                      (velocity[0] - state.velocity[0]) * right[0]
                    + (velocity[1] - state.velocity[1]) * right[1]
                    + (velocity[2] - state.velocity[2]) * right[2];
                state.held = lateral / (state.elapsed * gravity);
            }
            state.sampled = true;
            std::memcpy(state.velocity, velocity, sizeof(state.velocity));
            std::memcpy(state.position, position, sizeof(state.position));
            state.elapsed = 0.0f;
        }

        const float ratio = std::clamp(timeStep / kOriginalTimeStep,
                                       0.0f, 1.0f);
        const float filterWeight = 1.0f - ratio * ratio;
        const float filteredTarget =
            *target + (state.held - *target) * filterWeight;

        if (IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
            g_leanTargetRaw = *target;
            g_leanTargetHeld = filteredTarget;
        }
        *target = filteredTarget;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

// Scales a force vector the caller pushed by value. Identity at or below
// 30 FPS, where the ratio is 1.0 or greater and the original already applied
// the full force once per frame.
void __cdecl ScaleRollOntoWheelsForce(float* force) {
    __try {
        const float ratio = TimeStepRatio();
        if (!std::isfinite(ratio) || ratio <= 0.0f || ratio >= 1.0f) {
            return;
        }
        force[0] *= ratio;
        force[1] *= ratio;
        force[2] *= ratio;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

// Both are entered by a `call`, so the force vector the caller pushed starts at
// `[esp+4]`, which `pushfd` and `pushad` move to `[esp+0x28]`. The tail is a
// `jmp` so the real function returns straight to the original caller, and
// `ecx`, which carries `this`, is restored by `popad`.
__declspec(naked) void RollOntoWheelsTurnForceThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x28]
        push eax
        call ScaleRollOntoWheelsForce
        add esp, 4
        popad
        popfd
        jmp kApplyTurnForce
    }
}

__declspec(naked) void RollOntoWheelsMoveForceThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x28]
        push eax
        call ScaleRollOntoWheelsForce
        add esp, 4
        popad
        popfd
        jmp kApplyMoveForce
    }
}

// Each replaces the single arithmetic instruction that changed the wheel speed,
// entered by a `call` with the speed alone on the FPU stack and expected to
// leave the new speed there. The replaced instruction is six bytes, so the call
// fits with one byte of padding.
__declspec(naked) void WheelSpinDecelThunk() {
    __asm {
        fld dword ptr ds:[0x00858B1C]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fsubp st(1), st
        ret
    }
}

__declspec(naked) void WheelSpinAccelThunk() {
    __asm {
        fld dword ptr ds:[0x00858C28]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        faddp st(1), st
        ret
    }
}

// Damping is a ratio applied per frame, so it takes the exponent rather than
// the product, the same shape the game itself uses for the chassis door.
__declspec(naked) void WheelSpinDampThunk() {
    __asm {
        fld dword ptr ds:[0x00858EF0]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmulp st(1), st
        ret
    }
}

// Scales the free front wheel's angular velocity into the current frame before
// the pitch angle integrates it. `esi` is the bike and the field offset is the
// one the replaced `fadd` used; the original `fstp` at the return address
// stores the result.
__declspec(naked) void BikeWheelPitchIntegrateThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        fadd dword ptr [esi + 0x750]
        ret
    }
}

// The four ramp steps. Each replaces one `fadd`/`fsub` against a constant with
// the same operation scaled into the current frame; `fchs` before the add
// avoids relying on `fsubp` operand order, which assemblers disagree about.
// The value being ramped is already on the stack and the original store
// follows at the return address.
__declspec(naked) void JetPackFxRampUpThunk() {
    __asm {
        fld dword ptr ds:[0x00858B1C]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        faddp st(1), st
        ret
    }
}

__declspec(naked) void JetPackFxRampDownThunk() {
    __asm {
        fld dword ptr ds:[0x00858B1C]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        fchs
        faddp st(1), st
        ret
    }
}

__declspec(naked) void HeadBopRampUpThunk() {
    __asm {
        fld dword ptr ds:[0x00858C28]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        faddp st(1), st
        ret
    }
}

__declspec(naked) void HeadBopRampDownThunk() {
    __asm {
        fld dword ptr ds:[0x00858C28]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        fchs
        faddp st(1), st
        ret
    }
}

// The base, the writable `0.95`, is already in st(0) when these run.
__declspec(naked) void BmxLeanLeftDecayThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmul dword ptr [esi + 0x654]
        ret
    }
}

__declspec(naked) void BmxLeanFwdDecayThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmul dword ptr [esi + 0x658]
        ret
    }
}

// The same shape as the free wheel damping, against the 0.9 constant that
// `CVehicle::CanPedJumpOutCar` uses for both speeds.
// The gap between the wheel's drawn position and where the suspension wants it
// is already in st(0).
__declspec(naked) void WheelSettleThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetWheelSettleWeight
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        ret
    }
}

// The shift component is already in st(0); both thunks replace the multiply
// that scaled it by the stock constant.
__declspec(naked) void PushOutMainThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetPushOutScaleMain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        ret
    }
}

__declspec(naked) void PushOutAltThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetPushOutScaleAlt
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        ret
    }
}

__declspec(naked) void JumpOutDampThunk() {
    __asm {
        fld dword ptr ds:[0x00858C20]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmulp st(1), st
        ret
    }
}

// Rising-edge state for the pause menu map wheel. Sampled once per frame from
// the map bounds block; the two edge bytes are read by the gate thunks below.
uint8_t g_mapWheelUpEdge = 0;
uint8_t g_mapWheelDownEdge = 0;
uint8_t g_mapWheelUpPrev = 0;
uint8_t g_mapWheelDownPrev = 0;

void __cdecl SampleMapWheelEdges() {
    const uint8_t up = *reinterpret_cast<volatile uint8_t*>(kMouseWheelUpFlag);
    const uint8_t down =
        *reinterpret_cast<volatile uint8_t*>(kMouseWheelDownFlag);
    g_mapWheelUpEdge = (up && !g_mapWheelUpPrev) ? 1 : 0;
    g_mapWheelDownEdge = (down && !g_mapWheelDownPrev) ? 1 : 0;
    g_mapWheelUpPrev = up;
    g_mapWheelDownPrev = down;
}

// The x87 stack is empty here and the replaced load is reproduced before the
// return. The flags matter: a `test al,al` above and a `jge` below straddle
// this point.
__declspec(naked) void MapWheelSampleThunk() {
    __asm {
        pushfd
        pushad
        call SampleMapWheelEdges
        popad
        popfd
        fld dword ptr ds:[0x008653F4]
        ret
    }
}

// Entered with the elapsed pause-mode milliseconds in eax, replacing
// `cmp eax,14h / jbe skip`. Proceeds when the tick has passed, as before, or on
// a wheel notch.
__declspec(naked) void MapZoomInGateThunk() {
    __asm {
        cmp eax, 0x14
        ja proceed
        cmp byte ptr [g_mapWheelUpEdge], 0
        jne proceed
        jmp kMapZoomInSkip
    proceed:
        jmp kMapZoomInProceed
    }
}

__declspec(naked) void MapZoomOutGateThunk() {
    __asm {
        cmp eax, 0x14
        ja proceed
        cmp byte ptr [g_mapWheelDownEdge], 0
        jne proceed
        jmp kMapZoomOutSkip
    proceed:
        jmp kMapZoomOutProceed
    }
}


// One carry per patched fire event. Adding the timestep ratio each frame and
// spending a whole unit means the site is evaluated at the 30 FPS rate whatever
// the frame rate, so the odds per evaluation stay exactly as shipped.
std::array<float, 3> g_fireEventCarries{};

int32_t __cdecl FireEventTick(int32_t site) {
    if (site < 0 || static_cast<size_t>(site) >= g_fireEventCarries.size()) {
        return 1;
    }
    const float ratio = TimeStepRatio();
    if (!std::isfinite(ratio) || ratio >= 1.0f || ratio <= 0.0f) {
        return 1;
    }
    const float total = g_fireEventCarries[site] + ratio;
    if (total >= 1.0f) {
        g_fireEventCarries[site] = total - 1.0f;
        return 1;
    }
    g_fireEventCarries[site] = total;
    return 0;
}

// Entered with the freshly drawn random number in eax, replacing
// `test al,<mask> / jne skip`. `FireEventTick` clobbers eax, ecx and edx, all
// of which the rand call immediately above already clobbered, so only the draw
// itself has to be preserved. `pop` leaves the flags alone, so the mask test
// after it is the original test unchanged.
__declspec(naked) void FireVehicleGateThunk() {
    __asm {
        push eax
        push 0
        call FireEventTick
        add esp, 4
        test eax, eax
        pop eax
        je skipped
        test al, 0x1F
        jne skipped
        jmp kFireVehicleResume
    skipped:
        jmp kFireVehicleSkip
    }
}

__declspec(naked) void FireSpreadGateThunk() {
    __asm {
        push eax
        push 1
        call FireEventTick
        add esp, 4
        test eax, eax
        pop eax
        je skipped
        test al, 0x7F
        jne skipped
        jmp kFireSpreadResume
    skipped:
        jmp kFireSpreadSkip
    }
}

__declspec(naked) void FireMergeGateThunk() {
    __asm {
        push eax
        push 2
        call FireEventTick
        add esp, 4
        test eax, eax
        pop eax
        je skipped
        test al, 0x0F
        jne skipped
        jmp kFireMergeResume
    skipped:
        jmp kFireMergeSkip
    }
}

// A shared 30 FPS tick. Each slot answers, once per frame counter value, whether
// this frame is one of the thirty per second the original game would have had.
// The answer is cached per frame so that several callers in the same frame — two
// pads, or every boat in the world — all agree.
//
// Slot 0 is the drunk driving steering shift.
constexpr size_t kFrameTickSlots = 1;
constexpr int32_t kFrameTickDrunkSteer = 0;

struct FrameTickSlot {
    uint32_t frame;
    int32_t decision;
    float carry;
};
std::array<FrameTickSlot, kFrameTickSlots> g_frameTicks{};

void ResetFrameTicks() {
    for (auto& slot : g_frameTicks) {
        slot.frame = 0xFFFFFFFFu;
        slot.decision = 1;
        slot.carry = 0.0f;
    }
}

int32_t __cdecl FrameTick(int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= g_frameTicks.size()) {
        return 1;
    }
    FrameTickSlot& slot = g_frameTicks[index];

    const uint32_t frame = *reinterpret_cast<volatile uint32_t*>(kFrameCounter);
    if (frame == slot.frame) {
        return slot.decision;
    }
    slot.frame = frame;

    const float ratio = TimeStepRatio();
    if (!std::isfinite(ratio) || ratio >= 1.0f || ratio <= 0.0f) {
        slot.carry = 0.0f;
        slot.decision = 1;
        return 1;
    }
    const float total = slot.carry + ratio;
    if (total >= 1.0f) {
        slot.carry = total - 1.0f;
        slot.decision = 1;
    } else {
        slot.carry = total;
        slot.decision = 0;
    }
    return slot.decision;
}

// Replaces the shift loop's two set-up instructions. `ebx` is the pad and is
// callee-saved across the helper; eax, ecx and edx are dead here, and the two
// the loop needs are rebuilt before jumping into it.
__declspec(naked) void DrunkSteerShiftThunk() {
    __asm {
        push kFrameTickDrunkSteer
        call FrameTick
        add esp, 4
        test eax, eax
        je skipped
        lea eax, [ebx + 0x72]
        mov ecx, 9
        jmp kDrunkSteerShiftResume
    skipped:
        jmp kDrunkSteerShiftSkip
    }
}

// The remainder the integer divide used to throw away. It is a fraction of one
// counter unit and is bounded below one, so it never grows and a float holds
// it with room to spare.
float g_fatCounterCarry = 0.0f;

// `milliseconds` arrives in eax, already carry corrected by the skill progress
// wrapper above. `rate` is the exercise rate, the function's only argument.
// The divide by ten is taken in double so the remainder survives, and the
// fraction is kept for the next call rather than discarded.
void __cdecl FatCounterAdd(uint32_t milliseconds, uint32_t rate) {
    const double product =
        static_cast<double>(static_cast<uint64_t>(milliseconds) * rate);
    const double value = product / 10.0 + static_cast<double>(g_fatCounterCarry);
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    const double whole = std::floor(value);
    g_fatCounterCarry = static_cast<float>(value - whole);
    if (whole >= 1.0) {
        *reinterpret_cast<uint32_t*>(kFatCounter) +=
            static_cast<uint32_t>(whole);
    }
}

// Replaces the twenty nine bytes of integer arithmetic. The original clobbered
// eax and edx and left the new total in eax, which the instruction at the
// return address overwrites straight away, so nothing has to be handed back.
// The exercise rate sat at `[esp+8]` before the call, which is `[esp+20]` once
// the return address and the two saved registers are on the stack.
__declspec(naked) void FatCounterThunk() {
    __asm {
        push ecx
        push edx
        mov ecx, dword ptr [esp + 0x14]
        push ecx
        push eax
        call FatCounterAdd
        add esp, 8
        pop edx
        pop ecx
        ret
    }
}
// The two force paths are the additions to the latest Framerate Vigilante
// patch. Its source comments call for multiplying by 0.6 but accidentally name
// the reciprocal `normalizer` constant. Dividing by the original timestep here
// implements the documented ratio and is exactly 1.0 at 30 FPS.
__declspec(naked) void DoorForceChassisThunk() {
    __asm {
        fmul st, st(1)
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fadd dword ptr [esi + 0x14]
        jmp kDoorForceChassisReturn
    }
}

__declspec(naked) void DoorForceOtherThunk() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fadd dword ptr [esi + 0x14]
        fstp dword ptr [esi + 0x14]
        jmp kDoorForceOtherReturn
    }
}

// `_CIpow` takes the base in st(1) and the exponent in st(0). Raising the
// stock damping factor to the timestep ratio preserves the fraction left over
// across frames; unlike the linear approximation in the source patch, this is
// stable at both very short and long timesteps.
__declspec(naked) void DoorDampingFiretruckThunk() {
    __asm {
        fld dword ptr ds:[0x00872314]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        jmp kDoorDampingFiretruckReturn
    }
}

__declspec(naked) void DoorDampingOtherThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmul dword ptr [esi + 0x14]
        fstp dword ptr [esi + 0x14]
        jmp kDoorDampingOtherReturn
    }
}

// Scale the angular velocity before the original addition integrates it.
__declspec(naked) void DoorIntegrationThunk() {
    __asm {
        fld dword ptr [esi + 0x14]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        mov ecx, ebx
        jmp kDoorIntegrationReturn
    }
}

// The two replaced instructions store the target and drop the leftover
// accumulator, so both are reproduced before the filter runs. After `pushfd`,
// `pushad` and one argument push, the caller frame starts 0x28 bytes up, which
// puts the target slot at `esp + 0x3C`.
__declspec(naked) void BikeLeanTargetThunk() {
    __asm {
        fstp dword ptr [esp + 0x14]
        fstp st(0)
        pushfd
        pushad
        push esi
        lea eax, [esp + 0x3C]
        push eax
        call FilterBikeLeanTarget
        add esp, 8
        popad
        popfd
        jmp kBikeLeanTargetReturn
    }
}

// Reached by a jump, so `esp` still addresses the caller frame and `fFriction`
// is where the replaced instruction expected it. The push and pop leave the x87
// stack exactly as the original three instructions did.
__declspec(naked) void GroundFrictionClampThunk() {
    __asm {
        fld dword ptr [esp + 0x68]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fchs
        fstp dword ptr [esp + 0x68]
        jmp kGroundFrictionClampReturn
    }
}

__declspec(naked) void TurnAirResistanceThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetTurnAirResistanceFactor
        pop edx
        pop ecx
        pop eax
        popfd
        fld st(0)
        fmul dword ptr [esi + 0x50]
        fstp dword ptr [esi + 0x50]
        fld st(0)
        fmul dword ptr [esi + 0x54]
        fstp dword ptr [esi + 0x54]
        fmul dword ptr [esi + 0x58]
        fstp dword ptr [esi + 0x58]
        jmp kTurnAirResistanceReturn
    }
}

__declspec(naked) void ObjectFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov cl, byte ptr [esi + 0xB8]
        test eax, eax
        jz noTick
        inc cl
    noTick:
        jmp kObjectFakePhysicsReturn
    }
}

__declspec(naked) void CarFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov dl, byte ptr [esi + 0xB8]
        test eax, eax
        jz noTick
        inc dl
    noTick:
        jmp kCarFakePhysicsReturn
    }
}

__declspec(naked) void BikeFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov cl, byte ptr [esi + 0xB8]
        test eax, eax
        jz noTick
        inc cl
    noTick:
        jmp kBikeFakePhysicsReturn
    }
}

__declspec(naked) void TrailerFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov bl, byte ptr [esi + 0xB8]
        test eax, eax
        jz noTick
        inc bl
    noTick:
        jmp kTrailerFakePhysicsReturn
    }
}

__declspec(naked) void SirenTapThunk() {
    __asm {
        pushfd
        pushad
        push esi
        call SelectSirenReturnAddress
        add esp, 4
        mov dword ptr [esp + 28], eax
        popad
        popfd
        jmp eax
    }
}

__declspec(naked) void ScriptsProcessThunk() {
    __asm {
        pushfd
        pushad
        call ProcessAutoFpsLimit
        popad
        popfd
        mov al, byte ptr ds:[0x00A43088]
        jmp kScriptsProcessReturn
    }
}

// `SLIDE_OBJECT` takes its target at ScriptParams[1..3] and its three per-frame
// movement rates at [4..6]. Scale only those rates, then reproduce the two
// overwritten loads which select the object from its handle.
__declspec(naked) void ScriptSlideObjectThunk() {
    __asm {
        movss xmm0, dword ptr ds:[0x00A43C88]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr ds:[0x00A43C88], xmm0

        movss xmm0, dword ptr ds:[0x00A43C8C]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr ds:[0x00A43C8C], xmm0

        movss xmm0, dword ptr ds:[0x00A43C90]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr ds:[0x00A43C90], xmm0

        mov eax, dword ptr ds:[0x00A43C78]
        mov ecx, dword ptr ds:[0x00B7449C]
        jmp kScriptSlideObjectReturn
    }
}

// `ROTATE_OBJECT` takes the object handle in ScriptParams[0], a direction in
// [1], and its per-frame angular rate in [2].
__declspec(naked) void ScriptRotateObjectThunk() {
    __asm {
        movss xmm0, dword ptr ds:[0x00A43C80]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr ds:[0x00A43C80], xmm0

        mov ecx, dword ptr ds:[0x00A43C78]
        push ecx
        mov ecx, dword ptr ds:[0x00B7449C]
        jmp kScriptRotateObjectReturn
    }
}

// The glass pane stores a displacement and two angular displacements in stack
// locals. All three are per-frame quantities, whereas the integration below is
// a plain add into the pane's position/orientation.
__declspec(naked) void FallingGlassMoveThunk() {
    __asm {
        movss xmm0, dword ptr [esp + 0x20]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [esp + 0x20], xmm0
        movss xmm0, dword ptr [esp + 0x24]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [esp + 0x24], xmm0
        movss xmm0, dword ptr [esp + 0x28]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [esp + 0x28], xmm0
        fld dword ptr [esp + 0x20]
        fadd dword ptr [esi]
        jmp kFallingGlassMoveReturn
    }
}

__declspec(naked) void FallingGlassTurnAThunk() {
    __asm {
        movss xmm0, dword ptr [eax]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax], xmm0
        movss xmm0, dword ptr [eax + 0x04]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax + 0x04], xmm0
        movss xmm0, dword ptr [eax + 0x08]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax + 0x08], xmm0
        mov ecx, dword ptr [eax]
        mov dword ptr [esp + 0x2C], ecx
        jmp kFallingGlassTurnAReturn
    }
}

__declspec(naked) void FallingGlassTurnBThunk() {
    __asm {
        movss xmm0, dword ptr [eax]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax], xmm0
        movss xmm0, dword ptr [eax + 0x04]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax + 0x04], xmm0
        movss xmm0, dword ptr [eax + 0x08]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax + 0x08], xmm0
        mov edx, dword ptr [eax]
        mov dword ptr [esp + 0x38], edx
        jmp kFallingGlassTurnBReturn
    }
}

int __cdecl ConsumeBreakObjectLifetimeTicks(int32_t* lifetime) {
    if (!lifetime || *lifetime <= 0) {
        return 0;
    }
    const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
    if (frame != g_breakLifetimeLastFrame) {
        g_breakLifetimeLastFrame = frame;
        g_breakLifetimeCarry += *reinterpret_cast<const float*>(kTimerTimeStep)
                              / g_originalTimeStepValue;
        g_breakLifetimeTicks = static_cast<int>(g_breakLifetimeCarry);
        g_breakLifetimeCarry -= static_cast<float>(g_breakLifetimeTicks);
    }
    return std::min(g_breakLifetimeTicks, *lifetime);
}

__declspec(naked) void BreakObjectLifetimeThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [edi + eax + 0x70]
        push eax
        call ConsumeBreakObjectLifetimeTicks
        add esp, 4
        mov dword ptr [esp + 24], eax
        popad
        popfd
        mov edx, dword ptr [edi + eax + 0x70]
        lea eax, [edi + eax + 0x70]
        sub edx, ecx
        mov dword ptr [eax], edx
        jmp kBreakObjectLifetimeReturn
    }
}

__declspec(naked) void MenuBackgroundThunk() {
    __asm {
        pushfd
        pushad
        call OnPauseMenuBackground
        popad
        popfd
        jmp kMenuBackgroundTarget
    }
}

// ---------------------------------------------------------------------------
// Installers
// ---------------------------------------------------------------------------

bool InstallStuntJumpCameraFix() {
    if (!InstallCall(g_endTimerPatch, kEndTimerCall, &EndTimerThunk,
                     kExpectedEndTimerCall)) {
        Log("Stunt jump camera fix skipped: camera restore timer bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallCall(g_flightTimerPatch, kFlightTimerCall, &FlightTimerThunk,
                     kExpectedFlightTimerCall)) {
        RestoreSite(g_endTimerPatch);
        Log("Stunt jump camera fix skipped: in-flight timer bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed fraction-preserving unique stunt jump timers.");
    return true;
}

bool InstallAimCameraShakeFix() {
    if (!InstallAimTimeStepOperands()) {
        Log("Aim camera shake fix skipped: Process_AimWeapon timestep bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallDetour(g_aimWeaponPatch, kProcessAimWeapon,
                       &HookedProcessAimWeapon,
                       kExpectedProcessAimWeapon.data(),
                       kExpectedProcessAimWeapon.size())) {
        RestoreAbsoluteOperandPatches(g_aimTimeStepPatches);
        Log("Aim camera shake fix failed at Process_AimWeapon entry.");
        return false;
    }
    Log("Installed local aim-camera timestep normalization without global timer writes.");
    return true;
}

bool InstallAimingRifleWalkFix() {
    if (!InstallJump(g_aimingRifleWalkPatch, kAimingRifleWalkPatch,
                     &AimingRifleWalkThunk, kExpectedAimingRifleWalk)) {
        Log("Aiming rifle walk fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent aiming rifle walk speed.");
    return true;
}

bool InstallSwimmingMovementFix() {
    if (!SwimmingMovementCodeIsUnmodified()) {
        Log("Swimming movement fix skipped: another swimming FPS fix has "
            "modified CTaskSimpleSwim::ProcessSwimmingResistance.");
        return false;
    }
    if (!InstallJump(g_swimmingPatch, kSwimResistanceCall,
                     &SwimResistanceThunk, kExpectedSwimResistanceCall)) {
        Log("Swimming movement fix skipped: CTaskSimpleSwim bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent swimming speed.");
    return true;
}

bool InstallFollowCameraRateFix() {
    if (!InstallJump(g_followPedCameraPatch, kFollowPedCameraRate,
                     &FollowPedCameraRateThunk, kExpectedCameraRateClamp)) {
        Log("Follow camera rate fix skipped: CCam bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallJump(g_followCarCameraPatch, kFollowCarCameraRate,
                     &FollowCarCameraRateThunk, kExpectedCameraRateClamp)) {
        RestoreSite(g_followPedCameraPatch);
        Log("Follow camera rate fix skipped: the car camera site does not match.");
        return false;
    }
    Log("Installed a timestep-following rate in both follow cameras.");
    return true;
}

bool InstallAttachedEntitySpeedFix() {
    if (!InstallJump(g_attachedEntitySpeedPatch, kAttachedEntitySpeed,
                     &AttachedEntitySpeedThunk, kExpectedAttachedSpeedClamp)) {
        Log("Attached entity speed fix skipped: CPhysical::PositionAttachedEntity "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real timestep in the attached entity speed.");
    return true;
}

bool InstallAiAircraftSteerFix() {
    if (!InstallJump(g_aiAircraftSteerPatch, kAiAircraftSteerRate,
                     &AiAircraftSteerRateThunk, kExpectedCameraRateClamp)) {
        Log("AI aircraft steering fix skipped: the autopilot bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real timestep in the AI aircraft steering rate.");
    return true;
}

bool InstallMoneyCounterFix() {
    if (!InstallJump(g_moneyStepPatch, kMoneyStepStore, &MoneyStepThunk,
                     kExpectedMoneyStepStore)) {
        Log("Money counter fix skipped: CPlayerInfo::Process bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real-time HUD money counter.");
    return true;
}

bool InstallClimbSpeedFix() {
    if (!InstallJump(g_climbSpeedPatch, kClimbSpeedClamp, &ClimbSpeedClampThunk,
                     kExpectedClimbSpeedClamp)) {
        Log("Climb speed fix skipped: CTaskSimpleClimb bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a clamped climb move speed.");
    return true;
}

bool InstallWaterBuoyancyFix() {
    if (!MemoryMatches(kBuoyancyThreshold, kExpectedBuoyancyThreshold)
        || !MemoryMatches(kBuoyancyClampedStore,
                          kExpectedBuoyancyClampedStore)) {
        Log("Water buoyancy fix skipped: cBuoyancy bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallJump(g_buoyancyThresholdPatch, kBuoyancyThreshold,
                     &BuoyancyThresholdThunk, kExpectedBuoyancyThreshold)) {
        Log("Water buoyancy fix failed while installing the threshold hook.");
        return false;
    }
    if (!InstallJump(g_buoyancyClampedStorePatch, kBuoyancyClampedStore,
                     &BuoyancyClampedStoreThunk,
                     kExpectedBuoyancyClampedStore)) {
        RestoreSite(g_buoyancyThresholdPatch);
        Log("Water buoyancy fix failed while installing the clamped store hook.");
        return false;
    }
    Log("Installed a timestep-normalized buoyancy cutoff.");
    return true;
}

bool InstallPedPushVehicleFix() {
    if (!InstallJump(g_pedPushCarPatch, kPedPushCarPatch, &PedPushCarThunk,
                     kExpectedPedPushCar)) {
        Log("Ped push vehicle fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent ped push force on vehicles.");
    return true;
}

bool InstallWheelFrictionFix() {
    constexpr std::array<uintptr_t, 5> addresses{
        0x006D6E69, 0x006D6EA8, 0x006D767F, 0x006D76AB, 0x006D76CD
    };
    const std::array<const void*, 5> thunks{
        &WheelFrictionCarDriveThunk,
        &WheelFrictionCarBrakeThunk,
        &WheelFrictionBikeBaseThunk,
        &WheelFrictionBikeDriveThunk,
        &WheelFrictionBikeBrakeThunk,
    };

    for (const auto address : addresses) {
        if (!MemoryMatches(address, kExpectedWheelFriction)) {
            Log("Wheel friction fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!InstallJump(g_wheelFrictionPatches[i], addresses[i], thunks[i],
                         kExpectedWheelFriction)) {
            for (auto& patch : g_wheelFrictionPatches) {
                RestoreSite(patch);
            }
            Log("Wheel friction fix failed while installing hooks.");
            return false;
        }
    }
    Log("Installed timestep-scaled car and bike wheel friction.");
    return true;
}

bool InstallAbandonedBikePhysicsStepFix() {
    g_abandonedBikePhysicsStepEnabled = false;
    g_abandonedBikePhysicsLastFrame = 0;
    g_abandonedBikePhysicsCredit = 0.0f;
    g_abandonedBikePhysicsTick = false;
    g_abandonedBikeRenderStates = {};

    if (!InstallDetour(g_abandonedBikeCollisionPatch,
                       kPhysicalProcessCollision,
                       &HookedPhysicalProcessCollision,
                       kExpectedPhysicalProcessCollision.data(),
                       kExpectedPhysicalProcessCollision.size())) {
        Log("Abandoned bike physics step skipped: ProcessCollision entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallDetour(g_abandonedBikeShiftPatch, kPhysicalProcessShift,
                       &HookedPhysicalProcessShift,
                       kExpectedPhysicalProcessShift.data(),
                       kExpectedPhysicalProcessShift.size())) {
        RestoreDetour(g_abandonedBikeCollisionPatch);
        Log("Abandoned bike physics step skipped: ProcessShift entry does not "
            "match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallDetour(g_abandonedBikeRwFramePatch, kEntityUpdateRwFrame,
                       &HookedEntityUpdateRwFrame,
                       kExpectedEntityUpdateRwFrame.data(),
                       kExpectedEntityUpdateRwFrame.size())) {
        RestoreDetour(g_abandonedBikeShiftPatch);
        RestoreDetour(g_abandonedBikeCollisionPatch);
        Log("Abandoned bike physics step skipped: UpdateRwFrame entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    const bool bikeHookWasInstalled = g_bikeProcessPatch.installed;
    if (!bikeHookWasInstalled
        && !InstallDetour(g_bikeProcessPatch, kBikeProcessControl,
                           &HookedBikeProcessControl,
                           kExpectedBikeProcessControl.data(),
                           kExpectedBikeProcessControl.size())) {
        RestoreDetour(g_abandonedBikeRwFramePatch);
        RestoreDetour(g_abandonedBikeShiftPatch);
        RestoreDetour(g_abandonedBikeCollisionPatch);
        Log("Abandoned bike physics step skipped: CBike::ProcessControl entry "
            "does not match GTA SA 1.0 US.");
        return false;
    }

    g_abandonedBikePhysicsStepEnabled = true;
    Log("Installed complete 30 Hz physics steps with render interpolation for "
        "abandoned bikes.");
    return true;
}

bool InstallRailWheelSpinFix() {
    constexpr std::array<uintptr_t, 4> addresses{
        0x006B523F, 0x006B524F, 0x006B525D, 0x006B5269
    };
    const std::array<const void*, 4> thunks{
        &RailWheelSpinThunk0,
        &RailWheelSpinThunk1,
        &RailWheelSpinThunk2,
        &RailWheelSpinThunk3,
    };

    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!MemoryMatches(addresses[i], kExpectedRailWheelSpin[i])) {
            Log("Rail wheel spin fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!InstallJump(g_railWheelSpinPatches[i], addresses[i], thunks[i],
                         kExpectedRailWheelSpin[i])) {
            for (auto& patch : g_railWheelSpinPatches) {
                RestoreSite(patch);
            }
            Log("Rail wheel spin fix failed while installing hooks.");
            return false;
        }
    }
    Log("Installed frame-independent on-rails wheel rotation.");
    return true;
}

bool InstallBurnoutFix() {
    if (!InstallJump(g_burnoutPatch, kBurnoutPatch, &BurnoutThunk,
                     kExpectedBurnout)) {
        Log("Burnout fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent burnout wheel speed.");
    return true;
}

bool InstallSkimmerResistanceFix() {
    if (!InstallJump(g_skimmerResistancePatch, kSkimmerResistancePatch,
                     &SkimmerResistanceThunk, kExpectedSkimmerResistance)) {
        Log("Skimmer resistance fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent skimmer water resistance.");
    return true;
}

bool InstallHeliRotorSpeedFix() {
    constexpr std::array<uintptr_t, 2> addresses{0x006C4F29, 0x006C4F37};
    const std::array<std::array<uint8_t, 6>, 2> expected{
        kExpectedHeliRotorSlow, kExpectedHeliRotorFast
    };
    const std::array<const void*, 2> thunks{
        &HeliRotorSlowThunk, &HeliRotorFastThunk
    };

    if (!MemoryMatches(kHeliRotorSpeedOperand, kExpectedHeliRotorOperand)) {
        Log("Helicopter rotor fix skipped: rotor speed operand does not match GTA SA 1.0 US.");
        return false;
    }
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!MemoryMatches(addresses[i], expected[i])) {
            Log("Helicopter rotor fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!InstallJump(g_heliRotorPatches[i], addresses[i], thunks[i],
                         expected[i])) {
            for (auto& patch : g_heliRotorPatches) {
                RestoreSite(patch);
            }
            Log("Helicopter rotor fix failed while installing hooks.");
            return false;
        }
    }
    Log("Installed frame-independent helicopter rotor acceleration.");
    return true;
}

bool InstallHudFlashRateFix() {
    if (!HudFlashTestSiteMatches(kHudArmorBarOperand, 8)
        || !HudFlashTestSiteMatches(kHudBreathBarOperand, 8)
        || !HudFlashSiteMatches(kHudHealthBarOperand, kHudMovPrefix)
        || !HudFlashTestSiteMatches(kHudRadarOperand, 8)
        || !HudFlashTestSiteMatches(kHudWantedActiveOperand, 4)
        || !HudFlashTestSiteMatches(kHudWantedEmptyOperand, 4)) {
        Log("HUD flash rate fix skipped: flash sites do not match GTA SA 1.0 US.");
        return false;
    }

    // 320 ms on and 320 ms off is what `frameCounter & 8` produced at the 25 FPS
    // the HUD was drawn for, so there is nothing here for a player to tune.
    g_hudFlashIntervalMs = kDefaultHudFlashIntervalMs;
    g_hudDisableFlashing = ReadSetting("hud", "disableFlashing", false);

    // Seed the clock before the game can read it, then redirect the reads.
    g_hudFlashClock = 0;
    RepointHudFlashOperand(kHudArmorBarOperand, &g_hudFlashClock);
    RepointHudFlashOperand(kHudBreathBarOperand, &g_hudFlashClock);
    RepointHudFlashOperand(kHudHealthBarOperand, g_hudDisableFlashing
                                                     ? &g_hudVisibleClock
                                                     : &g_hudFlashClock);
    RepointHudFlashOperand(kHudRadarOperand, g_hudDisableFlashing
                                                 ? &g_hudVisibleClock
                                                 : &g_hudFlashClock);
    RepointHudFlashOperand(kHudWantedActiveOperand, &g_hudFlashClock);
    RepointHudFlashOperand(kHudWantedEmptyOperand, &g_hudFlashClock);
    g_hudFlashInstalled = true;

    g_hudFlashActive = true;
    HANDLE thread = CreateThread(nullptr, 0, HudFlashThread, nullptr, 0,
                                 nullptr);
    if (!thread) {
        g_hudFlashActive = false;
        Log("HUD flash rate fix failed to start its worker thread.");
        return false;
    }
    CloseHandle(thread);

    Log(g_hudDisableFlashing
            ? "Installed the HUD flash rate fix with radar and health flashing disabled."
            : "Installed a real-time HUD flash clock at the original 25 FPS rate.");
    return true;
}

bool InstallVehicleRestThresholdFix() {
    constexpr std::array<uintptr_t, 3> addresses{
        kCarRestThreshold, kBikeRestThreshold, kTrailerRestThreshold
    };
    const std::array<const void*, 3> thunks{
        &CarRestThresholdThunk,
        &BikeRestThresholdThunk,
        &TrailerRestThresholdThunk,
    };

    for (const auto address : addresses) {
        if (!MemoryMatches(address, kExpectedRestThreshold)) {
            Log("Vehicle rest threshold fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!InstallJump(g_restThresholdPatches[i], addresses[i], thunks[i],
                         kExpectedRestThreshold)) {
            for (auto& patch : g_restThresholdPatches) {
                RestoreSite(patch);
            }
            Log("Vehicle rest threshold fix failed while installing hooks.");
            return false;
        }
    }
    Log("Installed a timestep-normalized at-rest threshold for cars, bikes and trailers.");
    return true;
}

// The five bytes differ between sites because each holds its own relative
// displacement, so instead of comparing bytes this verifies the opcode and
// resolves the displacement to check the call really does land on the expected
// callee. That is a stronger check than a byte match, not a weaker one.
bool RepointCall(SitePatch& patch, uintptr_t address, uintptr_t expectedCallee,
                 const void* replacement) {
    __try {
        if (*reinterpret_cast<const uint8_t*>(address) != 0xE8) {
            return false;
        }
        const auto original = *reinterpret_cast<const int32_t*>(address + 1);
        if (address + 5 + static_cast<uintptr_t>(original) != expectedCallee) {
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    const intptr_t displacement = reinterpret_cast<intptr_t>(replacement)
                                - static_cast<intptr_t>(address + 5);
    if (displacement < std::numeric_limits<int32_t>::min()
        || displacement > std::numeric_limits<int32_t>::max()) {
        return false;
    }

    patch.address = address;
    patch.size = 5;
    std::memcpy(patch.original.data(), reinterpret_cast<const void*>(address), 5);

    std::array<uint8_t, 5> bytes{};
    bytes[0] = 0xE8;
    const auto relative = static_cast<int32_t>(displacement);
    std::memcpy(bytes.data() + 1, &relative, sizeof(relative));
    patch.installed = WriteBytes(address, bytes.data(), bytes.size());
    return patch.installed;
}

bool InstallBikeBalanceTrace() {
    constexpr std::array<uint8_t, 8> expectedInput{
        0xD9, 0x44, 0x24, 0x68, 0xD8, 0x4C, 0x24, 0x3C
    };
    if (!InstallJump(g_bikeBalanceInputPatch, kBikeBalanceInput,
                     &BikeBalanceInputThunk, expectedInput)) {
        Log("Vehicle state trace: bike balance input site does not match.");
        return false;
    }
    if (!RepointCall(g_bikeBalanceForcePatch, kBikeBalanceForceCall,
                     kApplyTurnForce, &BikeBalanceForceThunk)) {
        RestoreSite(g_bikeBalanceInputPatch);
        Log("Vehicle state trace: bike balance force call does not match.");
        return false;
    }
    Log("Vehicle state trace: recording stock bike balance inputs and force.");
    return true;
}


bool InstallTruncCarryGroup(uint8_t group, const char* what) {
    size_t installed = 0;
    for (size_t i = 0; i < kStatTruncSites.size(); ++i) {
        if (kStatTruncSites[i].group != group) {
            continue;
        }
        if (!RepointCall(g_statTruncPatches[i], kStatTruncSites[i].address,
                         kFtol, &StatTruncCarryThunk)) {
            for (size_t j = 0; j < kStatTruncSites.size(); ++j) {
                if (kStatTruncSites[j].group == group) {
                    RestoreSite(g_statTruncPatches[j]);
                }
            }
            char skipped[128];
            std::snprintf(skipped, sizeof(skipped),
                          "%s skipped: executable bytes do not match GTA SA 1.0 US.",
                          what);
            Log(skipped);
            return false;
        }
        g_statTruncCarries[i] = 0.0f;
        ++installed;
    }
    char message[128];
    std::snprintf(message, sizeof(message),
                  "Installed fractional carry in %zu %s truncations.",
                  installed, what);
    Log(message);
    return true;
}

bool InstallSkillProgressFix() {
    return InstallTruncCarryGroup(kTruncGroupStats, "stat counter");
}

bool InstallStuntCountersFix() {
    return InstallTruncCarryGroup(kTruncGroupStunt, "stunt counter");
}

bool InstallUpsideDownTimerFix() {
    return InstallTruncCarryGroup(kTruncGroupUpsideDown, "upside down car timer");
}

bool InstallTaskTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupTask, "ped task timer");
}

bool InstallVehicleTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupVehicle, "vehicle timer");
}

bool InstallIdleCameraTimerFix() {
    return InstallTruncCarryGroup(kTruncGroupIdleCam, "idle camera timer");
}

bool InstallHudTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupHud, "HUD timer");
}

bool InstallBurnTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupBurn, "vehicle burn timer");
}

bool InstallDoorSwingFix() {
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
        size_t size;
    };
    const std::array<Site, 5> sites{{
        {kDoorForceChassis, &DoorForceChassisThunk,
         kExpectedDoorForceChassis.data(), kExpectedDoorForceChassis.size()},
        {kDoorForceOther, &DoorForceOtherThunk,
         kExpectedDoorForceOther.data(), kExpectedDoorForceOther.size()},
        {kDoorDampingFiretruck, &DoorDampingFiretruckThunk,
         kExpectedDoorDampingFiretruck.data(),
         kExpectedDoorDampingFiretruck.size()},
        {kDoorDampingOther, &DoorDampingOtherThunk,
         kExpectedDoorDampingOther.data(), kExpectedDoorDampingOther.size()},
        {kDoorIntegration, &DoorIntegrationThunk,
         kExpectedDoorIntegration.data(), kExpectedDoorIntegration.size()},
    }};

    for (size_t i = 0; i < sites.size(); ++i) {
        const Site& site = sites[i];
        if (!InstallBranch(g_doorSwingPatches[i], site.address, site.thunk,
                           site.expected, site.size, 0xE9)) {
            for (auto& patch : g_doorSwingPatches) {
                RestoreSite(patch);
            }
            Log("Door swing fix skipped: CDoor::Process bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }

    if (ReadSetting("vehicles", "disableSwingingCompletely", false)) {
        __try {
            if (NearlyEqual(
                    *reinterpret_cast<const float*>(kDoorApplyRateChassis),
                    kStockDoorApplyRateChassis)
                && WriteProtectedGameFloat(kDoorApplyRateChassis, 0.0f)) {
                g_swingingDisabled = true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    Log(g_swingingDisabled
            ? "Installed timestep-scaled door and firetruck ladder physics "
              "with chassis sway disabled."
            : "Installed timestep-scaled door, swinging chassis and "
              "firetruck ladder physics.");
    return true;
}

bool InstallWheelSpinFix() {
    struct Site {
        SitePatch* patch;
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {&g_wheelSpinPatches[0], kWheelSpinDecelLeft, &WheelSpinDecelThunk,
         kExpectedWheelSpinDecel.data()},
        {&g_wheelSpinPatches[1], kWheelSpinDecelRight, &WheelSpinDecelThunk,
         kExpectedWheelSpinDecel.data()},
        {&g_wheelSpinPatches[2], kWheelSpinAccelLeft, &WheelSpinAccelThunk,
         kExpectedWheelSpinAccel.data()},
        {&g_wheelSpinPatches[3], kWheelSpinAccelRight, &WheelSpinAccelThunk,
         kExpectedWheelSpinAccel.data()},
        {&g_wheelSpinPatches[4], kWheelSpinDampLeft, &WheelSpinDampThunk,
         kExpectedWheelSpinDamp.data()},
        {&g_wheelSpinPatches[5], kWheelSpinDampRight, &WheelSpinDampThunk,
         kExpectedWheelSpinDamp.data()}
    };

    for (const auto& site : sites) {
        if (!InstallBranch(*site.patch, site.address, site.thunk, site.expected,
                           6, 0xE8)) {
            for (auto& patch : g_wheelSpinPatches) {
                RestoreSite(patch);
            }
            Log("Free wheel spin fix skipped: CAutomobile::ProcessCarWheelPair "
                "bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed a timestep-scaled free wheel spin rate.");
    return true;
}

bool InstallSwimPitchRateFix() {
    const uintptr_t sites[] = {kSwimPitchDecayA, kSwimPitchDecayB,
                               kSwimPitchDecayC};
    for (size_t i = 0; i < g_swimPitchPatches.size(); ++i) {
        if (!InstallBranch(g_swimPitchPatches[i], sites[i], &WheelSpinDampThunk,
                           kExpectedWheelSpinDamp.data(), 6, 0xE8)) {
            for (auto& patch : g_swimPitchPatches) {
                RestoreSite(patch);
            }
            Log("Swim pitch rate fix skipped: "
                "CTaskSimpleSwim::ProcessSwimmingResistance bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed a timestep-scaled swim pitch rate.");
    return true;
}

bool InstallDrunkSteerDelayFix() {
    if (!InstallBranch(g_drunkSteerPatch, kDrunkSteerShift,
                       &DrunkSteerShiftThunk, kExpectedDrunkSteerShift.data(),
                       8, 0xE9)) {
        Log("Drunk steering delay fix skipped: CPad::Update bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    ResetFrameTicks();
    Log("Installed a time-based drunk driving steering delay.");
    return true;
}

bool InstallFatCounterFix() {
    if (!InstallBranch(g_fatCounterPatch, kFatCounterMath, &FatCounterThunk,
                       kExpectedFatCounterMath.data(), 29, 0xE8)) {
        Log("Fat counter fix skipped: CStats::UpdateFatAndMuscleStats bytes do "
            "not match GTA SA 1.0 US.");
        return false;
    }
    g_fatCounterCarry = 0.0f;
    Log("Installed a fat counter that carries the discarded remainder.");
    return true;
}

bool InstallFireSpreadFix() {
    struct Site {
        SitePatch* patch;
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {&g_fireGatePatches[0], kFireVehicleGate, &FireVehicleGateThunk,
         kExpectedFireVehicleGate.data()},
        {&g_fireGatePatches[1], kFireSpreadGate, &FireSpreadGateThunk,
         kExpectedFireSpreadGate.data()},
        {&g_fireGatePatches[2], kFireMergeGate, &FireMergeGateThunk,
         kExpectedFireMergeGate.data()}
    };

    for (const auto& site : sites) {
        if (!InstallBranch(*site.patch, site.address, site.thunk, site.expected,
                           8, 0xE9)) {
            for (auto& patch : g_fireGatePatches) {
                RestoreSite(patch);
            }
            Log("Fire spread fix skipped: CFire::ProcessFire bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    g_fireEventCarries.fill(0.0f);
    Log("Installed frame-rate independent fire event rates.");
    return true;
}

bool InstallMapZoomWheelFix() {
    if (!InstallBranch(g_mapWheelSamplePatch, kMapWheelSample,
                       &MapWheelSampleThunk, kExpectedMapWheelSample.data(), 6,
                       0xE8)) {
        Log("Map zoom wheel fix skipped: the map bounds block does not match "
            "GTA SA 1.0 US.");
        return false;
    }
    if (!InstallBranch(g_mapZoomInGatePatch, kMapZoomInGate,
                       &MapZoomInGateThunk, kExpectedMapZoomInGate.data(), 9,
                       0xE9) ||
        !InstallBranch(g_mapZoomOutGatePatch, kMapZoomOutGate,
                       &MapZoomOutGateThunk, kExpectedMapZoomOutGate.data(), 5,
                       0xE9)) {
        RestoreSite(g_mapWheelSamplePatch);
        RestoreSite(g_mapZoomInGatePatch);
        RestoreSite(g_mapZoomOutGatePatch);
        Log("Map zoom wheel fix skipped: the map zoom gates do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a frame-rate independent pause menu map zoom.");
    return true;
}

bool InstallBmxSprintLeanFix() {
    if (!InstallBranch(g_bmxSprintLeanPatch, kBmxSprintLeanDecay,
                       &WheelSpinDampThunk, kExpectedWheelSpinDamp.data(), 6,
                       0xE8)) {
        Log("BMX sprint lean fix skipped: CBmx::ProcessControl bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled BMX sprint lean decay.");
    return true;
}


bool InstallBikeWheelSpinFix() {
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kBikeWheelSpinDampA, &WheelSpinDampThunk,
         kExpectedWheelSpinDamp.data()},
        {kBikeWheelSpinDampB, &WheelSpinDampThunk,
         kExpectedWheelSpinDamp.data()},
        {kBikeWheelPitchIntegrate, &BikeWheelPitchIntegrateThunk,
         kExpectedBikeWheelPitchIntegrate.data()},
        {kBikeRearWheelSpeedDecel, &WheelSpinDecelThunk,
         kExpectedWheelSpinDecel.data()},
        {kBikeRearWheelSpeedAccel, &WheelSpinAccelThunk,
         kExpectedWheelSpinAccel.data()},
    };
    for (size_t i = 0; i < g_bikeWheelSpinPatches.size(); ++i) {
        if (!InstallBranch(g_bikeWheelSpinPatches[i], sites[i].address,
                           sites[i].thunk, sites[i].expected, 6, 0xE8)) {
            for (auto& patch : g_bikeWheelSpinPatches) {
                RestoreSite(patch);
            }
            Log("Bike wheel spin fix skipped: CBike::ProcessControl bytes do "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed a timestep-scaled bike front wheel spin.");
    return true;
}

bool InstallJetPackFxRampFix() {
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kJetPackFxRampUp, &JetPackFxRampUpThunk,
         kExpectedJetPackRampUp.data()},
        {kJetPackFxRampDown, &JetPackFxRampDownThunk,
         kExpectedJetPackRampDown.data()},
    };
    for (size_t i = 0; i < g_jetPackFxPatches.size(); ++i) {
        if (!InstallBranch(g_jetPackFxPatches[i], sites[i].address,
                           sites[i].thunk, sites[i].expected, 6, 0xE8)) {
            for (auto& patch : g_jetPackFxPatches) {
                RestoreSite(patch);
            }
            Log("Jetpack flame ramp fix skipped: "
                "CTaskSimpleJetPack::DoJetPackEffect bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed a time-based jetpack flame ramp.");
    return true;
}

bool InstallHeadBoppingFix() {
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kHeadBopRampUp, &HeadBopRampUpThunk, kExpectedHeadBopRampUp.data()},
        {kHeadBopRampDown, &HeadBopRampDownThunk,
         kExpectedHeadBopRampDown.data()},
    };
    for (size_t i = 0; i < g_headBopPatches.size(); ++i) {
        if (!InstallBranch(g_headBopPatches[i], sites[i].address,
                           sites[i].thunk, sites[i].expected, 6, 0xE8)) {
            for (auto& patch : g_headBopPatches) {
                RestoreSite(patch);
            }
            Log("Head bopping fix skipped: "
                "CTaskSimpleCarDrive::ProcessHeadBopping bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed a time-based driver head bop ramp.");
    return true;
}

bool InstallWheelSettleFix() {
    const uintptr_t sites[] = {
        kWheelSettleCarA, kWheelSettleCarB, kWheelSettleCarC, kWheelSettleCarD,
        kWheelSettleBikeA, kWheelSettleBikeB,
        kWheelSettleBmxA, kWheelSettleBmxB,
        kWheelSettleHeli, kWheelSettlePlane,
    };
    for (size_t i = 0; i < g_wheelSettlePatches.size(); ++i) {
        if (!InstallBranch(g_wheelSettlePatches[i], sites[i],
                           &WheelSettleThunk, kExpectedWheelSettle.data(), 6,
                           0xE8)) {
            for (auto& patch : g_wheelSettlePatches) {
                RestoreSite(patch);
            }
            Log("Wheel settle fix skipped: vehicle PreRender bytes do not "
                "match GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed a real-time settle for the drawn wheel position.");
    return true;
}

bool InstallCollisionPushOutFix() {
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kPushOutScaleA, &PushOutMainThunk, kExpectedPushOutMain.data()},
        {kPushOutScaleB, &PushOutMainThunk, kExpectedPushOutMain.data()},
        {kPushOutScaleC, &PushOutMainThunk, kExpectedPushOutMain.data()},
        {kPushOutScaleD, &PushOutAltThunk, kExpectedPushOutAlt.data()},
        {kPushOutScaleE, &PushOutAltThunk, kExpectedPushOutAlt.data()},
        {kPushOutScaleF, &PushOutAltThunk, kExpectedPushOutAlt.data()},
    };
    for (size_t i = 0; i < g_pushOutPatches.size(); ++i) {
        if (!InstallBranch(g_pushOutPatches[i], sites[i].address,
                           sites[i].thunk, sites[i].expected, 6, 0xE8)) {
            for (auto& patch : g_pushOutPatches) {
                RestoreSite(patch);
            }
            Log("Collision push-out fix skipped: "
                "CPhysical::ProcessShiftSectorList bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed a timestep-scaled collision push-out.");
    return true;
}

bool InstallBmxLeanSettleFix() {
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kBmxLeanLeftDecayA, &BmxLeanLeftDecayThunk,
         kExpectedBmxLeanLeftDecay.data()},
        {kBmxLeanFwdDecayA, &BmxLeanFwdDecayThunk,
         kExpectedBmxLeanFwdDecay.data()},
        {kBmxLeanLeftDecayB, &BmxLeanLeftDecayThunk,
         kExpectedBmxLeanLeftDecay.data()},
        {kBmxLeanFwdDecayB, &BmxLeanFwdDecayThunk,
         kExpectedBmxLeanFwdDecay.data()},
    };
    for (size_t i = 0; i < g_bmxLeanPatches.size(); ++i) {
        if (!InstallBranch(g_bmxLeanPatches[i], sites[i].address,
                           sites[i].thunk, sites[i].expected, 6, 0xE8)) {
            for (auto& patch : g_bmxLeanPatches) {
                RestoreSite(patch);
            }
            Log("BMX lean settle fix skipped: CBmx::ProcessDrivingAnims bytes "
                "do not match GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed a timestep-scaled BMX rider lean settle.");
    return true;
}
bool InstallJumpOutCarSpeedFix() {
    const uintptr_t sites[] = {kJumpOutTurnDampX, kJumpOutTurnDampY,
                               kJumpOutTurnDampZ, kJumpOutMoveDampX,
                               kJumpOutMoveDampY, kJumpOutMoveDampZ};
    for (size_t i = 0; i < g_jumpOutDampPatches.size(); ++i) {
        if (!InstallBranch(g_jumpOutDampPatches[i], sites[i], &JumpOutDampThunk,
                           kExpectedJumpOutDamp.data(), 6, 0xE8)) {
            for (auto& patch : g_jumpOutDampPatches) {
                RestoreSite(patch);
            }
            Log("Jump out car speed fix skipped: CVehicle::CanPedJumpOutCar "
                "bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed a timestep-scaled jump out car damping.");
    return true;
}

bool InstallBoatEngineSpeedFix() {
    if (!InstallBranch(g_boatEngineDampingPatch, kBoatEngineDamping,
                       &WheelSpinDampThunk, kExpectedWheelSpinDamp.data(), 6,
                       0xE8)) {
        Log("Boat engine speed fix skipped: CBoat::ProcessControl bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled boat engine coast down.");
    return true;
}

bool InstallSuspensionDampingLimitFix() {
    __try {
        if (!NearlyEqual(*reinterpret_cast<const float*>(kDampingLimitInFrame),
                         kStockDampingLimitInFrame)) {
            Log("Suspension damping limit fix skipped: the limit does not match "
                "GTA SA 1.0 US.");
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("Suspension damping limit fix skipped: the limit is unreadable.");
        return false;
    }

    DWORD oldProtect{};
    if (!VirtualProtect(reinterpret_cast<void*>(kDampingLimitInFrame),
                        sizeof(float), PAGE_READWRITE, &oldProtect)) {
        Log("Suspension damping limit fix failed to unprotect the limit.");
        return false;
    }

    g_dampingLimitActive = true;
    HANDLE thread = CreateThread(nullptr, 0, SuspensionDampingLimitThread,
                                 nullptr, 0, nullptr);
    if (!thread) {
        g_dampingLimitActive = false;
        Log("Suspension damping limit fix failed to start its worker thread.");
        return false;
    }
    CloseHandle(thread);
    Log("Installed a timestep-scaled suspension damping limit.");
    return true;
}

bool InstallRollOntoWheelsFix() {
    if (!RepointCall(g_rollOntoWheelsTurnPatch, kRollOntoWheelsTurnForce,
                     kApplyTurnForce, &RollOntoWheelsTurnForceThunk)) {
        Log("Roll onto wheels fix skipped: CAutomobile::ProcessSuspension turn "
            "force bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!RepointCall(g_rollOntoWheelsMovePatch, kRollOntoWheelsMoveForce,
                     kApplyMoveForce, &RollOntoWheelsMoveForceThunk)) {
        RestoreSite(g_rollOntoWheelsTurnPatch);
        Log("Roll onto wheels fix skipped: the move force site does not match.");
        return false;
    }
    Log("Installed a timestep-scaled roll onto wheels assist.");
    return true;
}

bool InstallGangWarTimerFix() {
    return InstallTruncCarryGroup(kTruncGroupWorld, "gang war timer");
}

bool InstallScriptObjectSlideFix() {
    if (!InstallJump(g_scriptSlideObjectPatch, kScriptSlideObject,
                     &ScriptSlideObjectThunk, kExpectedScriptSlideObject)) {
        Log("Script object slide fix skipped: SLIDE_OBJECT bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled SLIDE_OBJECT script rate.");
    return true;
}

bool InstallScriptRotateObjectFix() {
    if (!InstallJump(g_scriptRotateObjectPatch, kScriptRotateObject,
                     &ScriptRotateObjectThunk, kExpectedScriptRotateObject)) {
        Log("Script object rotate fix skipped: ROTATE_OBJECT bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled ROTATE_OBJECT script rate.");
    return true;
}

bool InstallFallingGlassFix() {
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kFallingGlassMove, &FallingGlassMoveThunk,
         kExpectedFallingGlassMove.data()},
        {kFallingGlassTurnA, &FallingGlassTurnAThunk,
         kExpectedFallingGlassTurnA.data()},
        {kFallingGlassTurnB, &FallingGlassTurnBThunk,
         kExpectedFallingGlassTurnB.data()}
    };
    for (size_t i = 0; i < std::size(sites); ++i) {
        if (!InstallBranch(g_fallingGlassPatches[i], sites[i].address,
                           sites[i].thunk, sites[i].expected, 6, 0xE9)) {
            for (auto& patch : g_fallingGlassPatches) {
                RestoreSite(patch);
            }
            Log("Falling glass fix skipped: FallingGlassPane::Update bytes do "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    Log("Installed timestep-scaled falling glass motion and rotation.");
    return true;
}

bool InstallBreakableObjectLifetimeFix() {
    if (!InstallJump(g_breakObjectLifetimePatch, kBreakObjectLifetime,
                     &BreakObjectLifetimeThunk,
                     kExpectedBreakObjectLifetime)) {
        Log("Breakable object lifetime fix skipped: BreakObject_c::Update "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    g_breakLifetimeLastFrame = 0xFFFFFFFFu;
    g_breakLifetimeCarry = 0.0f;
    g_breakLifetimeTicks = 0;
    Log("Installed real-time breakable object lifetime counters.");
    return true;
}

bool InstallBikeLeanTargetFix() {
    if (!InstallJump(g_bikeLeanTargetPatch, kBikeLeanTarget,
                     &BikeLeanTargetThunk, kExpectedBikeLeanTarget)) {
        Log("Bike lean target fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real-time bike lean target derivative.");
    return true;
}

bool InstallBikePitchExperiment() {
    g_bikePitchExperimentStrength = static_cast<float>(std::clamp(
        ReadNumber("vehicles", "bikePitchExperimentStrength", 50), 0, 100))
                                  / 100.0f;
    if (!RepointCall(g_bikePitchExperimentPatch, kBikeWheelTurnForceCall,
                     kApplyTurnForce, &BikePitchExperimentThunk)) {
        Log("Bike pitch experiment skipped: wheel-contact ApplyTurnForce call "
            "does not match GTA SA 1.0 US.");
        return false;
    }
    char installed[128];
    std::snprintf(installed, sizeof(installed),
                  "Installed experimental %.0f%% correction of positive bike "
                  "pitch while climbing off a ramp above 30 FPS.",
                  g_bikePitchExperimentStrength * 100.0f);
    Log(installed);
    return true;
}

bool InstallGroundFrictionFix() {
    if (!InstallJump(g_groundFrictionPatch, kGroundFrictionClamp,
                     &GroundFrictionClampThunk, kExpectedGroundFriction)) {
        Log("Ground friction fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-normalized ground friction budget for vehicles.");
    return true;
}

bool InstallTurnAirResistanceFix() {
    if (!InstallJump(g_turnAirResistancePatch, kTurnAirResistance,
                     &TurnAirResistanceThunk, kExpectedTurnAirResistance)) {
        Log("Turn air resistance fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed timestep-normalized turn speed air resistance.");
    return true;
}

bool InstallMoveSpeedSnapFix() {
    const std::array<const void*, 6> thunks{
        &MoveSpeedSnapCarXThunk,
        &MoveSpeedSnapCarYThunk,
        &MoveSpeedSnapCarZThunk,
        &MoveSpeedSnapBikeXThunk,
        &MoveSpeedSnapBikeYThunk,
        &MoveSpeedSnapBikeZThunk,
    };

    for (const auto address : kMoveSpeedSnapSites) {
        if (!MemoryMatches(address, kExpectedMoveSpeedSnap)) {
            Log("Move speed snap fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < kMoveSpeedSnapSites.size(); ++i) {
        if (!InstallJump(g_moveSpeedSnapPatches[i], kMoveSpeedSnapSites[i],
                         thunks[i], kExpectedMoveSpeedSnap)) {
            for (auto& patch : g_moveSpeedSnapPatches) {
                RestoreSite(patch);
            }
            Log("Move speed snap fix failed while installing hooks.");
            return false;
        }
    }
    Log("Installed a timestep-normalized move speed snap limit for cars and bikes.");
    return true;
}

bool InstallPhysicsSleepRateFix() {
    constexpr std::array<uintptr_t, 4> addresses{
        kObjectFakePhysics, kCarFakePhysics, kBikeFakePhysics,
        kTrailerFakePhysics
    };
    const std::array<std::array<uint8_t, 8>, 4> expected{
        kExpectedObjectFakePhysics,
        kExpectedCarFakePhysics,
        kExpectedBikeFakePhysics,
        kExpectedTrailerFakePhysics,
    };
    const std::array<const void*, 4> thunks{
        &ObjectFakePhysicsThunk,
        &CarFakePhysicsThunk,
        &BikeFakePhysicsThunk,
        &TrailerFakePhysicsThunk,
    };

    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!MemoryMatches(addresses[i], expected[i])) {
            Log("Physics sleep rate fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!InstallJump(g_fakePhysicsPatches[i], addresses[i], thunks[i],
                         expected[i])) {
            for (auto& patch : g_fakePhysicsPatches) {
                RestoreSite(patch);
            }
            Log("Physics sleep rate fix failed while installing hooks.");
            return false;
        }
    }
    Log("Installed a real-time physics sleep counter for objects, cars, bikes and trailers.");
    return true;
}

bool InstallSirenTapFix() {
    if (!MemoryMatches(kSirenAnchor, kExpectedSirenAnchor)) {
        Log("Siren tap fix skipped: ProcessSirenAndHorn layout does not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallJump(g_sirenPatch, kSirenPatch, &SirenTapThunk,
                     kExpectedSiren)) {
        Log("Siren tap fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed wall-clock siren tap detection.");
    return true;
}

bool InstallParticleEmissionRateFix() {
    ResetParticleSites();
    g_generalBudget = {};
    if (!InstallDetour(g_fxAddParticlePatch, kFxAddParticle,
                       &HookedFxAddParticle, kExpectedFxAddParticle.data(),
                       kExpectedFxAddParticle.size())) {
        Log("Particle emission rate fix skipped: FxSystem_c::AddParticle bytes "
            "do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a frame-rate independent direct particle emission rate.");
    return true;
}

bool InstallContinuousWeaponParticlesFix() {
    if (!InstallDetour(g_fxCreateParticlesPatch, kFxCreateParticles,
                       &HookedFxCreateParticles,
                       kExpectedFxCreateParticles.data(),
                       kExpectedFxCreateParticles.size())) {
        Log("Continuous weapon emission carry failed at FxEmitter::CreateParticles.");
        return false;
    }
    Log("Installed fractional emission carry for continuous weapon FX systems.");
    return true;
}

bool InstallDrowningDamageFix() {
    if (!InstallJump(g_drowningDamagePatch, kDrowningDamage,
                     &DrowningDamageThunk, kExpectedDrowningDamage)) {
        Log("Drowning damage fix skipped: CPlayerPed::HandlePlayerBreath bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed fraction-preserving drowning damage.");
    return true;
}

bool InstallContinuousWeaponAmmoFix() {
    if (!InstallJump(g_continuousAmmoPatch, kContinuousAmmoPatch,
                     &ContinuousWeaponAmmoThunk, kExpectedContinuousAmmo)) {
        Log("Continuous weapon ammo fix skipped: CWeapon::Fire bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed time-based ammo consumption for continuous area-effect weapons.");
    return true;
}

bool InstallChainsawStrikeRateFix() {
    if (!InstallBranch(g_chainsawStrikePatch, kChainsawStrikeRewind,
                       &ChainsawStrikeRewindThunk,
                       kExpectedChainsawStrikeRewind.data(),
                       kExpectedChainsawStrikeRewind.size(), 0xE8)) {
        Log("Chainsaw strike rate fix skipped: CTaskSimpleFight::ProcessPed "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a frame-rate independent chainsaw strike rate.");
    return true;
}

bool InstallFrameLimit(int limit) {
    if (!MemoryMatches(kFrameLimiterGate, kExpectedFrameLimiterGate)
        || !MemoryMatches(kFrameLimitStore, kExpectedFrameLimitStore)) {
        Log("Frame limit skipped: frame limiter bytes do not match GTA SA 1.0 US.");
        return false;
    }
    // 0x75 is `jne`, 0xEB the unconditional `jmp` with the same displacement.
    if (!InstallByte(g_frameLimiterGatePatch, kFrameLimiterGate, 0xEB)
        || !InstallByte(g_frameLimitStorePatch, kFrameLimitStoreOperand,
                        static_cast<uint8_t>(limit))) {
        RestoreByte(g_frameLimitStorePatch);
        RestoreByte(g_frameLimiterGatePatch);
        Log("Frame limit failed while patching the frame limiter.");
        return false;
    }
    WriteFrameLimit(static_cast<uint8_t>(limit));
    Log("Installed the configured frame limit.");
    return true;
}

bool InstallRefreshRate(int refreshRate) {
    if (!MemoryMatches(kRefreshRateCompare, kExpectedRefreshRate)) {
        Log("Refresh rate skipped: mode selection bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallByte(g_refreshRatePatch, kRefreshRateOperand,
                     static_cast<uint8_t>(refreshRate))) {
        Log("Refresh rate failed while patching mode selection.");
        return false;
    }
    Log("Installed the configured minimum display refresh rate.");
    return true;
}

bool InstallAutoFpsLimit() {
    if (!InstallJump(g_scriptsProcessPatch, kScriptsProcess,
                     &ScriptsProcessThunk, kExpectedScriptsProcess)) {
        Log("Automatic FPS limit skipped: CTheScripts::Process bytes do not match GTA SA 1.0 US.");
        return false;
    }
    // The cases below write `RsGlobal.frameLimit`, which the engine only reads
    // once the limiter gate is open, so this needs it too.
    if (!g_frameLimiterGatePatch.installed
        && MemoryMatches(kFrameLimiterGate, kExpectedFrameLimiterGate)) {
        InstallByte(g_frameLimiterGatePatch, kFrameLimiterGate, 0xEB);
    }
    if (g_autoLimit.flags.forPauseMenu
        && !InstallJump(g_menuBackgroundPatch, kMenuBackground,
                        &MenuBackgroundThunk, kExpectedMenuBackground)) {
        Log("Automatic FPS limit installed without the pause menu case.");
        return true;
    }
    Log("Installed automatic FPS limiting for the configured game cases.");
    return true;
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void InstallFix(const char* section, const char* key, const char* name,
                bool (*installer)(), bool defaultOn = true) {
    if (ReadSetting(section, key, defaultOn)) {
        installer();
        return;
    }
    std::string message(name);
    message += " disabled by configuration.";
    Log(message.c_str());
}

DWORD WINAPI Initialize(void*) {
    g_iniPath = ModulePathWithExtension(".ini");
    g_logPath = ModulePathWithExtension(".log");
    CreateDefaultIniIfMissing();

    // `[general]` is not written to the canonical INI: the plugin is always on
    // and the log is off, so the file a player opens holds nothing but fix
    // switches. Every key below is still read when someone adds it by hand,
    // which is how a report gets turned into a log without shipping one.
    g_loggingEnabled = ReadSetting("general", "enableLogging", false);

    if (reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) != kImageBase) {
        Log("Initialization skipped: unexpected executable image base.");
        return 0;
    }

    Log("Initializing High FPS Fixes v0.9.2.");

    InstallFix("camera", "stuntJumpCamera", "Stunt jump camera fix",
               InstallStuntJumpCameraFix);
    InstallFix("camera", "aimCameraShake", "Aim camera shake fix",
               InstallAimCameraShakeFix);
    InstallFix("camera", "followCameraRate", "Follow camera rate fix",
               InstallFollowCameraRateFix);
    InstallFix("camera", "idleCameraTimer", "Idle camera timer fix",
               InstallIdleCameraTimerFix);
    InstallFix("player", "aimingRifleWalk", "Aiming rifle walk fix",
               InstallAimingRifleWalkFix);
    InstallFix("player", "swimmingMovement", "Swimming movement fix",
               InstallSwimmingMovementFix);
    InstallFix("player", "swimPitchRate", "Swim pitch rate fix",
               InstallSwimPitchRateFix);
    InstallFix("player", "pedPushVehicle", "Ped push vehicle fix",
               InstallPedPushVehicleFix);
    InstallFix("player", "drowningDamage", "Drowning damage fix",
               InstallDrowningDamageFix);
    InstallFix("player", "drunkSteerDelay", "Drunk steering delay fix",
               InstallDrunkSteerDelayFix);
    InstallFix("player", "jetPackFlame", "Jetpack flame ramp fix",
               InstallJetPackFxRampFix);
    InstallFix("player", "fatCounter", "Fat counter fix",
               InstallFatCounterFix);
    InstallFix("player", "waterBuoyancy", "Water buoyancy fix",
               InstallWaterBuoyancyFix);
    InstallFix("player", "climbSpeed", "Climb speed fix",
               InstallClimbSpeedFix);
    InstallFix("player", "skillProgress", "Skill progress fix",
               InstallSkillProgressFix);
    InstallFix("player", "stuntCounters", "Stunt counter fix",
               InstallStuntCountersFix);
    InstallFix("player", "taskTimers", "Ped task timer fix",
               InstallTaskTimersFix);
    InstallFix("vehicles", "restThreshold", "Vehicle rest threshold fix",
               InstallVehicleRestThresholdFix);
    InstallFix("vehicles", "bikeLeanTarget", "Bike lean target fix",
               InstallBikeLeanTargetFix);
    InstallFix("vehicles", "bikePitchExperiment", "Bike pitch experiment",
               InstallBikePitchExperiment, false);
    InstallFix("vehicles", "groundFriction", "Ground friction fix",
               InstallGroundFrictionFix);
    InstallFix("vehicles", "turnAirResistance", "Turn air resistance fix",
               InstallTurnAirResistanceFix);
    InstallFix("vehicles", "moveSpeedSnap", "Move speed snap fix",
               InstallMoveSpeedSnapFix);
    InstallFix("vehicles", "physicsSleepRate", "Physics sleep rate fix",
               InstallPhysicsSleepRateFix);
    InstallFix("vehicles", "wheelFriction", "Wheel friction fix",
               InstallWheelFrictionFix);
    InstallFix("vehicles", "abandonedBikePhysicsStep",
               "Abandoned bike physics step fix",
               InstallAbandonedBikePhysicsStepFix);
    InstallFix("vehicles", "railWheelSpin", "Rail wheel spin fix",
               InstallRailWheelSpinFix);
    InstallFix("vehicles", "burnout", "Burnout fix", InstallBurnoutFix);
    InstallFix("vehicles", "doorSwing", "Door swing fix",
               InstallDoorSwingFix);
    InstallFix("vehicles", "sirenTap", "Siren tap fix", InstallSirenTapFix);
    InstallFix("vehicles", "heliRotorSpeed", "Helicopter rotor fix",
               InstallHeliRotorSpeedFix);
    InstallFix("vehicles", "attachedEntitySpeed", "Attached entity speed fix",
               InstallAttachedEntitySpeedFix);
    InstallFix("vehicles", "aiAircraftSteer", "AI aircraft steering fix",
               InstallAiAircraftSteerFix);
    InstallFix("vehicles", "upsideDownTimer", "Upside down car timer fix",
               InstallUpsideDownTimerFix);
    InstallFix("vehicles", "vehicleTimers", "Vehicle timer fix",
               InstallVehicleTimersFix);
    InstallFix("vehicles", "burnTimers", "Vehicle burn timer fix",
               InstallBurnTimersFix);
    InstallFix("vehicles", "rollOntoWheels", "Roll onto wheels fix",
               InstallRollOntoWheelsFix);
    InstallFix("vehicles", "suspensionDampingLimit",
               "Suspension damping limit fix",
               InstallSuspensionDampingLimitFix);
    InstallFix("vehicles", "collisionPushOut", "Collision push-out fix",
               InstallCollisionPushOutFix);
    InstallFix("vehicles", "wheelSettle", "Wheel settle fix",
               InstallWheelSettleFix);
    InstallFix("vehicles", "wheelSpin", "Free wheel spin fix",
               InstallWheelSpinFix);
    InstallFix("vehicles", "boatEngineSpeed", "Boat engine speed fix",
               InstallBoatEngineSpeedFix);
    InstallFix("vehicles", "bmxSprintLean", "BMX sprint lean fix",
               InstallBmxSprintLeanFix);
    InstallFix("vehicles", "bmxLeanSettle", "BMX lean settle fix",
               InstallBmxLeanSettleFix);
    InstallFix("vehicles", "bikeWheelSpin", "Bike wheel spin fix",
               InstallBikeWheelSpinFix);
    InstallFix("vehicles", "headBopping", "Head bopping fix",
               InstallHeadBoppingFix);
    InstallFix("vehicles", "jumpOutCarSpeed", "Jump out car speed fix",
               InstallJumpOutCarSpeedFix);
    InstallFix("vehicles", "skimmerResistance", "Skimmer resistance fix",
               InstallSkimmerResistanceFix);

    if (ReadSetting("weapons", "continuousWeaponParticles", true)) {
        Sleep(500);
        InstallContinuousWeaponParticlesFix();
    } else {
        Log("Continuous weapon particle fix disabled by configuration.");
    }
    InstallFix("weapons", "continuousWeaponAmmo", "Continuous weapon ammo fix",
               InstallContinuousWeaponAmmoFix);
    InstallFix("weapons", "chainsawStrikeRate", "Chainsaw strike rate fix",
               InstallChainsawStrikeRateFix);
    // A hard ceiling on new particles a second, the way FxLimiter capped them.
    // Not written to the canonical INI: it trades effects away for frame time
    // rather than correcting a frame-rate dependence, and `emissionRate`
    // already restores the intended density. Off unless asked for by hand.
    g_particleBudget = static_cast<uint32_t>(
        std::clamp(ReadNumber("particles", "particlesPerSecond", 0), 0, 100000));
    g_particleRateGate = ReadSetting("particles", "emissionRate", true);
    if (g_particleRateGate || g_particleBudget != 0) {
        InstallParticleEmissionRateFix();
    } else {
        Log("Direct particle emission hook not needed by configuration.");
    }
    // One switch over three mechanisms: the flash clock, the money counter step
    // and the 46 timed-text accumulators. They are separate patches but one
    // symptom to a player, so they are configured together.
    if (ReadSetting("hud", "hudTiming", true)) {
        InstallMoneyCounterFix();
        InstallHudFlashRateFix();
        InstallHudTimersFix();
    } else {
        Log("HUD timing fixes disabled by configuration.");
    }
    InstallFix("world", "gangWarTimer", "Gang war timer fix",
               InstallGangWarTimerFix);
    InstallFix("world", "fireSpread", "Fire spread fix",
               InstallFireSpreadFix);
    InstallFix("world", "scriptObjectSlide", "Script object slide fix",
               InstallScriptObjectSlideFix);
    InstallFix("world", "scriptObjectRotate", "Script object rotate fix",
               InstallScriptRotateObjectFix);
    InstallFix("world", "fallingGlass", "Falling glass fix",
               InstallFallingGlassFix);
    InstallFix("world", "breakableObjectLifetime",
               "Breakable object lifetime fix", InstallBreakableObjectLifetimeFix);
    InstallFix("menu", "mapZoomWheel", "Map zoom wheel fix",
               InstallMapZoomWheelFix);

    g_fpsLimit = std::clamp(ReadNumber("framerate", "fpsLimit", 0), 0, 255);
    g_refreshRate = std::clamp(ReadNumber("framerate", "refreshRate", 0), 0,
                               255);
    if (g_fpsLimit > 0) {
        InstallFrameLimit(g_fpsLimit);
    }
    if (g_refreshRate > 0 && g_refreshRate != 60) {
        InstallRefreshRate(g_refreshRate);
    }
    g_traceCycleSkill = ReadSetting("general", "traceCycleSkill", false);
    g_traceChainsaw = ReadSetting("general", "traceChainsaw", false);
    if (g_traceChainsaw) {
        if (InstallBranch(g_fightStrikeTracePatch, kFightStrikeCall,
                          &FightStrikeTraceThunk,
                          kExpectedFightStrikeCall.data(),
                          kExpectedFightStrikeCall.size(), 0xE8)) {
            Log("Chainsaw trace: counting melee strikes.");
        } else {
            Log("Chainsaw trace: CTaskSimpleFight::ProcessPed bytes do not "
                "match GTA SA 1.0 US, strikes will read zero.");
        }
    }

    if (ReadSetting("general", "traceVehicleState", false)) {
        g_diagnosticPath = ModulePathWithExtension(".trace.log");
        InstallBikeBalanceTrace();
        constexpr std::array<uint8_t, 7> expectedBikeProcess{
            0x6A, 0xFF, 0x68, 0xEB, 0x82, 0x84, 0x00
        };
        if (InstallDetour(g_bikeProcessPatch, kBikeProcessControl,
                          &HookedBikeProcessControl, expectedBikeProcess.data(),
                          expectedBikeProcess.size())) {
            Log("Vehicle state trace: counting CBike::ProcessControl entries.");
        } else {
            Log("Vehicle state trace: CBike::ProcessControl counter unavailable.");
        }

        constexpr std::array<uint8_t, 6> expectedApplyGravity{
            0x83, 0xEC, 0x18, 0x56, 0x8B, 0xF1
        };
        if (InstallDetour(g_applyGravityPatch, kPhysicalApplyGravity,
                          &HookedApplyGravity, expectedApplyGravity.data(),
                          expectedApplyGravity.size())) {
            Log("Vehicle state trace: counting CPhysical::ApplyGravity calls.");
        } else {
            Log("Vehicle state trace: ApplyGravity counter unavailable.");
        }

        constexpr std::array<uint8_t, 6> expectedLeanWrite{
            0xD9, 0x9E, 0x48, 0x06, 0x00, 0x00
        };
        constexpr std::array<uintptr_t, 3> leanSites{
            kLeanWriteSmoother, kLeanWriteBike, kLeanWriteBmx
        };
        const std::array<const void*, 3> leanThunks{
            &LeanWriteSmootherThunk, &LeanWriteBikeThunk, &LeanWriteBmxThunk
        };
        size_t leanInstalled = 0;
        for (size_t i = 0; i < leanSites.size(); ++i) {
            if (InstallJump(g_leanWritePatches[i], leanSites[i], leanThunks[i],
                            expectedLeanWrite)) {
                ++leanInstalled;
            }
        }
        if (leanInstalled == leanSites.size()) {
            Log("Vehicle state trace: counting all three LeanAngle writers.");
        } else {
            Log("Vehicle state trace: some LeanAngle writers were not counted.");
        }
        // Four byte aligned and inside the object, or the default is kept. The range
// reaches past CPhysical so bike fields such as `DesiredLeanAngle` at 0x64C can
// be watched too.
        const int watchOffset =
            ReadNumber("general", "traceWatchOffset",
                       static_cast<int>(kPhysicalMoveSpeed + 8));
        if (watchOffset >= 0 && watchOffset <= 0x900 && (watchOffset % 4) == 0) {
            g_watchOffset = static_cast<size_t>(watchOffset);
        }
        g_watchMode = std::clamp(ReadNumber("general", "traceWatchMode", 0), 0, 2);
        g_watchHitLimit = static_cast<uint32_t>(std::clamp(
            ReadNumber("general", "traceWatchHits",
                       static_cast<int>(kWatchHitLimit)), 20, 100000));
        g_watchSampleLimit = static_cast<uint32_t>(
            std::clamp(ReadNumber("general", "traceWatchSamples", 100), 1, 10000));
        g_watchArmDelay = static_cast<uint32_t>(
            std::clamp(ReadNumber("general", "traceWatchArmDelay", 3), 1, 100));
        g_watchReportLimit = static_cast<uint32_t>(std::clamp(
            ReadNumber("general", "traceWatchReports",
                       static_cast<int>(kWatchReportLimit)), 1, 100000));

        g_watchHandler = AddVectoredExceptionHandler(1, &MoveSpeedWatchHandler);
        if (g_watchHandler) {
            Log("Vehicle state trace: write watchpoint ready.");
        } else {
            Log("Vehicle state trace: move speed write watchpoint unavailable.");
        }

        g_diagnosticActive = true;
        if (HANDLE trace = CreateThread(nullptr, 0, VehicleTraceThread, nullptr,
                                        0, nullptr)) {
            CloseHandle(trace);
            Log("Vehicle state trace enabled; writing HighFpsFixes.trace.log.");
        } else {
            g_diagnosticActive = false;
            Log("Vehicle state trace failed to start its worker thread.");
        }
    }

    if (ReadSetting("general", "tracePlayerPed", false)) {
        g_diagnosticPath = ModulePathWithExtension(".trace.log");
        g_diagnosticActive = true;
        if (HANDLE trace = CreateThread(nullptr, 0, PedTraceThread, nullptr, 0,
                                        nullptr)) {
            CloseHandle(trace);
            Log("Player ped trace enabled; writing HighFpsFixes.trace.log.");
        } else {
            Log("Player ped trace failed to start its worker thread.");
        }
    }

    g_autoLimit.value = 0;
    g_autoLimit.flags.forMissions =
        ReadSetting("autoLimitFps", "forMissions", false);
    g_autoLimit.flags.forMinigames =
        ReadSetting("autoLimitFps", "forMinigames", false);
    g_autoLimit.flags.forSchools =
        ReadSetting("autoLimitFps", "forSchools", false);
    g_autoLimit.flags.forCutscenes =
        ReadSetting("autoLimitFps", "forCutscenes", false);
    g_autoLimit.flags.forScriptedCutscenes =
        ReadSetting("autoLimitFps", "forScriptedCutscenes", false);
    g_autoLimit.flags.forPauseMenu =
        ReadSetting("autoLimitFps", "forPauseMenu", false);
    if (g_autoLimit.value != 0) {
        InstallAutoFpsLimit();
    }

    return 0;
}

void Shutdown() {
    g_diagnosticActive = false;
    if (g_watchArmed) {
        SetMoveSpeedWatch(0);
        g_watchArmed = false;
    }
    if (g_watchHandler) {
        RemoveVectoredExceptionHandler(g_watchHandler);
        g_watchHandler = nullptr;
    }
    RestoreDetour(g_bikeProcessPatch);
    RestoreDetour(g_applyGravityPatch);
    RestoreSite(g_bikeBalanceForcePatch);
    RestoreSite(g_bikeBalanceInputPatch);
    for (auto& patch : g_leanWritePatches) {
        RestoreSite(patch);
    }
    g_hudFlashActive = false;
    if (g_hudFlashInstalled) {
        for (const auto operand : {kHudArmorBarOperand, kHudBreathBarOperand,
                                   kHudHealthBarOperand, kHudRadarOperand,
                                   kHudWantedActiveOperand,
                                   kHudWantedEmptyOperand}) {
            constexpr uintptr_t original = kFrameCounter;
            WriteBytes(operand, reinterpret_cast<const uint8_t*>(&original),
                       sizeof(original));
        }
        g_hudFlashInstalled = false;
    }
    if (g_dampingLimitActive) {
        g_dampingLimitActive = false;
        Sleep(5);
        WriteGameFloat(kDampingLimitInFrame, kStockDampingLimitInFrame);
    }
    if (g_swingingDisabled) {
        WriteProtectedGameFloat(kDoorApplyRateChassis,
                                kStockDoorApplyRateChassis);
        g_swingingDisabled = false;
    }
    RestoreSite(g_menuBackgroundPatch);
    RestoreSite(g_scriptsProcessPatch);
    RestoreSite(g_scriptSlideObjectPatch);
    RestoreSite(g_scriptRotateObjectPatch);
    for (auto& patch : g_fallingGlassPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_breakObjectLifetimePatch);
    RestoreByte(g_refreshRatePatch);
    RestoreByte(g_frameLimitStorePatch);
    RestoreByte(g_frameLimiterGatePatch);
    RestoreSite(g_drowningDamagePatch);
    RestoreSite(g_continuousAmmoPatch);
    RestoreSite(g_chainsawStrikePatch);
    RestoreSite(g_fightStrikeTracePatch);
    RestoreDetour(g_fxCreateParticlesPatch);
    RestoreDetour(g_fxAddParticlePatch);
    RestoreSite(g_sirenPatch);
    for (auto& patch : g_fakePhysicsPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_restThresholdPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_moveSpeedSnapPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_turnAirResistancePatch);
    RestoreSite(g_groundFrictionPatch);
    RestoreSite(g_bikeLeanTargetPatch);
    RestoreSite(g_bikePitchExperimentPatch);
    for (auto& patch : g_heliRotorPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_skimmerResistancePatch);
    RestoreSite(g_burnoutPatch);
    for (auto& patch : g_railWheelSpinPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_wheelFrictionPatches) {
        RestoreSite(patch);
    }
    g_abandonedBikePhysicsStepEnabled = false;
    RestoreDetour(g_abandonedBikeRwFramePatch);
    RestoreDetour(g_abandonedBikeCollisionPatch);
    RestoreDetour(g_abandonedBikeShiftPatch);
    RestoreSite(g_pedPushCarPatch);
    RestoreSite(g_swimmingPatch);
    RestoreSite(g_climbSpeedPatch);
    RestoreSite(g_moneyStepPatch);
    RestoreSite(g_followPedCameraPatch);
    RestoreSite(g_followCarCameraPatch);
    RestoreSite(g_attachedEntitySpeedPatch);
    RestoreSite(g_aiAircraftSteerPatch);
    RestoreSite(g_rollOntoWheelsTurnPatch);
    RestoreSite(g_rollOntoWheelsMovePatch);
    for (auto& patch : g_doorSwingPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_wheelSpinPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_boatEngineDampingPatch);
    RestoreSite(g_bmxSprintLeanPatch);
    for (auto& patch : g_bmxLeanPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_bikeWheelSpinPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_mapWheelSamplePatch);
    RestoreSite(g_mapZoomInGatePatch);
    RestoreSite(g_mapZoomOutGatePatch);
    for (auto& patch : g_fireGatePatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_drunkSteerPatch);
    RestoreSite(g_fatCounterPatch);
    for (auto& patch : g_jetPackFxPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_headBopPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_jumpOutDampPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_pushOutPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_wheelSettlePatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_swimPitchPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_statTruncPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_buoyancyThresholdPatch);
    RestoreSite(g_buoyancyClampedStorePatch);
    RestoreSite(g_aimingRifleWalkPatch);
    RestoreDetour(g_aimWeaponPatch);
    RestoreAbsoluteOperandPatches(g_aimTimeStepPatches);
    RestoreSite(g_flightTimerPatch);
    RestoreSite(g_endTimerPatch);
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
        if (HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0,
                                         nullptr)) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        Shutdown();
    }
    return TRUE;
}
