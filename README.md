# High FPS Fixes

`HighFpsFixes.asi` corrects frame-rate-dependent behavior in GTA San Andreas
without imposing an FPS cap.

The current build targets only GTA San Andreas 1.0 US (Hoodlum). It validates
the original instructions at every patch site and leaves the game untouched if
they do not match. Other executable versions are not supported.

Every fix rescales one original engine calculation against the timestep the
game had at 30 FPS, so behavior at 30 FPS is unchanged and the same result is
reached at any higher frame rate. The plugin never rewrites GTA's global
timestep, and it never caps the frame rate unless the optional frame limiting
settings are enabled explicitly.

## Features

Camera:

- Prevents unique stunt jump camera timers from stalling at very high FPS.
- Prevents high-FPS aiming-camera shake while keeping player task and roll
  timing on the real game timestep.

Player:

- Keeps the walk step used while aiming a rifle independent of FPS.
- Normalizes surface swimming, the initial dive, underwater movement, ascent
  and player buoyancy against the original 30 FPS behavior.
- Delivers the push a ped gives a vehicle at the original rate, so a walking
  ped cannot drive a car to highway speed at high FPS.

Vehicles:

- Keeps a vehicle from being snapped to a standstill at high FPS, which froze
  bikes in mid-air after a jump and stopped pushed cars dead between shoves.
- Damps vehicle turn speed by real time rather than once per rendered frame, so
  angular velocity is not bled away far faster at high FPS.
- Stops a bike rocking from side to side while standing still, by measuring the
  rider lean over real time instead of over one rendered frame.
- Measures the friction that holds a vehicle to the ground in real time rather
  than per rendered frame, so a parked vehicle can still be pushed at high FPS.
- Keeps the engine from parking an abandoned or wrecked vehicle sooner in real
  time at high FPS.
- Scales wheel friction to stop cars and bikes from braking or losing inertia
  too abruptly at high FPS.
- Keeps on-rails wheel rotation, burnout wheel speed, helicopter rotor
  acceleration and skimmer water resistance independent of FPS.
- Keeps the swinging chassis of lowriders and similar cars, and the fire truck
  ladder, swaying at the original speed instead of shaking at high FPS.
- Detects a horn tap by wall-clock time, so tapping the horn still toggles the
  siren at high FPS instead of sounding the horn.
- Eases the drawn wheel back down in real time, so a wheel kicked up by a rail
  or a kerb bounces instead of flicking for a single frame.
- Pushes a vehicle out of world geometry it overlaps at the original rate, so
  riding a rail or a kerb does not throw the car at high FPS.
- Keeps suspension damping, free wheel spin, boat propeller coast-down, rider
  lean, head bop and the roll-onto-wheels assist on real time.

Weapons:

- Keeps extinguisher foam, spraycan paint and flamethrower particles visible
  when the frame limiter is disabled.
- Keeps extinguisher, spraycan and flamethrower ammunition consumption at the
  original 30 FPS rate when the game runs faster.
- Holds a chainsaw's sustained attack to its original fifteen strikes a second
  instead of letting the hit rate rise with the frame rate.

HUD:

- Keeps the low-health, armor, breath and wanted-star flashes, and the
  scripted radar flash, blinking at their original rate instead of strobing.

General:

- Optional frame limiting, minimum display refresh rate and automatic FPS
  limiting for specific game cases. All are disabled by default.
- Can disable any individual fix through an INI file that holds nothing else.
- Writes no log and no file of its own unless a diagnostic key is added by hand.

## Requirements

- GTA San Andreas 1.0 US (Hoodlum executable).
- A compatible ASI loader.
- Windows on x86-compatible hardware.

## Installation

The release archive contains `HighFpsFixes.asi`, `HighFpsFixes.ini` and a short
`README.txt` at its root.

1. Copy `HighFpsFixes.asi` and `HighFpsFixes.ini` into the game's `scripts`
   directory.
2. Start the game normally.

The plugin creates the canonical INI beside itself if it is missing. Remove the
ASI and INI to uninstall it.

## Configuration

The shipped INI holds nothing but fix switches: the plugin is always active and
writes no log. Three groups of keys are read but not written, so a normal
install has nothing to explain and a diagnostic session is still one line away.

| Hidden key | Section | Default | Meaning |
| --- | --- | ---: | --- |
| `enableLogging` | `general` | `0` | `1` writes `HighFpsFixes.log` beside the ASI, listing every fix that installed and every one that was skipped with the reason. Add it when reporting a problem. |
| `traceVehicleState`, `traceWatchOffset`, `traceWatchMode`, `traceWatchHits`, `traceWatchSamples`, `traceWatchReports`, `traceWatchArmDelay`, `tracePlayerPed`, `traceCycleSkill`, `traceChainsaw` | `general` | `0` | Development traces. They sample vehicle or player state, or count a specific loop, into `HighFpsFixes.trace.log` and the main log. Only useful with the source at hand. |
| `particlesPerSecond` | `particles` | `0` | A hard ceiling on new particles a second, the way FxLimiter capped them. This trades effects away for frame time rather than correcting a frame-rate dependence, and `emissionRate` already restores the intended density, so it is off unless asked for by hand. |

