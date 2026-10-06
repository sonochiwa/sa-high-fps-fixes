#include "vehicles/rest.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/vehicles.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace hff::vehicles {

namespace {

std::array<SitePatch, 3> g_restThresholdPatches{};
std::array<SitePatch, 4> g_fakePhysicsPatches{};
uint32_t g_fakePhysicsLastFrame{0xFFFFFFFF};
float g_fakePhysicsCarry{};
int32_t g_fakePhysicsTick{1};

// Each site is `fld [esi+0xD4]`, `m_fMovingSpeed`, the per-frame distance,
// which is rescaled into the units the fixed limit was written for before
// the comparison. At 30 FPS this multiplies by exactly one.
__declspec(naked) void CarRestThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xD4]
        fmul g_originalTimeStepValue
        fdiv dword ptr ds:[0x00B7CB5C]
        jmp kCarRestThresholdReturn
    }
}

__declspec(naked) void BikeRestThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xD4]
        fmul g_originalTimeStepValue
        fdiv dword ptr ds:[0x00B7CB5C]
        jmp kBikeRestThresholdReturn
    }
}

__declspec(naked) void TrailerRestThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xD4]
        fmul g_originalTimeStepValue
        fdiv dword ptr ds:[0x00B7CB5C]
        jmp kTrailerRestThresholdReturn
    }
}

// `CObject`, `CAutomobile`, `CBike` and `CTrailer` all end their per-frame
// physics with the same idea:
//
//     m_vecForce = (m_vecForce + m_vecMoveSpeed) / 2;
//     if (still moving) { m_nFakePhysics = 0; }
//     else if (++m_nFakePhysics > 10) {
//         m_nFakePhysics = 10;
//         ResetMoveSpeed(); ResetTurnSpeed(); skipPhysics = true;
//     }
//
// `m_nFakePhysics` counts rendered frames, not time. At 30 FPS an entity has to
// stay nearly still for 11 frames, about 0.37 s, before the engine parks it. At
// 300 FPS that is 0.037 s, so a bike that is momentarily slow at the apex of a
// jump has its speed zeroed and its physics skipped, and hangs in mid-air; and
// a parked car being pushed is put back to sleep between pushes, which is why
// it becomes hard to move.
//
// The counter is therefore stepped in real time at the original 30 FPS rate
// instead of once per rendered frame. The decision is made once per game frame
// and shared by every entity, so each entity keeps its own counter and the
// `> 10` comparisons are untouched. At 30 FPS and below every frame ticks, so
// the original behavior is reproduced exactly.
int32_t __cdecl ShouldTickFakePhysicsCounter() {
    if (InsideOriginalTimeStep()) {
        return 1;
    }
    __try {
        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        if (frame != g_fakePhysicsLastFrame) {
            g_fakePhysicsLastFrame = frame;
            g_fakePhysicsCarry += TimeStepRatio();
            if (g_fakePhysicsCarry >= 1.0f) {
                g_fakePhysicsCarry -= std::floor(g_fakePhysicsCarry);
                g_fakePhysicsTick = 1;
            } else {
                g_fakePhysicsTick = 0;
            }
        }
        return g_fakePhysicsTick;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

// Each site is `mov r8, [esi+0xB8]` followed by `inc r8`, and every one of them
// overwrites AL on the very next instruction, so returning the tick flag in EAX
// is safe. A parked entity holds the counter at 10 and the game parks it again
// on every frame the increment takes it past 10, so a counter that has reached
// 10 is incremented on every frame; only the count up to it waits for a tick.
// The code after each site derives its own flags.
__declspec(naked) void ObjectFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov cl, byte ptr [esi + 0xB8]
        test eax, eax
        jnz tick
        cmp cl, 10
        jb noTick
    tick:
        inc cl
    noTick:
        jmp kObjectFakePhysicsReturn
    }
}

__declspec(naked) void CarFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov dl, byte ptr [esi + 0xB8]
        test eax, eax
        jnz tick
        cmp dl, 10
        jb noTick
    tick:
        inc dl
    noTick:
        jmp kCarFakePhysicsReturn
    }
}

__declspec(naked) void BikeFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov cl, byte ptr [esi + 0xB8]
        test eax, eax
        jnz tick
        cmp cl, 10
        jb noTick
    tick:
        inc cl
    noTick:
        jmp kBikeFakePhysicsReturn
    }
}

__declspec(naked) void TrailerFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov bl, byte ptr [esi + 0xB8]
        test eax, eax
        jnz tick
        cmp bl, 10
        jb noTick
    tick:
        inc bl
    noTick:
        jmp kTrailerFakePhysicsReturn
    }
}

} // namespace

bool InstallVehicleRestThresholdFix() {
    PatchSet patches("Vehicle rest threshold fix");
    constexpr std::array<uintptr_t, 3> addresses{
        kCarRestThreshold, kBikeRestThreshold, kTrailerRestThreshold
    };
    const std::array<const void*, 3> thunks{
        &CarRestThresholdThunk,
        &BikeRestThresholdThunk,
        &TrailerRestThresholdThunk,
    };

    for (const auto address : addresses) {
        if (!MemoryMatches(address, kExpectedRestThreshold)) {
            Log("Vehicle rest threshold fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!patches.Track(InstallJump(g_restThresholdPatches[i], addresses[i],
                                       thunks[i], kExpectedRestThreshold),
                           g_restThresholdPatches[i])) {
            Log("Vehicle rest threshold fix failed while installing hooks.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-normalized at-rest threshold for cars, bikes and trailers.");
    return true;
}

bool InstallPhysicsSleepRateFix() {
    PatchSet patches("Physics sleep rate fix");
    constexpr std::array<uintptr_t, 4> addresses{
        kObjectFakePhysics, kCarFakePhysics, kBikeFakePhysics,
        kTrailerFakePhysics
    };
    const std::array<std::array<uint8_t, 8>, 4> expected{
        kExpectedObjectFakePhysics,
        kExpectedCarFakePhysics,
        kExpectedBikeFakePhysics,
        kExpectedTrailerFakePhysics,
    };
    const std::array<const void*, 4> thunks{
        &ObjectFakePhysicsThunk,
        &CarFakePhysicsThunk,
        &BikeFakePhysicsThunk,
        &TrailerFakePhysicsThunk,
    };

    if (!InstallJumpTable(patches, g_fakePhysicsPatches, addresses, thunks,
                          expected)) {
        Log("Physics sleep rate fix skipped: executable bytes do not match the active game profile.");
        return false;
    }
    patches.Commit();
    Log("Installed a real-time physics sleep counter for objects, cars, bikes and trailers.");
    return true;
}

} // namespace hff::vehicles
