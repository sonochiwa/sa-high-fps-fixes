#include "vehicles/aircraft.h"

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

std::array<SitePatch, 2> g_heliRotorPatches{};
SitePatch g_aiAircraftSteerPatch{};
SitePatch g_skimmerResistancePatch{};

// The rotor speed constant is reached through the original instruction operand
// so mods that repoint it keep working.
float ReadHeliRotorFinalSpeed() {
    __try {
        const auto operand = *reinterpret_cast<const uintptr_t*>(
            kHeliRotorSpeedOperand + 2);
        return ReadGameFloat(operand, 0.22f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0.22f;
    }
}

float __cdecl GetHeliRotorSlowStep() {
    return (ReadHeliRotorFinalSpeed() / kHeliRotorSpeedDivisor)
         * TimeStepRatio();
}

float __cdecl GetHeliRotorFastStep() {
    return (ReadHeliRotorFinalSpeed() / kHeliRotorSpeedDivisor) * 3.0f
         * TimeStepRatio();
}

float __cdecl GetSkimmerResistance() {
    return ReadGameFloat(kSkimmerResistanceConstant, 30.0f) * TimeStepRatio();
}

__declspec(naked) void HeliRotorSlowThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetHeliRotorSlowStep
        pop edx
        pop ecx
        pop eax
        popfd
        faddp st(1), st
        jmp kHeliRotorSlowReturn
    }
}

__declspec(naked) void HeliRotorFastThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetHeliRotorFastStep
        pop edx
        pop ecx
        pop eax
        popfd
        faddp st(1), st
        jmp kHeliRotorFastReturn
    }
}

// The same contract as the follow camera thunks: the replaced block left the
// divisor alone on the FPU stack and so does this.
__declspec(naked) void AiAircraftSteerRateThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        jmp kAiAircraftSteerRateReturn
    }
}

__declspec(naked) void SkimmerResistanceThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetSkimmerResistance
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kSkimmerResistanceReturn
    }
}

} // namespace

bool InstallHeliRotorSpeedFix() {
    PatchSet patches("Helicopter rotor fix");
    const std::array<std::array<uint8_t, 6>, 2> expected{
        kExpectedHeliRotorSlow, kExpectedHeliRotorFast
    };
    const std::array<const void*, 2> thunks{
        &HeliRotorSlowThunk, &HeliRotorFastThunk
    };

    if (!MemoryMatches(kHeliRotorSpeedOperand, kExpectedHeliRotorOperand)) {
        Log("Helicopter rotor fix skipped: rotor speed operand does not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallJumpTable(patches, g_heliRotorPatches, kHeliRotorSites, thunks,
                          expected)) {
        Log("Helicopter rotor fix skipped: executable bytes do not match the active game profile.");
        return false;
    }
    patches.Commit();
    Log("Installed frame-independent helicopter rotor acceleration.");
    return true;
}

bool InstallAiAircraftSteerFix() {
    if (!InstallJump(g_aiAircraftSteerPatch, kAiAircraftSteerRate,
                     &AiAircraftSteerRateThunk, kExpectedCameraRateClamp)) {
        Log("AI aircraft steering fix skipped: the autopilot bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real timestep in the AI aircraft steering rate.");
    return true;
}

bool InstallSkimmerResistanceFix() {
    if (!InstallJump(g_skimmerResistancePatch, kSkimmerResistancePatch,
                     &SkimmerResistanceThunk, kExpectedSkimmerResistance)) {
        Log("Skimmer resistance fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent skimmer water resistance.");
    return true;
}

} // namespace hff::vehicles
