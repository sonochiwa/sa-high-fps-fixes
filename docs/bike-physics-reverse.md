# GTA SA bike physics: reverse-engineering map

This note tracks the GTA SA 1.0 US code paths that can change the motion of a
motorcycle or BMX between takeoff, airborne flight, landing, and settling after
the rider leaves. Addresses below refer to the 1.0 US executable.

The local `gta-reversed` tree supplies class layouts and the recovered generic
`CPhysical` pipeline. Its SA `CBike::ProcessControl` and
`CVehicle::ProcessBikeWheel` bodies are still stubs, so those two functions are
checked against the executable disassembly. reVC's recovered
`CVehicle::ProcessBikeWheel` is used only as a structural analogue where its
instructions and constants match the SA binary.

## Control and wheel-contact path

| Address | Function/site | Role | Timestep verdict | Hook risk |
| --- | --- | --- | --- | --- |
| `0x6B9250` | `CBike::ProcessControl` | Whole bike control, suspension, wheel contacts, rider balance, wheelie/stoppie stabilization | Mixed; audit each call site | Very high |
| `0x6B9DD9` | call to `CPhysical::ProcessControl` | Gravity, accumulated friction, linear and angular air resistance | Mixed | Medium |
| `0x6BA03A` | `ApplySpringCollisionAlt` | Suspension spring impulse along the contact normal | Already proportional to timestep | High |
| `0x6BA05C` | `ApplySpringCollision` | Suspension spring impulse along the suspension line | Already proportional to timestep | High |
| `0x6BA4EA` | `ApplySpringDampening` | Removes velocity along the suspension line | Timestep-scaled and impulse-limited | High |
| `0x6BAF3D` | first `ProcessBikeWheel` call | One wheel-contact ordering branch | See `0x6D73B0` | Very high |
| `0x6BB4C1` | second `ProcessBikeWheel` call | Alternate front/rear ordering branch | See `0x6D73B0` | Very high |
| `0x6BB855` | third `ProcessBikeWheel` call | Remaining wheel-contact branch | See `0x6D73B0` | Very high |
| `0x6BC2C3`, `0x6BC348` | upright balance turn forces | Rider/bike balance near the end of control | Already multiplied by `CTimer::GetTimeStep()` | High |
| `0x6BC510`, `0x6BC5A6` | wheelie stabilization | Wheelie pitch and steering correction | Already multiplied by timestep | Very high |
| `0x6BC827`, `0x6BC90F` | stoppie stabilization | Stoppie pitch and steering correction | Already multiplied by timestep | Very high |
| `0x6D73B0` | `CVehicle::ProcessBikeWheel` | Converts longitudinal/lateral contact speed into move and turn impulses | Adhesion is timestep-scaled; low-speed cancellation is an impulse | Very high |
| `0x6D7B17` | first wheel `ApplyTurnForce` | Pitch-plane part of the wheel-contact impulse | Not an independent rate; derived from the contact impulse | Do not scale broadly |
| `0x6D7BA3` | second wheel `ApplyTurnForce` | Remaining lateral/yaw part | Not an independent rate; derived from the contact impulse | Do not scale broadly |

The first turn-force call at `0x6D7B17` is the normal pitch response used by
ramps, wheelies, and landings. Scaling it globally did reduce backward pitch,
but it also made wheelies harder and changed ground/sliding behaviour. The
current experiment therefore leaves that call untouched while the front wheel
is grounded and changes only positive pitch once the front suspension is clear
and the bike has meaningful upward velocity. That covers the rear-wheel
takeoff phase plus the stale contact-timer tail while excluding a level-ground
wheelie.

A BMX bunny hop has a different takeoff order. `CBmx::LaunchBunnyHopCB` applies
one move impulse and one turn impulse as an animation event. Matched full-charge
traces showed that a small excess in local backward angular speed persists
through the contact-free flight and accumulates into about five to six extra
degrees of backward tilt before the rear wheel lands. The callback is latched
and a tested 24% asymptotic correction is applied once after the stock launch
pass, fading continuously to an exact no-op at 30 FPS. During the following
landing window, false upright rider-fall events and vertical damage knock-offs
up to intensity 31 are suppressed, while harder or sideways impacts remain
stock. The BMX suspension damping uses the exact exponential equivalent of one
30-FPS damping step, preventing the wheels from rebounding farther at high FPS.
Ordinary wheelies and jumps that did not receive a stock launch impulse are not
affected.

## Generic collision and settling path