The shipped file, and what every switch means:

```ini
# High FPS Fixes v0.9.2
# Created by sonochiwa
# Source code: https://github.com/sonochiwa/sa-high-fps-fixes

[camera]
stuntJumpCamera=1
aimCameraShake=1
followCameraRate=1
idleCameraTimer=1

[player]
aimingRifleWalk=1
swimmingMovement=1
swimPitchRate=1
pedPushVehicle=1
drowningDamage=1
drunkSteerDelay=1
jetPackFlame=1
fatCounter=1
waterBuoyancy=1
climbSpeed=1
skillProgress=1
stuntCounters=1
taskTimers=1

[vehicles]
bikeLeanTarget=1
groundFriction=1
turnAirResistance=1
moveSpeedSnap=1
restThreshold=1
physicsSleepRate=1
wheelFriction=1
railWheelSpin=1
burnout=1
swingingChassis=1
disableSwingingCompletely=1
sirenTap=1
heliRotorSpeed=1
skimmerResistance=1
attachedEntitySpeed=1
aiAircraftSteer=1
upsideDownTimer=1
vehicleTimers=1
burnTimers=1
rollOntoWheels=1
suspensionDampingLimit=1
collisionPushOut=1
wheelSettle=1
wheelSpin=1
boatEngineSpeed=1
bmxSprintLean=1
bmxLeanSettle=1
bikeWheelSpin=1
headBopping=1
jumpOutCarSpeed=1
doorSwing=1

[weapons]
continuousWeaponParticles=1
continuousWeaponAmmo=1
chainsawStrikeRate=1

[particles]
emissionRate=1

[hud]
hudTiming=1
disableFlashing=0

[world]
gangWarTimer=1
fireSpread=1
scriptObjectSlide=1
scriptObjectRotate=1
fallingGlass=1
breakableObjectLifetime=1

[menu]
mapZoomWheel=1

[framerate]
fpsLimit=0
refreshRate=0

[autoLimitFps]
forMissions=0
forMinigames=0
forSchools=0
forCutscenes=0
forScriptedCutscenes=0
forPauseMenu=0
```

