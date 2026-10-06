#include "world/explosions.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/world.h"
#include "timers/frame_time_carry.h"

#include <cstdint>

// An explosion keeps its frame count in a byte that ends it when it wraps, so
// above about 60 FPS the longer explosions stop early; the count now stops at
// 255 instead, and the explosion ends at its own expiry time as at 30 FPS. The
// chance of an aircraft's secondary blast and of setting the victim alight is
// rolled, and the burning fuel drawn, on the frames on which an original
// 30 FPS frame has passed, so they happen as often per second as at 30 FPS.
// The fuel timer keeps the fraction of each frame it used to drop.

namespace hff::world {

namespace {

using RandomInRangeFn = int32_t(__cdecl*)(int32_t, int32_t);
using RandFn = int32_t(__cdecl*)();
using CreateFxSystemFn = void*(__thiscall*)(void*, const char*, const void*, void*, bool);

SitePatch g_frameCountPatch{};
SitePatch g_aircraftBlastPatch{};
SitePatch g_victimFirePatch{};
SitePatch g_fuelEffectPatch{};

__declspec(naked) void ExplosionFrameCountThunk() {
    __asm {
        cmp byte ptr [esi-8], 0xFF
        je counted
        inc byte ptr [esi-8]
    counted:
        jmp kExplosionFrameCountResume
    }
}

// Returning the top of the range keeps the roll above the 5 that sets off
// another blast.
int32_t __cdecl GatedAircraftBlastRoll(int32_t low, int32_t high) {
    if (!FrameTick(kFrameTickExplosions)) {
        return high;
    }
    return reinterpret_cast<RandomInRangeFn>(kRandomNumberInRange)(low, high);
}

// An odd number fails the `& 0x1F` test that sets the victim alight.
int32_t __cdecl GatedVictimFireRoll() {
    if (!FrameTick(kFrameTickExplosions)) {
        return 1;
    }
    return reinterpret_cast<RandFn>(kRand)();
}

// The caller skips the effect when no system is returned.
void* __fastcall GatedFuelEffect(void* manager, void*, const char* name,
                                 const void* position, void* matrix, bool ignoreBounds) {
    if (!FrameTick(kFrameTickExplosions)) {
        return nullptr;
    }
    return reinterpret_cast<CreateFxSystemFn>(kCreateFxSystem)(manager, name, position,
                                                              matrix, ignoreBounds);
}

} // namespace

bool InstallExplosionsFix() {
    PatchSet patches("Explosion fix");
    if (!patches.Track(InstallJump(g_frameCountPatch, kExplosionFrameCount,
                                   &ExplosionFrameCountThunk,
                                   kExpectedExplosionFrameCount),
                       g_frameCountPatch)
        || !patches.Track(RepointCall(g_aircraftBlastPatch, kExplosionAircraftBlastRoll,
                                      kRandomNumberInRange, &GatedAircraftBlastRoll),
                          g_aircraftBlastPatch)
        || !patches.Track(RepointCall(g_victimFirePatch, kExplosionVictimFireRoll, kRand,
                                      &GatedVictimFireRoll),
                          g_victimFirePatch)
        || !patches.Track(RepointCall(g_fuelEffectPatch, kExplosionFuelEffect,
                                      kCreateFxSystem, &GatedFuelEffect),
                          g_fuelEffectPatch)) {
        Log("Explosion fix skipped: CExplosion::Update bytes do not match GTA SA "
            "1.0 US.");
        return false;
    }
    if (!timers::InstallExplosionFuelTimerCarry()) {
        return false;
    }
    patches.Commit();
    Log("Installed explosion lifetime, secondary blasts and burning fuel at the "
        "original rate.");
    return true;
}

} // namespace hff::world
