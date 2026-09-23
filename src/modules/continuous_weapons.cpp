#include "modules/modules.h"

// A held spraycan, extinguisher or flamethrower fires every rendered frame:
// `CWeapon::FireAreaEffect` runs once a frame, and each of its calls to
// `CShotInfo::AddShot` makes a shot that damages a ped once, paints a tag or
// puts out a fire, while the flamethrower also tries to start a fire nearby
// a third of the time. Above 30 FPS all of that happens many times as often.
// The two calls are gated per shooter so they run once per original frame;
// the spray itself is still drawn every frame. Putting a fire out is the one
// effect the game already scales by the frame's time, so a gated shot does it
// with the strength of a whole original frame.

namespace hff {

namespace {

struct AreaShotSlot {
    void* creator;
    uint32_t lastMs;
    float credit;
};

std::array<AreaShotSlot, 16> g_areaShotSlots{};
bool g_areaShotAllowed = true;

AreaShotSlot& FindAreaShotSlot(void* creator) {
    AreaShotSlot* oldest = &g_areaShotSlots.front();
    for (auto& slot : g_areaShotSlots) {
        if (slot.creator == creator || !slot.creator) {
            return slot;
        }
        if (static_cast<int32_t>(slot.lastMs - oldest->lastMs) < 0) {
            oldest = &slot;
        }
    }
    return *oldest;
}

// One original frame's worth of shooting is banked per rendered frame; a
// shot is taken each time a whole one has built up. A shooter that has not
// fired for a while starts a new burst and shoots at once.
bool TakeAreaShot(void* creator) {
    const float ratio = TimeStepRatio();
    if (!(ratio > 0.0f) || ratio >= 1.0f) {
        return true;
    }
    const uint32_t now = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
    auto& slot = FindAreaShotSlot(creator);
    constexpr uint32_t kNewBurstThresholdMs = 200;
    if (slot.creator != creator || now - slot.lastMs > kNewBurstThresholdMs) {
        slot = {creator, now, 0.0f};
        return true;
    }
    slot.lastMs = now;
    slot.credit += ratio;
    if (slot.credit < 1.0f) {
        return false;
    }
    slot.credit -= 1.0f;
    return true;
}

} // namespace

bool __cdecl GatedAreaEffectAddShot(void* creator, int32_t weaponType,
                                    ShotVector origin, ShotVector target) {
    bool allowed = true;
    __try {
        allowed = TakeAreaShot(creator);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        allowed = true;
    }
    g_areaShotAllowed = allowed;
    if (!allowed) {
        return false;
    }
    using AddShotFn = bool(__cdecl*)(void*, int32_t, ShotVector, ShotVector);
    return reinterpret_cast<AddShotFn>(kShotInfoAddShot)(creator, weaponType,
                                                         origin, target);
}

// The fire loses the strength times the frame's time for each shot that
// reaches it. A gated shot stands for a whole original frame, so it is given
// the strength of the frames it covers; left alone, the extinguisher would
// put fires out that many times more slowly.
bool __fastcall ExtinguishShotWithWater(void* fireManager, void*, ShotVector point,
                                        float radius, float strength) {
    const float ratio = TimeStepRatio();
    if (ratio > 0.0f && ratio < 1.0f) {
        strength /= ratio;
    }
    using ExtinguishFn = bool(__thiscall*)(void*, ShotVector, float, float);
    return reinterpret_cast<ExtinguishFn>(kFireManagerExtinguishPointWithWater)(
        fireManager, point, radius, strength);
}

// Runs after the gated shot in the same `FireAreaEffect` call, so it follows
// that shot's decision.
bool __cdecl GatedAreaEffectCreepingFire(ShotVector position, int32_t generations,
                                         int32_t allowSpread, int32_t scriptFire,
                                         float zDistance) {
    if (!g_areaShotAllowed) {
        return false;
    }
    using TryToStartFn = bool(__cdecl*)(ShotVector, int32_t, int32_t, int32_t, float);
    return reinterpret_cast<TryToStartFn>(kCreepingFireTryToStart)(
        position, generations, allowSpread, scriptFire, zDistance);
}

} // namespace hff
