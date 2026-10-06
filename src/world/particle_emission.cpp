#include "world/particle_emission.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/world.h"

#include <intrin.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace hff::world {

namespace {

using FxAddParticleFn = void(__fastcall*)(void* self, void* edx, const void* pos,
                                          const void* vel, float timeSince,
                                          const void* mults, float rotZ,
                                          float lightMult, float lightMultLimit,
                                          int32_t createLocal);

// One entry per call site, found by the return address of the call into
// `FxSystem_c::AddParticle`. Open addressing, and a full table fails open: a
// missed gate costs some particles, a wrong one costs the effect entirely.
constexpr uint32_t kParticleSiteSlots = 512;
constexpr uint32_t kParticleSiteMask = kParticleSiteSlots - 1;

struct ParticleSiteState {
    uintptr_t site;
    uint32_t lastFrame;
    float carry;
    uint8_t open;
};

DetourPatch g_fxAddParticlePatch{};
std::array<ParticleSiteState, kParticleSiteSlots> g_particleSites{};

void ResetParticleSites() {
    for (auto& slot : g_particleSites) {
        slot.site = 0;
        slot.lastFrame = 0;
        slot.carry = 0.0f;
        slot.open = 1;
    }
}

ParticleSiteState* FindParticleSite(uintptr_t site) {
    uint32_t key = static_cast<uint32_t>(site);
    key ^= key >> 4;
    key *= 2654435761u;
    uint32_t index = (key >> 8) & kParticleSiteMask;
    for (uint32_t probe = 0; probe < 32; ++probe) {
        ParticleSiteState& slot = g_particleSites[index];
        if (slot.site == site) {
            return &slot;
        }
        if (slot.site == 0) {
            slot.site = site;
            slot.lastFrame = 0;
            slot.carry = 0.0f;
            slot.open = 1;
            return &slot;
        }
        index = (index + 1) & kParticleSiteMask;
    }
    return nullptr;
}

// True when this call site may emit on this frame. The decision is taken once
// per frame counter value and reused, so every call the site makes within one
// frame agrees: the exhaust adds up to eight particles in its loop and either
// all of them belong to this frame or none do.
bool ParticleSiteOpen(uintptr_t site) {
    if (InsideOriginalTimeStep()) {
        return true;
    }
    ParticleSiteState* slot = FindParticleSite(site);
    if (!slot) {
        return true;
    }
    const uint32_t frame = *reinterpret_cast<volatile uint32_t*>(kFrameCounter);
    if (slot->lastFrame == frame) {
        return slot->open != 0;
    }

    float ratio = TimeStepRatio();
    if (!std::isfinite(ratio) || ratio <= 0.0f) {
        ratio = 1.0f;
    }
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }

    const uint32_t elapsedFrames = frame - slot->lastFrame;
    if (slot->lastFrame == 0
        || static_cast<float>(elapsedFrames) * ratio >= 1.0f) {
        // The call site was idle for at least one original frame, so this is a
        // fresh event rather than a short stochastic gap in a stream. Never
        // drop the first particle of a fresh event.
        slot->carry = 0.0f;
        slot->open = 1;
    } else {
        slot->carry += ratio;
        if (slot->carry >= 1.0f) {
            slot->carry -= 1.0f;
            slot->open = 1;
        } else {
            slot->open = 0;
        }
    }
    slot->lastFrame = frame;
    return slot->open != 0;
}

__declspec(noinline) void __fastcall HookedFxAddParticle(
    void* self, void* edx, const void* pos, const void* vel, float timeSince,
    const void* mults, float rotZ, float lightMult, float lightMultLimit,
    int32_t createLocal) {
    const uintptr_t site = reinterpret_cast<uintptr_t>(_ReturnAddress());
    if (!ParticleSiteOpen(site)) {
        return;
    }
    reinterpret_cast<FxAddParticleFn>(g_fxAddParticlePatch.gateway)(
        self, edx, pos, vel, timeSince, mults, rotZ, lightMult, lightMultLimit,
        createLocal);
}

} // namespace

bool InstallParticleEmissionRateFix() {
    ResetParticleSites();
    if (!InstallDetour(g_fxAddParticlePatch, kFxAddParticle,
                       &HookedFxAddParticle, kExpectedFxAddParticle.data(),
                       kExpectedFxAddParticle.size())) {
        Log("Particle emission rate fix skipped: FxSystem_c::AddParticle bytes "
            "do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a frame-rate independent direct particle emission rate.");
    return true;
}

} // namespace hff::world
