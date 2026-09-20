# Changelog

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