| Setting | Default | Meaning |
| --- | ---: | --- |
| `stuntJumpCamera` | `1` | Enables fraction-preserving stunt timers. |
| `aimCameraShake` | `1` | Uses a local minimum timestep only inside the on-foot aim-camera calculations. 
| `followCameraRate` | `1` | Divides the follow cameras' turn rate by the real timestep instead of clamping the divisor at 1.0. The clamp only binds above 50 FPS, where it leaves the rate short by the ratio. |
| `idleCameraTimer` | `1` | Same carry on `CIdleCam::ProcessIdleCamTicker`, which counts truncated frame time until the idle camera starts drifting. |
| `aimingRifleWalk` | `1` | Scales the walk step used while aiming a rifle. |
| `swimPitchRate` | `1` | Raises the swim pitch rate decay in `CTaskSimpleSwim::ProcessSwimmingResistance` to the timestep. Unpatched, the rate at which a swimmer pitches up or down decays once per frame while the build-up and the angle integration two instructions away both use the timestep, so the swim angle barely responds at a high frame rate. |
| `swimmingMovement` | `1` | Converts the per frame animation shift into a speed for the swim task, which is the target its already time-correct blend converges to. 
| `waterBuoyancy` | `1` | Evaluates the buoyancy cutoff in original timestep units. Unpatched, a rising swimmer loses all lift above a few hundred FPS and surfacing crawls. |
| `climbSpeed` | `1` | Clamps the climb move speed the way the sibling branch already does. Unpatched, the last part of a climb leaves a move speed the impact code reads as a lethal fall. |
| `skillProgress` | `1` | Carries the fraction that `_ftol` discards in all 21 stat counter truncations in `CStats::UpdateStatsWhen*`. Unpatched, every skill that levels through use (stamina, cycling, swimming, lung capacity, driving, flying, motorbike, fat, max health) advances more slowly the higher the frame rate, and stops entirely above about 1000 FPS. |
| `stuntCounters` | `1` | Same carry applied to the wheelie, stoppie and two-wheel counters in `CPlayerInfo::Process`, and to the grace buffers that let a stunt survive a brief interruption. Unpatched, stunt time accumulates more slowly the higher the frame rate. |
| `taskTimers` | `1` | Same carry on six ped and player task timers: target evaluation, stealth kill, time in air, the climb timeout and the melee combo window. Each is compared against a threshold in milliseconds, so the truncation moves the threshold. |
| `pedPushVehicle` | `1` | Delivers the collision impulse a ped applies to a vehicle at the original 30 FPS rate. |
| `drowningDamage` | `1` | Carries the fraction of drowning damage that integer truncation discards into the next frame. Above roughly 150 FPS the unpatched game deals none at all. |
| `drunkSteerDelay` | `1` | Shifts the steering delay line in `CPad::Update` at the original 30 FPS rate. The buffer is a ten deep FIFO of steering samples shifted once per frame, and a script sets how many entries of lag the player gets when drunk, so the lag is measured in frames: nine entries are 300 ms at 30 FPS and 4.5 ms at 2000, which removes the effect entirely. |
| `jetPackFlame` | `1` | Ramps the jetpack thruster flame by time rather than by frames. `CTaskSimpleJetPack::DoJetPackEffect` moves `m_FxKeyTime` by 0.1 per frame toward 1 while the thrusters fire and back toward 0 when they stop, and hands it to the particle system as its constant time; ten frames is a third of a second at 30 FPS and twenty milliseconds at 500, so the flame snaps between its two states instead of blending. Cosmetic. |
| `fatCounter` | `1` | Carries the remainder that `CStats::UpdateFatAndMuscleStats` throws away. The counter takes `milliseconds * exerciseRate / 10` in integer arithmetic, and that divide keeps no remainder: at 30 FPS the numerator is 33 times the rate, at 500 FPS it is 2 times the rate, so any exercise rate below five yields zero on every frame and fat never burns off however far the player runs. The divide is done in floating point and the fraction is kept for the next frame. Sits below the `_ftol` that `skillProgress` already repaired, and needs it. |
| `bikeLeanTarget` | `1` | Measures the lean target over one original frame and blends that stabilized value in continuously above 30 FPS; the correction is exactly zero at the stock rate. The measurement carries the whole velocity vector and projects it onto the bike's right axis only after differencing, so a steady corner still reports its centripetal term. |
| `groundFriction` | `1` | Scales the per-contact friction budget that holds a vehicle to the ground by the timestep ratio. |
| `turnAirResistance` | `1` | Raises the `0.99` turn speed damping to the timestep ratio instead of applying it once per frame. |
| `moveSpeedSnap` | `1` | Rescales the fixed move speed limit that cars and bikes snap to a stop under. |
| `restThreshold` | `1` | Rescales the at-rest move distance limit for abandoned and wrecked vehicles. |
| `physicsSleepRate` | `1` | Steps the `m_nFakePhysics` sleep counter in real time instead of once per frame. |
| `wheelFriction` | `1` | Scales car and bike wheel friction by the current timestep. |
| `railWheelSpin` | `1` | Scales on-rails wheel rotation by the current timestep. |
| `burnout` | `1` | Scales burnout wheel speed by the current timestep. |
| `swingingChassis` | `1` | Rescales the `CDoor::Process` swing tuning against the current frame time. |
| `disableSwingingCompletely` | `1` | Keeps the body rigid, which is a preference rather than a fix: it suppresses the sway the original game has at 30 FPS as well. Set `0` for the stock behavior, corrected by `doorSwing` and `swingingChassis`. |
| `sirenTap` | `1` | Detects a horn tap by wall-clock time instead of frame count. |
| `heliRotorSpeed` | `1` | Scales helicopter rotor acceleration by the current timestep. |
| `skimmerResistance` | `1` | Scales skimmer water resistance by the current timestep. |
| `attachedEntitySpeed` | `1` | Divides the distance an attached entity moved by the real timestep instead of clamping the divisor at 1.0. The clamp only binds above 50 FPS, where the resulting speed, and the force fed back into whatever the entity hangs off, come out short by the ratio. |
| `aiAircraftSteer` | `1` | Divides the AI aircraft autopilot damping term by the real timestep instead of clamping the divisor at 1.0. The clamp only binds above 50 FPS, where the term fades out as the frame rate rises and leaves AI planes and helicopters under-damped. |
| `upsideDownTimer` | `1` | Same carry applied to `CUpsideDownCarCheck::UpdateTimers`, which adds the truncated frame time to the timer of every car currently on its roof. |
| `vehicleTimers` | `1` | Same carry on the `CCarCtrl::UpdateCarAI` timer and the `CVehicle::FlyingControl` timer. |
| `burnTimers` | `1` | Same carry on the burn timers of cars, bikes and boats, which count how long a burning vehicle has before it explodes. |
| `rollOntoWheels` | `1` | Scales the roll-onto-wheels assist in `CAutomobile::ProcessSuspension` by the timestep ratio. Unpatched it applies a fixed righting impulse once per frame with no timestep, so a car resting on its side is pushed upright in proportion to the frame rate. Only fires while a nearly stationary car is on its side. |
| `suspensionDampingLimit` | `1` | Scales the per-frame suspension damping limit by the timestep ratio. Unpatched the limit is a fixed 0.25 measured in frames, so it clips the damping of Infernus, Cheetah, Super GT, Elegy and a few others at 30 FPS and never clips it at a high frame rate, leaving their suspension about a third stiffer than intended. |
| `collisionPushOut` | `1` | Scales the penetration push-out in `CPhysical::ProcessShiftSectorList` by the timestep ratio. The function ends by adding the frame's deepest collision point, along the averaged contact normal, times `0.75` or `1.5` straight onto the entity's position, with no timestep anywhere in the product. A one-off impact hides it because the penetration comes from the same frame's movement, but a vehicle riding something it overlaps by a fixed depth — a rail, a kerb, a low wall — is pushed out once per rendered frame, so at 150 FPS it leaves five times as fast and is thrown off instead of eased over. All six multiplies are scaled, capped at the stock value so 30 FPS and below are untouched. Shared by every physical entity, not only vehicles. |
| `wheelSettle` | `1` | Eases the drawn wheel back down in real time. Every vehicle `PreRender` keeps a visual wheel offset separate from the suspension, snaps it up in one step and eases it down with `position += (target - position) * 0.75` once per rendered frame, with no timestep. Three frames cover the move, so a wheel kicked up by a rail or a kerb settles over 100 ms at 30 FPS and over 20 ms at 150 FPS, where it reads as a one-frame flick rather than a bounce. Ten sites: four wheels on cars, two on bikes, two on BMX, and the gear loops on helicopters and planes. Cosmetic — the drawn wheel, not the suspension. |
| `boatEngineSpeed` | `1` | Scales the boat engine coast down in `CBoat::ProcessControl` by the timestep. The propeller speed of a boat nobody is driving falls by a fixed 5% per frame, while the three branches that drive the same field under control all use the timestep, so an abandoned boat's propeller stops and its engine note dies far sooner at a high frame rate. |
| `bikeWheelSpin` | `1` | Coasts a bike's free front wheel down in real time. `CBike::ProcessControl` holds two copies of the same five instructions, on the two sides of a rider flag; the copy at `0x6BB59B` multiplies the wheel's angular velocity by the timestep before the pitch angle integrates it and the copy at `0x6BAC77` does not, so the free front wheel spins sixteen times as fast at 500 FPS as at 30. The rear wheel a page below carries the timestep too, which makes that one copy the odd one out of three. The `0.95` decay, the same instruction `wheelSpin` fixes on cars, is raised to the timestep in both copies. Cosmetic; it is the visible wheel spin, not the physics. |
| `headBopping` | `1` | Ramps the driver's head bop by time rather than by frames. `CTaskSimpleCarDrive::ProcessHeadBopping` moves the bop weight by 0.05 per frame between 0 and 1, twenty frames from still to full, and the weight drives how far the head actually moves. Cosmetic. |
| `bmxSprintLean` | `1` | Raises the BMX sprint lean decay in `CBmx::ProcessControl` to the timestep. Cosmetic: the rider's body sway returns to neutral once per frame regardless of frame length, so it snaps back instead of easing at a high frame rate. |
| `bmxLeanSettle` | `1` | Settles the BMX rider's animated lean in real time. `CBmx::ProcessDrivingAnims` decays `AnimLeanLeft` and `AnimLeanFwd` by `0.95` once per frame in two branches, four instructions in all, while twenty bytes above the first pair the same function decays another field with `pow(rate, GetTimeStep())`. At 500 FPS the lean snaps to neutral instead of easing. Cosmetic, and the sibling of `bmxSprintLean`. |
| `jumpOutCarSpeed` | `1` | Raises the two speed dampings in `CVehicle::CanPedJumpOutCar` to the timestep. Both are applied once per call with no timestep, between a comparison and a fallthrough that are timestep-correct, so a slow vehicle the player is bailing out of is brought to a halt harder the higher the frame rate. |
| `wheelSpin` | `1` | Scales the free wheel spin rate in `CAutomobile::ProcessCarWheelPair` by the timestep. The wheel rotation is integrated with a timestep two instructions later, but the speed feeding it is changed once per frame without one, so an airborne wheel spins up or stops almost instantly at a high frame rate. |
| `doorSwing` | `1` | Scales the damping and the integration in `CDoor::Process` by the timestep. Unpatched, doors, boots and bonnets damp once per frame and integrate their angle once per frame with no timestep, so they barely move at a high frame rate. Also governs the lowrider chassis sway, which is implemented through the same code. |
| `continuousWeaponParticles` | `1` | Preserves fractional continuous-weapon particle emission. |
| `continuousWeaponAmmo` | `1` | Limits continuous area-effect weapon ammo use to the original 30 FPS rate. |
| `chainsawStrikeRate` | `1` | Holds the player's held chainsaw to the original fifteen strikes a second. `CTaskSimpleFight::ProcessPed` keeps the cut going by rewinding the moving-attack animation to `hit - 0.01` every time it passes `chain`, and `melee.dat` places the chainsaw's `hit` and `chain` 0.0033 s apart, which is shorter than one frame even at 30 FPS. The rewind and the strike cannot fall on the same frame, so the loop costs a near constant two to four frames whatever the frame rate: fifteen hits a second at 30 FPS, three times that at 144 FPS, against peds and vehicles alike. A strike is now armed on a millisecond clock every 66.7 ms; the passes in between park the animation just past `hit`, where the strike test cannot fire. No effect at 30 FPS or below. |
| `emissionRate` | `1` | Opens each direct particle call site in `FxSystem_c::AddParticle` thirty times a second. Almost every one of the 43 sites sits in a per-frame update and adds a fixed number of particles with no timestep, so exhaust smoke, tyre spray, boat wake, water cannon and sandstorm all thicken with the frame rate — about 66 times the intended density at 2000 FPS. A site that was idle on the previous frame is always let through, so intermittent spawns such as shell casings, sparks and debris never lose a particle. No effect at 30 FPS or below. |
| `hudTiming` | `1` | One switch over the three HUD timing fixes. The flash clock is driven from real time instead of the frame counter, at the original 320 ms period. The money counter step is scaled into the current frame with the fraction carried, so it counts at the original rate instead of racing through the difference. And the same carry is applied across all 46 timers behind the HUD's timed text and bars: area and vehicle names, help text, mission title, odd job, busted and wasted, success and failed, the fade state, the wanted stars and the player info bars. The timer carry is not an identity at 30 FPS; text stays up about 1% longer than stock. |
| `disableFlashing` | `0` | `1` keeps the radar and the low-health bar permanently visible. |
| `gangWarTimer` | `1` | Same carry on the gang war countdown in `CGangWars::Update`. |
| `fireSpread` | `1` | Evaluates the three random fire events in `CFire::ProcessFire` at the original 30 FPS rate. Each is a per-frame probability with no timestep, so nearby cars catch fire, fires propagate and fires merge as many times more often as there are frames; at 2000 FPS that is about 66 times the shipped rate. The fourth gate, object burn damage, is deliberately left alone because its body carries a timestep that cancels the extra frames. |
| `scriptObjectSlide` | `1` | Scales the per-frame movement rate of the `SLIDE_OBJECT` script opcode to the timestep. Target coordinates are untouched, so a scripted gate or platform takes the same wall-clock time to travel at any frame rate. |
| `scriptObjectRotate` | `1` | The same for the `ROTATE_OBJECT` opcode's angular rate. |
| `fallingGlass` | `1` | Scales all three per-frame vectors in `FallingGlassPane::Update` — translation and both angular components — before the stock position and orientation integration, so shattered glass falls and tumbles at the original speed. |
| `breakableObjectLifetime` | `1` | Spends each breakable object's integer lifetime from a shared 30 FPS fractional carry instead of decrementing it once per rendered frame, so debris lives for the same wall-clock time at any frame rate. |
| `mapZoomWheel` | `1` | Lets a mouse wheel notch through the pause menu map's 20 ms input tick. The wheel flag is rebuilt from the DirectInput delta every frame, so one notch is up for one frame only; at a high frame rate almost every notch misses the tick and the map zoom crawls. Held keys and the shoulder buttons keep their 50 Hz repeat, and panning is untouched. |
| `fpsLimit` | `0` | Frame limit in FPS, `1`–`255`. `0` leaves the game's limiter alone. |
| `refreshRate` | `0` | Minimum display refresh rate accepted during mode selection. `0` and `60` leave it alone. Prefer SilentPatch. |
| `forMissions` | `0` | Limits FPS during missions known to break at high FPS. |
| `forMinigames` | `0` | Limits FPS to 30 during pool and the intimacy minigame. |
| `forSchools` | `0` | Limits FPS to 80 during driving, boat and bike school. |
| `forCutscenes` | `0` | Limits FPS to 60 during engine cutscenes. |
| `forScriptedCutscenes` | `0` | Limits FPS to 80 while letterbox borders are active. |
| `forPauseMenu` | `0` | Limits FPS to 60 while the pause menu is drawn. |

