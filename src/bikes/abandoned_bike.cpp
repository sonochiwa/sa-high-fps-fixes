#include "bikes/abandoned_bike.h"

#include "bikes/process_control.h"
#include "bikes/render_transform.h"
#include "core/log.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/bikes.h"
#include "game/vehicles.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdint>

// A riderless bike is the one remaining vehicle case where mathematically
// scaling individual constants does not reproduce the 30 FPS outcome. Contact
// generation, collision retries, suspension and wheel impulses form one
// nonlinear step. This fix therefore preserves that whole step: above 30 FPS an
// abandoned bike runs ProcessControl, ProcessCollision and ProcessShift at the
// original cadence and with the original timestep, and is drawn interpolated
// between its last two steps. Player vehicles and every other physical entity
// remain on the normal high-FPS path.

namespace hff::bikes {

namespace {

using ThisCallVoidFn = void(__thiscall*)(void*);

// One abandoned bike that steps at the original rate. A bike keeps its slot
// while it is stepped; a slot not stepped for `kStaleStepMs` belongs to a bike
// that was picked up, went to sleep or was deleted, and is free again.
struct AbandonedBikeState {
    void* bike{};
    uint32_t identity{};
    uint32_t lastStepMs{};
    // The frame ProcessControl took the original-rate path, and whether it
    // stepped on it, so the collision, shift and render calls of that frame
    // follow the same decision.
    uint32_t frame{};
    bool tick{};
    Transform previous{};
    Transform current{};
    bool valid{};
    bool previousCaptured{};
};

constexpr uint32_t kStaleStepMs = 500;

DetourPatch g_abandonedBikeCollisionPatch{};
DetourPatch g_abandonedBikeShiftPatch{};
DetourPatch g_abandonedBikeRwFramePatch{};
DetourPatch g_abandonedBikePreRenderPatch{};
DetourPatch g_abandonedBikeRenderPatch{};
bool g_abandonedBikePhysicsStepEnabled{};
uint32_t g_abandonedBikePhysicsLastFrame{};
float g_abandonedBikePhysicsCredit{};
bool g_abandonedBikePhysicsTick{};
std::array<AbandonedBikeState, 32> g_abandonedBikes{};
bool g_abandonedBikeTableFullLogged{};

uint32_t Now() {
    return *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
}

uint32_t CurrentFrame() {
    return *reinterpret_cast<const uint32_t*>(kFrameCounter);
}

bool IsStale(const AbandonedBikeState& state, uint32_t now) {
    return !state.bike || now - state.lastStepMs > kStaleStepMs;
}

// The state of `bike`, or null when it has none. A slot that still names the
// pointer but belongs to an earlier vehicle at the same address is not it.
AbandonedBikeState* FindAbandonedBike(void* bike) {
    const uint32_t now = Now();
    for (auto& state : g_abandonedBikes) {
        if (state.bike == bike && !IsStale(state, now)) {
            return state.identity == VehicleIdentity(bike) ? &state : nullptr;
        }
    }
    return nullptr;
}

// A fresh state for `bike` in a free or stale slot, or null when every slot
// holds a bike that is still stepping; that bike then runs at the frame rate.
AbandonedBikeState* CreateAbandonedBike(void* bike) {
    const uint32_t now = Now();
    for (auto& state : g_abandonedBikes) {
        if (IsStale(state, now) || state.bike == bike) {
            state = {};
            state.bike = bike;
            state.identity = VehicleIdentity(bike);
            state.lastStepMs = now;
            return &state;
        }
    }
    if (!g_abandonedBikeTableFullLogged) {
        g_abandonedBikeTableFullLogged = true;
        Log("Abandoned bike physics step: more abandoned bikes are moving than "
            "it tracks; the rest run at the frame rate.");
    }
    return nullptr;
}

// A bike at rest has its speeds zeroed by the game, which parks it; it needs
// no slot until something moves it.
bool IsMoving(const void* bike) {
    __try {
        const auto address = reinterpret_cast<uintptr_t>(bike);
        const auto* move = reinterpret_cast<const float*>(address + kPhysicalMoveSpeed);
        const auto* turn = reinterpret_cast<const float*>(address + kPhysicalTurnSpeed);
        for (size_t i = 0; i < 3; ++i) {
            if (move[i] != 0.0f || turn[i] != 0.0f) {
                return true;
            }
        }
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool IsAbandonedBike(const void* entity) {
    __try {
        if (!entity) {
            return false;
        }
        const auto address = reinterpret_cast<uintptr_t>(entity);
        constexpr uint8_t kStatusAbandoned = 4;
        const uint8_t packed = *reinterpret_cast<const uint8_t*>(
            address + kEntityTypeAndStatus);
        if ((packed & 0x07) != kEntityTypeVehicle) {
            return false;
        }
        const uint8_t status = packed >> 3;
        const uint8_t subClass = *reinterpret_cast<const uint8_t*>(
            address + kVehicleSubClass);
        const uint8_t bikeFlags = *reinterpret_cast<const uint8_t*>(
            address + kBikeFlags);
        return status == kStatusAbandoned
            && (subClass == kVehicleSubClassBike
                || subClass == kVehicleSubClassBmx)
            && !(bikeFlags & kBikeGettingPickedUp);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The state of an abandoned bike whose ProcessControl this frame took the
// original-rate path, or null.
AbandonedBikeState* SteppingThisFrame(void* entity) {
    if (!g_abandonedBikePhysicsStepEnabled) {
        return nullptr;
    }
    auto* state = FindAbandonedBike(entity);
    return state && state->frame == CurrentFrame() ? state : nullptr;
}

bool ShouldRunAbandonedBikePhysicsStep() {
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            // Every frame steps, so every frame is drawn at its own step.
            g_abandonedBikePhysicsCredit = 1.0f;
            g_abandonedBikePhysicsTick = true;
            return true;
        }

        const uint32_t frame = CurrentFrame();
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

void CallAbandonedBikePhysicsStep(const DetourPatch& patch, void* entity) {
    const ScopedOriginalTimeStep timeStepScope;
    reinterpret_cast<ThisCallVoidFn>(patch.gateway)(entity);
}

// On the frames between two steps the entity's collision and shift are skipped
// as well, and it is marked as resting in a safe position so CWorld does not
// retry them.
void SkipAbandonedBikeStep(void* physical) {
    __try {
        *reinterpret_cast<uint8_t*>(
            reinterpret_cast<uintptr_t>(physical) + kEntityFlags) |= 0x20;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void StepAbandonedBikePart(const DetourPatch& patch, void* physical) {
    auto* state = SteppingThisFrame(physical);
    if (!state) {
        reinterpret_cast<ThisCallVoidFn>(patch.gateway)(physical);
        return;
    }
    if (!state->tick) {
        SkipAbandonedBikeStep(physical);
        return;
    }
    CallAbandonedBikePhysicsStep(patch, physical);
    state->valid = state->previousCaptured
                && CopyEntityTransform(physical, state->current);
}

void __fastcall HookedPhysicalProcessCollision(void* physical, void*) {
    StepAbandonedBikePart(g_abandonedBikeCollisionPatch, physical);
}

void __fastcall HookedPhysicalProcessShift(void* physical, void*) {
    StepAbandonedBikePart(g_abandonedBikeShiftPatch, physical);
}

// The transform to draw an abandoned bike at, or false when it is drawn where
// its physics left it.
bool AbandonedBikeRenderTransform(void* bike, Transform& out) {
    if (!g_abandonedBikePhysicsStepEnabled || !IsAbandonedBike(bike)) {
        return false;
    }
    const auto* state = FindAbandonedBike(bike);
    if (!state || !state->valid) {
        return false;
    }
    out = InterpolateTransform(state->previous, state->current,
                               g_abandonedBikePhysicsCredit);
    return true;
}

void __fastcall HookedEntityUpdateRwFrame(void* entity, void*) {
    Transform renderTransform{};
    Transform physicalTransform{};
    if (AbandonedBikeRenderTransform(entity, renderTransform)
        && CopyEntityTransform(entity, physicalTransform)
        && WriteEntityTransform(entity, renderTransform)) {
        // UpdateRwMatrix may be detoured by another plugin. Call its public
        // entry so that hook still runs.
        reinterpret_cast<ThisCallVoidFn>(kEntityUpdateRwMatrix)(entity);
        WriteEntityTransform(entity, physicalTransform);
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
void CallAbandonedBikeRender(const DetourPatch& patch, void* bike) {
    Transform renderTransform{};
    Transform physicalTransform{};
    Transform savedLeanTransform{};
    uint8_t savedLeanCalculated{};
    bool swapped = false;
    const auto address = reinterpret_cast<uintptr_t>(bike);

    if (AbandonedBikeRenderTransform(bike, renderTransform)
        && CopyEntityTransform(bike, physicalTransform)
        && CopyMatrixTransform(address + kBikeLeanMatrix, savedLeanTransform)) {
        __try {
            savedLeanCalculated = *reinterpret_cast<const uint8_t*>(
                address + kBikeLeanMatrixCalculated);
            if (WriteEntityTransform(bike, renderTransform)) {
                *reinterpret_cast<uint8_t*>(
                    address + kBikeLeanMatrixCalculated) = 0;
                swapped = true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            swapped = false;
        }
    }

    reinterpret_cast<ThisCallVoidFn>(patch.gateway)(bike);

    if (swapped) {
        WriteEntityTransform(bike, physicalTransform);
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

bool InstallAbandonedBikeRenderHooks() {
    PatchSet renderPatches("Abandoned bike light interpolation");
    const bool renderInstalled = renderPatches.Track(
        InstallDetour(g_abandonedBikePreRenderPatch, kBikePreRender,
                      &HookedBikePreRender, kExpectedBikePreRender.data(),
                      kExpectedBikePreRender.size()),
        g_abandonedBikePreRenderPatch) && renderPatches.Track(
        InstallDetour(g_abandonedBikeRenderPatch, kBikeRender,
                      &HookedBikeRender, kExpectedBikeRender.data(),
                      kExpectedBikeRender.size()),
        g_abandonedBikeRenderPatch);
    if (!renderInstalled) {
        Log("Abandoned bike light interpolation skipped: CBike render entries "
            "do not match GTA SA 1.0 US.");
        return false;
    }
    renderPatches.Commit();
    return true;
}

bool InstallAbandonedBikeStepHooks(PatchSet& patches) {
    if (!patches.Track(
            InstallDetour(g_abandonedBikeCollisionPatch,
                          kPhysicalProcessCollision,
                          &HookedPhysicalProcessCollision,
                          kExpectedPhysicalProcessCollision.data(),
                          kExpectedPhysicalProcessCollision.size()),
            g_abandonedBikeCollisionPatch)) {
        Log("Abandoned bike physics step skipped: ProcessCollision entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            InstallDetour(g_abandonedBikeShiftPatch, kPhysicalProcessShift,
                          &HookedPhysicalProcessShift,
                          kExpectedPhysicalProcessShift.data(),
                          kExpectedPhysicalProcessShift.size()),
            g_abandonedBikeShiftPatch)) {
        Log("Abandoned bike physics step skipped: ProcessShift entry does not "
            "match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            InstallDetour(g_abandonedBikeRwFramePatch, kEntityUpdateRwFrame,
                          &HookedEntityUpdateRwFrame,
                          kExpectedEntityUpdateRwFrame.data(),
                          kExpectedEntityUpdateRwFrame.size()),
            g_abandonedBikeRwFramePatch)) {
        Log("Abandoned bike physics step skipped: UpdateRwFrame entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    return true;
}

} // namespace

bool ProcessAbandonedBikeControl(void* bike, const DetourPatch& processControl) {
    if (!g_abandonedBikePhysicsStepEnabled) {
        return false;
    }
    if (!IsAbandonedBike(bike)) {
        if (auto* state = FindAbandonedBike(bike)) {
            // Free the slot as soon as the bike stops being abandoned.
            *state = {};
        }
        return false;
    }
    auto* state = FindAbandonedBike(bike);
    const bool moving = IsMoving(bike);
    if (state && !moving) {
        // The game has parked the bike; the stock path keeps it parked and the
        // slot goes to a bike that moves.
        *state = {};
        return false;
    }
    if (!state && moving) {
        state = CreateAbandonedBike(bike);
    }
    if (!state) {
        return false;
    }
    state->frame = CurrentFrame();
    state->tick = ShouldRunAbandonedBikePhysicsStep();
    if (!state->tick) {
        // Physics remains at the last complete 30 Hz state, but refresh the
        // RenderWare hierarchy with an interpolated transform.
        reinterpret_cast<ThisCallVoidFn>(kEntityUpdateRwFrame)(bike);
        return true;
    }
    state->lastStepMs = Now();
    state->valid = false;
    state->previousCaptured = CopyEntityTransform(bike, state->previous);
    CallAbandonedBikePhysicsStep(processControl, bike);
    return true;
}

void DisableAbandonedBikePhysicsStep() {
    g_abandonedBikePhysicsStepEnabled = false;
}

bool InstallAbandonedBikePhysicsStepFix() {
    PatchSet patches("Abandoned bike physics step");
    g_abandonedBikePhysicsStepEnabled = false;
    g_abandonedBikePhysicsLastFrame = 0;
    g_abandonedBikePhysicsCredit = 0.0f;
    g_abandonedBikePhysicsTick = false;
    g_abandonedBikes = {};

    if (!InstallAbandonedBikeStepHooks(patches)) {
        return false;
    }
    const bool bikeHookWasInstalled = BikeProcessControlPatch().installed;
    if (!EnsureBikeProcessControlHook()
        || (!bikeHookWasInstalled
            && !patches.Track(true, BikeProcessControlPatch()))) {
        Log("Abandoned bike physics step skipped: CBike::ProcessControl entry "
            "does not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();

    const bool renderInstalled = InstallAbandonedBikeRenderHooks();
    g_abandonedBikePhysicsStepEnabled = true;
    Log("Installed complete 30 Hz physics steps with render interpolation for "
        "abandoned bikes.");
    if (renderInstalled) {
        Log("Installed interpolated abandoned-bike render matrices for lights.");
    }
    return true;
}

} // namespace hff::bikes