| Address | Function/site | Role | Timestep verdict | Current action |
| --- | --- | --- | --- | --- |
| `0x544C40` / `0x544D29` | `ApplyAirResistance` angular branch | Multiplies turn speed by `0.99` once per frame | Broken in stock | `turnAirResistance` raises `0.99` to the timestep ratio |
| `0x5454C0` / `0x545736` | `ApplyFriction(float, CColPoint&)` clamp | Limits tangential impulse at a body contact | Caller-dependent | Scale raw active-vehicle budgets, not abandoned collision-scaled budgets |
| `0x54BA60` | `ProcessCollisionSectorList` | Produces collision impulses and the friction budget for body contacts | Mixed | Primary body-landing audit point |
| `0x54CC20`-`0x54CC59` | vehicle friction-budget branch | Active upright contacts keep raw surface friction; abandoned/oblique contacts multiply it by collision intensity | The latter is already timestep-sensitive | Explains riderless high-FPS sliding when scaled twice |
| `0x546670` | `ProcessShiftSectorList` | Resolves penetration by moving the entity out of geometry | Stock push is per processed frame | `collisionPushOut` scales six push sites |
| `0x6BDEA0` | `CBike::ProcessEntityCollision` | Runs body-model collision and four suspension lines, retaining the closest line hit across entities | No direct per-frame force here | Contact generation/order, not an obvious rate bug |

## Working conclusions

1. Excess backward pitch and the later fall are related but not identical
   problems. The pitch is acquired through the wheel/suspension contact path;
   the fate of a riderless bike is then controlled mainly by body collision,
   friction, angular damping, and sleep/rest logic.
2. The broad `0x6D7B17` pitch reduction is intentionally disabled. It changes a
   legitimate contact torque and therefore also changes wheelies and landing.
3. `turnAirResistance` is directly relevant: stock GTA removes angular velocity
   once per rendered frame and nearly freezes rotation at high FPS.
4. Excluding `STATUS_ABANDONED` from `groundFriction` and preserving the stock
   move-speed snap for a grounded abandoned bike both produced no observable
   change and were reverted. The next A/B isolates `wheelFriction`, which owns
   the no-throttle rolling/braking budget inside `ProcessBikeWheel`.
5. The next unresolved area is `CBike::ProcessEntityCollision`: suspension-line
   versus body-contact ordering, contact persistence, and any frame-counted
   state that decides whether a wheel catches the ground or the bike lands on
   its side.

## Contact persistence details

`CBike::ProcessEntityCollision` saves the four old wheel ratios, asks
`CCollision::ProcessColModels` for body points and suspension-line hits, and
accepts a new wheel hit only if its ratio is below both `1.0` and the saved
ratio. This selects the closest hit when several world entities are examined in
one collision pass. It does not apply a force itself.

`CBike::ProcessControl` later turns each real line hit into a short contact
timer. A hit writes `4.0`; a miss subtracts `CTimer::GetTimeStep()` down to zero.
The timer is therefore already measured in game time, not frames. While it is
positive the corresponding wheel is still considered grounded and may enter
`ProcessBikeWheel` using the last contact point and normal. At 30 FPS that tail
occupies only a few rendered frames; at high FPS it occupies many frames over
the same real duration. The adhesion cap in `ProcessBikeWheel` is timestep
scaled, so changing this timer would be premature, but its interaction with
the low-speed full-cancellation branch remains a focused candidate for the
takeoff pitch audit.

## Rider exit and the abandoned-bike path

The moving-bike exit does not apply a hidden launch force to the motorcycle.
The relevant sequence is:

1. `CTaskSimpleCarJumpOut::ProcessPed` (`0x64DD60`) plays the rollout animation.
2. At animation time `0.35`, it runs `CTaskSimpleCarSetPedOut::ProcessPed`
   (`0x647D10`).
3. `0x647E0D` calls `CVehicle::RemoveDriver` (`0x6D1950`). The vehicle status is
   then explicitly packed as `STATUS_ABANDONED` at `0x647E18`-`0x647E21`.
4. `CVehicle::CanPedJumpOutCar` (`0x6D2030`) returns early for bikes after only
   checking horizontal speed. Its `0.9` move/turn damping path is for non-bikes,
   so the plugin's `jumpOutCarSpeed` hook cannot explain riderless bike motion.
5. On the next bike control call, the abandoned branch clears gas and rider
   balance, enables the handbrake only below speed `0.1`, and switches the local
   centre of mass Z to `fNoPlayerCOMz`. None of those assignments is a
   frame-rate-dependent impulse.

This confines the reported prolonged tossing to the ordinary post-exit
physical pipeline: integration, penetration shift, collision impulse, body
friction, suspension, and eventual static/sleep detection.

## Penetration push-out finding

`CPhysical::ProcessShiftSectorList` (`0x546670`) runs after motion integration.
It collects actual positive `CColPoint::m_fDepth` values, keeps the deepest
penetration, averages the contact normals, and directly changes the entity
position:

```text
normal/upward branch: position += normal * depth * 1.5
opposite branch:      position += normal * depth * 0.75
```

