// ---------------------------------------------------------------------------
// Optional vehicle state trace
// ---------------------------------------------------------------------------

// Samples the vehicle the player is riding and writes one line per sample to
// `HighFpsFixes.trace.log`. This exists to diagnose frame-rate-dependent
// behavior that cannot be identified from the disassembly alone; it patches
// nothing and is disabled by default.

struct VehicleSample {
    uintptr_t vehicle;
    uint32_t time;
    float timeStep;
    float timeScale;
    float posZ;
    float move[3];
    float turn[3];
    float force[3];
    float torque[3];
    float friction[3];
    float mass;
    float airResistance;
    float barSteer;
    float lean;
    float desiredLean;
    float sprintLean;
    float brakePedal;
    float gasPedal;
    float wheelAngularVelocity[2];
    float localPitchTurnSpeed;
    uint32_t wheelTurnCalls;
    float wheelTurnPitchImpulse;
    uint32_t entityFlags;
    uint32_t physicalFlags;
    uint32_t lastCollisionTime;
    uintptr_t attachedTo;
    uint32_t processCalls;
    uint32_t gravityCalls;
    uint32_t leanWrites[3];
    float rightZ;
    float mvZAfterGravity;
    float upZ;
    uint8_t fakePhysics;
    uint8_t status;
    uint8_t subClass;
    uint8_t contactWheels;
};

// Counts CBike::ProcessControl entries for the vehicle the player is riding,
// so the trace can tell "physics ran and did nothing" from "physics never ran".
volatile uint32_t g_bikeProcessCalls{};
volatile uint32_t g_gravityCalls{};
volatile float g_moveSpeedAfterGravity{};
uint32_t g_leanWrites[3]{};
using ThisCallVoidFn = void(__thiscall*)(void*);
DetourPatch g_bikeProcessPatch{};
DetourPatch g_applyGravityPatch{};
DetourPatch g_abandonedBikeCollisionPatch{};
DetourPatch g_abandonedBikeShiftPatch{};
DetourPatch g_abandonedBikeRwFramePatch{};
DetourPatch g_abandonedBikePreRenderPatch{};
DetourPatch g_abandonedBikeRenderPatch{};
std::array<SitePatch, 3> g_leanWritePatches{};

// A riderless bike is the one remaining vehicle case where mathematically
// scaling individual constants does not reproduce the 30 FPS outcome. Contact
// generation, collision retries, suspension and wheel impulses form one
// nonlinear step. This experiment therefore preserves that whole step: above
// 30 FPS an abandoned bike runs ProcessControl, ProcessCollision and
// ProcessShift at the original cadence and with the original timestep. Player
// vehicles and every other physical entity remain on the normal high-FPS path.
bool g_abandonedBikePhysicsStepEnabled{};
uint32_t g_abandonedBikePhysicsLastFrame{};
float g_abandonedBikePhysicsCredit{};
bool g_abandonedBikePhysicsTick{};

struct AbandonedBikeRenderState {
    void* bike{};
    std::array<float, 12> previous{};
    std::array<float, 12> current{};
    bool valid{};
    bool previousCaptured{};
};

std::array<AbandonedBikeRenderState, 16> g_abandonedBikeRenderStates{};

AbandonedBikeRenderState* FindAbandonedBikeRenderState(void* bike,
                                                       bool create) {
    AbandonedBikeRenderState* empty = nullptr;
    for (auto& state : g_abandonedBikeRenderStates) {
        if (state.bike == bike) {
            return &state;
        }
        if (!state.bike && !empty) {
            empty = &state;
        }
    }
    if (!create) {
        return nullptr;
    }
    auto* state = empty ? empty : &g_abandonedBikeRenderStates[0];
    *state = {};
    state->bike = bike;
    return state;
}

