#include "vehicles/wheels.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/frame_steps.h"
#include "game/sites/wheels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace hff::vehicles {

namespace {

std::array<SitePatch, 5> g_wheelFrictionPatches{};
std::array<SitePatch, 4> g_railWheelSpinPatches{};
SitePatch g_burnoutPatch{};
std::array<SitePatch, 6> g_wheelSpinPatches{};
std::array<SitePatch, 6> g_wheelSettlePatches{};

float __cdecl GetFrameIndependentWheelFriction() {
    return ReadGameFloat(kWheelFriction, 0.9f) * TimeStepRatio();
}

float __cdecl GetBurnoutWheelSpeed() {
    return ReadGameFloat(kBurnoutConstant, 3000.0f) * TimeStepRatio();
}

// A lerp weight, so what has to hold across frames is the fraction of the gap
// left over: `1 - weight` per original frame becomes `(1 - weight)` raised to
// the timestep ratio per rendered frame. The ratio is capped at one so 30 FPS
// and below keep the stock weight exactly.
float __cdecl GetWheelSettleWeight() {
    const float weight = ReadGameFloat(kWheelSettleConstant, 0.75f);
    const float ratio = std::min(TimeStepRatio(), 1.0f);
    if (!(weight > 0.0f) || weight >= 1.0f || ratio >= 1.0f) {
        return weight;
    }
    return 1.0f - std::pow(1.0f - weight, ratio);
}

__declspec(naked) void WheelFrictionCarDriveThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionCarDriveReturn
    }
}

__declspec(naked) void WheelFrictionCarBrakeThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionCarBrakeReturn
    }
}

__declspec(naked) void WheelFrictionBikeBaseThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionBikeBaseReturn
    }
}

__declspec(naked) void WheelFrictionBikeDriveThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionBikeDriveReturn
    }
}

__declspec(naked) void WheelFrictionBikeBrakeThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionBikeBrakeReturn
    }
}

// Wheels of a simple (on-rails) AI car turn by `-dot(forward, moveSpeed) / radius`
// once per frame with no timestep. The physics wheels integrate the same kind of
// speed as `GetTimeStep() * m_wheelSpeed`, so the raw timestep is applied here
// too, exactly as FramerateVigilante does, rather than the ratio: at 30 FPS the
// stock rotation is 1.67 times too slow next to a physics car and this brings
// the two into step at every frame rate.
__declspec(naked) void RailWheelSpinThunk0() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fadd dword ptr [esi + 0x828]
        jmp kRailWheelSpinReturn0
    }
}

__declspec(naked) void RailWheelSpinThunk1() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fadd dword ptr [esi + 0x82C]
        jmp kRailWheelSpinReturn1
    }
}

__declspec(naked) void RailWheelSpinThunk2() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fadd dword ptr [esi + 0x830]
        jmp kRailWheelSpinReturn2
    }
}

__declspec(naked) void RailWheelSpinThunk3() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fadd dword ptr [esi + 0x834]
        jmp kRailWheelSpinReturn3
    }
}

__declspec(naked) void BurnoutThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetBurnoutWheelSpeed
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kBurnoutReturn
    }
}

// The gap between the wheel's drawn position and where the suspension wants it
// is already in st(0).
__declspec(naked) void WheelSettleThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetWheelSettleWeight
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        ret
    }
}

} // namespace

bool InstallWheelFrictionFix() {
    PatchSet patches("Wheel friction fix");
    const std::array<const void*, 5> thunks{
        &WheelFrictionCarDriveThunk,
        &WheelFrictionCarBrakeThunk,
        &WheelFrictionBikeBaseThunk,
        &WheelFrictionBikeDriveThunk,
        &WheelFrictionBikeBrakeThunk,
    };

    if (!InstallJumpTable(patches, g_wheelFrictionPatches, kWheelFrictionSites,
                          thunks, kExpectedWheelFriction)) {
        Log("Wheel friction fix skipped: executable bytes do not match the active game profile.");
        return false;
    }
    patches.Commit();
    Log("Installed timestep-scaled car and bike wheel friction.");
    return true;
}

bool InstallRailWheelSpinFix() {
    PatchSet patches("Rail wheel spin fix");
    const std::array<const void*, 4> thunks{
        &RailWheelSpinThunk0,
        &RailWheelSpinThunk1,
        &RailWheelSpinThunk2,
        &RailWheelSpinThunk3,
    };

    if (!InstallJumpTable(patches, g_railWheelSpinPatches, kRailWheelSpinSites,
                          thunks, kExpectedRailWheelSpin)) {
        Log("Rail wheel spin fix skipped: executable bytes do not match the active game profile.");
        return false;
    }
    patches.Commit();
    Log("Installed frame-independent on-rails wheel rotation.");
    return true;
}

bool InstallBurnoutFix() {
    if (!InstallJump(g_burnoutPatch, kBurnoutPatch, &BurnoutThunk,
                     kExpectedBurnout)) {
        Log("Burnout fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent burnout wheel speed.");
    return true;
}

bool InstallWheelSpinFix() {
    PatchSet patches("Free wheel spin fix");
    struct Site {
        SitePatch* patch;
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {&g_wheelSpinPatches[0], kWheelSpinDecelLeft, &FrameStepDecrementThunk,
         kExpectedFrameStepDecrement.data()},
        {&g_wheelSpinPatches[1], kWheelSpinDecelRight, &FrameStepDecrementThunk,
         kExpectedFrameStepDecrement.data()},
        {&g_wheelSpinPatches[2], kWheelSpinAccelLeft, &FrameStepIncrementThunk,
         kExpectedFrameStepIncrement.data()},
        {&g_wheelSpinPatches[3], kWheelSpinAccelRight, &FrameStepIncrementThunk,
         kExpectedFrameStepIncrement.data()},
        {&g_wheelSpinPatches[4], kWheelSpinDampLeft, &FrameStepDecayThunk,
         kExpectedFrameStepDecay.data()},
        {&g_wheelSpinPatches[5], kWheelSpinDampRight, &FrameStepDecayThunk,
         kExpectedFrameStepDecay.data()}
    };

    for (const auto& site : sites) {
        if (!patches.Track(
                InstallBranch(*site.patch, site.address, site.thunk,
                              site.expected, 6, 0xE8),
                *site.patch)) {
            Log("Free wheel spin fix skipped: CAutomobile::ProcessCarWheelPair "
                "bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled free wheel spin rate.");
    return true;
}

bool InstallWheelSettleFix() {
    PatchSet patches("Wheel settle fix");
    const uintptr_t sites[] = {
        kWheelSettleBikeA, kWheelSettleBikeB,
        kWheelSettleBmxA, kWheelSettleBmxB,
        kWheelSettleHeli, kWheelSettlePlane,
    };
    for (size_t i = 0; i < g_wheelSettlePatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_wheelSettlePatches[i], sites[i],
                              &WheelSettleThunk, kExpectedWheelSettle.data(), 6,
                              0xE8),
                g_wheelSettlePatches[i])) {
            Log("Wheel settle fix skipped: bike or aircraft PreRender bytes do not "
                "match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a real-time settle for drawn bike and aircraft wheels; "
        "automobile wheel travel remains stock.");
    return true;
}

} // namespace hff::vehicles