For an upward ground normal, the stock `1.5` deliberately separates the entity
past the surface in one pass. The existing `collisionPushOut` hook multiplies
both constants by `timestep / originalTimestep`. At 500 FPS this changes `1.5`
to about `0.09` and `0.75` to about `0.045`. A bike that should be separated in
one pass is therefore left roughly 91% inside the ground and is processed again
on later frames. That can turn one landing into a long sequence of alternating
body contacts, collision torques, and direction changes.

This is not analogous to a force or velocity increment. It is a geometric
constraint correction based on penetration already measured for the current
integrated position. The current global linear timestep scaling is therefore
not valid for ordinary dynamic impacts. It may have helped persistent rail or
kerb overlaps, but those must be distinguished from fresh penetration rather
than changing all six shift components. This was the leading static candidate,
but a later in-game run with `collisionPushOut=0` produced no observable change
in the reported travel distance. It is therefore a separate collision-recovery
audit, not the cause of this riderless-bike symptom.

### Confirmed world-pipeline order

The recovered `CWorld::Process` and `CPhysical` bodies establish the exact
order, rather than only the local order inside `CBike::ProcessControl`:

1. Every moving entity runs `ProcessControl`. For a bike this computes
   suspension/wheel forces and ends with the generic `CPhysical` control work.
2. `CWorld` then runs `CPhysical::ProcessCollision`. It integrates the proposed
   position, calls `CheckCollision`, and leaves the entity unsafe when the
   normal collision pass cannot accept that position. The world retries unsafe
   entities up to four more times.
3. An entity that is still unsafe enters `CPhysical::ProcessShift`. This
   integrates once more, calls `ProcessShiftSectorList` to move it out of
   penetration, and then calls `ProcessCollisionSectorList` to validate the
   shifted position and produce collision/friction impulses.
4. A second shift pass is possible. If that still fails for a player-treated
   entity, the world applies an additional stuck-object damping path.

Consequently the six patched multiplications are not a continuously applied
driving force. They belong to the fallback that must make an already invalid,
penetrating pose valid again. Weakening that correction can keep an abandoned
bike in the retry/shift path, where the same body contact can create new
collision torque repeatedly.

The retail 1.0 US executable independently confirms the recovered source:
`EDI` is `this`; the `vecShift.z >= -0.5` branch sends non-peds through the
three `1.5` multiplications at `0x546B8E`, `0x546B9A`, and `0x546BA4`; the
opposite-normal branch uses the three `0.75` multiplications at `0x546ACA`,
`0x546ADE`, and `0x546AEC`. The vehicle-only velocity nudge after the position
correction is separately multiplied by the real timestep and therefore was
already frame-rate aware.

### Why linear scaling cannot preserve this operation

At 500 FPS the plugin's normalizer is about `30 / 500 = 0.06`. It changes the
upward/non-ped correction from `1.5 * depth` to `0.09 * depth`. Ignoring new
motion, that leaves about 91 percent of the penetration after one pass. The
opposite branch changes `0.75` to `0.045`, leaving about 95.5 percent.

Even the branch below one cannot be converted as a linear rate: a repeated
fractional correction would require an exponential complement such as
`1 - pow(1 - weight, ratio)`. The `1.5` branch deliberately overshoots the
surface, so it is not a fractional decay at all and has no meaningful
timestep-scaled equivalent. This rules out the current global scaling on
mathematical grounds as well as by control-flow context.

## Audit of enabled hooks that can affect a riderless bike

| Hook | Can affect the reported phase? | Static verdict |
| --- | --- | --- |
| `bikePitchExperiment` | Only during upward takeoff with both front lines clear and a lingering wheel contact | Narrow and already validated in game; unrelated to ground-settling retries |
| `wheelFriction` | Yes, while suspension/wheel contacts are processed | Ported from FramerateVigilante; disabling it broke ordinary coast/brake behaviour, so it must remain |
| `groundFriction` | Yes, for body contacts | Global hook is caller-dependent and suspicious for abandoned contacts, but excluding abandoned bikes did not change the symptom; secondary audit item |
| `turnAirResistance` | Yes, for angular velocity | Correct exponential conversion of a stock once-per-frame `0.99`; disabling it did not help |
| `suspensionDampingLimit` | Only through suspension damping | Changes a timestep-based impulse cap, not body penetration; no exit-specific branch found |
| `moveSpeedSnap` | Only when each speed component is already near zero | Cannot explain active tossing; abandoned-bike A/B did not help |
| `restThreshold` | Only the near-rest distance comparison | Affects when sleep is considered, not the preceding impact sequence |
| `physicsSleepRate` | Only the fake-physics/rest counter | Affects settling completion; disabling it did not change the active motion |
| `jumpOutCarSpeed` | No | Bikes return before the patched damping instructions in `CanPedJumpOutCar` |
| `collisionPushOut` | Yes, precisely when normal collision resolution leaves the bike penetrating | Global conversion is questionable, but disabling it did not affect the reported riderless travel |

