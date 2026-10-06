#include "vehicles/siren.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/vehicles.h"

#include <windows.h>

#include <array>
#include <cstdint>

namespace hff::vehicles {

namespace {

struct HornTapState {
    uint32_t pressLastTime{};
    bool hasPressed{};
};

using PadStateFn = bool(__thiscall*)(void*);

SitePatch g_sirenPatch{};
std::array<HornTapState, 2> g_hornTapStates{};

void* PadAt(int index) {
    return reinterpret_cast<void*>(
        kPads + static_cast<uintptr_t>(index) * kPadSize);
}

// CVehicle::ProcessSirenAndHorn separates a horn tap from a hold using a
// per-frame history buffer, so a tap covers fewer real milliseconds as the
// frame rate rises. This replaces the buffer with a wall-clock threshold.
//
// Only local players own a CPad. In particular, a SA-MP remote driver is not
// the second local player: treating every driver other than Players[0] as pad
// 1 lets a nearby network vehicle observe the idle second pad, consume player
// 0's shared tap state and make the siren impossible to toggle. Match both
// local player slots explicitly and keep independent state for split-screen.
// All other vehicles continue through the original code: forcing its no-horn
// branch would erase the counter that SA-MP synchronizes for remote sirens,
// leaving the lights active while suppressing their sound.
uintptr_t __cdecl SelectSirenReturnAddress(uintptr_t vehicle) {
    __try {
        void* driver = *reinterpret_cast<void**>(vehicle + kVehicleDriverOffset);
        int playerIndex = -1;
        for (int i = 0; i < 2; ++i) {
            void* player = *reinterpret_cast<void**>(
                kWorldPlayers + static_cast<uintptr_t>(i) * kPlayerInfoSize);
            if (player && driver == player) {
                playerIndex = i;
                break;
            }
        }
        if (playerIndex < 0) {
            return kSirenOriginalReturn;
        }

        void* pad = PadAt(playerIndex);
        HornTapState& state = g_hornTapStates[playerIndex];

        const uint32_t now = *reinterpret_cast<uint32_t*>(
            kTimerTimeInMilliseconds);
        if (reinterpret_cast<PadStateFn>(kPadHornJustDown)(pad)) {
            state.pressLastTime = now;
            state.hasPressed = true;
        }
        const bool horn = reinterpret_cast<PadStateFn>(kPadGetHorn)(pad);

        if (horn && now - state.pressLastTime >= kSirenTapMilliseconds) {
            return kSirenHornReturn;
        }
        if (!horn && state.hasPressed) {
            state.hasPressed = false;
            if (now - state.pressLastTime < kSirenTapMilliseconds) {
                return kSirenToggleReturn;
            }
        }
        return kSirenNoHornReturn;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return kSirenOriginalReturn;
    }
}

__declspec(naked) void SirenTapThunk() {
    __asm {
        pushfd
        pushad
        push esi
        call SelectSirenReturnAddress
        add esp, 4
        mov dword ptr [esp + 28], eax
        popad
        popfd
        jmp eax
    }
}

} // namespace

bool InstallSirenTapFix() {
    if (!MemoryMatches(kSirenAnchor, kExpectedSirenAnchor)) {
        Log("Siren tap fix skipped: ProcessSirenAndHorn layout does not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallJump(g_sirenPatch, kSirenPatch, &SirenTapThunk,
                     kExpectedSiren)) {
        Log("Siren tap fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed wall-clock siren tap detection.");
    return true;
}

} // namespace hff::vehicles
