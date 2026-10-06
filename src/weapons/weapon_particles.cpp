#include "weapons/weapon_particles.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/sites/weapons.h"

#include <windows.h>

#include <array>
#include <cstdint>

// A continuous weapon's effect system turns the elapsed time into a whole
// number of particles each frame. At a high frame rate each frame's share is
// below one particle, and the fraction kept in the emitter's intensity is lost
// whenever the emitter starts a frame from zero, so the spray vanishes. The
// intensity is kept per effect blueprint and handed back in that case.

namespace hff::weapons {

namespace {

using FxCreateParticlesFn = void(__thiscall*)(void*, float, float);

struct EmissionCarrySlot {
    void* blueprint{};
    float intensity{};
};

DetourPatch g_fxCreateParticlesPatch{};
std::array<EmissionCarrySlot, 16> g_weaponFxEmissionCarry{};

bool IsWeaponFxEmitter(void* emitter) {
    if (!emitter) {
        return false;
    }
    __try {
        void* system = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(emitter) + kFxEmitterSystem);
        return system && (*reinterpret_cast<uint8_t*>(
            reinterpret_cast<uintptr_t>(system) + kFxSystemFlags)
            & kFxSystemWeaponFlag) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

EmissionCarrySlot* FindEmissionCarrySlot(void* blueprint) {
    EmissionCarrySlot* empty{};
    for (auto& slot : g_weaponFxEmissionCarry) {
        if (slot.blueprint == blueprint) {
            return &slot;
        }
        if (!slot.blueprint && !empty) {
            empty = &slot;
        }
    }
    if (empty) {
        empty->blueprint = blueprint;
    }
    return empty;
}

void __fastcall HookedFxCreateParticles(void* emitter, void*, float currentTime,
                                        float deltaTime) {
    EmissionCarrySlot* carry{};
    float* intensity{};
    if (IsWeaponFxEmitter(emitter)) {
        __try {
            void* blueprint = *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(emitter) + kFxEmitterBlueprint);
            carry = FindEmissionCarrySlot(blueprint);
            intensity = reinterpret_cast<float*>(
                reinterpret_cast<uintptr_t>(emitter) + kFxEmitterIntensity);
            if (carry && *intensity == 0.0f && carry->intensity > 0.0f
                && carry->intensity < 1.0f) {
                *intensity = carry->intensity;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            carry = nullptr;
            intensity = nullptr;
        }
    }

    reinterpret_cast<FxCreateParticlesFn>(g_fxCreateParticlesPatch.gateway)(
        emitter, currentTime, deltaTime);

    if (carry && intensity) {
        __try {
            carry->intensity = *intensity;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            carry->intensity = 0.0f;
        }
    }
}

} // namespace

bool InstallContinuousWeaponParticlesFix() {
    if (!InstallDetour(g_fxCreateParticlesPatch, kFxCreateParticles,
                       &HookedFxCreateParticles,
                       kExpectedFxCreateParticles.data(),
                       kExpectedFxCreateParticles.size())) {
        Log("Continuous weapon emission carry failed at FxEmitter::CreateParticles.");
        return false;
    }
    Log("Installed fractional emission carry for continuous weapon FX systems.");
    return true;
}

} // namespace hff::weapons
