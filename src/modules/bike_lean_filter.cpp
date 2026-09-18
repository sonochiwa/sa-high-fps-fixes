#include "modules/modules.h"

namespace hff {

LeanTargetState g_leanStates[kLeanSlots]{};

void ResetLeanStateFor(uintptr_t bike) {
    for (auto& state : g_leanStates) {
        if (state.bike == bike) {
            state = LeanTargetState{};
            return;
        }
    }
}

LeanTargetState& LeanStateFor(uintptr_t bike, uint32_t frame) {
    size_t oldest = 0;
    for (size_t i = 0; i < kLeanSlots; ++i) {
        if (g_leanStates[i].bike == bike) {
            g_leanStates[i].lastFrame = frame;
            return g_leanStates[i];
        }
        if (g_leanStates[i].bike == 0) {
            oldest = i;
            break;
        }
        if (frame - g_leanStates[i].lastFrame
            > frame - g_leanStates[oldest].lastFrame) {
            oldest = i;
        }
    }
    LeanTargetState& fresh = g_leanStates[oldest];
    fresh = LeanTargetState{};
    fresh.bike = bike;
    fresh.lastFrame = frame;
    return fresh;
}

void __cdecl FilterBikeLeanTarget(float* target, uintptr_t bike) {
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        const float gravity = ReadGameFloat(kGravityConstant, 0.008f);
        if (!std::isfinite(*target) || timeStep <= 0.0f || gravity <= 0.0f) {
            return;
        }

        // A nominal 30 FPS cap does not produce one bit-exact timestep. The old
        // hard boundary alternated between the stock target and a target held
        // over two frames when limiter jitter crossed it, making the rider snap
        // sideways. At and below the original rate the filter has zero weight;
        // above it the weight grows continuously, so there is no new FPS
        // boundary at which the behavior switches.
        if (timeStep >= kOriginalTimeStep) {
            // Do not retain a high-FPS sample across a limiter transition. The
            // next filtered frame starts from the current position and target.
            ResetLeanStateFor(bike);
            return;
        }

        const auto matrix =
            *reinterpret_cast<const uintptr_t*>(bike + kEntityMatrix);
        if (!matrix) {
            return;
        }
        const auto* position =
            reinterpret_cast<const float*>(matrix + kMatrixPosition);
        const auto* right =
            reinterpret_cast<const float*>(matrix + kMatrixRight);

        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        LeanTargetState& state = LeanStateFor(bike, frame);
        if (!state.primed) {
            state.primed = true;
            std::memcpy(state.position, position, sizeof(state.position));
            state.held = *target;
        }

        // The function runs more than once per rendered frame for a given bike,
        // so the interval advances on the frame counter, not on calls.
        if (frame != state.frame) {
            state.frame = frame;
            state.elapsed += timeStep;
        }

        if (state.elapsed >= kOriginalTimeStep) {
            // Speed is taken from how far the bike actually travelled, not from
            // `m_vecMoveSpeed`. The resting contact cancels that field partway
            // through a frame, so sampling it catches transients that never
            // moved the bike; the matrix position carries no such spikes and is
            // exactly zero for a bike standing still.
            // The whole velocity vector is carried, not its component along
            // the bike's right axis. Cornering, the bike's own axes turn with
            // it, so a steady turn holds that component almost constant and
            // differencing it gives zero: the first version of this filter
            // did exactly that and stood the bike upright through every
            // corner. What the engine measures is the change in the world
            // velocity projected onto the right axis, which in a steady turn
            // is the centripetal term and is not zero at all.
            float velocity[3];
            for (int i = 0; i < 3; ++i) {
                velocity[i] = (position[i] - state.position[i]) / state.elapsed;
            }
            if (state.sampled) {
                const float lateral =
                      (velocity[0] - state.velocity[0]) * right[0]
                    + (velocity[1] - state.velocity[1]) * right[1]
                    + (velocity[2] - state.velocity[2]) * right[2];
                state.held = lateral / (state.elapsed * gravity);
            }
            state.sampled = true;
            std::memcpy(state.velocity, velocity, sizeof(state.velocity));
            std::memcpy(state.position, position, sizeof(state.position));
            state.elapsed = 0.0f;
        }

        const float ratio = std::clamp(timeStep / kOriginalTimeStep,
                                       0.0f, 1.0f);
        const float filterWeight = 1.0f - ratio * ratio;
        const float filteredTarget =
            *target + (state.held - *target) * filterWeight;

        if (IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
            g_leanTargetRaw = *target;
            g_leanTargetHeld = filteredTarget;
        }
        *target = filteredTarget;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

} // namespace hff