Automatic FPS limiting never raises the limit above the one already in effect,
and it restores the previous limit when the case ends. It is a workaround
rather than a fix, which is why every case defaults to `0`.

Settings are read when the ASI loads; restart the game after changing them.

## Building

Open `HighFpsFixes.sln` in Visual Studio 2022 and build `Release|Win32` with the
v143 toolset. Outputs are written to `build`, and the canonical INI is copied
there after a successful build.

## Release Integrity

Tagged releases are compiled and packaged by GitHub Actions. Each release
contains the ZIP archive, a SHA-256 checksum file, and a signed GitHub artifact
attestation that binds the archive to its source commit and workflow:

```bat
gh attestation verify HighFpsFixes-v0.9.2.zip -R sonochiwa/sa-high-fps-fixes
```

The attestation is provenance and integrity verification: it proves the bytes
were produced by this repository's workflow from a specific revision. It does
not by itself say anything about the behavior of that revision.

GitHub does not issue attestations for user-owned private repositories, so while
the repository is private the workflow skips that step and the release ships the
archive and its checksum only.

## Repository Layout

```text
HighFpsFixes.sln              Visual Studio solution
Config\HighFpsFixes.ini       Canonical release configuration
src\HighFpsFixes.cpp          The whole plugin
src\HighFpsFixes.vcxproj      Visual Studio project
build\                        Generated binaries and intermediates
references\                   Local research material; not published
```

