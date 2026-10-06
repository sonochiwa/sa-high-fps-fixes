#include "world/fire_spread.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/world.h"

#include <array>
#include <cstdint>

namespace hff::world {

namespace {

std::array<SitePatch, 3> g_fireGatePatches{};

// Entered with the freshly drawn random number in eax, replacing
// `test al,<mask> / jne skip`. One decision a frame covers every fire and all
// three events, as a 30 FPS frame lets every fire roll each of its events
// once; a carry per call would make the same fires win the draw frame after
// frame while the others never roll at all. `FrameTick` clobbers eax, ecx and
// edx, all of which the rand call immediately above already clobbered, so only
// the draw itself has to be preserved. `pop` leaves the flags alone, so the
// mask test after it is the original test unchanged.
__declspec(naked) void FireVehicleGateThunk() {
    __asm {
        push eax
        push kFrameTickFireEvents
        call FrameTick
        add esp, 4
        test eax, eax
        pop eax
        je skipped
        test al, 0x1F
        jne skipped
        jmp kFireVehicleResume
    skipped:
        jmp kFireVehicleSkip
    }
}

__declspec(naked) void FireSpreadGateThunk() {
    __asm {
        push eax
        push kFrameTickFireEvents
        call FrameTick
        add esp, 4
        test eax, eax
        pop eax
        je skipped
        test al, 0x7F
        jne skipped
        jmp kFireSpreadResume
    skipped:
        jmp kFireSpreadSkip
    }
}

__declspec(naked) void FireMergeGateThunk() {
    __asm {
        push eax
        push kFrameTickFireEvents
        call FrameTick
        add esp, 4
        test eax, eax
        pop eax
        je skipped
        test al, 0x0F
        jne skipped
        jmp kFireMergeResume
    skipped:
        jmp kFireMergeSkip
    }
}

} // namespace

bool InstallFireSpreadFix() {
    PatchSet patches("Fire spread fix");
    struct Site {
        SitePatch* patch;
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {&g_fireGatePatches[0], kFireVehicleGate, &FireVehicleGateThunk,
         kExpectedFireVehicleGate.data()},
        {&g_fireGatePatches[1], kFireSpreadGate, &FireSpreadGateThunk,
         kExpectedFireSpreadGate.data()},
        {&g_fireGatePatches[2], kFireMergeGate, &FireMergeGateThunk,
         kExpectedFireMergeGate.data()}
    };

    for (const auto& site : sites) {
        if (!patches.Track(
                InstallBranch(*site.patch, site.address, site.thunk,
                              site.expected, 8, 0xE9),
                *site.patch)) {
            Log("Fire spread fix skipped: CFire::ProcessFire bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }
    ResetFrameTicks();
    patches.Commit();
    Log("Installed frame-rate independent fire event rates.");
    return true;
}

} // namespace hff::world
