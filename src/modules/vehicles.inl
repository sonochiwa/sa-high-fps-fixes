// ---------------------------------------------------------------------------
// Vehicles
// ---------------------------------------------------------------------------

float __cdecl GetFrameIndependentWheelFriction() {
    return ReadGameFloat(kWheelFriction, 0.9f) * TimeStepRatio();
}

float __cdecl GetSkimmerResistance() {
    return ReadGameFloat(kSkimmerResistanceConstant, 30.0f) * TimeStepRatio();
}

// The 0.99 is read through the game constant so mods that repoint it keep
// working. Raising it to the timestep ratio rather than the raw timestep keeps
// the result bit-exact `0.99` at 30 FPS, where the ratio is one.
float __cdecl GetTurnAirResistanceFactor() {
    const float base = ReadGameFloat(kTurnAirResistanceConstant, 0.99f);
    return std::pow(base, TimeStepRatio());
}

float __cdecl GetBurnoutWheelSpeed() {
    return ReadGameFloat(kBurnoutConstant, 3000.0f) * TimeStepRatio();
}

// The push-out is a distance, not a decay, and the overlap it works against is
// held by the shape the vehicle is riding rather than created by the previous
// frame, so the fraction is scaled linearly: the same depth then leaves the
// entity at the same speed per second whatever the frame rate. The ratio is
// never raised above one, so at 30 FPS and below the stock constant is used
// unchanged. Both constants live in writable globals and are read through the
// original operand, so a mod that repoints or retunes them keeps working.
// A lerp weight, so what has to hold across frames is the fraction of the gap
// left over: `1 - weight` per original frame becomes `(1 - weight)` raised to
// the timestep ratio per rendered frame. The ratio is capped at one so 30 FPS
// and below keep the stock weight exactly.
float __cdecl GetWheelSettleWeight() {
    const float weight = ReadGameFloat(kWheelSettleConstant, 0.75f);
    const float ratio = std::min(TimeStepRatio(), 1.0f);
    if (!(weight > 0.0f) || weight >= 1.0f || ratio >= 1.0f) {
        return weight;
    }
    return 1.0f - std::pow(1.0f - weight, ratio);
}

float __cdecl GetPushOutScaleMain() {
    return ReadGameFloat(kPushOutConstantMain, 0.75f)
         * std::min(TimeStepRatio(), 1.0f);
}

float __cdecl GetPushOutScaleAlt() {
    return ReadGameFloat(kPushOutConstantAlt, 1.5f)
         * std::min(TimeStepRatio(), 1.0f);
}

// The rotor speed constant is reached through the original instruction operand
// so mods that repoint it keep working.
float ReadHeliRotorFinalSpeed() {
    __try {
        const auto operand = *reinterpret_cast<const uintptr_t*>(
            kHeliRotorSpeedOperand + 2);
        return ReadGameFloat(operand, 0.22f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0.22f;
    }
}

float __cdecl GetHeliRotorSlowStep() {
    return (ReadHeliRotorFinalSpeed() / kHeliRotorSpeedDivisor)
         * TimeStepRatio();
}

float __cdecl GetHeliRotorFastStep() {
    return (ReadHeliRotorFinalSpeed() / kHeliRotorSpeedDivisor) * 3.0f
         * TimeStepRatio();
}

bool NearlyEqual(float a, float b) {
    return std::fabs(a - b) < 0.002f;
}

void WriteGameFloat(uintptr_t address, float value) {
    *reinterpret_cast<float*>(address) = value;
}

bool WriteProtectedGameFloat(uintptr_t address, float value) {
    DWORD oldProtect{};
    if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(value),
                        PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    WriteGameFloat(address, value);
    DWORD ignored{};
    VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), oldProtect,
                   &ignored);
    return true;
}

// ---------------------------------------------------------------------------
// Experimental bike ramp pitch isolation
// ---------------------------------------------------------------------------