## How It Works

Most fixes replace one original floating-point instruction with a short thunk
that reapplies it against `CTimer::ms_fTimeStep / (50 / 30)`. That ratio is
`1.0` at the original 30 FPS timestep, so the patched instruction produces the
original result there and the correct per-second result above it. The remaining
fixes replace a frame counter with a wall-clock comparison, carry a fractional
remainder across frames, or rescale a global tuning value that the engine
already reads every frame, which needs no code patch at all.

Patch sites for GTA San Andreas 1.0 US:

- `0x49C505` and `0x49C6FB`: the two integer conversions in
  `CStuntJumpManager::Update` that truncate to zero during slow motion.
- `0x521500` plus thirteen direct timestep operands inside
  `CCam::Process_AimWeapon`: a private normalized timestep replaces GTA's global
  one for that function only.
- `0x61E0CA`: aiming rifle walk step.
- `0x68A42B`, `0x68A4CA`, `0x68A50E` and `0x6C27AE`: initial dive, ascent,
  swimming movement vectors and player buoyancy.
- `0x549652`: ped push force applied to a vehicle.
- `0x6D6E69`, `0x6D6EA8`, `0x6D767F`, `0x6D76AB` and `0x6D76CD`: car and bike
  wheel friction.
- `0x6B523F`, `0x6B524F`, `0x6B525D` and `0x6B5269`: on-rails wheel rotation.
- `0x6A4FE6`: burnout wheel speed.
- `0x544D29`: the flat per-frame turn speed damping in
  `CPhysical::ApplyAirResistance`.
