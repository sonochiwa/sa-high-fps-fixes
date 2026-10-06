#include "vehicles/aircraft.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/handling.h"
#include "game/sites/vehicles.h"
#include "game/sites/water.h"
#include "handling/steer_input.h"
#include "timers/frame_time_carry.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace hff::vehicles {

namespace {

std::array<SitePatch, 2> g_heliRotorPatches{};
SitePatch g_aiAircraftSteerPatch{};
SitePatch g_skimmerResistancePatch{};
std::array<SitePatch, 3> g_planeDamageDecayPatches{};
SitePatch g_swatRopePatch{};

using PartStatusFn = uint8_t(__thiscall*)(void*, uint8_t);

struct PlaneAxis {
    const uint32_t* parts;
    size_t count;
};

constexpr PlaneAxis kPlaneAxes[] = {
    {kPlaneSkidParts, std::size(kPlaneSkidParts)},
    {kPlanePitchParts, std::size(kPlanePitchParts)},
    {kPlaneRollParts, std::size(kPlaneRollParts)},
};

// The share of an axis's control input that its damaged parts keep together
// in one frame, and how many of them there are.
float AxisKeptShare(uintptr_t plane, const PlaneAxis& axis, float step, size_t& damaged) {
    float kept = 1.0f;
    damaged = 0;
    for (size_t i = 0; i < axis.count; ++i) {
        const uint32_t part = axis.parts[i];
        if (!*reinterpret_cast<const uintptr_t*>(plane + kAutomobileNodes + part * 4)) {
            continue;
        }
        const uint8_t status = reinterpret_cast<PartStatusFn>(kGetAeroplaneCompStatus)(
            reinterpret_cast<void*>(plane + kAutomobileDamageManager),
            static_cast<uint8_t>(part));
        if (status != 0) {
            kept *= 1.0f - step * status;
            ++damaged;
        }
    }
    return kept;
}

// At 30 FPS the input first closes the share S of its gap to the stick, then
// keeps the share F, and settles at S F / (1 - F (1 - S)) of the stick. Above
// 30 FPS it closes a smaller share s each frame, and keeping
// g = settled / (s + settled (1 - s)) settles it at the same level, which
// scaling either step on its own does not. Each damaged part of the axis keeps
// an equal root of g, so the frame keeps g in all.
float __cdecl PlaneDamageDecay(float share, uintptr_t plane, int32_t axis) {
    __try {
        const float ratio = TimeStepRatio();
        if (!(ratio > 0.0f && ratio < 1.0f) || axis < 0
            || static_cast<size_t>(axis) >= std::size(kPlaneAxes)) {
            return share;
        }
        const float step = ReadGameFloat(kPlaneSteerInputConstant, 0.2f);
        size_t damaged = 0;
        const float kept = AxisKeptShare(plane, kPlaneAxes[axis], step, damaged);
        const float closed = step * kOriginalTimeStep;
        const float frameClosed = handling::PlaneSteerShare();
        if (damaged == 0 || !(kept > 0.0f && kept < 1.0f)
            || !(closed > 0.0f && closed < 1.0f)
            || !(frameClosed > 0.0f && frameClosed < 1.0f)) {
            return share;
        }
        const float settled = kept * closed / (1.0f - kept * (1.0f - closed));
        const float frameKept = settled / (frameClosed + settled * (1.0f - frameClosed));
        return std::pow(frameKept, 1.0f / static_cast<float>(damaged));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return share;
    }
}

// Each replaces `fmul [esi+field]` with the share on the x87 stack, and passes
// the plane and the axis along. The registers and flags are kept, as the call
// that computes the share may change them.
__declspec(naked) void PlaneDamageSkidDecayThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        push 0
        push esi
        sub esp, 4
        fstp dword ptr [esp]
        call PlaneDamageDecay
        add esp, 12
        pop edx
        pop ecx
        pop eax
        popfd
        fmul dword ptr [esi+0x988]
        ret
    }
}

__declspec(naked) void PlaneDamagePitchDecayThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        push 1
        push esi
        sub esp, 4
        fstp dword ptr [esp]
        call PlaneDamageDecay
        add esp, 12
        pop edx
        pop ecx
        pop eax
        popfd
        fmul dword ptr [esi+0x98C]
        ret
    }
}

__declspec(naked) void PlaneDamageRollDecayThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        push 2
        push esi
        sub esp, 4
        fstp dword ptr [esp]
        call PlaneDamageDecay
        add esp, 12
        pop edx
        pop ecx
        pop eax
        popfd
        fmul dword ptr [esi+0x990]
        ret
    }
}

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

// Counts a rope down only on the frames on which an original 30 FPS frame has
// passed, then takes the instruction the call displaced, reading 4 bytes
// further up the stack past the return address. `FrameTick` may change eax,
// ecx and edx, and al still holds the rope's state.
__declspec(naked) void SwatRopeCountdownThunk() {
    __asm {
        push ecx
        push edx
        push eax
        push kFrameTickSwatRopes
        call FrameTick
        add esp, 4
        test eax, eax
        pop eax
        je held
        dec al
        mov byte ptr [edi], al
    held:
        pop edx
        pop ecx
        mov eax, dword ptr [esp+0x1C]
        ret
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

bool InstallDamagedPlaneControlFix() {
    PatchSet patches("Damaged plane control fix");
    if (!patches.Track(InstallCall(g_planeDamageDecayPatches[0], kPlaneDamageSkidDecay,
                                   &PlaneDamageSkidDecayThunk,
                                   kExpectedPlaneDamageSkidDecay),
                       g_planeDamageDecayPatches[0])
        || !patches.Track(InstallCall(g_planeDamageDecayPatches[1], kPlaneDamagePitchDecay,
                                      &PlaneDamagePitchDecayThunk,
                                      kExpectedPlaneDamagePitchDecay),
                          g_planeDamageDecayPatches[1])
        || !patches.Track(InstallCall(g_planeDamageDecayPatches[2], kPlaneDamageRollDecay,
                                      &PlaneDamageRollDecayThunk,
                                      kExpectedPlaneDamageRollDecay),
                          g_planeDamageDecayPatches[2])) {
        Log("Damaged plane control fix skipped: CPlane::ProcessFlyingCarStuff bytes "
            "do not match GTA SA 1.0 US.");
        return false;
    }
    if (!timers::InstallPlaneDamageWaveCarry()) {
        return false;
    }
    patches.Commit();
    Log("Installed a damaged plane's loss of control at the original rate.");
    return true;
}

bool InstallSwatRopesFix() {
    if (!InstallCall(g_swatRopePatch, kSwatRopeCountdown, &SwatRopeCountdownThunk,
                     kExpectedSwatRopeCountdown)) {
        Log("SWAT rope fix skipped: CHeli::ProcessControl bytes do not match GTA SA "
            "1.0 US.");
        return false;
    }
    Log("Installed SWAT helicopter ropes at the original rate.");
    return true;
}

} // namespace hff::vehicles
