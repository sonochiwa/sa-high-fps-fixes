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
constexpr uintptr_t kBikePreRender = 0x006BD090;
constexpr uintptr_t kBikeRender = 0x006BDE20;
constexpr uintptr_t kBmxLaunchBunnyHop = 0x006C0390;
constexpr uintptr_t kBikeDamageKnockOffRider = 0x006B5A10;
constexpr uintptr_t kApplySpringDampening = 0x00543E90;
constexpr std::array<uintptr_t, 2> kBikeRiderFallEventAddCalls{
    0x006B74B4, // excessive turn speed
    0x006B769B  // excessive velocity along the bike's up axis
};
constexpr uintptr_t kEventGroupAdd = 0x004AB420;
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
constexpr size_t kBikeLeanMatrixCalculated = 0x5C8;
constexpr size_t kBikeLeanMatrix = 0x5CC;
constexpr size_t kBikeFlags = 0x614;
constexpr uint8_t kBikeGettingPickedUp = 0x08;
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
constexpr size_t kMatrixForward = 0x20;
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
// Do not rewrite that global constant from a polling thread. The function entry
// is wrapped instead. Uncapped damping is converted to the short-frame alpha
// that leaves exactly the same velocity after one 30 FPS interval. Coefficients
// which reached the stock cap use its equivalent linear rate, leaving the
// nonlinear spring-force and direction clamps inside ApplySpringDampening.
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
// Rendered wheel settle. Vehicle `PreRender` functions keep a visual wheel
// offset separate from the physical suspension and ease a wheel returning down
// with `position += (target - position) * 0.75` once per rendered frame. The
// timestep-scaled weight is retained for bikes and aircraft, where it smooths
// the animation. Automobiles deliberately keep the stock weight: stretching
// their downward travel to the 30 FPS duration lets long-travel rear wheels
// visibly hang below the body after the physical suspension has already moved.
// Cosmetic — none of these sites changes the suspension that moves the vehicle.
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