- `0x545736`: the per-frame friction budget in the vehicle branch of
  `CPhysical::ApplyFriction`, beside a ped branch that already scales it.
- `0x6BBB0D`: the rider lean target in `CBike::ProcessControl`, a per-call
  derivative whose conditioning collapses as the timestep shrinks.
- `0x6B33F6`, `0x6B340C`, `0x6B3422`, `0x6BC101`, `0x6BC117` and `0x6BC129`: the
  fixed move speed limit that `CAutomobile::ProcessControl` and
  `CBike::ProcessControl` snap an entity to a standstill under.
- `0x6B1C9C`, `0x6B9955` and `0x6F9B92`: the at-rest move distance limit for
  abandoned and wrecked vehicles.
- `0x5A241F`, `0x6B1D2A`, `0x6B9972` and `0x6F9BD1`: the `m_nFakePhysics` sleep
  counter in `CObject`, `CAutomobile`, `CBike` and `CTrailer`.
- `0x872314`-`0x872338`: the `CDoor::Process` swing tuning globals, rescaled
  every frame rather than patched.
- `0x5890AF`, `0x58919F`, `0x58927E`, `0x58A363`, `0x58DDBC` and `0x58DE69`:
  the six HUD flash sites, repointed from `CTimer::m_FrameCounter` at a
  real-time plugin counter.
- `0x6E0961`: horn tap versus hold in `CVehicle::ProcessSirenAndHorn`.
- `0x6C4F29` and `0x6C4F37`: helicopter rotor acceleration.
- `0x6D2771`: skimmer water resistance.
- `0x4A41E0`: preserves fractional emission intensity across short-lived
  continuous weapon FX system instances in `FxEmitter::CreateParticles`.
