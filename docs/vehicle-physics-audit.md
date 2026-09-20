# Vehicle physics audit

> **Status (2026-09-13):** superseded. Player reports of excessive sliding and
> wrong in-air rotation led to the driving physics being reset to exactly what
> FramerateVigilante patches: `wheelFriction`, `burnout` and `railWheelSpin`.
> `turnAirResistance`, `groundFriction`, `wheelSlipScale`, `moveSpeedSnap`,
> `rollOntoWheels`, `suspensionDampingLimit` and `collisionPushOut` were
> removed from the plugin. The analysis below is kept as the record of why
> each was tried and what it changed; `bikeLeanTarget` and
> `bikePitchExperiment` remain.
>
> **2026-09-20:** `docs/vehicle-control-reverse.md` extends this audit to the
> yaw chain, the steering inputs and `CVehicle::FlyingControl`, read from the
> executable. It explains the "wrong in-air rotation" and the aircraft change
> as the 30 FPS behaviour reaching every vehicle class, and scopes the
> `turnAirResistance` reinstatement to cars and bikes on their wheels.

Why cars feel different at a high frame rate, what this plugin changes, and
which of those changes are corrections rather than preferences. Addresses are
GTA SA 1.0 US. Source references are `D:\dev\_refs\gta-reversed`.

## Method

Every per-frame quantity in the physics path falls into one of five kinds, and
each kind has exactly one correct conversion. Most frame-rate bugs, and most
mistaken fixes, are a quantity treated as the wrong kind.

| Kind | Stock shape | Correct conversion |
| --- | --- | --- |
| Decay factor | `x *= k` once per frame, `0 < k < 1` | `pow(k, ratio)` |
| Rate | `x += step` once per frame | `step * ratio` |
| Impulse | one velocity delta per frame | `delta * ratio` |
| Budget | a per-frame cap on how much may be removed | already `* timeStep`; leave it |
| Constraint | "cancel this entirely", capped by a budget | leave both sides alone |

`ratio` is `ms_fTimeStep / (50/30)`, so it is exactly `1.0` at 30 FPS and every
conversion is a no-op there.

The trap that produced two bad patches in this repo is the last two rows. A
budget and the velocity it is compared against are in *different units*: the
budget carries a timestep, the velocity does not. Rescaling either one changes
how much momentum is applied per second, which is what actually moves the car.

## The per-frame chain for a driven car

`CAutomobile::ProcessControl` (`0x6A6860`) each frame:

1. `ProcessControlInputs` — `m_fRawSteerAngle` slews toward the pad value,
   `m_fSteerAngle` is derived from it.
2. `acceleration` from `cTransmission::CalculateDriveAcceleration`, which ends
   in `* CTimer::GetTimeStep()`. `brake` likewise carries `GetTimeStep()`.
3. `traction`, then a steering limiter: `m_fSteerAngle *= steerAngle` where
   `steerAngle` is derived from adhesion and forward speed.
4. `ProcessCarWheelPair` per axle, then `CVehicle::ProcessWheel` per wheel
   (`0x6D6C00`; bikes use `ProcessBikeWheel` at `0x6D73B0`).
5. `CPhysical::ProcessCollision` → `ApplyFriction` for body contacts.
6. `CPhysical::ApplyAirResistance` (`0x544D10`).

## Verified findings

### `CPhysical::ApplyAirResistance` — the dominant effect, and the fix is correct

```cpp
m_vecMoveSpeed *= pow(1.0f - fSpeedMagnitude, CTimer::GetTimeStep());
m_vecTurnSpeed *= 0.99f;
```

Move speed is correctly exponentiated. Turn speed is a flat `0.99` **per
rendered frame** — a decay factor treated as if the frame length were fixed.

Retention of angular velocity over one second:

| FPS | Stock | With `turnAirResistance` |
| ---: | ---: | ---: |
| 30 | `0.99^30` ≈ 0.74 | 0.74 |
| 120 | `0.99^120` ≈ 0.30 | 0.74 |
| 500 | `0.99^500` ≈ 0.0066 | 0.74 |

