#include "player/bloody_footprints.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/player.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace hff::player {

namespace {

// CPed::PlayFootSteps stores the remaining bloody-footprint lifetime as an
// integer in m_nDeathTimeMS and subtracts one every time this path runs. At the
// original 30 Hz that produces the intended duration, but at a high frame rate
// all 200-300 ticks can disappear between two animation footsteps. Keep a
// fractional tick per ped so the field still changes at the original cadence.
// A gap in calls or an externally replaced counter resets the fraction, which
// also makes pool-slot reuse harmless.
struct BloodyFootprintTickState {
    uintptr_t ped{};
    uint32_t lastFrame{};
    uint32_t lastCounter{};
    float carry{};
};

struct BloodyFootprintHeightState {
    uintptr_t ped{};
    uint32_t leftTime{};
    float leftZ{};
};

SitePatch g_bloodyFootprintCounterPatch{};
SitePatch g_bloodyFootLandedSidePatch{};
SitePatch g_bloodyFootprintShadowPatch{};
std::array<BloodyFootprintTickState, 160> g_bloodyFootprintTickStates{};
std::array<BloodyFootprintHeightState, 160> g_bloodyFootprintHeightStates{};
uintptr_t g_currentBloodyFootprintPed{};
bool g_currentBloodyFootprintIsLeft{};

uint32_t __cdecl UpdateBloodyFootprintCounter(uintptr_t ped,
                                              uint32_t counter) {
    if (!ped || counter == 0) {
        return counter;
    }

    const uint32_t frame =
        *reinterpret_cast<volatile const uint32_t*>(kFrameCounter);
    BloodyFootprintTickState* state = nullptr;
    BloodyFootprintTickState* oldest = &g_bloodyFootprintTickStates[0];
    for (auto& candidate : g_bloodyFootprintTickStates) {
        if (candidate.ped == ped) {
            state = &candidate;
            break;
        }
        if (!candidate.ped) {
            oldest = &candidate;
            break;
        }
        if (frame - candidate.lastFrame > frame - oldest->lastFrame) {
            oldest = &candidate;
        }
    }
    if (!state) {
        state = oldest;
        *state = {};
        state->ped = ped;
    }

    const bool continuous = state->lastFrame != 0
                         && frame - state->lastFrame <= 2
                         && state->lastCounter == counter;
    if (!continuous) {
        state->carry = 0.0f;
    }
    state->lastFrame = frame;

    const float ratio = TimeStepRatio();
    uint32_t ticks = 1;
    if (std::isfinite(ratio) && ratio > 0.0f && ratio < 1.0f) {
        const float total = state->carry + ratio;
        ticks = static_cast<uint32_t>(total);
        state->carry = total - static_cast<float>(ticks);
    } else {
        state->carry = 0.0f;
    }

    const uint32_t result = counter > ticks ? counter - ticks : 0;
    state->lastCounter = result;
    return result;
}

void __cdecl SelectBloodyFootprintSide(uintptr_t ped, uint32_t leftFoot) {
    g_currentBloodyFootprintPed = ped;
    g_currentBloodyFootprintIsLeft = leftFoot != 0;
}

void __cdecl StabilizeBloodyFootprintHeight(float* position) {
    __try {
        if (!position || !g_currentBloodyFootprintPed) {
            return;
        }

        const uint32_t now = *reinterpret_cast<volatile const uint32_t*>(
            kTimerTimeInMilliseconds);
        BloodyFootprintHeightState* state = nullptr;
        BloodyFootprintHeightState* empty = nullptr;
        BloodyFootprintHeightState* oldest =
            &g_bloodyFootprintHeightStates.front();
        uint32_t oldestAge = now - oldest->leftTime;
        for (auto& candidate : g_bloodyFootprintHeightStates) {
            if (candidate.ped == g_currentBloodyFootprintPed) {
                state = &candidate;
                break;
            }
            if (!candidate.ped && !empty) {
                empty = &candidate;
            }
            const uint32_t candidateAge = now - candidate.leftTime;
            if (candidateAge > oldestAge) {
                oldest = &candidate;
                oldestAge = candidateAge;
            }
        }
        if (!state) {
            state = empty ? empty : oldest;
            *state = {};
            state->ped = g_currentBloodyFootprintPed;
        }

        if (g_currentBloodyFootprintIsLeft) {
            state->leftZ = position[2];
            state->leftTime = now;
            return;
        }

        const float ratio = TimeStepRatio();
        if (state->leftTime && now - state->leftTime <= 1000
            && std::isfinite(state->leftZ) && std::isfinite(ratio)
            && ratio > 0.0f && ratio < 1.0f) {
            position[2] = state->leftZ;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

// Entry contract: esi is CPed, ecx is the current countdown and edx contains
// the ped flags. The stock zero test after the replaced block clears the
// bloody-footprint flag when the scaled counter reaches zero.
__declspec(naked) void BloodyFootprintCounterThunk() {
    __asm {
        push eax
        push edx
        push ecx
        push esi
        call UpdateBloodyFootprintCounter
        add esp, 8
        mov ecx, eax
        pop edx
        pop eax
        mov dword ptr [esi + 0x750], ecx
        test ecx, ecx
        jmp kBloodyFootprintCounterReturn
    }
}

__declspec(naked) void BloodyFootLandedSideThunk() {
    __asm {
        pushfd
        pushad
        mov eax, dword ptr [esp + 0x28]
        push eax
        push ecx
        call SelectBloodyFootprintSide
        add esp, 8
        popad
        popfd
        jmp kDoFootLanded
    }
}

// Keep the game's cdecl AddPermanentShadow call intact. Only its stack-local
// position vector may be adjusted before tail-calling the original function.
__declspec(naked) void BloodyFootprintShadowThunk() {
    __asm {
        pushfd
        pushad
        mov eax, dword ptr [esp + 0x30]
        push eax
        call StabilizeBloodyFootprintHeight
        add esp, 4
        popad
        popfd
        jmp kAddPermanentShadow
    }
}

} // namespace

bool InstallBloodyFootprintsFix() {
    PatchSet patches("Bloody footprints fix");
    g_bloodyFootprintTickStates = {};
    g_bloodyFootprintHeightStates = {};
    if (!patches.Track(InstallJump(g_bloodyFootprintCounterPatch,
                                   kBloodyFootprintCounterPatch,
                                   &BloodyFootprintCounterThunk,
                                   kExpectedBloodyFootprintCounter),
                       g_bloodyFootprintCounterPatch)) {
        Log("Bloody footprints fix skipped: CPed::PlayFootSteps bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallCall(g_bloodyFootLandedSidePatch,
                                   kPlayFootStepsLandedCall,
                                   &BloodyFootLandedSideThunk,
                                   kExpectedPlayFootStepsLandedCall),
                       g_bloodyFootLandedSidePatch)
        || !patches.Track(InstallCall(g_bloodyFootprintShadowPatch,
                                      kBloodyFootprintShadowCall,
                                      &BloodyFootprintShadowThunk,
                                      kExpectedBloodyFootprintShadowCall),
                          g_bloodyFootprintShadowPatch)) {
        Log("Bloody footprints fix skipped: foot-side or shadow call bytes do not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed a real-time bloody-footprint countdown and right-foot projection stabilization.");
    return true;
}

} // namespace hff::player
