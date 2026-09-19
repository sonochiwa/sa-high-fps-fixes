# High FPS Fixes

`HighFpsFixes.asi` is a GTA San Andreas plugin that corrects
frame-rate-dependent behavior without imposing an FPS cap.

Much of the game was tuned for 30 FPS: above it, cars brake too abruptly,
bikes rock while standing still, swimming and climbing change speed, timers
run fast and the HUD strobes. Every fix rescales one original calculation to
the real frame time, so behavior at 30 FPS is unchanged and the same result
is reached at any higher frame rate. Each fix has its own switch.

## Features

- Camera: stunt jump, aim shake, follow, idle and drunk camera timing.
- Player: swimming, diving, buoyancy, climbing, aiming walk, skill and
  stunt counters, drowning, drunk steering, jetpack flame.
- Vehicles: wheel friction, burnout, bike lean and pitch, rotor and
  propeller speed, door swing, head bop, siren tap and the timers that
  park, burn or flip a vehicle.
- Weapons: extinguisher, spraycan and flamethrower particles and ammunition,
  chainsaw strike rate.
- HUD: health, armor, breath, wanted-star and radar flashes at their
  original rate.
- World: gang war countdown, fire spread, scripted and SA-MP object movement,
  falling glass, breakable objects, pause menu map zoom.
- Optional frame limiting and automatic FPS limits for specific game cases,
  all off by default.

## Requirements

- GTA San Andreas 1.0 US (Compact or Hoodlum executable).
- An ASI loader, such as Silent's ASI Loader or Ultimate ASI Loader.

Other executables are left untouched; the log names the first site that did
not match.

## Installation

1. Extract `HighFpsFixes.asi` and `HighFpsFixes.ini` into the GTA San Andreas
   directory or its `scripts` directory.
2. Start the game.

Missing keys are added to an existing INI with their defaults, so keep a fix
disabled with `setting=0` rather than by deleting its line. Settings are read
when the game starts.

## Configuration

