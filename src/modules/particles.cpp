#include "modules/modules.h"

namespace hff {

bool g_particleRateGate = true;
uint32_t g_particleBudget = 0;
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

// Optional hard ceiling on new particles per second, kept for parity with the
// standalone FxLimiter plugin. Off by default: the rate gate above already
// restores the density the game was drawn for, and a fixed ceiling is a
// performance knob rather than a fix.
double g_qpcToSeconds = 0.0;

double NowSeconds() {
    if (g_qpcToSeconds == 0.0) {
        LARGE_INTEGER frequency{};
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0) {
            return 0.0;
        }
        g_qpcToSeconds = 1.0 / static_cast<double>(frequency.QuadPart);
    }
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) * g_qpcToSeconds;
}
ParticleBudgetState g_generalBudget{};

bool ParticleBudgetAllows(ParticleBudgetState& state, uint32_t perSecond) {
    if (perSecond == 0) {
        return true;
    }
    const double now = NowSeconds();
    if (now == 0.0) {
        return true;
    }
    if (state.lastRefill == 0.0 || now < state.lastRefill) {
        state.lastRefill = now;
        state.credit = 1.0;
    } else {
        state.credit += (now - state.lastRefill) * static_cast<double>(perSecond);
        state.lastRefill = now;
        if (state.credit > 1.0) {
            state.credit = 1.0;
        }
    }
    if (state.credit >= 1.0) {
        state.credit -= 1.0;
        return true;
    }
    return false;
}

__declspec(noinline) void __fastcall HookedFxAddParticle(
    void* self, void* edx, const void* pos, const void* vel, float timeSince,
    const void* mults, float rotZ, float lightMult, float lightMultLimit,
    int32_t createLocal) {
    const uintptr_t site = reinterpret_cast<uintptr_t>(_ReturnAddress());

    // Always evaluated so the site's frame stamp stays current even when the
    // gate is switched off and only the budget or the multipliers are wanted.
    const bool open = ParticleSiteOpen(site);
    if (g_particleRateGate && !open) {
        return;
    }

    if (g_particleBudget != 0
        && !ParticleBudgetAllows(g_generalBudget, g_particleBudget)) {
        return;
    }

    reinterpret_cast<FxAddParticleFn>(g_fxAddParticlePatch.gateway)(
        self, edx, pos, vel, timeSince, mults, rotZ, lightMult, lightMultLimit,
        createLocal);
}

} // namespace hff
