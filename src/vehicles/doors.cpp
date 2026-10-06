#include "vehicles/doors.h"

#include "core/config.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/vehicles.h"

#include <windows.h>

#include <array>
#include <cstdint>

namespace hff::vehicles {

namespace {

std::array<SitePatch, 4> g_doorSwingPatches{};
bool g_swingingDisabled{};

// `ang` is in st(0), the selected chassis apply rate is in st(1). Contact
// impulses can feed the same delta once per rendered frame, so normalize this
// chassis-only input to the original frame duration. The non-chassis path is
// deliberately left in the game for firetruck ladder movement.
__declspec(naked) void DoorForceChassisThunk() {
    __asm {
        fmul st, st(1)
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fadd dword ptr [esi + 0x14]
        jmp kDoorForceChassisReturn
    }
}

// `_CIpow` takes the base in st(1) and the exponent in st(0). Raising the
// stock damping factor to the timestep ratio preserves the fraction left over
// across frames, and stays stable at both very short and long timesteps. The
// registers `_CIpow` writes are kept across it.
__declspec(naked) void DoorDampingFiretruckThunk() {
    __asm {
        fld dword ptr ds:[0x00872314]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        pushfd
        push eax
        push ecx
        push edx
        call kPow
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kDoorDampingFiretruckReturn
    }
}

__declspec(naked) void DoorDampingOtherThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        pushfd
        push eax
        push ecx
        push edx
        call kPow
        pop edx
        pop ecx
        pop eax
        popfd
        fmul dword ptr [esi + 0x14]
        fstp dword ptr [esi + 0x14]
        jmp kDoorDampingOtherReturn
    }
}

// Scale the angular velocity before the original addition integrates it.
__declspec(naked) void DoorIntegrationThunk() {
    __asm {
        fld dword ptr [esi + 0x14]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        mov ecx, ebx
        jmp kDoorIntegrationReturn
    }
}

void TryDisableSwingingCompletely() {
    __try {
        if (NearlyEqual(*reinterpret_cast<const float*>(kDoorApplyRateChassis),
                        kStockDoorApplyRateChassis)
            && WriteProtectedGameFloat(kDoorApplyRateChassis, 0.0f)) {
            g_swingingDisabled = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

} // namespace

bool InstallDoorSwingFix() {
    PatchSet patches("Door swing fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
        size_t size;
    };
    const std::array<Site, 4> sites{{
        {kDoorForceChassis, &DoorForceChassisThunk,
         kExpectedDoorForceChassis.data(), kExpectedDoorForceChassis.size()},
        {kDoorDampingFiretruck, &DoorDampingFiretruckThunk,
         kExpectedDoorDampingFiretruck.data(),
         kExpectedDoorDampingFiretruck.size()},
        {kDoorDampingOther, &DoorDampingOtherThunk,
         kExpectedDoorDampingOther.data(), kExpectedDoorDampingOther.size()},
        {kDoorIntegration, &DoorIntegrationThunk,
         kExpectedDoorIntegration.data(), kExpectedDoorIntegration.size()},
    }};

    for (size_t i = 0; i < sites.size(); ++i) {
        const Site& site = sites[i];
        if (!patches.Track(
                InstallBranch(g_doorSwingPatches[i], site.address, site.thunk,
                              site.expected, site.size, 0xE9),
                g_doorSwingPatches[i])) {
            Log("Door swing fix skipped: CDoor::Process bytes do not match "
                "GTA SA 1.0 US.");
            return false;
        }
    }

    if (ReadSetting("vehicles", "disableSwingingCompletely", false)) {
        TryDisableSwingingCompletely();
    }

    patches.Commit();
    Log(g_swingingDisabled
            ? "Installed timestep-scaled door and firetruck ladder physics "
              "with chassis sway disabled."
            : "Installed timestep-scaled door, swinging chassis and "
              "firetruck ladder physics.");
    return true;
}

void RestoreDoorSwinging() {
    if (g_swingingDisabled) {
        WriteProtectedGameFloat(kDoorApplyRateChassis,
                                kStockDoorApplyRateChassis);
        g_swingingDisabled = false;
    }
}

} // namespace hff::vehicles