// The wheel-contact call at 0x6D7B17 is the first known source of the excess
// backward angular speed seen at takeoff. Preserve the complete angular-velocity
// delta produced by the stock ApplyTurnForce except for the excess positive
// pitch produced during takeoff: both front suspension lines are clear, the
// bike has meaningful positive world-Z velocity, and a rear or lingering wheel
// contact still reaches ProcessBikeWheel. This catches the phase where the rear
// wheel continues to pitch the bike after the front has left a ramp without
// changing a wheelie on level ground. Negative (nose-down) pitch and 30 FPS or
// below are exact no-ops.
uintptr_t g_bikePitchExperimentBike{};
float g_bikePitchExperimentBefore[3]{};
float g_bikePitchExperimentAxis[3]{};
float g_bikePitchExperimentStrength{1.0f};
float g_bikePitchExperimentFrameCorrection{};
bool g_bikePitchExperimentActive{};
uintptr_t g_bmxLaunchCorrectionBike{};

// Matched full-charge trajectories reach about 47.9 degrees of backward
// rotation at 500+ FPS versus 42.1 degrees at 30 FPS. Comparing synchronized
// samples after the first 250 ms shows excess angular speed; the earlier
// first-sample comparison was taken at different positions inside a 30 Hz
// frame and understated it. A 24% asymptotic correction produced the closest
// visual match in game. It fades to an exact no-op at the original timestep.
constexpr float kBmxLaunchPitchExcess = 0.24f;
constexpr float kBmxStockLandingPitchLimit = 0.105f;
constexpr float kBmxFalseLandingDamageLimit = 31.0f;
constexpr uint32_t kBmxLandingProtectionMaxMs = 3000;
constexpr uint32_t kBmxLandingProtectionGraceMs = 200;

uintptr_t g_bmxLandingProtectionBike{};
uint32_t g_bmxLandingProtectionUntil{};
bool g_bmxLandingWasAirborne{};
bool g_bmxLandingContactSeen{};

