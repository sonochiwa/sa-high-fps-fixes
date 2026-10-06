#include "bikes/lean_target.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/bikes.h"
#include "game/vehicles.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

// The value handed in is the engine's estimate of the lateral acceleration in
// g, measured across one call as `deltaSpeed / (timeStep * gravity)`. Standing
// still it reached 0.7451 at 500 FPS against 0.0134 at 30 FPS, while the bike's
// actual roll was the same to within a factor of 1.5, so the estimate is not
// reporting real motion.
//
// It cannot be repaired by accumulating the per-call deltas over a real-time
// window. Between calls the resting contact cancels the tangential speed, so a
// delta is not an exact difference and the sum keeps the contact chatter that
// the endpoints would have cancelled.
//
// What works is a plain finite difference of the state: the velocity is
// measured from how far the bike actually travelled over one original frame of
// real time, and the difference between two such measurements is the
// acceleration across that interval. Standing still, both samples are zero and
// the estimate is zero. Cornering, the difference is the real change in
// velocity over 1/30 s, which is what the engine measures at 30 FPS.
//
// The velocity kept between windows is the whole vector. Keeping only its
// component along the bike's right axis looks equivalent and is not: those axes
// turn with the bike, so a steady corner holds that component roughly constant
// and its difference is zero, which would hold the bike upright through every
// corner. The engine's numerator is a change of world velocity, and the
// projection happens after the difference, not before.
//
// The direction dotted against is the matrix right vector, which is what the
// original numerator uses at `0x6BBAC2` through `edi`, loaded from `[esi+0x14]`.
// At or below the original frame rate the engine's own value is passed through,
// so stock behavior is reproduced by construction rather than by arithmetic.

namespace hff::bikes {

namespace {

// `CBike::ProcessControl` runs for every bike in the world, not just the one
// the player is on, so the state is kept per bike: the local player's bike in
// a slot of its own, every other ridden bike in a table that evicts whichever
// entry has gone longest without being touched.
constexpr size_t kLeanSlots = 32;

struct LeanTargetState {
    uintptr_t bike;
    float position[3];
    float velocity[3];
    float elapsed;
    uint32_t frame;
    uint32_t lastFrame;
    bool primed;
    bool sampled;
    float held;
};

SitePatch g_bikeLeanTargetPatch{};
LeanTargetState g_playerLeanState{};
LeanTargetState g_leanStates[kLeanSlots]{};

// The slot of `bike`, emptied first when the bike was not filtered on the
// previous frame: its old window would difference two positions far apart in
// time, or belong to an earlier bike at the same address.
LeanTargetState& LeanStateFor(uintptr_t bike, uint32_t frame) {
    LeanTargetState* state = nullptr;
    if (IsDrivenByLocalPlayer(bike)) {
        state = &g_playerLeanState;
    } else {
        for (auto& candidate : g_leanStates) {
            if (candidate.bike == bike) {
                state = &candidate;
                break;
            }
        }
    }
    if (!state) {
        state = &g_leanStates[0];
        for (auto& candidate : g_leanStates) {
            if (frame - candidate.lastFrame > frame - state->lastFrame) {
                state = &candidate;
            }
        }
    }
    if (state->bike != bike || frame - state->lastFrame > 1) {
        *state = LeanTargetState{};
        state->bike = bike;
    }
    state->lastFrame = frame;
    return *state;
}

// The stock target needs no help in the air or while the bike hangs off
// something: there it comes from the steering or is zero, not from a
// difference of speeds.
bool LeanTargetIsDerivative(uintptr_t bike) {
    return *reinterpret_cast<const uint8_t*>(bike + kBikeContactWheels) != 0
        && *reinterpret_cast<const uintptr_t*>(bike + kPhysicalAttachedTo) == 0;
}

// Speed is taken from how far the bike actually travelled, not from
// `m_vecMoveSpeed`. The resting contact cancels that field partway through a
// frame, so sampling it catches transients that never moved the bike; the
// matrix position carries no such spikes and is exactly zero for a bike
// standing still.
void SampleLeanWindow(LeanTargetState& state, const float* position,
                      const float* right, float gravity) {
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

void __cdecl FilterBikeLeanTarget(float* target, uintptr_t bike) {
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        const float gravity = ReadGameFloat(kGravityConstant, 0.008f);
        if (!std::isfinite(*target) || timeStep <= 0.0f || gravity <= 0.0f) {
            return;
        }

        // A nominal 30 FPS cap does not produce one bit-exact timestep, so
        // there is no hard boundary at the original rate: at and below it the
        // filter has zero weight, and above it the weight grows continuously.
        // A frame it skips leaves a gap, so the next filtered frame starts from
        // the current position and target.
        if (timeStep >= kOriginalTimeStep || !LeanTargetIsDerivative(bike)) {
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
            SampleLeanWindow(state, position, right, gravity);
        }

        const float ratio = std::clamp(timeStep / kOriginalTimeStep,
                                       0.0f, 1.0f);
        const float filterWeight = 1.0f - ratio * ratio;
        *target += (state.held - *target) * filterWeight;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

// The two replaced instructions store the target and drop the leftover
// accumulator, so both are reproduced before the filter runs. After `pushfd`,
// `pushad` and one argument push, the caller frame starts 0x28 bytes up, which
// puts the target slot at `esp + 0x3C`.
__declspec(naked) void BikeLeanTargetThunk() {
    __asm {
        fstp dword ptr [esp + 0x14]
        fstp st(0)
        pushfd
        pushad
        push esi
        lea eax, [esp + 0x3C]
        push eax
        call FilterBikeLeanTarget
        add esp, 8
        popad
        popfd
        jmp kBikeLeanTargetReturn
    }
}

} // namespace

bool InstallBikeLeanTargetFix() {
    if (!InstallJump(g_bikeLeanTargetPatch, kBikeLeanTarget,
                     &BikeLeanTargetThunk, kExpectedBikeLeanTarget)) {
        Log("Bike lean target fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real-time bike lean target derivative.");
    return true;
}

} // namespace hff::bikes