- `0x7428A8`: skips the per-frame ammo-decrement branch in `CWeapon::Fire`
  until one original-rate consumption interval has elapsed.
- `0x53E94C`, `0x619626`, `0x74612A`, `0x46A000` and `0x57C324`: used only by
  the optional frame limiting settings.

The emission hook is limited to systems marked by GTA as must-create weapon FX.
Ordinary world, vehicle and weather emitters retain their original behavior.
The aim fix never writes `CTimer::ms_fTimeStep` or
`CTimer::ms_fTimeStepNonClipped`, so unrelated camera processing and player
task transitions continue using the real frame duration.
The ammo hook is restricted to the flamethrower, spraycan and fire extinguisher;
other weapons keep the original path.

## Validation Status

Implementation and runtime validation are tracked separately. A fix that
compiles and installs is not a validated fix.

61 behavioral fixes ship enabled by default, across more than 230 patched
instruction sites. 22 of them have been checked in game: 18 individually, and
four more as one group whose combined symptom was confirmed without separating
which member carries the improvement. That is about 36 percent of the shipped
fixes validated, and roughly half of the work this project has mapped out
closed. Three of the fixes — the HUD flash clock, the money counter and the 46
timed-text accumulators — share the single `hudTiming` switch, so there are 59
switches for 61 fixes.

### Confirmed in game

Each was checked by comparing a capped 30 FPS run against an uncapped one.

| Key | Evidence |
| --- | --- |
| `followCameraRate` | Confirmed 2026-08-25 |
| `idleCameraTimer` | Confirmed 2026-08-27 |
| `swimmingMovement` | Underwater swimming ran at roughly a seventeenth speed at 500 FPS and matches 30 FPS after the rewrite |
| `waterBuoyancy` | Surfacing near the waterline was very slow at 500 FPS and is normal after the fix |
| `swimPitchRate` | Confirmed 2026-08-27 |
| `climbSpeed` | The trace shows the clamp engaging: 61 consecutive samples pinned at 0.200000 where the unpatched division reached 3.16 |
| `drowningDamage` | Confirmed 2026-08-25 |
| `skillProgress` | Cycling confirmed 2026-08-27: 1000 a second at 550+ FPS, the same as at 30 FPS. The other twenty counters have never been run |
| `bikeLeanTarget` | Standing wobble fell from 12-20 degrees peak to peak to 1.31, and cornering lean was confirmed after the projection order was corrected |
| `wheelSettle` | A wheel kicked up by a rail reads the same at 30 FPS and uncapped |
| `doorSwing` | Confirmed 2026-08-25, chassis sway only; vehicle doors not separately checked |
| `hudTiming` | Money counter confirmed 2026-08-25; the flash clock and the 46 text timers it also switches are pending |
| `scriptObjectSlide` | Confirmed on the airport gates: normal, 30-FPS-compatible duration at high FPS |
| `fallingGlass` | Confirmed with shattered vehicle glass |
| `breakableObjectLifetime` | Confirmed at 30 FPS and uncapped; a long-session regression test is still open |
| `chainsawStrikeRate` | Fifteen strikes a second at 30 FPS and uncapped, against roughly forty-eight before |
| `continuousWeaponParticles` | Extinguisher foam confirmed 2026-08-21; spraycan and flamethrower pending |
| `continuousWeaponAmmo` | Extinguisher confirmed 2026-08-21; spraycan and flamethrower pending |
| `groundFriction`, `moveSpeedSnap`, `wheelFriction`, `swingingChassis` | Confirmed as a group: with the plugin off at about 500 FPS a car is barely drivable and a Tahoma's rear axle shakes it undrivable, and both go away with the plugin on. Which member carries which part has not been separated |

### Implemented, not yet checked

`stuntJumpCamera`, `aimCameraShake`, `aimingRifleWalk`, `pedPushVehicle`,
`drunkSteerDelay`, `jetPackFlame`, `fatCounter`, `stuntCounters`, `taskTimers`,
`turnAirResistance`, `restThreshold`, `physicsSleepRate`, `railWheelSpin`,
`burnout`, `sirenTap`, `heliRotorSpeed`, `skimmerResistance`,
`attachedEntitySpeed`, `aiAircraftSteer`, `upsideDownTimer`, `vehicleTimers`,
`burnTimers`, `rollOntoWheels`, `suspensionDampingLimit`, `collisionPushOut`,
`wheelSpin`, `boatEngineSpeed`, `bmxSprintLean`, `bmxLeanSettle`,
`bikeWheelSpin`, `headBopping`, `jumpOutCarSpeed`, `emissionRate`,
`gangWarTimer`, `fireSpread`, `scriptObjectRotate`,
`mapZoomWheel`.