`GetTurnAirResistanceFactor` returns `pow(0.99, ratio)`, which is the correct
conversion for a decay factor and is bit-exact at 30 FPS. **Verified correct.**

This is by far the largest handling change the plugin makes. Unpatched at a high
frame rate the car sheds angular velocity roughly 112 times faster than the game
was designed for, which reads as heavy, planted, understeery handling that
refuses to rotate. Restoring it makes the car rotate and hold a slide the way it
does at 30 FPS. **A car that drifts more after installing this plugin is the
expected result, not a bug.** See "What correct feels like" below.

### `CVehicle::ProcessWheel` — the units mismatch is real, the two attempts to fix it were not

At `0x6D6C7B` the function does `adhesion *= CTimer::GetTimeStep()`, making it a
per-frame budget. The lateral slip weighed against it,
`right = -contactSpeedRight / wheelsOnGround`, is a plain velocity.

Off the throttle those two are in different units, so
`speedSq > adhesion * adhesion` (`0x6D6F51`) becomes true at a fraction of the
slip it needed at 30 FPS. Under throttle both sides scale together — `thrust`
carries a timestep and the lateral term is pre-clamped to `adhesion` — so the
driving path is already consistent.

Two attempts, both reverted:

- Raising the threshold (`882a9f7`). This also stopped the clamp binding, so the
  unclamped full slip cancel ran every frame. That path applies `ApplyTurnForce`
  as well as `ApplyMoveForce`, so running it several times more often per second
  over-rotated the car badly.
- Moving only the wheel state classification (`51abc62`). Sound in principle —
  no impulse magnitude changes — but tested as still wrong in game.

The lesson from the first attempt is the general rule: **the slip is regenerated
by the vehicle's motion every frame, so it is not a one-shot quantity.** Any
change that lets more momentum be applied per second changes the car, no matter
how the threshold is reasoned about.

### `CPhysical::ApplyFriction` — same shape, fix is correct in kind

```cpp
fCollisionMass = -(effectiveMass * fMoveSpeedMagnitude);   // cancel tangential speed
if (fCollisionMass < -fFriction) fCollisionMass = -fFriction;   // capped by budget
```

`fFriction` comes from `g_surfaceInfos.GetAdhesiveLimit(colPoint)` divided by the
contact count and carries no timestep, while the sibling ped branch of the same
function computes its limit as `CTimer::GetTimeStep() / m_fMass * fFriction`.
Scaling the vehicle budget by `ratio` (`groundFriction`) is therefore the right
kind. This governs body and chassis contacts, not tyre grip.

### `CPhysical::ApplySpringDampening` — clamp only binds at low FPS

`fDampingForceTimeStep = min(timeStep, 3.0f) * fDampingForce`, then clamped to
`DAMPING_LIMIT_IN_FRAME`. The product is already timestep-scaled, and the clamp
binds when the timestep is *large*, so it is a low-frame-rate concern. Not a
suspect for high-FPS handling.

### Keyboard and pad steering response — a real dependence, currently unfixed

`CAutomobile::ProcessControlInputs` (`0x6AD690`):

```cpp
GetSteeringDeltaForFrame = (-pad/128.f - m_fRawSteerAngle) / 5.f * CTimer::GetTimeStep();
m_fRawSteerAngle += GetSteeringDeltaForFrame();
```

This is an exponential approach written as a linear step. The timestep is
present, so it is not badly broken, but linear stepping of an exponential is only
exact in the limit. Retention of the steering error per 30 FPS-equivalent frame:

| FPS | Retained |
| ---: | ---: |
| 30 | 0.667 |
| 120 | 0.706 |
| ∞ | 0.717 |