void __cdecl HookedBmxLaunchBunnyHop(void* association, void* data) {
    bool appliedLaunchImpulse = false;
    __try {
        const auto bike = reinterpret_cast<uintptr_t>(data);
        const auto* wheelCounts = reinterpret_cast<const float*>(
            bike + kBikeWheelContactTimers);
        appliedLaunchImpulse =
            (wheelCounts[0] > 0.0f || wheelCounts[1] > 0.0f)
            && (wheelCounts[2] > 0.0f || wheelCounts[3] > 0.0f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        appliedLaunchImpulse = false;
    }

    reinterpret_cast<void(__cdecl*)(void*, void*)>(
        g_bmxLaunchBunnyHopPatch.gateway)(association, data);

    __try {
        if (appliedLaunchImpulse) {
            g_bmxLaunchCorrectionBike = reinterpret_cast<uintptr_t>(data);
            g_bmxLandingProtectionBike = g_bmxLaunchCorrectionBike;
            const uint32_t now = *reinterpret_cast<const uint32_t*>(
                kTimerTimeInMilliseconds);
            g_bmxLandingProtectionUntil =
                now + kBmxLandingProtectionMaxMs;
            g_bmxLandingWasAirborne = false;
            g_bmxLandingContactSeen = false;
            Log("BMX trace: effective launch callback latched.");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bmxLaunchCorrectionBike = 0;
    }
}

void UpdateBmxLandingProtection(void* vehicle) {
    const auto bike = reinterpret_cast<uintptr_t>(vehicle);
    if (!bike || bike != g_bmxLandingProtectionBike) {
        return;
    }
    __try {
        const uint32_t now = *reinterpret_cast<const uint32_t*>(
            kTimerTimeInMilliseconds);
        const uint8_t status = *reinterpret_cast<const uint8_t*>(
            bike + kEntityTypeAndStatus) >> 3;
        if (status != 0
            || static_cast<int32_t>(g_bmxLandingProtectionUntil - now) < 0) {
            g_bmxLandingProtectionBike = 0;
            return;
        }

        const bool hasWheelContact = *reinterpret_cast<const uint8_t*>(
            bike + kBikeContactWheels) != 0;
        if (!hasWheelContact) {
            g_bmxLandingWasAirborne = true;
            return;
        }
        if (g_bmxLandingWasAirborne && !g_bmxLandingContactSeen) {
            g_bmxLandingContactSeen = true;
            g_bmxLandingProtectionUntil =
                now + kBmxLandingProtectionGraceMs;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bmxLandingProtectionBike = 0;
    }
}

void CorrectBmxLaunchPitch(void* vehicle) {
    const auto bike = reinterpret_cast<uintptr_t>(vehicle);
    if (!bike || bike != g_bmxLaunchCorrectionBike) {
        return;
    }
    g_bmxLaunchCorrectionBike = 0;

    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            return;
        }
        const uint8_t status = *reinterpret_cast<const uint8_t*>(
            bike + kEntityTypeAndStatus) >> 3;
        const uint8_t subClass = *reinterpret_cast<const uint8_t*>(
            bike + kVehicleSubClass);
        if (status != 0 || subClass != 10) { // player / VEHICLE_TYPE_BMX
            return;
        }

        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            bike + kEntityMatrix);
        if (!matrix) {
            return;
        }
        const auto* right = reinterpret_cast<const float*>(
            matrix + kMatrixRight);
        auto* turn = reinterpret_cast<float*>(
            bike + kPhysicalTurnSpeed);
        float localPitch = 0.0f;
        float axisLengthSquared = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            if (!std::isfinite(turn[i]) || !std::isfinite(right[i])) {
                return;
            }
            localPitch += turn[i] * right[i];
            axisLengthSquared += right[i] * right[i];
        }
        if (localPitch <= 0.0f || !std::isfinite(axisLengthSquared)
            || axisLengthSquared < 0.25f) {
            return;
        }

        const float frameCorrection = std::clamp(
            1.0f - timeStep / kOriginalTimeStep, 0.0f, 1.0f);
        const float projection = localPitch / axisLengthSquared
                               * kBmxLaunchPitchExcess * frameCorrection;
        for (size_t i = 0; i < 3; ++i) {
            turn[i] -= right[i] * projection;
        }
        char line[192];
        std::snprintf(
            line, sizeof(line),
            "BMX trace: launch correction fps=%.1f pitch=%.5f removed=%.5f.",
            50.0f / timeStep, localPitch,
            localPitch * kBmxLaunchPitchExcess * frameCorrection);
        Log(line);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool __cdecl HookedBikeDamageKnockOffRider(
    void* vehicle, float damageIntensity, uint32_t pieceType, void* damager,
    const float* collisionPosition, const float* collisionImpactVelocity) {
    using Fn = bool(__cdecl*)(
        void*, float, uint32_t, void*, const float*, const float*);

    __try {
        const auto bike = reinterpret_cast<uintptr_t>(vehicle);
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        const uint32_t now = *reinterpret_cast<const uint32_t*>(
            kTimerTimeInMilliseconds);
        if (bike && collisionImpactVelocity
            && bike == g_bmxLandingProtectionBike
            && static_cast<int32_t>(g_bmxLandingProtectionUntil - now) >= 0
            && *reinterpret_cast<const uint8_t*>(
                bike + kVehicleSubClass) == 10
            && (*reinterpret_cast<const uint8_t*>(
                bike + kEntityTypeAndStatus) >> 3) == 0
            && std::isfinite(timeStep) && timeStep > 0.0f
            && timeStep < kOriginalTimeStep) {
            const auto matrix = *reinterpret_cast<const uintptr_t*>(
                bike + kEntityMatrix);
            const float horizontalImpactSquared =
                collisionImpactVelocity[0] * collisionImpactVelocity[0]
                + collisionImpactVelocity[1] * collisionImpactVelocity[1];
            if (matrix
                && *reinterpret_cast<const float*>(matrix + kMatrixUp + 8)
                    > 0.5f
                && damageIntensity <= kBmxFalseLandingDamageLimit
                && collisionImpactVelocity[2] > 0.75f
                && horizontalImpactSquared < 0.25f) {
                char line[192];
                std::snprintf(
                    line, sizeof(line),
                    "BMX trace: suppressed vertical landing damage knock-off "
                    "damage=%.3f impact=(%.3f,%.3f,%.3f).",
                    damageIntensity, collisionImpactVelocity[0],
                    collisionImpactVelocity[1], collisionImpactVelocity[2]);
                Log(line);
                return false;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("BMX trace: failed to inspect damage knock-off.");
    }

    return reinterpret_cast<Fn>(g_bikeDamageKnockOffPatch.gateway)(
        vehicle, damageIntensity, pieceType, damager, collisionPosition,
        collisionImpactVelocity);
}

bool __fastcall HookedSpringDampening(
    void* physical, void*, float dampingForce, float springForceLimit,
    float* direction, float* collisionPoint, float* collisionSpeed) {
    using Fn = bool(__thiscall*)(
        void*, float, float, float*, float*, float*);
    float adjustedDampingForce = dampingForce;
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (physical && std::isfinite(timeStep) && timeStep > 0.0f
            && timeStep < kOriginalTimeStep
            && std::isfinite(dampingForce) && dampingForce != 0.0f) {
            const auto address = reinterpret_cast<uintptr_t>(physical);
            const float massMultiplier =
                (*reinterpret_cast<const uint8_t*>(
                    address + kPhysicalFlags) & 0x01) != 0
                    ? 2.0f : 1.0f;
            const float stockAlpha = std::clamp(
                kOriginalTimeStep * dampingForce * massMultiplier,
                -kStockDampingLimitInFrame, kStockDampingLimitInFrame);
            const float unclampedStockAlpha =
                kOriginalTimeStep * dampingForce * massMultiplier;
            float adjusted = dampingForce;
            if (std::fabs(unclampedStockAlpha)
                    <= kStockDampingLimitInFrame) {
                // Linear Euler damping leaves a different amount of motion
                // when the same interval is split into many short frames.
                // Convert the stock 30 FPS alpha to the exactly equivalent
                // short-frame alpha. Tahoma's 0.08 path is handled here.
                const float absoluteAlpha = std::fabs(stockAlpha);
                if (absoluteAlpha > 0.0f && absoluteAlpha < 1.0f) {
                    const float frameRatio = timeStep / kOriginalTimeStep;
                    const float desiredAlpha = std::copysign(
                        1.0f - std::pow(1.0f - absoluteAlpha, frameRatio),
                        stockAlpha);
                    adjusted = desiredAlpha / (timeStep * massMultiplier);
                }
            } else {
                // The stock game capped this coefficient at 30 FPS. At a short
                // timestep the same raw dampingForce falls below the fixed cap,
                // changing the suspension. Cap the coefficient itself at the
                // equivalent rate; the original function can then keep all of
                // its spring-force and direction limiting intact.
                adjusted = stockAlpha / (kOriginalTimeStep * massMultiplier);
            }
            if (std::isfinite(adjusted)) {
                adjustedDampingForce = adjusted;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    return reinterpret_cast<Fn>(g_suspensionDampingPatch.gateway)(
        physical, adjustedDampingForce, springForceLimit, direction,
        collisionPoint, collisionSpeed);
}

void __fastcall HookedBmxRiderFallEventAdd(
    void* eventGroup, void*, void* event, bool addToEventGroup) {
    const uintptr_t callSite =
        reinterpret_cast<uintptr_t>(_ReturnAddress()) - 5;
    bool suppressFalseLandingFall = false;
    __try {
        const auto eventAddress = reinterpret_cast<uintptr_t>(event);
        const auto bike = eventAddress
            ? *reinterpret_cast<const uintptr_t*>(eventAddress + 0x38)
            : 0;
        if (bike && *reinterpret_cast<const uint8_t*>(
                bike + kVehicleSubClass) == 10) {
            const auto matrix = *reinterpret_cast<const uintptr_t*>(
                bike + kEntityMatrix);
            if (matrix) {
                const auto* up = reinterpret_cast<const float*>(
                    matrix + kMatrixUp);
                const auto* right = reinterpret_cast<const float*>(
                    matrix + kMatrixRight);
                const auto* forward = reinterpret_cast<const float*>(
                    matrix + kMatrixForward);
                auto* turn = reinterpret_cast<float*>(
                    bike + kPhysicalTurnSpeed);
                const auto* move = reinterpret_cast<const float*>(
                    bike + kPhysicalMoveSpeed);
                const float timeStep = *reinterpret_cast<const float*>(
                    kTimerTimeStep);
                const float turnMagnitude = std::sqrt(
                    turn[0] * turn[0] + turn[1] * turn[1]
                    + turn[2] * turn[2]);
                char line[256];
                std::snprintf(
                    line, sizeof(line),
                    "BMX trace: rider-fall event site=%08X fps=%.1f upZ=%.4f "
                    "fwdZ=%.4f moveZ=%.4f turn=%.5f.",
                    static_cast<unsigned>(callSite),
                    std::isfinite(timeStep) && timeStep > 0.0f
                        ? 50.0f / timeStep : 0.0f,
                    up[2], forward[2], move[2], turnMagnitude);
                Log(line);

                const uint32_t now = *reinterpret_cast<const uint32_t*>(
                    kTimerTimeInMilliseconds);
                const uint8_t status = *reinterpret_cast<const uint8_t*>(
                    bike + kEntityTypeAndStatus) >> 3;
                suppressFalseLandingFall =
                    (callSite == kBikeRiderFallEventAddCalls[0]
                     || callSite == kBikeRiderFallEventAddCalls[1])
                    && bike == g_bmxLandingProtectionBike
                    && static_cast<int32_t>(
                        g_bmxLandingProtectionUntil - now) >= 0
                    && status == 0
                    && std::isfinite(timeStep)
                    && timeStep > 0.0f
                    && timeStep < kOriginalTimeStep
                    && up[2] > 0.5f;
                if (suppressFalseLandingFall) {
                    if (callSite == kBikeRiderFallEventAddCalls[0]) {
                        float localPitch = 0.0f;
                        float axisLengthSquared = 0.0f;
                        for (size_t i = 0; i < 3; ++i) {
                            localPitch += turn[i] * right[i];
                            axisLengthSquared += right[i] * right[i];
                        }
                        if (std::isfinite(localPitch)
                            && std::fabs(localPitch)
                                > kBmxStockLandingPitchLimit
                            && std::isfinite(axisLengthSquared)
                            && axisLengthSquared >= 0.25f) {
                            const float excess =
                                localPitch
                                - std::copysign(
                                    kBmxStockLandingPitchLimit, localPitch);
                            for (size_t i = 0; i < 3; ++i) {
                                turn[i] -= right[i]
                                         * excess / axisLengthSquared;
                            }
                            Log("BMX trace: clamped excessive landing rebound.");
                        }
                    }
                    Log("BMX trace: suppressed false upright landing fall.");
                }
            }

        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("BMX trace: failed to read rider-fall event state.");
    }
    if (!suppressFalseLandingFall) {
        reinterpret_cast<void(__thiscall*)(void*, void*, bool)>(kEventGroupAdd)(
            eventGroup, event, addToEventGroup);
    }
}

void __cdecl BeginBikePitchExperiment(uintptr_t bike) {
    g_bikePitchExperimentActive = false;
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (!bike || !std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            return;
        }

        // The correction exists for the rider-generated takeoff pitch. Once
        // RemoveDriver changes the packed entity status to STATUS_ABANDONED,
        // an ordinary ground bounce can otherwise satisfy the same wheel-line
        // and vertical-speed conditions and have its stock angular response
        // suppressed. Leave every riderless and non-player bike completely
        // untouched.
        const uint8_t status = *reinterpret_cast<const uint8_t*>(
            bike + kEntityTypeAndStatus) >> 3;
        if (status != 0) { // STATUS_PLAYER
            return;
        }

        const auto* wheelRatios = reinterpret_cast<const float*>(
            bike + kBikeWheelRatios);
        const auto* contactTimers = reinterpret_cast<const float*>(
            bike + kBikeWheelContactTimers);
        bool hasLingeringWheelContact = false;
        for (size_t i = 0; i < 4; ++i) {
            if (!std::isfinite(wheelRatios[i])
                || !std::isfinite(contactTimers[i])) {
                return;
            }
            hasLingeringWheelContact |= contactTimers[i] > 0.0f;
        }
        const bool hasFrontWheelContact = wheelRatios[0] < 1.0f
                                       || wheelRatios[1] < 1.0f;
        const float verticalSpeed = *reinterpret_cast<const float*>(
            bike + kPhysicalMoveSpeed + 2 * sizeof(float));
        if (!std::isfinite(verticalSpeed) || hasFrontWheelContact
            || !hasLingeringWheelContact || verticalSpeed <= 0.02f) {
            return;
        }

        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            bike + kEntityMatrix);
        if (!matrix) {
            return;
        }

        const auto* turn = reinterpret_cast<const float*>(
            bike + kPhysicalTurnSpeed);
        const auto* right = reinterpret_cast<const float*>(
            matrix + kMatrixRight);
        float axisLengthSquared = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            if (!std::isfinite(turn[i]) || !std::isfinite(right[i])) {
                return;
            }
            g_bikePitchExperimentBefore[i] = turn[i];
            g_bikePitchExperimentAxis[i] = right[i];
            axisLengthSquared += right[i] * right[i];
        }
        if (!std::isfinite(axisLengthSquared) || axisLengthSquared < 0.25f) {
            return;
        }

        g_bikePitchExperimentBike = bike;
        g_bikePitchExperimentFrameCorrection = std::clamp(
            1.0f - timeStep / kOriginalTimeStep, 0.0f, 1.0f);
        g_bikePitchExperimentActive = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bikePitchExperimentActive = false;
    }
}

void __cdecl FinishBikePitchExperiment() {
    if (!g_bikePitchExperimentActive) {
        return;
    }
    g_bikePitchExperimentActive = false;

    __try {
        auto* turn = reinterpret_cast<float*>(
            g_bikePitchExperimentBike + kPhysicalTurnSpeed);
        float pitchDelta = 0.0f;
        float axisLengthSquared = 0.0f;
        for (size_t i = 0; i < 3; ++i) {
            const float delta = turn[i] - g_bikePitchExperimentBefore[i];
            pitchDelta += delta * g_bikePitchExperimentAxis[i];
            axisLengthSquared += g_bikePitchExperimentAxis[i]
                               * g_bikePitchExperimentAxis[i];
        }
        if (!std::isfinite(pitchDelta) || pitchDelta <= 0.0f
            || !std::isfinite(axisLengthSquared)
            || axisLengthSquared < 0.25f) {
            return;
        }

        const float projection = (pitchDelta / axisLengthSquared)
                               * g_bikePitchExperimentStrength
                               * g_bikePitchExperimentFrameCorrection;
        for (size_t i = 0; i < 3; ++i) {
            turn[i] -= g_bikePitchExperimentAxis[i] * projection;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Entered through the original five-byte call. ApplyTurnForce ends in `ret 18h`,
// so an ordinary nested call would put this wrapper's return address where the
// callee expects its first float and would then corrupt the caller's stack. Make
// a private copy of all six arguments for the nested call and reproduce the
// original `ret 18h` when the wrapper itself returns.
__declspec(naked) void BikePitchExperimentThunk() {
    __asm {
        pushfd
        pushad
        push ecx
        call BeginBikePitchExperiment
        add esp, 4
        popad
        popfd
        mov eax, esp
        push dword ptr [eax + 0x18]
        push dword ptr [eax + 0x14]
        push dword ptr [eax + 0x10]
        push dword ptr [eax + 0x0C]
        push dword ptr [eax + 0x08]
        push dword ptr [eax + 0x04]
        call kApplyTurnForce
        pushfd
        pushad
        call FinishBikePitchExperiment
        popad
        popfd
        ret 0x18
    }
}
