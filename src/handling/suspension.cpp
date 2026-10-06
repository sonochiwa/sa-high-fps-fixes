#include "handling/suspension.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/handling.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace hff::handling {

namespace {

DetourPatch g_suspensionDampingPatch{};
DetourPatch g_physicalProcessControlPatch{};
bool g_suspensionDampingLimit = false;
bool g_suspensionLoadLean = false;

// The move and turn speed of the entity whose frame is in progress, taken
// at the entry of CPhysical::ProcessControl, before gravity, air resistance
// and the springs have acted. The damper wrapper reads it to know how much
// of the contact speed it sees was made this frame.
struct FrameStartSpeeds {
    const void* physical;
    float move[3];
    float turn[3];
};
FrameStartSpeeds g_frameStart{};

float Dot3(const float* a, const float* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Cross3(const float* a, const float* b, float* out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

// The damper's same-frame bite on the frame's own speed change at the
// contact point, capped like the stock damping by the spring force.
// `fraction` is what the stock arithmetic will remove of the whole contact
// speed after this.
void ApplyLoadLeanBite(void* physical, float stockFraction, float fraction,
                       float springForceLimit, const float* direction,
                       const float* collisionPoint) {
    const float extra = stockFraction - fraction;
    if (!(extra > 0.0f) || physical != g_frameStart.physical) {
        return;
    }
    const auto base = reinterpret_cast<uintptr_t>(physical);
    const float* move = reinterpret_cast<const float*>(base + kPhysicalMoveSpeed);
    const float* turn = reinterpret_cast<const float*>(base + kPhysicalTurnSpeed);
    const float mass = *reinterpret_cast<const float*>(base + kPhysicalMass);
    const float turnMass = *reinterpret_cast<const float*>(base + kPhysicalTurnMass);
    const float* centre = reinterpret_cast<const float*>(base + kPhysicalCentreOfMass);
    const float* matrix = *reinterpret_cast<const float* const*>(base + kEntityMatrix);
    if (!matrix || !(mass > 0.0f) || !(turnMass > 0.0f)) {
        return;
    }
    // The contact point relative to the centre of mass, as GetSpeed sees it.
    float centreWorld[3];
    for (int i = 0; i < 3; ++i) {
        centreWorld[i] = matrix[i] * centre[0] + matrix[4 + i] * centre[1]
                       + matrix[8 + i] * centre[2];
    }
    float distance[3];
    for (int i = 0; i < 3; ++i) {
        distance[i] = collisionPoint[i] - centreWorld[i];
    }
    // The speed change this frame has made at the contact point.
    float turnDelta[3];
    for (int i = 0; i < 3; ++i) {
        turnDelta[i] = turn[i] - g_frameStart.turn[i];
    }
    float delta[3];
    Cross3(turnDelta, distance, delta);
    for (int i = 0; i < 3; ++i) {
        delta[i] += move[i] - g_frameStart.move[i];
    }
    // The game's no-reversal clamp is not applied: it compares the removal
    // with the whole contact speed, and this removal is a fraction of one
    // frame's change, which at a high frame rate is far below the residual
    // motion of the body. The stock damper's own clamp never binds either,
    // since it removes a fraction of the same speed it compares against.
    const float removal = -extra * Dot3(delta, direction);
    if (removal == 0.0f || !std::isfinite(removal)) {
        return;
    }
    float lever[3];
    Cross3(distance, direction, lever);
    const float pointMass = 1.0f / (Dot3(lever, lever) / turnMass + 1.0f / mass);
    float force = pointMass * removal;
    const float cap = std::fabs(springForceLimit)
                    * *reinterpret_cast<const float*>(kDampingLimitOfSpringForce);
    if (force > cap) {
        force = cap;
    } else if (force < -cap) {
        force = -cap;
    }
    // Both vectors go by value, six floats on the stack.
    using ApplyForceFn = void(__thiscall*)(void*, float, float, float, float, float, float, bool);
    reinterpret_cast<ApplyForceFn>(kPhysicalApplyForce)(
        physical, force * direction[0], force * direction[1], force * direction[2],
        collisionPoint[0], collisionPoint[1], collisionPoint[2], true);
}

void __fastcall HookedPhysicalProcessControl(void* physical, void*) {
    __try {
        const auto base = reinterpret_cast<uintptr_t>(physical);
        g_frameStart.physical = physical;
        std::memcpy(g_frameStart.move, reinterpret_cast<const void*>(base + kPhysicalMoveSpeed),
                    sizeof(g_frameStart.move));
        std::memcpy(g_frameStart.turn, reinterpret_cast<const void*>(base + kPhysicalTurnSpeed),
                    sizeof(g_frameStart.turn));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_frameStart.physical = nullptr;
    }
    reinterpret_cast<void(__thiscall*)(void*)>(g_physicalProcessControlPatch.gateway)(physical);
}

// Two fixes share this wrapper. With `suspensionDampingLimit` the damping
// level is rewritten so the per-frame removal, after the game's own clamp,
// equals the 30 FPS removal spread over this frame. With
// `suspensionLoadLean` the damper first takes its 30 FPS bite of the speed
// change made earlier in the frame, which the stock arithmetic no longer
// does above 30 FPS. `physical` is read for the bMakeMassTwiceAsBig flag,
// which the game folds into the product before clamping.
bool __fastcall HookedSpringDampening(void* physical, void*, float dampingLevel,
                                      float springForceLimit, float* direction,
                                      float* collisionPoint, float* collisionSpeed) {
    using Fn = bool(__thiscall*)(void*, float, float, float*, float*, float*);
    float level = dampingLevel;
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        const float limit = *reinterpret_cast<const float*>(kDampingLimitInFrame);
        if (physical && std::isfinite(timeStep) && timeStep > 0.0f
            && timeStep < kOriginalTimeStep && std::isfinite(dampingLevel)
            && dampingLevel > 0.0f && limit > 0.0f) {
            const float mass =
                (*reinterpret_cast<const uint8_t*>(
                     reinterpret_cast<uintptr_t>(physical) + kPhysicalFlags) & 0x01) != 0
                    ? 2.0f : 1.0f;
            const float stockFraction =
                std::min(kOriginalTimeStep * dampingLevel * mass, limit);
            if (stockFraction > 0.0f && stockFraction < 1.0f) {
                if (g_suspensionDampingLimit) {
                    const float fraction =
                        1.0f - std::pow(1.0f - stockFraction, timeStep / kOriginalTimeStep);
                    level = fraction / (timeStep * mass);
                }
                if (g_suspensionLoadLean) {
                    const float fraction = std::min(timeStep * level * mass, limit);
                    ApplyLoadLeanBite(physical, stockFraction, fraction, springForceLimit,
                                      direction, collisionPoint);
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        level = dampingLevel;
    }
    return reinterpret_cast<Fn>(g_suspensionDampingPatch.gateway)(
        physical, level, springForceLimit, direction, collisionPoint, collisionSpeed);
}

void LogSkipped(const char* fixName, const char* reason) {
    char line[256];
    std::snprintf(line, sizeof(line), "%s skipped: %s", fixName, reason);
    Log(line);
}

// The damper wrapper serves two fixes; the first of them to install puts
// it in place, after checking the limit it depends on.
bool EnsureSpringDampeningWrapper(const char* fixName) {
    if (g_suspensionDampingPatch.installed) {
        return true;
    }
    __try {
        if (!NearlyEqual(*reinterpret_cast<const float*>(kDampingLimitInFrame),
                         kStockDampingLimitInFrame)) {
            LogSkipped(fixName, "the damping limit does not match GTA SA 1.0 US.");
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogSkipped(fixName, "the damping limit is unreadable.");
        return false;
    }
    if (!InstallDetour(g_suspensionDampingPatch, kApplySpringDampening,
                       &HookedSpringDampening,
                       kExpectedApplySpringDampening.data(),
                       kExpectedApplySpringDampening.size())) {
        LogSkipped(fixName, "ApplySpringDampening entry does not match GTA SA 1.0 US.");
        return false;
    }
    return true;
}

} // namespace

bool InstallSuspensionDampingLimitFix() {
    if (!EnsureSpringDampeningWrapper("Suspension damping limit fix")) {
        return false;
    }
    g_suspensionDampingLimit = true;
    Log("Installed suspension damping at its 30 FPS strength.");
    return true;
}

bool InstallSuspensionLoadLeanFix() {
    if (!EnsureSpringDampeningWrapper("Suspension load lean fix")) {
        return false;
    }
    if (!InstallDetour(g_physicalProcessControlPatch, kPhysicalProcessControl,
                       &HookedPhysicalProcessControl,
                       kExpectedPhysicalProcessControl.data(),
                       kExpectedPhysicalProcessControl.size())) {
        Log("Suspension load lean fix skipped: CPhysical::ProcessControl entry "
            "does not match GTA SA 1.0 US.");
        return false;
    }
    g_suspensionLoadLean = true;
    Log("Installed the damper's 30 FPS bite on the frame's own suspension "
        "load.");
    return true;
}

} // namespace hff::handling
