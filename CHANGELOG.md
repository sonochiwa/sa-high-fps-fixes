# Changelog

## 1.3.0

- Added `turnAirResistance` with `turnAirResistanceStrength`: the per-frame
  `0.99` yaw damping in `CPhysical::ApplyAirResistance` is raised to the
  timestep ratio for cars and bikes with a wheel on the ground, so they turn
  as at 30 FPS. Aircraft, boats, trains and airborne vehicles keep the stock
  damping, which is what the 0.9.x version of this fix got wrong.
- Added `steerInputRate`: the car and bike steering input step is
  exponential, so full lock arrives in the same time at any frame rate.
- Added `gearChangeKick`: the nose-up kick a player's car gets on an upshift
  is triggered by the audio engine after a ten frame wait, which above
  30 FPS is short enough for it to fire every eleventh frame while the
  acceleration sound sits at its end; the car pitched up to 8 degrees and
  lifted its front wheels at 100 km/h. The wait is now a third of a second
  at any frame rate, which also stops the engine note from running to top
  gear early.
- Added `wheelSlipRate`: a car wheel's lateral slip, and its longitudinal
  slip off the throttle, are velocities weighed against a per-frame grip
  budget, so above 30 FPS a cornering tyre counted as skidding every frame
  and lost the handling's traction loss for the whole corner. The slip is
  now weighed per original frame.
- Added `suspensionLoadLean`: the damper takes its bite of the contact
  speed after the springs and before the tyres, so at 30 FPS the body
  settles a quarter further under a cornering or braking load than the
  spring rate alone allows; above 30 FPS the bite shrinks with the frame
  and the lean, the weight transfer and the grip that follows it fall
  short. The 30 FPS bite is now taken at any frame rate.
- Added `suspensionDampingLimit`: the per-frame cap on suspension damping
  binds at 30 FPS for any car with a damping level above 0.15, and stops
  binding above it, so such cars were damped up to a third harder at a
  high frame rate. The 30 FPS strength is kept at any frame rate.
- Added `gearChangeInertia`: the transmission's engine inertia term and
  gear change smoother are per-frame steps and now run at the original
  rate.
- The startup log line now carries the real plugin version.

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
