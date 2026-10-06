#include "weapons/area_shots.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/weapons.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>

// A held spraycan, extinguisher or flamethrower keeps calling
// `CWeapon::FireAreaEffect`: the spraycan on every rendered frame, the
// extinguisher and the flamethrower each time their 14 ms fire animation loop
// passes its fire time, which is every frame at 30 FPS and every second to
// fifth frame above 71 FPS. Each call to `CShotInfo::AddShot` makes a shot that
// damages a ped once, paints a tag or puts out a fire, while the flamethrower
// also tries to start a fire nearby a third of the time. Above 30 FPS all of
// that happens many times as often for the spraycan and as rarely as 6 times a
// second for the others. The two calls are gated per shooter so they run 30
// times a second of rendered frames, whichever weapon it is; the spray itself
// is still drawn every frame. Putting a fire out is the one effect the game
// already scales by the frame's time, so a gated shot does it with the
// strength of a whole original frame.

namespace hff::weapons {

namespace {

// A CVector passed by value to the game's cdecl functions.
struct ShotVector {
    float x;
    float y;
    float z;
};

struct AreaShotSlot {
    void* creator;
    uint32_t lastMs;
    uint32_t lastFrame;
    float credit;
};

std::array<SitePatch, 3> g_continuousShotPatches{};
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

// One original frame's worth of shooting is banked for every rendered frame
// since the shooter's previous call, so a weapon that fires every few frames
// earns the same as one that fires every frame; a shot is taken each time a
// whole one has built up. A shooter that has not fired for a while starts a
// new burst and shoots at once.
bool TakeAreaShot(void* creator) {
    const float ratio = TimeStepRatio();
    if (!(ratio > 0.0f) || ratio >= 1.0f) {
        return true;
    }
    const uint32_t now = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
    const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
    auto& slot = FindAreaShotSlot(creator);
    constexpr uint32_t kNewBurstThresholdMs = 200;
    if (slot.creator != creator || now - slot.lastMs > kNewBurstThresholdMs) {
        slot = {creator, now, frame, 0.0f};
        return true;
    }
    // A shooter never earns more than one original frame's worth per call.
    const auto maxFrames = static_cast<uint32_t>(1.0f / ratio) + 1;
    const uint32_t frames = std::min(frame - slot.lastFrame, maxFrames);
    slot.lastMs = now;
    slot.lastFrame = frame;
    slot.credit += ratio * static_cast<float>(frames);
    if (slot.credit < 1.0f) {
        return false;
    }
    slot.credit -= 1.0f;
    return true;
}

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

} // namespace

bool InstallContinuousWeaponShotsFix() {
    PatchSet patches("Continuous weapon shot rate fix");
    if (!patches.Track(InstallCall(g_continuousShotPatches[0], kAreaEffectAddShotCall,
                                   &GatedAreaEffectAddShot,
                                   kExpectedAreaEffectAddShotCall),
                       g_continuousShotPatches[0])
        || !patches.Track(InstallCall(g_continuousShotPatches[1],
                                      kAreaEffectCreepingFireCall,
                                      &GatedAreaEffectCreepingFire,
                                      kExpectedAreaEffectCreepingFireCall),
                          g_continuousShotPatches[1])) {
        Log("Continuous weapon shot rate fix skipped: CWeapon::FireAreaEffect "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallCall(g_continuousShotPatches[2], kShotExtinguishCall,
                                   &ExtinguishShotWithWater,
                                   kExpectedShotExtinguishCall),
                       g_continuousShotPatches[2])) {
        Log("Continuous weapon shot rate fix skipped: CShotInfo::Update bytes "
            "do not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed spraycan, extinguisher and flamethrower shots at the "
        "original rate.");
    return true;
}

} // namespace hff::weapons
