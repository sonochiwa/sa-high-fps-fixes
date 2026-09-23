# Changelog

## 1.6.0

- Added `objectPickUp`: the player steps up to an object being picked up
  at the original speed instead of being thrown at it.
- Added `movingParts`: forklift forks, the dozer blade, the dumper bed and
  cargo ramps move at the original speed.
- Added `burglaryNoise`: the burglary noise meter fills as at 30 FPS.
- Changed the default of `forMinigames` to `30`, the rate pool was made for;
  an existing INI keeps its value, so set `forMinigames=30` by hand.
- Fixed the car radio holding the game at 30 FPS.
- Fixed the extinguisher barely putting fires out since 1.5.0.

## 1.5.0

- Added `parachuteFlight`: freefall and parachute steering turn and settle
  as at 30 FPS instead of spinning above it.
- Added `continuousWeaponShots`: the spraycan, extinguisher and
  flamethrower hurt, paint, put out and start fires at the original rate.

## 1.4.3

- Fixed `forPauseMenu` having no effect in menus, SA-MP included.

## 1.4.2

- Changed the six automatic limit keys in `[framerate]` to hold the FPS
  limit to apply instead of `0` or `1`; an existing `1` is migrated to the
  limit that case used before. A limit under `20` is raised to `20`.
- Changed the defaults of `forMissions`, `forMinigames`, `forCutscenes`,
  `forScriptedCutscenes` and `forPauseMenu` to `200`; an existing INI keeps
  its values.
- Fixed the automatic limits not installing at all since 1.3.0.
- Fixed `forPauseMenu` having no effect in the main menu.

## 1.4.1

- Fixed `log=0` still writing `HighFpsFixes.log` when a setting or a patch
  failed.

## 1.4.0

- Added `classicHandling`: `1` restores the handling of versions before
  1.3.0; the seven 1.3.0 handling keys are then ignored.

## 1.3.1

- Changed the 1.3.0 release notes and configuration descriptions to the
  short form.

## 1.3.0

- Added `turnAirResistance` and `turnAirResistanceStrength`: cars and bikes
  turn as at 30 FPS; aircraft, boats and airborne vehicles are unchanged.
- Added `steerInputRate`: steering reaches full lock in the same time at
  any frame rate.
- Added `gearChangeInertia`: engine inertia and gear change smoothing run
  at the original rate.
- Added `gearChangeKick`: an upshift kicks the car once instead of
  repeatedly, and the engine note no longer jumps to top gear early.
- Added `suspensionDampingLimit`: suspension damping keeps its 30 FPS
  strength.
- Added `suspensionLoadLean`: the body leans and dives under cornering and
  braking as far as at 30 FPS.
- Added `wheelSlipRate`: a cornering car keeps its tyre grip as at 30 FPS.
- Fixed the startup log line to carry the plugin version.

## 1.2.0

- Removed `refreshRate`; use SilentPatch for that.
- Merged `[autoLimitFps]` into `[framerate]`. An existing INI is migrated.
- Added `README.txt` to the release archive.

## 1.1.0

- Changed `enableLogging` to `log`, off by default.
- Changed the INI to be created from the file compiled into the plugin.
- Added version information to the plugin file.
- Removed `README.txt` from the release archive.

## 1.0.0

- 61 frame-rate fixes across the camera, the player, vehicles, weapons,
  particles, the HUD, the world and the menu, each with its own switch.
- Optional frame limiter, refresh-rate override and automatic limits for
  specific game cases, all off by default.
- Wins over other frame-rate plugins at shared patch sites.
- Log of every fix that installed or was skipped.