`collisionPushOut` is not a FramerateVigilante fix. It was introduced by this
plugin in its initial `0.9.0` commit and is still listed in the README's
not-yet-checked group. The source argument for it assumes a persistent fixed
overlap such as riding a rail, while the hook is global and therefore also
changes fresh dynamic impacts. Those two cases cannot safely share the same
linear correction.

## Implementation conclusion

The negative `collisionPushOut` run rules penetration recovery out as the
primary cause of the excessive travel. The exit chain also supplies no launch
impulse, copies vehicle speed only to the ped, and makes the ped ignore the
vehicle for collision. The remaining difference is therefore the way existing
speed is removed by wheel and body contacts after `STATUS_ABANDONED`. The
global push-out and caller-dependent body-friction questions remain separate
audits and are not changed by the wheel-cadence experiment below.

## Abandoned wheel-impulse cadence experiment (rejected)

A control run without the plugin established that the stock high-FPS bug is
the opposite extreme: as soon as throttle is released, the unscaled wheel
slowdown limit is applied on every rendered frame, stopping and overturning the
bike almost immediately. The continuous `wheelFriction` correction fixes that
runaway rate, but it distributes the original-frame impulse across all high-FPS
frames. For an active vehicle this gives the intended smooth deceleration. For
a riderless landing it changes a nonlinear contact outcome: the single larger
30-FPS impulse that plants a wheel becomes many small impulses, allowing the
bike to skim past the contact and travel much farther.

The `abandonedBikeWheelStep` experiment detours `ProcessBikeWheel` at
`0x6D73B0`. It leaves every player-controlled call unchanged. For
`STATUS_ABANDONED`, all wheel calls in one selected rendered frame receive the
stock `0.9` slowdown limit and an adhesion budget expanded back to the original
30-FPS timestep; intervening frames receive zero adhesion. A fractional
accumulator selects frames at 30 Hz. This preserves the same total real-time
budget while also preserving the discrete impulse cadence that determines
whether a wheel catches the ground.

The in-game A/B produced no observable improvement. That rules out wheel
impulse cadence by itself, so this experiment has been removed.

## Complete abandoned-bike physics cadence experiment

The next A/B treats the nonlinear pipeline as one indivisible operation. Above
30 FPS, `abandonedBikePhysicsStep` runs `CBike::ProcessControl`,
`CPhysical::ProcessCollision`, and `CPhysical::ProcessShift` for
`STATUS_ABANDONED` bikes at the original 30 Hz cadence with the original
timestep. Intervening rendered frames preserve the last safe transform. This
keeps wheel forces, suspension, body collision, collision retries, and
penetration recovery in the same temporal quantisation as the 30 FPS reference
while leaving the player-controlled bike untouched. A follow-up interpolation
layer blends only the RenderWare transform between completed physics states;
the collision matrix itself remains on the validated 30 Hz trajectory.

The pickup task deliberately leaves a fallen bike in `STATUS_ABANDONED`, so
status alone is not sufficient to decide whether the 30 Hz cadence should
continue. `CTaskSimplePickUpBike::ProcessPed` sets
`CBike::bikeFlags.bGettingPickedUp` once the animation reaches the point where
the bike starts moving. The cadence and render interpolation are disabled while
that flag is set, allowing the animation-driven pickup to update every rendered
frame without changing the physics of a genuinely riderless bike.

`CEntity::UpdateRwFrame` covers the visible clump but not every effect attached
to it. `CBike::Render` calls `CalculateLeanMatrix` and passes the resulting
`CBike::m_mLeanMatrix` to `CVehicle::DoHeadLightBeam`; the rest of the vehicle
light work is prepared in `CBike::PreRender`. Because the lean matrix is cached
from the collision matrix, the headlight otherwise remains on the last 30 Hz
physics pose while the model moves smoothly. During those two render calls the
fix temporarily substitutes the same interpolated entity transform and forces
the lean matrix cache to rebuild. Both gameplay matrices are restored before
the call returns, so only light and other render placement sees that pose.

## RW36 scope

The available RW36 tree contains the RenderWare 3.6 binary libraries, including
`rwcore.lib`, rather than recovered GTA gameplay code. It confirms the
RenderWare matrix/vector ABI used by GTA's `CMatrix` wrappers, but penetration
depth, collision impulse, bike status, and suspension policy are Rockstar game
logic. Those decisions are consequently recovered from `gta-reversed` and the
GTA SA executable; RW36 cannot supply a missing bike-physics routine.
