# Roadmap

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

## Implemented and validated

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
| `wheelSettle` | Bike and aircraft paths retain the previously validated real-time easing; automobile easing was removed after it exposed excessive long-travel wheel extension |
| `doorSwing` | Reconfirmed 2026-09-01 on Tahoma: the isolated swinging-chassis input fix matches the 30-FPS rear-body response without suppressing the separate firetruck ladder path; ordinary vehicle doors are not separately checked |
| `hudTiming` | Money counter confirmed 2026-08-25; the flash clock and the 46 text timers it also switches are pending |
| `scriptObjectSlide` | Confirmed on the airport gates: normal, 30-FPS-compatible duration at high FPS |
| `fallingGlass` | Confirmed with shattered vehicle glass |
| `breakableObjectLifetime` | Confirmed at 30 FPS and uncapped; a long-session regression test is still open |
| `chainsawStrikeRate` | Fifteen strikes a second at 30 FPS and uncapped, against roughly forty-eight before |
| `continuousWeaponParticles` | Extinguisher foam confirmed 2026-08-21; spraycan and flamethrower pending |
| `continuousWeaponAmmo` | Extinguisher confirmed 2026-08-21; spraycan and flamethrower pending |
| `wheelFriction`, `doorSwing` | Initially confirmed as a group at about 500 FPS. The Tahoma rear-body symptom was subsequently isolated to `doorSwing` and reconfirmed after separating its chassis and firetruck input paths |

## Implemented, validation pending

`stuntJumpCamera`, `aimCameraShake`, `aimingRifleWalk`, `pedPushVehicle`,
`drunkSteerDelay`, `jetPackFlame`, `fatCounter`, `stuntCounters`, `taskTimers`,
`restThreshold`, `physicsSleepRate`, `railWheelSpin`,
`burnout`, `sirenTap`, `heliRotorSpeed`, `skimmerResistance`,
`attachedEntitySpeed`, `aiAircraftSteer`, `upsideDownTimer`, `vehicleTimers`,
`burnTimers`, `wheelSpin`, `boatEngineSpeed`, `bmxSprintLean`, `bmxLeanSettle`,
`bikeWheelSpin`, `headBopping`, `jumpOutCarSpeed`, `emissionRate`,
`gangWarTimer`, `fireSpread`, `scriptObjectRotate`,
`mapZoomWheel`.

What each one corrects is described in the README configuration table.

## Required validation

Cheapest and most visible first. For entries noted as an identity below a given
frame rate, an A/B that shows no difference is a real result.

| Fix | Test |
| --- | --- |
| `hudTiming` | Trigger help text or a mission title and time how long it stays up. Not an identity at 30 FPS; expect about 1% longer than stock |
| `burnTimers` | Set a car on fire and time it to the explosion |
| `upsideDownTimer` | Flip a car onto its roof and time it to catching fire |
| `wheelSpin` | Get a drive wheel off the ground and watch it spin up, then watch a free wheel stop |
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

## Open work

- **Airborne motorcycle throttle pitch.** Holding the throttle in the air
  pitches a bike backward faster at high FPS. Traced as far as the excess speed
  appearing on the ramp rather than from wheel spin in the air; the engine and
  brake reaction torque is already timestep scaled, so the cause is elsewhere.
- **The last half degree of bike roll while standing still**, left over after
  `bikeLeanTarget` removed the large wobble.
- **Landing after a jump.** The apex matches 30 FPS and its remaining gap is the
  integrator, with nothing to patch. The landing is a genuine defect: the
  suspension absorber is one call site that stops firing at high FPS.
- **Remaining uses of the 0.005 rest limit** outside the sites `restThreshold`
  covers.
- **Vehicle input smoothing** and the remaining swimming state machine, neither
  yet reproduced here as a single-player defect.
- **Effects and world scheduling** — creeping fire grid, explosion cadence,
  population and traffic generation — gated behind a confirmed high-FPS symptom.

## Out of scope

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
