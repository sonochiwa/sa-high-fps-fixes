#include "hud/money_counter.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/hud.h"

#include <windows.h>

#include <cmath>
#include <cstdint>

namespace hff::hud {

namespace {

SitePatch g_moneyStepPatch{};

// The step is scaled into this frame and the fraction is carried, so the
// counter covers the same ground per second as it does at 30 FPS. The carry is
// dropped when the direction reverses, otherwise a leftover from counting up
// would eat into the first step of counting down.
float g_moneyStepCarry{};

void __cdecl ApplyMoneyStep(int32_t* field, int32_t proposed) {
    __try {
        if (!field) {
            return;
        }
        const int32_t current = *field;
        const int32_t step = proposed - current;
        if (step == 0) {
            return;
        }
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f
            || timeStep >= kOriginalTimeStep) {
            *field = proposed;
            g_moneyStepCarry = 0.0f;
            return;
        }
        if ((step < 0) != (g_moneyStepCarry < 0.0f)) {
            g_moneyStepCarry = 0.0f;
        }
        const float scaled = static_cast<float>(step)
                                 * (timeStep / kOriginalTimeStep)
                             + g_moneyStepCarry;
        const float whole = std::trunc(scaled);
        g_moneyStepCarry = scaled - whole;
        *field = current + static_cast<int32_t>(whole);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *field = proposed;
    }
}

// Replaces the store of the stepped value. `edx` carries what the original
// would have written and `esi` the player info, whose old value is still in
// place, so the helper can recover the step the game chose.
__declspec(naked) void MoneyStepThunk() {
    __asm {
        pushfd
        pushad
        push edx
        lea eax, [esi + 0xBC]
        push eax
        call ApplyMoneyStep
        add esp, 8
        popad
        popfd
        jmp kMoneyStepReturn
    }
}

} // namespace

bool InstallMoneyCounterFix() {
    if (!InstallJump(g_moneyStepPatch, kMoneyStepStore, &MoneyStepThunk,
                     kExpectedMoneyStepStore)) {
        Log("Money counter fix skipped: CPlayerInfo::Process bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real-time HUD money counter.");
    return true;
}

} // namespace hff::hud
