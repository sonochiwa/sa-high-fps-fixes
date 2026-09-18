#include "modules/modules.h"

namespace hff {

// Counts CBike::ProcessControl entries for the vehicle the player is riding,
// so the trace can tell "physics ran and did nothing" from "physics never ran".
volatile uint32_t g_bikeProcessCalls{};
volatile uint32_t g_gravityCalls{};
volatile float g_moveSpeedAfterGravity{};
uint32_t g_leanWrites[3]{};
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

void CallAbandonedBikePhysicsStep(DetourPatch& patch, void* entity) {
    const ScopedOriginalTimeStep timeStepScope;
    reinterpret_cast<ThisCallVoidFn>(patch.gateway)(entity);
}

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

} // namespace hff
