#include "handling/steer_input.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/handling.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace hff::handling {

namespace {

std::array<SitePatch, 4> g_steerInputPatches{};
std::array<SitePatch, 4> g_planeSteerInputPatches{};

// The fraction of the remaining steering error closed this frame. The game
// closes `k * timeStep` of it in one linear step; the exponential form keeps
// the same fraction per second at every frame rate and equals the stock step
// exactly at 30 FPS. Falls back to the stock step when the constant has been
// retuned outside the range where the exponential is defined.
float SteerInputGain(float k) {
    const float ratio = TimeStepRatio();
    const float base = 1.0f - k * kOriginalTimeStep;
    if (!(base > 0.0f) || base >= 1.0f) {
        return k * kOriginalTimeStep * ratio;
    }
    return 1.0f - std::pow(base, ratio);
}

float __cdecl GetCarSteerInputGain() {
    return SteerInputGain(ReadGameFloat(kCarSteerInputConstant, 0.2f));
}

float __cdecl GetBikeSteerInputGain() {
    return SteerInputGain(ReadGameFloat(kBikeSteerInputConstant, 0.2f));
}

float __cdecl GetPlaneSteerInputGain() {
    return SteerInputGain(ReadGameFloat(kPlaneSteerInputConstant, 0.2f));
}

// Each replaces the `fmul [esp+..] / fmul [0.2]` pair with `(target - raw)`
// alone on the x87 stack; the gain arrives on top of it and the product is
// left for the `fadd` / `fstp` at the return address.
__declspec(naked) void CarSteerInputAThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetCarSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kCarSteerInputAReturn
    }
}

__declspec(naked) void CarSteerInputBThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetCarSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kCarSteerInputBReturn
    }
}

__declspec(naked) void BikeSteerInputAThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetBikeSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kBikeSteerInputAReturn
    }
}

__declspec(naked) void BikeSteerInputBThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetBikeSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kBikeSteerInputBReturn
    }
}

__declspec(naked) void PlaneSteerInput0Thunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetPlaneSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kPlaneSteerInputReturn0
    }
}
__declspec(naked) void PlaneSteerInput1Thunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetPlaneSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kPlaneSteerInputReturn1
    }
}
__declspec(naked) void PlaneSteerInput2Thunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetPlaneSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kPlaneSteerInputReturn2
    }
}
__declspec(naked) void PlaneSteerInput3Thunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetPlaneSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kPlaneSteerInputReturn3
    }
}

// The plane's own transaction: a mod that changed its controls leaves planes
// with the stock step without taking the fix from cars and bikes.
void InstallPlaneSteerInputRate() {
    PatchSet patches("Plane steer input rate fix");
    const std::array<const void*, 4> thunks{
        &PlaneSteerInput0Thunk, &PlaneSteerInput1Thunk,
        &PlaneSteerInput2Thunk, &PlaneSteerInput3Thunk,
    };
    if (!InstallJumpTable(patches, g_planeSteerInputPatches, kPlaneSteerInput,
                          thunks, kExpectedPlaneSteerInput)) {
        Log("Steer input rate fix: CPlane::ProcessControlInputs bytes do not "
            "match GTA SA 1.0 US; planes keep the stock step.");
        return;
    }
    patches.Commit();
}
} // namespace

float PlaneSteerShare() {
    const float k = ReadGameFloat(kPlaneSteerInputConstant, 0.2f);
    if (g_planeSteerInputPatches[0].installed) {
        return SteerInputGain(k);
    }
    return k * *reinterpret_cast<const float*>(kTimerTimeStep);
}

bool InstallSteerInputRateFix() {
    PatchSet patches("Steer input rate fix");
    struct Site {
        SitePatch* patch;
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
        size_t size;
    };
    const Site sites[] = {
        {&g_steerInputPatches[0], kCarSteerInputA, &CarSteerInputAThunk,
         kExpectedCarSteerInputA.data(), kExpectedCarSteerInputA.size()},
        {&g_steerInputPatches[1], kCarSteerInputB, &CarSteerInputBThunk,
         kExpectedCarSteerInputB.data(), kExpectedCarSteerInputB.size()},
        {&g_steerInputPatches[2], kBikeSteerInputA, &BikeSteerInputAThunk,
         kExpectedBikeSteerInput.data(), kExpectedBikeSteerInput.size()},
        {&g_steerInputPatches[3], kBikeSteerInputB, &BikeSteerInputBThunk,
         kExpectedBikeSteerInput.data(), kExpectedBikeSteerInput.size()},
    };
    for (const Site& site : sites) {
        if (!patches.Track(InstallBranch(*site.patch, site.address, site.thunk,
                                         site.expected, site.size, 0xE9),
                           *site.patch)) {
            Log("Steer input rate fix skipped: ProcessControlInputs bytes do "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    InstallPlaneSteerInputRate();
    Log("Installed an exponential car, bike and plane steering input step.");
    return true;
}

} // namespace hff::handling