What each one corrects is described in the configuration table above.

### How to check the rest

Cheapest and most visible first. For entries noted as an identity below a given
frame rate, an A/B that shows no difference is a real result.

| Fix | Test |
| --- | --- |
| `hudTiming` | Trigger help text or a mission title and time how long it stays up. Not an identity at 30 FPS; expect about 1% longer than stock |
| `burnTimers` | Set a car on fire and time it to the explosion |
| `upsideDownTimer` | Flip a car onto its roof and time it to catching fire |
| `wheelSpin` | Get a drive wheel off the ground and watch it spin up, then watch a free wheel stop |
| `suspensionDampingLimit` | Drive an Infernus, Cheetah, Super GT or Elegy over bumps at both frame rates |
| `collisionPushOut` | Ride a rail, a kerb and a low wall at both frame rates, and check that nothing sinks into or sticks in world geometry |
| `rollOntoWheels` | Tip a car onto its side and time the righting. The code predicts faster at high FPS; the old report says slower, so one of them is wrong |
| `boatEngineSpeed` | Leave a boat with the propeller turning and listen to it die |
| `drunkSteerDelay` | Get drunk, drive, and see whether the wheel lags |
| `emissionRate` | Watch exhaust smoke, tyre spray and boat wake at both frame rates, and confirm shell casings, sparks and shattering glass still appear |
| `fireSpread` | Set a car alight next to another car, and watch a fire spread on grass |
| `mapZoomWheel` | Open the pause menu map and zoom with the wheel |
| `bmxSprintLean`, `bmxLeanSettle`, `bikeWheelSpin`, `jetPackFlame`, `headBopping` | Cosmetic ramps and decays; watch each settle at both frame rates |
| `jumpOutCarSpeed` | Roll slowly, hold the exit key without leaving, and see how fast the car stops |
| `skillProgress` | Run, swim, drive and fly for a fixed wall-clock time and compare the stat bars. Only the cycling counter has been measured |
| `fatCounter` | Get fat, then run for a fixed wall-clock time at both frame rates. Needs `skillProgress` on |
| `stuntCounters` | Hold a wheelie for a fixed wall-clock time and compare the counter |
| `attachedEntitySpeed` | Drive with a trailer, or a forklift or crane load. Identity at or below 50 FPS |
| `aiAircraftSteer` | Hydra or Hunter at five stars, watching it turn toward the player. Identity at or below 50 FPS |
| `taskTimers`, `vehicleTimers` | Hard to see directly; the climb timeout and AI car behavior are the likeliest to show |
| `gangWarTimer` | Start a gang war and time a wave |
| `continuousWeaponParticles`, `continuousWeaponAmmo` | Spraycan and flamethrower; only the extinguisher has been checked |
| `groundFriction`, `moveSpeedSnap`, `wheelFriction`, `swingingChassis` | Disable one at a time at high FPS to separate which member carries the confirmed improvement |

### Open work

- **Airborne motorcycle throttle pitch.** Holding the throttle in the air
  pitches a bike backward faster at high FPS. Traced as far as the excess speed
  appearing on the ramp rather than from wheel spin in the air; the engine and
  brake reaction torque is already timestep scaled, so the cause is elsewhere.
- **The last half degree of bike roll while standing still**, left over after
  `bikeLeanTarget` removed the large wobble.
- **Landing after a jump.** The apex matches 30 FPS and its remaining gap is the
  integrator, with nothing to patch. The landing is a genuine defect: the
  suspension absorber is one call site that stops firing at high FPS.
- **The swinging chassis is damped rather than corrected.** `swingingChassis`
  rescales the tuning globals; the scale is not an identity at 30 FPS and it
  flattens the spring instead of correcting it. It needs rework, not validation.
- **Remaining uses of the 0.005 rest limit** outside the sites `restThreshold`
  covers.
- **Vehicle input smoothing** and the remaining swimming state machine, neither
  yet reproduced here as a single-player defect.
- **Effects and world scheduling** — creeping fire grid, explosion cadence,
  population and traffic generation — gated behind a confirmed high-FPS symptom.

### Out of scope

- Mission script cadence, for the compatibility surface it would touch.
- A global "make vehicle physics frame-rate independent" rewrite. Work is split
  into narrowly testable single-player behaviors instead.
- FPS caps for missions, minigames, schools and cutscenes as a substitute for
  fixing the underlying behavior. They are implemented for parity, disabled by
  default.
- `CCam::Process_Cam_TwoPlayer`, the same clamp defect in a mode that cannot be
  verified here.
- MTA-only behavior such as `setCameraShakeLevel`, whose API path may not exist
  in single-player.

## License

High FPS Fixes is released under the [MIT License](LICENSE).
