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


// A ped/vehicle collision builds vecThisMoveForce for the ped and
// vecEntityMoveForce for the vehicle. They are one collision response and must
// remain paired. Repeating either vector every rendered frame over-pushes a
// car; continuously scaling the pair makes the vertical component lift a ped
// while pushing a fallen bike. Deliver the complete stock pair on the original
// 30 Hz cadence instead. At 30 FPS every frame is selected and the code is an
// exact no-op.

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

// Keep all contacts in one rendered frame on the same 30 Hz phase so a
// multi-point collision remains coherent.
uint32_t g_pedPushLastFrame{};
float g_pedPushCarry{};
bool g_pedPushOriginalRateFrame{true};

bool ShouldApplyOriginalRatePedPush() {
    const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
    if (frame != g_pedPushLastFrame) {
        g_pedPushLastFrame = frame;
        const float ratio = std::clamp(TimeStepRatio(), 0.0f, 1.0f);
        g_pedPushCarry += ratio;
        if (g_pedPushCarry >= 1.0f) {
            g_pedPushCarry -= std::floor(g_pedPushCarry);
            g_pedPushOriginalRateFrame = true;
        } else {
            g_pedPushOriginalRateFrame = false;
        }
    }
    return g_pedPushOriginalRateFrame;
}

void __cdecl ScalePedCollisionForces(uintptr_t stackFrame, uintptr_t vehicle) {
    __try {
        auto* pedForce = reinterpret_cast<float*>(stackFrame + 0x5C);
        auto* vehicleForce = reinterpret_cast<float*>(stackFrame + 0x20);
        const float scale = ShouldApplyOriginalRatePedPush() ? 1.0f : 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            pedForce[i] *= scale;
            vehicleForce[i] *= scale;
        }
        if (scale == 0.0f) {
            return;
        }

        if (!g_diagnosticActive || !vehicle) {
            return;
        }
        const float mass =
            *reinterpret_cast<const float*>(vehicle + kPhysicalMass);
        const float applied = mass > 0.0f
            ? VectorLength(vehicleForce) / mass : 0.0f;
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