bool CopyMatrixTransform(uintptr_t matrix, std::array<float, 12>& out) {
    __try {
        if (!matrix) {
            return false;
        }
        constexpr std::array<size_t, 4> offsets{0x00, 0x10, 0x20, 0x30};
        for (size_t vector = 0; vector < offsets.size(); ++vector) {
            const auto* source = reinterpret_cast<const float*>(
                matrix + offsets[vector]);
            for (size_t axis = 0; axis < 3; ++axis) {
                const float value = source[axis];
                if (!std::isfinite(value)) {
                    return false;
                }
                out[vector * 3 + axis] = value;
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool WriteMatrixTransform(uintptr_t matrix,
                          const std::array<float, 12>& transform) {
    __try {
        if (!matrix) {
            return false;
        }
        constexpr std::array<size_t, 4> offsets{0x00, 0x10, 0x20, 0x30};
        for (size_t vector = 0; vector < offsets.size(); ++vector) {
            auto* destination = reinterpret_cast<float*>(
                matrix + offsets[vector]);
            for (size_t axis = 0; axis < 3; ++axis) {
                destination[axis] = transform[vector * 3 + axis];
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool CopyBikeTransform(void* bike, std::array<float, 12>& out) {
    __try {
        const auto address = reinterpret_cast<uintptr_t>(bike);
        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            address + kEntityMatrix);
        return CopyMatrixTransform(matrix, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool WriteBikeTransform(void* bike, const std::array<float, 12>& transform) {
    __try {
        const auto address = reinterpret_cast<uintptr_t>(bike);
        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            address + kEntityMatrix);
        return WriteMatrixTransform(matrix, transform);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void NormalizeRenderVector(std::array<float, 12>& transform, size_t base) {
    const float lengthSquared = transform[base] * transform[base]
                              + transform[base + 1] * transform[base + 1]
                              + transform[base + 2] * transform[base + 2];
    if (lengthSquared < 0.000001f || !std::isfinite(lengthSquared)) {
        return;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    transform[base] *= inverseLength;
    transform[base + 1] *= inverseLength;
    transform[base + 2] *= inverseLength;
}

std::array<float, 12> InterpolateBikeTransform(
    const AbandonedBikeRenderState& state, float alpha) {
    std::array<float, 12> out{};
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = state.previous[i]
               + (state.current[i] - state.previous[i]) * alpha;
    }

    // Nlerp the basis and remove accumulated shear. A 30 Hz step is small
    // enough that this follows the short rotation arc without a quaternion.
    NormalizeRenderVector(out, 0);
    const float projection = out[3] * out[0] + out[4] * out[1]
                           + out[5] * out[2];
    out[3] -= out[0] * projection;
    out[4] -= out[1] * projection;
    out[5] -= out[2] * projection;
    NormalizeRenderVector(out, 3);
    out[6] = out[1] * out[5] - out[2] * out[4];
    out[7] = out[2] * out[3] - out[0] * out[5];
    out[8] = out[0] * out[4] - out[1] * out[3];
    NormalizeRenderVector(out, 6);
    return out;
}

void BeginAbandonedBikePhysicsStep(void* bike) {
    auto* state = FindAbandonedBikeRenderState(bike, true);
    state->valid = false;
    state->previousCaptured = CopyBikeTransform(bike, state->previous);
}

void FinishAbandonedBikePhysicsStep(void* bike) {
    auto* state = FindAbandonedBikeRenderState(bike, true);
    state->valid = state->previousCaptured
                && CopyBikeTransform(bike, state->current);
}

bool IsAbandonedBike(const void* entity) {
    __try {
        if (!entity) {
            return false;
        }
        const auto address = reinterpret_cast<uintptr_t>(entity);
        constexpr uint8_t kTypeVehicle = 2;
        constexpr uint8_t kStatusAbandoned = 4;
        const uint8_t packed = *reinterpret_cast<const uint8_t*>(
            address + kEntityTypeAndStatus);
        if ((packed & 0x07) != kTypeVehicle) {
            return false;
        }
        const uint8_t status = packed >> 3;
        const uint8_t subClass = *reinterpret_cast<const uint8_t*>(
            address + kVehicleSubClass);
        const uint8_t bikeFlags = *reinterpret_cast<const uint8_t*>(
            address + kBikeFlags);
        return status == kStatusAbandoned
            && (subClass == 9 || subClass == 10)
            && !(bikeFlags & kBikeGettingPickedUp);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ShouldRunAbandonedBikePhysicsStep() {
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            g_abandonedBikePhysicsTick = true;
            return true;
        }

        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        if (frame != g_abandonedBikePhysicsLastFrame) {
            uint32_t elapsedFrames = frame - g_abandonedBikePhysicsLastFrame;
            if (g_abandonedBikePhysicsLastFrame == 0 || elapsedFrames > 10000) {
                elapsedFrames = 1;
            }
            g_abandonedBikePhysicsLastFrame = frame;
            g_abandonedBikePhysicsCredit +=
                timeStep / kOriginalTimeStep * static_cast<float>(elapsedFrames);
            if (g_abandonedBikePhysicsCredit >= 1.0f) {
                g_abandonedBikePhysicsCredit -=
                    std::floor(g_abandonedBikePhysicsCredit);
                g_abandonedBikePhysicsTick = true;
            } else {
                g_abandonedBikePhysicsTick = false;
            }
        }
        return g_abandonedBikePhysicsTick;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_abandonedBikePhysicsTick = true;
        return true;
    }
}

class ScopedOriginalTimeStep {
public:
    ScopedOriginalTimeStep() {
        __try {
            m_saved = *reinterpret_cast<float*>(kTimerTimeStep);
            m_changed = std::isfinite(m_saved) && m_saved > 0.0f
                     && m_saved < kOriginalTimeStep;
            if (m_changed) {
                *reinterpret_cast<float*>(kTimerTimeStep) = kOriginalTimeStep;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            m_changed = false;
        }
    }

    ~ScopedOriginalTimeStep() {
        if (!m_changed) {
            return;
        }
        __try {
            *reinterpret_cast<float*>(kTimerTimeStep) = m_saved;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    ScopedOriginalTimeStep(const ScopedOriginalTimeStep&) = delete;
    ScopedOriginalTimeStep& operator=(const ScopedOriginalTimeStep&) = delete;

private:
    float m_saved{kOriginalTimeStep};
    bool m_changed{};
};

void CallAbandonedBikePhysicsStep(DetourPatch& patch, void* entity) {
    const ScopedOriginalTimeStep timeStepScope;
    reinterpret_cast<ThisCallVoidFn>(patch.gateway)(entity);
}

// One sample is published per final bike balance force. It is intentionally
// only diagnostic state: the hooks below reproduce the displaced instructions
// and pass the exact original vector to CPhysical::ApplyTurnForce.
struct BikeBalanceSample {
    uint32_t time{};
    float timeStep{};
    float local34{};
    float coefficientA{};
    float coefficientB{};
    float input68{};
    float input6C{};
    float input78{};
    float force[3]{};
    float turnY{};
    uint8_t contactWheels{};
};

struct BikeBalanceWindow {
    uint32_t time{};
    uint32_t calls{};
    float elapsed{};
    float peakInput68{};
    float sumInput68{};
    float sumInput6C{};
    float force[3]{};
};

BikeBalanceSample g_bikeBalanceSample{};
volatile LONG g_bikeBalanceSequence{};
BikeBalanceWindow g_bikeBalanceWindow{};
BikeBalanceWindow g_bikeBalanceWindowSnapshot{};
volatile LONG g_bikeBalanceWindowSequence{};
SitePatch g_bikeBalanceInputPatch{};
SitePatch g_bikeBalanceForcePatch{};
SitePatch g_bikeWheelTurnTracePatch{};

// Cumulative, exception-free telemetry for the wheel-contact turn force that
// supplies the excess backward pitch on a ramp. Unlike the hardware data
// breakpoint this adds one ordinary call at the already identified site and
// therefore does not trap every angular-velocity write in the physics engine.
volatile uint32_t g_bikeWheelTurnCalls{};
volatile float g_bikeWheelTurnPitchImpulse{};

bool IsThePlayerVehicle(const void* entity);

void __cdecl RecordBikeWheelTurnForce(uintptr_t bike, const float* arguments) {
    if (!IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
        return;
    }

    __try {
        const auto matrix = *reinterpret_cast<const uintptr_t*>(bike + kEntityMatrix);
        const float turnMass = *reinterpret_cast<const float*>(bike + kPhysicalMass + 4);
        if (!matrix || !std::isfinite(turnMass) || turnMass <= 0.0f) {
            return;
        }

        const float* force = arguments;
        float point[3]{arguments[3], arguments[4], arguments[5]};
        const auto* right = reinterpret_cast<const float*>(matrix + kMatrixRight);
        const auto* up = reinterpret_cast<const float*>(matrix + kMatrixUp);
        const auto* forward = reinterpret_cast<const float*>(matrix + 0x20);
        const auto* centre = reinterpret_cast<const float*>(bike + 0xA4);
        for (size_t i = 0; i < 3; ++i) {
            point[i] -= right[i] * centre[0]
                      + forward[i] * centre[1]
                      + up[i] * centre[2];
        }

        const float delta[3]{
            (point[1] * force[2] - point[2] * force[1]) / turnMass,
            (point[2] * force[0] - point[0] * force[2]) / turnMass,
            (point[0] * force[1] - point[1] * force[0]) / turnMass
        };
        g_bikeWheelTurnPitchImpulse += delta[0] * right[0]
                                     + delta[1] * right[1]
                                     + delta[2] * right[2];
        ++g_bikeWheelTurnCalls;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

__declspec(naked) void BikeWheelTurnTraceThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x28]
        push eax
        push ecx
        call RecordBikeWheelTurnForce
        add esp, 8
        popad
        popfd
        jmp kApplyTurnForce
    }
}

bool IsThePlayerVehicle(const void* entity) {
    __try {
        const auto ped = *reinterpret_cast<uintptr_t*>(kWorldPlayers);
        if (!ped) {
            return false;
        }
        return *reinterpret_cast<uintptr_t*>(ped + kPedVehicle)
            == reinterpret_cast<uintptr_t>(entity);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void __fastcall HookedBikeProcessControl(void* bike, void*) {
    if (IsThePlayerVehicle(bike)) {
        ++g_bikeProcessCalls;
    }
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(bike)) {
        if (!ShouldRunAbandonedBikePhysicsStep()) {
            // Physics remains at the last complete 30 Hz state, but refresh
            // the RenderWare hierarchy with an interpolated transform.
            reinterpret_cast<ThisCallVoidFn>(kEntityUpdateRwFrame)(bike);
            return;
        }
        BeginAbandonedBikePhysicsStep(bike);
        CallAbandonedBikePhysicsStep(g_bikeProcessPatch, bike);
        return;
    }
    if (g_abandonedBikePhysicsStepEnabled) {
        if (auto* state = FindAbandonedBikeRenderState(bike, false)) {
            // Release the slot as soon as the entity stops being an
            // abandoned bike. Keeping the pointer around made the fixed-size
            // cache retain destroyed bikes and allowed a recycled address to
            // inherit a stale render transform.
            *state = {};
        }
    }
    reinterpret_cast<ThisCallVoidFn>(g_bikeProcessPatch.gateway)(bike);
    CorrectBmxLaunchPitch(bike);
    UpdateBmxLandingProtection(bike);
}

bool EnsureBikeProcessControlHook() {
    return g_bikeProcessPatch.installed
        || InstallDetour(g_bikeProcessPatch, kBikeProcessControl,
                         &HookedBikeProcessControl,
                         kExpectedBikeProcessControl.data(),
                         kExpectedBikeProcessControl.size());
}

void __fastcall HookedPhysicalProcessCollision(void* physical, void*) {
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(physical)) {
        if (!g_abandonedBikePhysicsTick) {
            __try {
                // Prevent CWorld from retrying the deliberately skipped step.
                *reinterpret_cast<uint8_t*>(
                    reinterpret_cast<uintptr_t>(physical) + kEntityFlags)
                    |= 0x20; // m_bIsInSafePosition
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
            return;
        }
        CallAbandonedBikePhysicsStep(g_abandonedBikeCollisionPatch, physical);
        FinishAbandonedBikePhysicsStep(physical);
        return;
    }
    reinterpret_cast<ThisCallVoidFn>(
        g_abandonedBikeCollisionPatch.gateway)(physical);
}

void __fastcall HookedPhysicalProcessShift(void* physical, void*) {
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(physical)) {
        if (!g_abandonedBikePhysicsTick) {
            __try {
                *reinterpret_cast<uint8_t*>(
                    reinterpret_cast<uintptr_t>(physical) + kEntityFlags)
                    |= 0x20; // m_bIsInSafePosition
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
            return;
        }
        CallAbandonedBikePhysicsStep(g_abandonedBikeShiftPatch, physical);
        FinishAbandonedBikePhysicsStep(physical);
        return;
    }
    reinterpret_cast<ThisCallVoidFn>(g_abandonedBikeShiftPatch.gateway)(
        physical);
}

void __fastcall HookedEntityUpdateRwFrame(void* entity, void*) {
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(entity)) {
        auto* state = FindAbandonedBikeRenderState(entity, false);
        if (state && state->valid) {
            std::array<float, 12> physicalTransform{};
            if (CopyBikeTransform(entity, physicalTransform)) {
                const auto renderTransform = InterpolateBikeTransform(
                    *state, g_abandonedBikePhysicsCredit);
                if (WriteBikeTransform(entity, renderTransform)) {
                    // UpdateRwMatrix is already detoured by the modpack. Call
                    // its public entry so that compatibility hook still runs.
                    reinterpret_cast<ThisCallVoidFn>(
                        kEntityUpdateRwMatrix)(entity);
                    WriteBikeTransform(entity, physicalTransform);
                }
            }
        }
    }
    reinterpret_cast<ThisCallVoidFn>(g_abandonedBikeRwFramePatch.gateway)(
        entity);
}

// The clump already receives the interpolated transform in UpdateRwFrame, but
// bike lights are generated later from CBike::m_mLeanMatrix. That matrix is a
// render cache built from the collision matrix and is consequently left at the
// last 30 Hz physics state. Give PreRender/Render the same interpolated entity
// transform, force their lean cache to be rebuilt, then restore both matrices
// before returning to gameplay code.
void CallAbandonedBikeRender(DetourPatch& patch, void* bike) {
    std::array<float, 12> physicalTransform{};
    std::array<float, 12> savedLeanTransform{};
    uint8_t savedLeanCalculated{};
    bool swapped = false;

    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(bike)) {
        if (auto* state = FindAbandonedBikeRenderState(bike, false);
            state && state->valid) {
            const auto address = reinterpret_cast<uintptr_t>(bike);
            const auto leanMatrix = address + kBikeLeanMatrix;
            if (CopyBikeTransform(bike, physicalTransform)
                && CopyMatrixTransform(leanMatrix, savedLeanTransform)) {
                const auto renderTransform = InterpolateBikeTransform(
                    *state, g_abandonedBikePhysicsCredit);
                __try {
                    savedLeanCalculated = *reinterpret_cast<const uint8_t*>(
                        address + kBikeLeanMatrixCalculated);
                    if (WriteBikeTransform(bike, renderTransform)) {
                        *reinterpret_cast<uint8_t*>(
                            address + kBikeLeanMatrixCalculated) = 0;
                        swapped = true;
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    swapped = false;
                }
            }
        }
    }

    reinterpret_cast<ThisCallVoidFn>(patch.gateway)(bike);

    if (swapped) {
        const auto address = reinterpret_cast<uintptr_t>(bike);
        WriteBikeTransform(bike, physicalTransform);
        WriteMatrixTransform(address + kBikeLeanMatrix, savedLeanTransform);
        __try {
            *reinterpret_cast<uint8_t*>(address + kBikeLeanMatrixCalculated) =
                savedLeanCalculated;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

void __fastcall HookedBikePreRender(void* bike, void*) {
    CallAbandonedBikeRender(g_abandonedBikePreRenderPatch, bike);
}

void __fastcall HookedBikeRender(void* bike, void*) {
    CallAbandonedBikeRender(g_abandonedBikeRenderPatch, bike);
}

// Gravity is called every frame during the freeze, so the interesting value is
// what the move speed looks like the instant gravity returns. If it is non-zero
// there and zero by sample time, something downstream is wiping it.
void __fastcall HookedApplyGravity(void* physical, void*) {
    g_gameThreadId = GetCurrentThreadId();
    const bool isPlayer = IsThePlayerVehicle(physical);
    if (isPlayer) {
        ++g_gravityCalls;
    }
    reinterpret_cast<ThisCallVoidFn>(g_applyGravityPatch.gateway)(physical);
    if (isPlayer) {
        __try {
            g_moveSpeedAfterGravity = *reinterpret_cast<const float*>(
                reinterpret_cast<uintptr_t>(physical) + kPhysicalMoveSpeed + 8);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

void __cdecl RecordBikeBalanceInputs(uintptr_t bike, const float* stack) {
    if (!IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
        return;
    }

    __try {
        g_bikeBalanceSample.time = *reinterpret_cast<const uint32_t*>(
            kTimerTimeInMilliseconds);
        g_bikeBalanceSample.timeStep = *reinterpret_cast<const float*>(
            kTimerTimeStep);
        g_bikeBalanceSample.local34 = stack[0x34 / sizeof(float)];
        g_bikeBalanceSample.coefficientA = stack[0x3C / sizeof(float)];
        g_bikeBalanceSample.coefficientB = stack[0x40 / sizeof(float)];
        g_bikeBalanceSample.input68 = stack[0x68 / sizeof(float)];
        g_bikeBalanceSample.input6C = stack[0x6C / sizeof(float)];
        g_bikeBalanceSample.input78 = stack[0x78 / sizeof(float)];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void __cdecl RecordBikeBalanceForce(uintptr_t bike, const float* force) {
    if (!IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
        return;
    }

    __try {
        std::memcpy(g_bikeBalanceSample.force, force, sizeof(g_bikeBalanceSample.force));
        g_bikeBalanceSample.turnY = *reinterpret_cast<const float*>(
            bike + kPhysicalTurnSpeed + sizeof(float));
        g_bikeBalanceSample.contactWheels = *reinterpret_cast<const uint8_t*>(
            bike + kBikeContactWheels);

        const float timeStep = g_bikeBalanceSample.timeStep;
        if (std::isfinite(timeStep) && timeStep > 0.0f
            && timeStep <= kOriginalTimeStep * 1.1f) {
            g_bikeBalanceWindow.time = g_bikeBalanceSample.time;
            ++g_bikeBalanceWindow.calls;
            g_bikeBalanceWindow.elapsed += timeStep;
            g_bikeBalanceWindow.peakInput68 = std::max(
                g_bikeBalanceWindow.peakInput68,
                std::fabs(g_bikeBalanceSample.input68));
            g_bikeBalanceWindow.sumInput68 += g_bikeBalanceSample.input68;
            g_bikeBalanceWindow.sumInput6C += g_bikeBalanceSample.input6C;
            for (size_t i = 0; i < 3; ++i) {
                g_bikeBalanceWindow.force[i] += g_bikeBalanceSample.force[i];
            }
            if (g_bikeBalanceWindow.elapsed >= kOriginalTimeStep) {
                g_bikeBalanceWindowSnapshot = g_bikeBalanceWindow;
                g_bikeBalanceWindow.elapsed -= kOriginalTimeStep;
                g_bikeBalanceWindow.calls = 0;
                g_bikeBalanceWindow.peakInput68 = 0.0f;
                g_bikeBalanceWindow.sumInput68 = 0.0f;
                g_bikeBalanceWindow.sumInput6C = 0.0f;
                std::memset(g_bikeBalanceWindow.force, 0,
                            sizeof(g_bikeBalanceWindow.force));
                InterlockedIncrement(&g_bikeBalanceWindowSequence);
            }
        }
        InterlockedIncrement(&g_bikeBalanceSequence);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// `kBikeBalanceInput` starts with `fld [esp+68h]; fmul [esp+3Ch]`. The hook is
// entered with an empty x87 stack, records the completed local coefficients,
// then runs those exact instructions before continuing the stock calculation.
__declspec(naked) void BikeBalanceInputThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x24]
        push eax
        mov eax, esi
        push eax
        call RecordBikeBalanceInputs
        add esp, 8
        popad
        popfd
        fld dword ptr [esp + 0x68]
        fmul dword ptr [esp + 0x3C]
        jmp kBikeBalanceInputReturn
    }
}

// This is entered through the original five-byte call. The force argument is
// at `[esp+4]` before saving registers and `[esp+28h]` afterwards.
__declspec(naked) void BikeBalanceForceThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x28]
        push eax
        mov eax, esi
        push eax
        call RecordBikeBalanceForce
        add esp, 8
        popad
        popfd
        jmp kApplyTurnForce
    }
}

// Each site is exactly `fstp dword ptr [esi+0x648]`, so the store is reproduced
// and only a counter is added.
__declspec(naked) void LeanWriteSmootherThunk() {
    __asm {
        fstp dword ptr [esi + 0x648]
        pushfd
        inc dword ptr [g_leanWrites]
        popfd
        jmp kLeanWriteSmootherReturn
    }
}

__declspec(naked) void LeanWriteBikeThunk() {
    __asm {
        fstp dword ptr [esi + 0x648]
        pushfd
        inc dword ptr [g_leanWrites + 4]
        popfd
        jmp kLeanWriteBikeReturn
    }
}

__declspec(naked) void LeanWriteBmxThunk() {
    __asm {
        fstp dword ptr [esi + 0x648]
        pushfd
        inc dword ptr [g_leanWrites + 8]
        popfd
        jmp kLeanWriteBmxReturn
    }
}

// ---------------------------------------------------------------------------
// Move speed write watchpoint
// ---------------------------------------------------------------------------

// Three separate hypotheses for the mid-air bike freeze were wrong, so instead
// of guessing which function wipes the move speed the CPU is asked directly. A
// hardware data breakpoint is armed on `m_vecMoveSpeed.z` of the bike the
// player is riding the moment the freeze is observed, and every instruction
// that writes it is recorded. Debug registers are per-thread, so the breakpoint
// goes on the game thread, which is identified by whichever thread calls the
// gravity hook.
constexpr size_t kWatchSlots = 48;
// The executable's code, used to tell a return address from stack garbage.
constexpr uintptr_t kTextLow = 0x00401000;
constexpr uintptr_t kTextHigh = 0x00857000;
constexpr uint32_t kWatchHitLimit = 4000;
// `traceWatchHits` and `traceWatchSamples` shrink the accumulation window. A
// landing lasts a few hundred milliseconds, and at the default window it shares
// a report with the seconds of flight and standing on either side of it.
uint32_t g_watchHitLimit = kWatchHitLimit;
uint32_t g_watchSampleLimit = 100;
// Samples the candidate has to stay the same before the breakpoint goes back on.
// Every report disarms it, so this is dead time between windows: worth lowering
// when the event under study is short.
uint32_t g_watchArmDelay = 3;

// Which CPhysical field the watchpoint arms on, as an offset from the vehicle.
// Defaults to `m_vecMoveSpeed.z`; `traceWatchOffset` in `[general]` moves it, so
// one build can name the writers of any four byte field.
size_t g_watchOffset = kPhysicalMoveSpeed + 8;
volatile uintptr_t g_watchAddress{};
volatile uint32_t g_watchTotalHits{};
uintptr_t g_watchEips[kWatchSlots]{};
uintptr_t g_watchCallers[kWatchSlots]{};
// How much each caller actually moved the field. The trap fires after the
// write, so consecutive traps bracket every change exactly, and the call count
// alone cannot say which caller dominates.
float g_watchDeltaSum[kWatchSlots]{};
// Signed as well as absolute: a force that swings both ways can add a large
// absolute total while its net effect is nearly nothing, and it is the net that
// holds a bike upright.
float g_watchSignedSum[kWatchSlots]{};
float g_watchLastValue{};
bool g_watchHaveLastValue{};
uint32_t g_watchCounts[kWatchSlots]{};
volatile long g_watchLock{};
void* g_watchHandler{};
bool g_watchArmed{};
bool g_watchReported{};
uint32_t g_watchReports{};
constexpr uint32_t kWatchReportLimit = 60;
uint32_t g_watchReportLimit = kWatchReportLimit;

// The generic force helpers are shared by the whole engine, so the instruction
// that writes the field names `CPhysical::ApplyTurnForce` and says nothing
// about which system asked for the force. The stack above the trap is scanned
// for the first value that looks like a return address into the executable,
// that is, one whose preceding five bytes are a direct `call`. That is the
// caller, which is what the search is actually after.
uintptr_t FindCallerOnStack(uintptr_t esp) {
    for (size_t i = 0; i < 96; ++i) {
        const auto candidate =
            *reinterpret_cast<const uintptr_t*>(esp + i * sizeof(uintptr_t));
        if (candidate < kTextLow || candidate >= kTextHigh) {
            continue;
        }
        if (*reinterpret_cast<const uint8_t*>(candidate - 5) == 0xE8) {
            return candidate;
        }
    }
    return 0;
}

LONG CALLBACK MoveSpeedWatchHandler(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    CONTEXT* ctx = info->ContextRecord;
    if ((ctx->Dr6 & 1u) == 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    ctx->Dr6 = 0;

    const auto eip = static_cast<uintptr_t>(ctx->Eip);
    float delta = 0.0f;
    __try {
        const float value = *reinterpret_cast<const float*>(g_watchAddress);
        if (g_watchHaveLastValue) {
            delta = value - g_watchLastValue;
        }
        g_watchLastValue = value;
        g_watchHaveLastValue = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    uintptr_t caller = 0;
    __try {
        caller = FindCallerOnStack(static_cast<uintptr_t>(ctx->Esp));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        caller = 0;
    }

    while (InterlockedCompareExchange(&g_watchLock, 1, 0) != 0) {
        YieldProcessor();
    }
    ++g_watchTotalHits;
    for (size_t i = 0; i < kWatchSlots; ++i) {
        if (g_watchEips[i] == eip && g_watchCallers[i] == caller) {
            ++g_watchCounts[i];
            g_watchDeltaSum[i] += std::fabs(delta);
            g_watchSignedSum[i] += delta;
            break;
        }
        if (g_watchEips[i] == 0) {
            g_watchEips[i] = eip;
            g_watchCallers[i] = caller;
            g_watchCounts[i] = 1;
            g_watchDeltaSum[i] = std::fabs(delta);
            g_watchSignedSum[i] = delta;
            break;
        }
    }
    InterlockedExchange(&g_watchLock, 0);
    return EXCEPTION_CONTINUE_EXECUTION;
}

// `Dr7` enables the first slot locally, as a four byte write watch:
// L0, RW0 = 01 (write) and LEN0 = 11 (four bytes).
constexpr DWORD kDr7WriteFourBytes = 0x1u | (0x1u << 16) | (0x3u << 18);

bool SetMoveSpeedWatch(uintptr_t address) {
    const DWORD threadId = g_gameThreadId;
    if (!threadId) {
        return false;
    }
    const HANDLE thread = OpenThread(
        THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
        FALSE,
        threadId);
    if (!thread) {
        return false;
    }
    bool ok = false;
    if (SuspendThread(thread) != static_cast<DWORD>(-1)) {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(thread, &ctx)) {
            ctx.Dr0 = address;
            ctx.Dr6 = 0;
            ctx.Dr7 = address ? kDr7WriteFourBytes : 0;
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            ok = SetThreadContext(thread, &ctx) != FALSE;
        }
        ResumeThread(thread);
    }
    CloseHandle(thread);
    return ok;
}

// The counters are copied out and cleared under the lock, and the formatting
// happens after it is released. Printing while holding it made the game thread
// spin inside the exception handler for the length of a buffered file write,
// once per report, which is not something the render thread can afford.
void ReportMoveSpeedWatch(FILE* file) {
    uintptr_t eips[kWatchSlots];
    uintptr_t callers[kWatchSlots];
    uint32_t counts[kWatchSlots];
    float absSum[kWatchSlots];
    float netSum[kWatchSlots];
    uint32_t hits = 0;

    while (InterlockedCompareExchange(&g_watchLock, 1, 0) != 0) {
        YieldProcessor();
    }
    hits = g_watchTotalHits;
    for (size_t i = 0; i < kWatchSlots; ++i) {
        eips[i] = g_watchEips[i];
        callers[i] = g_watchCallers[i];
        counts[i] = g_watchCounts[i];
        absSum[i] = g_watchDeltaSum[i];
        netSum[i] = g_watchSignedSum[i];
        g_watchEips[i] = 0;
        g_watchCallers[i] = 0;
        g_watchCounts[i] = 0;
        g_watchDeltaSum[i] = 0.0f;
        g_watchSignedSum[i] = 0.0f;
    }
    g_watchTotalHits = 0;
    g_watchHaveLastValue = false;
    InterlockedExchange(&g_watchLock, 0);

    std::fprintf(file, "# writers of +%u, %u hits total, ts=%.5f\n",
                 static_cast<uint32_t>(g_watchOffset), hits,
                 *reinterpret_cast<const float*>(kTimerTimeStep));
    for (size_t i = 0; i < kWatchSlots && eips[i]; ++i) {
        std::fprintf(file,
                     "# writer %08X from %08X x%u abs=%.6f net=%.6f\n",
                     static_cast<uint32_t>(eips[i]),
                     static_cast<uint32_t>(callers[i]),
                     counts[i], absSum[i], netSum[i]);
    }
    std::fflush(file);
}

float VectorLength(const float v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

bool SampleThePlayerVehicle(VehicleSample& out) {
    __try {
        const auto ped = *reinterpret_cast<uintptr_t*>(kWorldPlayers);
        if (!ped) {
            return false;
        }
        const auto vehicle = *reinterpret_cast<uintptr_t*>(ped + kPedVehicle);
        if (!vehicle) {
            return false;
        }

        out.vehicle = vehicle;
        out.time = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
        out.timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        out.timeScale = *reinterpret_cast<const float*>(kTimerTimeScale);

        const auto matrix = *reinterpret_cast<uintptr_t*>(vehicle + kEntityMatrix);
        out.posZ = matrix
            ? *reinterpret_cast<const float*>(matrix + kMatrixPosition + 8)
            : 0.0f;

        std::memcpy(out.move,
                    reinterpret_cast<const void*>(vehicle + kPhysicalMoveSpeed),
                    sizeof(out.move));
        std::memcpy(out.turn,
                    reinterpret_cast<const void*>(vehicle + kPhysicalTurnSpeed),
                    sizeof(out.turn));
        std::memcpy(out.force,
                    reinterpret_cast<const void*>(vehicle + kPhysicalForce),
                    sizeof(out.force));
        std::memcpy(out.torque,
                    reinterpret_cast<const void*>(vehicle + kPhysicalTorque),
                    sizeof(out.torque));

        std::memcpy(out.friction,
                    reinterpret_cast<const void*>(vehicle
                                                  + kPhysicalFrictionMoveSpeed),
                    sizeof(out.friction));

        out.physicalFlags =
            *reinterpret_cast<const uint32_t*>(vehicle + kPhysicalFlags);
        out.lastCollisionTime = *reinterpret_cast<const uint32_t*>(
            vehicle + kPhysicalLastCollisionTime);
        out.attachedTo =
            *reinterpret_cast<const uintptr_t*>(vehicle + kPhysicalAttachedTo);
        out.processCalls = g_bikeProcessCalls;
        out.gravityCalls = g_gravityCalls;
        out.leanWrites[0] = g_leanWrites[0];
        out.leanWrites[1] = g_leanWrites[1];
        out.leanWrites[2] = g_leanWrites[2];
        out.mvZAfterGravity = g_moveSpeedAfterGravity;
        out.rightZ = matrix
            ? *reinterpret_cast<const float*>(matrix + kMatrixRight + 8)
            : 0.0f;
        out.upZ = matrix
            ? *reinterpret_cast<const float*>(matrix + kMatrixUp + 8)
            : 0.0f;
        out.barSteer = *reinterpret_cast<const float*>(vehicle + kBikeBarSteerAngle);
        out.lean = *reinterpret_cast<const float*>(vehicle + kBikeLeanAngle);
        out.desiredLean =
            *reinterpret_cast<const float*>(vehicle + kBikeDesiredLeanAngle);
        out.brakePedal =
            *reinterpret_cast<const float*>(vehicle + kVehicleBrakePedal);
        out.gasPedal =
            *reinterpret_cast<const float*>(vehicle + kVehicleGasPedal);

        out.mass = *reinterpret_cast<const float*>(vehicle + kPhysicalMass);
        out.airResistance =
            *reinterpret_cast<const float*>(vehicle + kPhysicalAirResistance);
        out.entityFlags = *reinterpret_cast<const uint32_t*>(vehicle + kEntityFlags);
        out.fakePhysics =
            *reinterpret_cast<const uint8_t*>(vehicle + kFakePhysicsOffset);
        out.status = static_cast<uint8_t>(
            (*reinterpret_cast<const uint8_t*>(vehicle + kEntityTypeAndStatus) >> 3)
            & 0x1F);
        out.subClass = *reinterpret_cast<const uint8_t*>(vehicle + kVehicleSubClass);
        // Only meaningful for the bike classes.
        const bool isBike = out.subClass == 9 || out.subClass == 10;
        out.contactWheels = isBike
            ? *reinterpret_cast<const uint8_t*>(vehicle + kBikeContactWheels)
            : 0xFF;
        if (isBike) {
            std::memcpy(out.wheelAngularVelocity,
                        reinterpret_cast<const void*>(
                            vehicle + kBikeWheelAngularVelocity),
                        sizeof(out.wheelAngularVelocity));
            if (matrix) {
                const auto* right =
                    reinterpret_cast<const float*>(matrix + kMatrixRight);
                out.localPitchTurnSpeed = out.turn[0] * right[0]
                                        + out.turn[1] * right[1]
                                        + out.turn[2] * right[2];
            }
        }
        // Only the BMX classes carry the sprint lean angle.
        out.sprintLean = out.subClass == 10
            ? *reinterpret_cast<const float*>(vehicle + kBmxSprintLeanAngle)
            : 0.0f;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The pushing ped is on foot, so this cannot ride on the vehicle sample. It
// reports how much velocity the vehicle was handed per unit of real time, which
// is the quantity that has to match between a capped and an uncapped run.
// `traceWatchMode` picks what the watchpoint is looking for: mode 0 waits for a
// ridden bike to stand nearly still, mode 1 waits for the ped to start pushing
// a vehicle. Either way the candidate has to be the same object for three
// samples running before the breakpoint is armed on it.
void MaybeArmWatch(FILE* file) {
    static uintptr_t lastCandidate = 0;
    static uint32_t stableSamples = 0;
    static uint32_t armedSamples = 0;

    if (g_watchReported) {
        return;
    }
    if (!g_watchArmed) {
        const uintptr_t candidate = g_watchCandidate;
        if (candidate != 0 && candidate == lastCandidate) {
            ++stableSamples;
        } else {
            stableSamples = candidate != 0 ? 1 : 0;
            lastCandidate = candidate;
        }
        if (stableSamples >= g_watchArmDelay) {
            g_watchAddress = candidate + g_watchOffset;
            g_watchArmed = SetMoveSpeedWatch(g_watchAddress);
            if (g_watchArmed) {
                std::fprintf(file, "# watching %08X mode=%d offset=%u\n",
                             static_cast<uint32_t>(g_watchAddress), g_watchMode,
                             static_cast<uint32_t>(g_watchOffset));
                std::fflush(file);
                armedSamples = 0;
            } else {
                stableSamples = 0;
            }
        }
        return;
    }
    // Reported repeatedly rather than once, so a single session can cover more
    // than one frame rate: the timestep goes into every report line, and the
    // counters reset after each one.
    // The breakpoint stays on between reports. Taking it down and putting it
    // back up each window meant suspending the render thread once per report,
    // and it also left the field unwatched during the re-arm gap, which is
    // exactly where a short event such as a landing falls.
    const uintptr_t candidate = g_watchCandidate;
    if (candidate != 0 && candidate + g_watchOffset != g_watchAddress) {
        g_watchAddress = candidate + g_watchOffset;
        if (SetMoveSpeedWatch(g_watchAddress)) {
            std::fprintf(file, "# rewatching %08X offset=%u\n",
                         static_cast<uint32_t>(g_watchAddress),
                         static_cast<uint32_t>(g_watchOffset));
            std::fflush(file);
        }
    }
    if (++armedSamples >= g_watchSampleLimit ||
        g_watchTotalHits >= g_watchHitLimit) {
        armedSamples = 0;
        ReportMoveSpeedWatch(file);
        if (++g_watchReports >= g_watchReportLimit) {
            SetMoveSpeedWatch(0);
            g_watchArmed = false;
            g_watchReported = true;
        }
    }
}

void DrainPushSamples(FILE* file) {
    const uint32_t head = g_pushWriteIndex;
    if (head - g_pushReadIndex > kPushSampleSlots) {
        // Overrun; skip to what is still intact rather than report garbage.
        g_pushReadIndex = head - kPushSampleSlots;
    }
    bool wrote = false;
    while (g_pushReadIndex != head && g_pushSamplesLogged < kPushSampleLimit) {
        const PushSample& s = g_pushSamples[g_pushReadIndex % kPushSampleSlots];
        std::fprintf(file, "# imp f=%u dv=%.7f v=%.6f ts=%.5f\n",
                     s.frame, s.deltaV, s.carSpeed, s.timeStep);
        ++g_pushReadIndex;
        ++g_pushSamplesLogged;
        wrote = true;
    }
    if (wrote) {
        std::fflush(file);
    }
}

void ReportPedPushTelemetry(FILE* file, uint32_t& lastReport) {
    const uint32_t now = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
    if (now - lastReport < 500) {
        return;
    }
    const uint32_t applications = g_pushApplications;
    if (applications != 0) {
        const float moved[3]{
            g_pushLastPos[0] - g_pushFirstPos[0],
            g_pushLastPos[1] - g_pushFirstPos[1],
            g_pushLastPos[2] - g_pushFirstPos[2]
        };
        std::fprintf(file,
                     "# push dt=%u n=%u dv=%.5f vmax=%.5f ped=%.5f "
                     "dist=%.5f ts=%.5f\n",
                     now - lastReport, applications, g_pushDeltaVSum,
                     g_pushCarSpeedPeak, g_pushPedSpeed, VectorLength(moved),
                     *reinterpret_cast<const float*>(kTimerTimeStep));
        std::fflush(file);
        g_pushApplications = 0;
        g_pushDeltaVSum = 0.0f;
        g_pushCarSpeedPeak = 0.0f;
        g_pushHavePos = false;
    }
    lastReport = now;
}

// The on-foot counterpart of the vehicle trace. Written for the climbing death,
// where the question is what the player's move speed is on the frame the health
// drops, so it records speed and health together and nothing else.
struct PedSample {
    uint32_t time;
    float timeStep;
    float position[3];
    float move[3];
    float health;
    float armour;
};

bool SampleThePlayerPed(PedSample& out) {
    __try {
        const auto ped = *reinterpret_cast<const uintptr_t*>(kWorldPlayers);
        if (!ped) {
            return false;
        }
        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            ped + kEntityMatrix);
        if (!matrix) {
            return false;
        }
        out.time = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
        out.timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        for (int i = 0; i < 3; ++i) {
            out.position[i] = reinterpret_cast<const float*>(matrix + 0x30)[i];
            out.move[i] = reinterpret_cast<const float*>(
                ped + kPhysicalMoveSpeed)[i];
        }
        out.health = *reinterpret_cast<const float*>(ped + kPedHealth);
        out.armour = *reinterpret_cast<const float*>(ped + kPedArmour);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

DWORD WINAPI PedTraceThread(void*) {
    FILE* file = _fsopen(g_diagnosticPath.c_str(), "a", _SH_DENYNO);
    if (!file) {
        return 0;
    }
    std::fprintf(file, "# ped session fpsLimit=%d\n", g_fpsLimit);
    std::fprintf(file, "time ts posX posY posZ mvX mvY mvZ mvLen health armour\n");

    uint32_t written = 0;
    uint32_t lastTime = 0;
    while (g_diagnosticActive && written < kDiagnosticLineLimit) {
        PedSample s{};
        if (SampleThePlayerPed(s) && s.time != lastTime) {
            lastTime = s.time;
            const float length = std::sqrt(s.move[0] * s.move[0]
                                           + s.move[1] * s.move[1]
                                           + s.move[2] * s.move[2]);
            std::fprintf(file,
                         "%u %.5f %.3f %.3f %.3f %.5f %.5f %.5f %.5f %.2f %.2f\n",
                         s.time, s.timeStep,
                         s.position[0], s.position[1], s.position[2],
                         s.move[0], s.move[1], s.move[2], length,
                         s.health, s.armour);
            if ((++written % 32) == 0) {
                std::fflush(file);
            }
        }
        Sleep(kDiagnosticIntervalMs);
    }
    std::fflush(file);
    std::fclose(file);
    return 0;
}

DWORD WINAPI VehicleTraceThread(void*) {
    FILE* file{};
    // Appended, not truncated, so a capped run and an uncapped run of the same
    // test can be compared against each other in one file. Opened with sharing
    // so the log can be read while the game is still running; the default
    // `fopen` on this toolchain locks it exclusively.
    file = _fsopen(g_diagnosticPath.c_str(), "a", _SH_DENYNO);
    if (!file) {
        return 0;
    }
    std::fprintf(file, "# session fpsLimit=%d\n", g_fpsLimit);
    std::fprintf(file,
                 "time ts scale posZ mvLen tnLen frcLen trqLen fricLen "
                 "barSteer lean desiredLean sprintLean "
                 "eFlags pFlags lastCol attached calls fake status sub wheels "
                  "grav lw0 lw1 lw2 mvZpostGrav rightZ upZ leanRaw leanHeld "
                  "brake gas wheel0 wheel1 localPitch "
                  "mvX mvY mvZ tnX tnY tnZ\n");

    uint32_t written = 0;
    uint32_t lastTime = 0;
    uint32_t lastCalls = 0;
    uint32_t lastGravity = 0;
    uint32_t lastLean[3]{};
    uint32_t lastPushReport = 0;
    LONG lastBikeBalanceSequence = 0;
    LONG lastBikeBalanceWindowSequence = 0;
    while (g_diagnosticActive && written < kDiagnosticLineLimit) {
        VehicleSample s{};
        DrainPushSamples(file);
        ReportPedPushTelemetry(file, lastPushReport);
        const LONG balanceBefore = g_bikeBalanceSequence;
        if (balanceBefore != 0 && balanceBefore != lastBikeBalanceSequence) {
            MemoryBarrier();
            BikeBalanceSample balance{};
            std::memcpy(&balance, &g_bikeBalanceSample, sizeof(balance));
            MemoryBarrier();
            if (balanceBefore == g_bikeBalanceSequence) {
                std::fprintf(file,
                             "# bike-balance t=%u ts=%.5f l34=%.6f "
                             "a=%.6f b=%.6f in68=%.6f in6c=%.6f in78=%.6f "
                             "force=%.6f,%.6f,%.6f turnY=%.6f wheels=%u\n",
                             balance.time, balance.timeStep, balance.local34,
                             balance.coefficientA, balance.coefficientB,
                             balance.input68, balance.input6C, balance.input78,
                             balance.force[0], balance.force[1], balance.force[2],
                             balance.turnY, balance.contactWheels);
                lastBikeBalanceSequence = balanceBefore;
            }
        }
        const LONG windowBefore = g_bikeBalanceWindowSequence;
        if (windowBefore != 0 && windowBefore != lastBikeBalanceWindowSequence) {
            MemoryBarrier();
            BikeBalanceWindow window{};
            std::memcpy(&window, &g_bikeBalanceWindowSnapshot, sizeof(window));
            MemoryBarrier();
            if (windowBefore == g_bikeBalanceWindowSequence) {
                std::fprintf(file,
                             "# bike-balance-window t=%u calls=%u elapsed=%.5f "
                             "peak68=%.6f sum68=%.6f sum6c=%.6f "
                             "sum-force=%.6f,%.6f,%.6f\n",
                             window.time, window.calls, window.elapsed,
                             window.peakInput68, window.sumInput68,
                             window.sumInput6C, window.force[0],
                             window.force[1], window.force[2]);
                lastBikeBalanceWindowSequence = windowBefore;
            }
        }
        if (SampleThePlayerVehicle(s) && s.time != lastTime) {
            lastTime = s.time;
            const uint32_t callDelta = s.processCalls - lastCalls;
            lastCalls = s.processCalls;
            const uint32_t gravityDelta = s.gravityCalls - lastGravity;
            lastGravity = s.gravityCalls;
            uint32_t leanDelta[3];
            for (int i = 0; i < 3; ++i) {
                leanDelta[i] = s.leanWrites[i] - lastLean[i];
                lastLean[i] = s.leanWrites[i];
            }
            std::fprintf(file,
                         "%u %.5f %.3f %.3f "
                         "%.5f %.5f %.5f %.5f %.5f "
                         "%.4f %.4f %.4f %.4f "
                         "%08X %08X %u %08X %u %u %u %u %u "
                          "%u %u %u %u %.6f %.5f %.5f "
                          "%.5f %.5f "
                          "%.4f %.4f %.6f %.6f %.6f "
                          "%.5f %.5f %.5f %.5f %.5f %.5f\n",
                         s.time, s.timeStep, s.timeScale, s.posZ,
                         VectorLength(s.move), VectorLength(s.turn),
                         VectorLength(s.force), VectorLength(s.torque),
                         VectorLength(s.friction),
                         s.barSteer, s.lean, s.desiredLean, s.sprintLean,
                         s.entityFlags, s.physicalFlags, s.lastCollisionTime,
                         static_cast<uint32_t>(s.attachedTo), callDelta,
                         s.fakePhysics, s.status, s.subClass, s.contactWheels,
                         gravityDelta, leanDelta[0], leanDelta[1], leanDelta[2],
                          s.mvZAfterGravity, s.rightZ, s.upZ,
                          g_leanTargetRaw, g_leanTargetHeld,
                          s.brakePedal, s.gasPedal,
                          s.wheelAngularVelocity[0],
                          s.wheelAngularVelocity[1],
                          s.localPitchTurnSpeed,
                          s.move[0], s.move[1], s.move[2],
                         s.turn[0], s.turn[1], s.turn[2]);
            if ((++written % 64) == 0) {
                std::fflush(file);
            }

            // Once a ridden bike has been near still for a moment, watch the
            // chosen field and record every instruction that writes it.
            const bool isBike = s.subClass == 9 || s.subClass == 10;
            if (g_watchMode == 0) {
                g_watchCandidate = isBike && VectorLength(s.move) < 0.006f
                    ? s.vehicle
                    : 0;
            } else if (g_watchMode == 2) {
                // Any ridden bike, moving or not. Mode 0 waits for it to stand
                // still, which disarms the breakpoint for the whole of a jump
                // and its landing.
                g_watchCandidate = isBike ? s.vehicle : 0;
            }
        }

        MaybeArmWatch(file);
        Sleep(kDiagnosticIntervalMs);
    }

    std::fflush(file);
    std::fclose(file);
    return 0;
}