```ini
# High FPS Fixes v1.1.0
# Created by sonochiwa
# Source code: https://github.com/sonochiwa/sa-high-fps-fixes

[general]
log=0
overrideConflictingHooks=1

[camera]
stuntJumpCamera=1
aimCameraShake=1
followCameraRate=1
idleCameraTimer=1
drunkCameraShake=1
drunkCameraShake=1

[player]
aimingRifleWalk=1
swimmingMovement=1
swimPitchRate=1
pedPushVehicle=1
bloodyFootprints=1
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
bikePitchExperiment=1
bikePitchExperimentStrength=100
restThreshold=1
physicsSleepRate=1
wheelFriction=1
abandonedBikePhysicsStep=1
railWheelSpin=1
burnout=1
disableSwingingCompletely=0
sirenTap=1
heliRotorSpeed=1
skimmerResistance=1
attachedEntitySpeed=1
aiAircraftSteer=1
upsideDownTimer=1
vehicleTimers=1
burnTimers=1
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
sampObjectRotation=1
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
| `[general]` | | |
| `log` | `0` | Writes `HighFpsFixes.log` beside the plugin. Forced on when a setting or a patch fails. |
| `overrideConflictingHooks` | `1` | Wins over another frame-rate plugin, such as FramerateVigilante, at the sites both patch. `0` leaves the first one in place. |
| `[camera]` | | |
| `stuntJumpCamera` | `1` | Stunt jump camera timers no longer stall at very high FPS. |
| `aimCameraShake` | `1` | Removes the aim camera shake at high FPS. |
| `followCameraRate` | `1` | Follow camera turns at the same speed at any FPS. |
| `idleCameraTimer` | `1` | Idle camera starts after the same time at any FPS. |
| `drunkCameraShake` | `1` | Drunk camera sways at its original speed. |
| `[player]` | | |
| `aimingRifleWalk` | `1` | Walk step while aiming a rifle. |
| `swimmingMovement` | `1` | Surface swimming, diving and underwater movement speed. |
| `swimPitchRate` | `1` | Swim pitch settles at the original rate. |
| `pedPushVehicle` | `1` | A walking ped no longer shoves cars at high FPS. |
| `bloodyFootprints` | `1` | Bloody footprints fade in real time. |
| `drowningDamage` | `1` | Drowning damage at the original rate. |
| `drunkSteerDelay` | `1` | Drunk steering delay at the original rate. |
| `jetPackFlame` | `1` | Jetpack flame ramps in real time. |
| `fatCounter` | `1` | Fat and muscle change at the original rate. |
| `waterBuoyancy` | `1` | Buoyancy no longer fails at high FPS. |
| `climbSpeed` | `1` | Climb speed at the original rate. |
| `skillProgress` | `1` | Skill stats progress at the original rate. |
| `stuntCounters` | `1` | Wheelie, stoppie and two-wheel counters run in real time. |
| `taskTimers` | `1` | Ped task timers run in real time. |
| `[vehicles]` | | |
| `bikeLeanTarget` | `1` | A standing bike no longer rocks from side to side. |
| `bikePitchExperiment` | `1` | Removes the excessive backward pitch at bike takeoff. |
| `bikePitchExperimentStrength` | `100` | Percent of that excess removed. |
| `restThreshold` | `1` | Abandoned and wrecked vehicles come to rest after the same time. |
| `physicsSleepRate` | `1` | Vehicle physics sleep in real time. |
| `wheelFriction` | `1` | Cars and bikes brake and coast as at 30 FPS. |
| `abandonedBikePhysicsStep` | `1` | Experimental: riderless bikes run their physics at the original rate. |
| `railWheelSpin` | `1` | Train wheels turn at the original rate. |
| `burnout` | `1` | Burnout wheel speed at the original rate. |
| `disableSwingingCompletely` | `0` | `1` keeps lowrider and similar swinging bodies rigid. |
| `sirenTap` | `1` | A horn tap toggles the siren at any FPS. |
| `heliRotorSpeed` | `1` | Helicopter rotors accelerate at the original rate. |
| `skimmerResistance` | `1` | Skimmer water resistance at the original rate. |
| `attachedEntitySpeed` | `1` | Attached entities move at the same speed at any FPS. |
| `aiAircraftSteer` | `1` | AI aircraft steer at the same rate at any FPS. |
| `upsideDownTimer` | `1` | Upside-down vehicle timer runs in real time. |
| `vehicleTimers` | `1` | AI and flight timers run in real time. |
| `burnTimers` | `1` | Burning vehicles explode after the same time. |
| `wheelSettle` | `1` | Bike and aircraft wheels settle in real time. |
| `wheelSpin` | `1` | Free wheel spin at the original rate. |
| `boatEngineSpeed` | `1` | Boat propellers coast down in real time. |
| `bmxSprintLean` | `1` | BMX sprint lean returns at the original rate. |
| `bmxLeanSettle` | `1` | BMX rider lean settles in real time. |
| `bikeWheelSpin` | `1` | Free bike wheels coast down in real time. |
| `headBopping` | `1` | Driver head bop in real time. |
| `jumpOutCarSpeed` | `1` | Jumping out of a car is allowed at the same speeds at any FPS. |
| `doorSwing` | `1` | Vehicle doors swing at the original rate. |
| `[weapons]` | | |
| `continuousWeaponParticles` | `1` | Extinguisher, spraycan and flamethrower particles stay visible without the frame limiter. |
| `continuousWeaponAmmo` | `1` | Those weapons use ammunition at the original rate. |
| `chainsawStrikeRate` | `1` | Chainsaw hits fifteen times a second at any FPS. |
| `[particles]` | | |
| `emissionRate` | `1` | Particle effects emit at the original rate. |
| `[hud]` | | |
| `hudTiming` | `1` | Health, armor, breath, wanted-star and radar flashes blink at the original rate. |
| `disableFlashing` | `0` | `1` keeps the radar and the low-health bar permanently visible. |
| `[world]` | | |
| `gangWarTimer` | `1` | Gang war countdown runs in real time. |
| `fireSpread` | `1` | Fire spreads at the original rate. |
| `scriptObjectSlide` | `1` | Scripted object movement at the original speed. |
| `scriptObjectRotate` | `1` | Scripted object rotation at the original speed. |
| `sampObjectRotation` | `1` | SA-MP moving objects rotate in real time. |
| `fallingGlass` | `1` | Falling glass moves at the original speed. |
| `breakableObjectLifetime` | `1` | Broken object pieces last the same time at any FPS. |
| `[menu]` | | |
| `mapZoomWheel` | `1` | The mouse wheel zooms the pause menu map at high FPS. |
| `[framerate]` | | |
| `fpsLimit` | `0` | Frame limit in FPS, `1` to `255`. `0` leaves the game's limiter alone. |
| `refreshRate` | `0` | Minimum display refresh rate accepted during mode selection. `0` and `60` leave it alone. |
| `[autoLimitFps]` | | |
| `forMissions` | `0` | Limits FPS during missions known to break at high FPS. |
| `forMinigames` | `0` | Limits FPS to 30 during pool and the intimacy minigame. |
| `forSchools` | `0` | Limits FPS to 80 during driving, boat and bike school. |
| `forCutscenes` | `0` | Limits FPS to 60 during engine cutscenes. |
| `forScriptedCutscenes` | `0` | Limits FPS to 80 while letterbox borders are active. |
| `forPauseMenu` | `0` | Limits FPS to 60 while the pause menu is drawn. |

Automatic FPS limiting never raises the limit above the one already in effect
and restores the previous limit when the case ends.

## Release Integrity

Releases are built by GitHub Actions from the tagged commit and carry a
SHA-256 file and a build-provenance attestation:

```text
gh attestation verify HighFpsFixes-vX.Y.Z.zip -R sonochiwa/sa-high-fps-fixes
```

## License

MIT. See [LICENSE](LICENSE).
