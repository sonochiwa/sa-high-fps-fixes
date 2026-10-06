#include "bikes/bmx_jump.h"

#include "core/log.h"
#include "game/addresses.h"
#include "game/sites/bikes.h"
#include "game/vehicles.h"

#include <windows.h>
#include <intrin.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// A bunny hop launches a BMX with more backward pitch above 30 FPS than at 30,
// and the landing that follows can throw the rider off. After the
// ProcessControl that applied the hop impulse, part of the backward pitch rate
// is taken away once. From the hop until shortly after the bike next touches
// the ground, a soft and mostly vertical impact with the nose at least 30
// degrees up no longer knocks the rider off or raises a rider-fall event, and
// the pitch rate that landing rebounds with is held to what 30 FPS produces.
// Only the local player's BMX is protected, and 30 FPS or below is an exact
// no-op.

namespace hff::bikes {

namespace {

// Matched full-charge trajectories reach about 47.9 degrees of backward
// rotation at 500+ FPS versus 42.1 degrees at 30 FPS. A 24% asymptotic
// correction produced the closest visual match in game. It fades to an exact
// no-op at the original timestep.
constexpr float kBmxLaunchPitchExcess = 0.24f;
constexpr float kBmxStockLandingPitchLimit = 0.105f;
constexpr float kBmxFalseLandingDamageLimit = 31.0f;
constexpr uint32_t kBmxLandingProtectionMaxMs = 3000;
constexpr uint32_t kBmxLandingProtectionGraceMs = 200;

std::array<SitePatch, 2> g_bmxRiderFallPatches{};
DetourPatch g_bmxLaunchBunnyHopPatch{};
DetourPatch g_bikeDamageKnockOffPatch{};
uintptr_t g_bmxLaunchCorrectionBike{};
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
            && (wheelCounts[2] > 0.0f || wheelCounts[3] > 0.0f)
            && IsDrivenByLocalPlayer(bike);
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
            g_bmxLandingProtectionUntil = now + kBmxLandingProtectionMaxMs;
            g_bmxLandingWasAirborne = false;
            g_bmxLandingContactSeen = false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bmxLaunchCorrectionBike = 0;
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
            && *reinterpret_cast<const uint8_t*>(bike + kVehicleSubClass)
                == kVehicleSubClassBmx
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
                && *reinterpret_cast<const float*>(matrix + kMatrixForward + 8) > 0.5f
                && damageIntensity <= kBmxFalseLandingDamageLimit
                && collisionImpactVelocity[2] > 0.75f
                && horizontalImpactSquared < 0.25f) {
                return false;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    return reinterpret_cast<Fn>(g_bikeDamageKnockOffPatch.gateway)(
        vehicle, damageIntensity, pieceType, damager, collisionPosition,
        collisionImpactVelocity);
}

// Takes back the pitch rate a protected landing rebounds with beyond what the
// stock game produces at 30 FPS.
void ClampLandingRebound(float* turn, const float* right) {
    float localPitch = 0.0f;
    float axisLengthSquared = 0.0f;
    for (size_t i = 0; i < 3; ++i) {
        localPitch += turn[i] * right[i];
        axisLengthSquared += right[i] * right[i];
    }
    if (std::isfinite(localPitch)
        && std::fabs(localPitch) > kBmxStockLandingPitchLimit
        && std::isfinite(axisLengthSquared) && axisLengthSquared >= 0.25f) {
        const float excess = localPitch
                           - std::copysign(kBmxStockLandingPitchLimit, localPitch);
        for (size_t i = 0; i < 3; ++i) {
            turn[i] -= right[i] * excess / axisLengthSquared;
        }
    }
}

void __fastcall HookedBmxRiderFallEventAdd(
    void* eventGroup, void*, void* event, bool addToEventGroup) {
    const uintptr_t callSite =
        reinterpret_cast<uintptr_t>(_ReturnAddress()) - 5;
    bool suppressFalseLandingFall = false;
    __try {
        const auto eventAddress = reinterpret_cast<uintptr_t>(event);
        const auto bike = eventAddress
            ? *reinterpret_cast<const uintptr_t*>(
                  eventAddress + kRiderFallEventVehicle)
            : 0;
        const auto matrix = bike
                && *reinterpret_cast<const uint8_t*>(bike + kVehicleSubClass)
                       == kVehicleSubClassBmx
            ? *reinterpret_cast<const uintptr_t*>(bike + kEntityMatrix)
            : 0;
        if (matrix) {
            const auto* forward =
                reinterpret_cast<const float*>(matrix + kMatrixForward);
            const auto* right =
                reinterpret_cast<const float*>(matrix + kMatrixRight);
            auto* turn = reinterpret_cast<float*>(bike + kPhysicalTurnSpeed);
            const float timeStep =
                *reinterpret_cast<const float*>(kTimerTimeStep);
            const uint32_t now = *reinterpret_cast<const uint32_t*>(
                kTimerTimeInMilliseconds);
            const uint8_t status = *reinterpret_cast<const uint8_t*>(
                bike + kEntityTypeAndStatus) >> 3;
            suppressFalseLandingFall =
                (callSite == kBikeRiderFallEventAddCalls[0]
                 || callSite == kBikeRiderFallEventAddCalls[1])
                && bike == g_bmxLandingProtectionBike
                && static_cast<int32_t>(g_bmxLandingProtectionUntil - now) >= 0
                && status == 0 && std::isfinite(timeStep) && timeStep > 0.0f
                && timeStep < kOriginalTimeStep && forward[2] > 0.5f;
            if (suppressFalseLandingFall
                && callSite == kBikeRiderFallEventAddCalls[0]) {
                ClampLandingRebound(turn, right);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        suppressFalseLandingFall = false;
    }
    if (!suppressFalseLandingFall) {
        reinterpret_cast<void(__thiscall*)(void*, void*, bool)>(kEventGroupAdd)(
            eventGroup, event, addToEventGroup);
    }
}

} // namespace

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
            g_bmxLandingProtectionUntil = now + kBmxLandingProtectionGraceMs;
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
        if (status != 0 || subClass != kVehicleSubClassBmx) {
            return;
        }

        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            bike + kEntityMatrix);
        if (!matrix) {
            return;
        }
        const auto* right = reinterpret_cast<const float*>(
            matrix + kMatrixRight);
        auto* turn = reinterpret_cast<float*>(bike + kPhysicalTurnSpeed);
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
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool InstallBmxJumpHooks(PatchSet& patches) {
    g_bmxLaunchCorrectionBike = 0;
    g_bmxLandingProtectionBike = 0;
    g_bmxLandingProtectionUntil = 0;
    g_bmxLandingWasAirborne = false;
    g_bmxLandingContactSeen = false;
    if (!patches.Track(
            InstallDetour(g_bmxLaunchBunnyHopPatch, kBmxLaunchBunnyHop,
                          &HookedBmxLaunchBunnyHop,
                          kExpectedBmxLaunchBunnyHop.data(),
                          kExpectedBmxLaunchBunnyHop.size()),
            g_bmxLaunchBunnyHopPatch)) {
        Log("Bike pitch experiment skipped: BMX launch callback entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            InstallDetour(g_bikeDamageKnockOffPatch, kBikeDamageKnockOffRider,
                          &HookedBikeDamageKnockOffRider,
                          kExpectedBikeDamageKnockOffRider.data(),
                          kExpectedBikeDamageKnockOffRider.size()),
            g_bikeDamageKnockOffPatch)) {
        Log("Bike pitch experiment skipped: DamageKnockOffRider entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    for (size_t i = 0; i < kBikeRiderFallEventAddCalls.size(); ++i) {
        if (!patches.Track(
                RepointCall(g_bmxRiderFallPatches[i],
                            kBikeRiderFallEventAddCalls[i], kEventGroupAdd,
                            &HookedBmxRiderFallEventAdd),
                g_bmxRiderFallPatches[i])) {
            Log("Bike pitch experiment skipped: rider-fall event call does "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    return true;
}

} // namespace hff::bikes