So steering reaches its target about 7% slower at a high frame rate than at 30
FPS, converging to 7.5%. The correct form is
`x += (target - x) * (1 - pow(1 - 0.2 * originalTimeStep, ratio))`. The plugin
does not currently patch this site. The mouse path immediately above it is
already exponentiated correctly (`pow(0.975f, GetTimeStep())`), which suggests
the linear form here was an oversight rather than a deliberate choice.

Small on its own, and in the direction of *less* responsive rather than
twitchier, so it does not explain a loose rear end. Worth fixing on its own
merits.

## What "correct" feels like

Every fix in this plugin targets 30 FPS behaviour. That is the only defensible
reference: it is what the handling data was tuned against. But it means the
result is not "vanilla at high FPS with the bugs removed" — it is "30 FPS
handling, at a high frame rate". Those are very different cars.

Vanilla at 144 FPS is not neutral. It is a car whose angular velocity is bled
away 112 times too fast, and whose tyres break traction early. It feels tight and
planted because two separate bugs happen to push in the same direction. Players
who learned the game at a high frame rate learned *that* car.

So there are two different questions, and they need different answers:

1. *Is the plugin faithful to 30 FPS?* — a correctness question, answered by
   the audit above and by test A below.
2. *Do I want 30 FPS handling?* — a preference. If the answer is no, the honest
   mechanism is a tunable knob, not a "fix".

The `liteDrift` setting that existed in the working tree before this session was
exactly that knob: it applied the stock `0.99` per rendered frame, damping
rotation harder as FPS rises. It was removed as a reintroduced frame-rate
dependence, which it is — but it was answering a real preference, and removing it
removed the only lever over this.

If tighter rotation is wanted, the better shape is a *strength* percentage rather
than a switch, matching `bikePitchExperimentStrength`: interpolate the exponent
between `ratio` (fully corrected, 30 FPS behaviour) and `1.0` (stock per-frame
damping), so any point in between can be dialled in and 30 FPS stays a no-op at
every setting.

## Test protocol

The audit narrows the suspects; it cannot pick between "the plugin is wrong" and
"30 FPS drives like this". These do.

### Test A — is the plugin faithful? (do this first)

1. `wheelSkidState=0` (already set), everything else default.
2. In `[framerate]`, cap the game to 30 and drive with the plugin's vehicle
   fixes **off**. This is the reference car.
3. Uncap, vehicle fixes **on**. Drive the same road.

If the two feel the same, the plugin is doing its job and the remaining question
is preference, not correctness. If uncapped-with-fixes is looser than capped
vanilla, something over-corrects and test B finds it.

### Test B — bisect, only if A shows a real difference

The `[vehicles]` section is a set of independent switches, so this takes about
five restarts. Set all of these to `0`, confirm the car feels stock-at-high-FPS,
then re-enable in halves:

```
turnAirResistance, groundFriction, wheelFriction, moveSpeedSnap,
suspensionDampingLimit, rollOntoWheels, collisionPushOut, restThreshold,
physicsSleepRate
```

Expect `turnAirResistance` to account for nearly all of the difference on its
own. If it does, that is confirmation of the audit, not a bug.

## Not yet audited

Verified above: air resistance, wheel adhesion, body friction, spring damping,
steering input. Remaining vehicle switches are documented in
`src/modules/game_addresses.inl` but have not been re-derived against
`gta-reversed` in this pass:

`moveSpeedSnap`, `restThreshold`, `physicsSleepRate`, `railWheelSpin`,
`burnout`, `heliRotorSpeed`, `skimmerResistance`, `attachedEntitySpeed`,
`upsideDownTimer`, `rollOntoWheels`, `collisionPushOut`, `wheelSettle`,
`wheelSpin`, `boatEngineSpeed`, `bikeWheelSpin`, `bikeLeanTarget`,
`bmxSprintLean`, `bmxLeanSettle`.

Most are wheel-rotation cosmetics or low-speed settling and cannot plausibly
produce a loose rear end at speed. `rollOntoWheels` and `collisionPushOut` apply
real impulses and should be checked next if test B implicates them.
