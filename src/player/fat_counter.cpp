#include "player/fat_counter.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/sites/player.h"

#include <cmath>
#include <cstdint>

namespace hff::player {

namespace {

SitePatch g_fatCounterPatch{};
float g_fatCounterCarry = 0.0f;

void __cdecl FatCounterAdd(uint32_t milliseconds, uint32_t rate) {
    const double product =
        static_cast<double>(static_cast<uint64_t>(milliseconds) * rate);
    const double value = product / 10.0 + static_cast<double>(g_fatCounterCarry);
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    const double whole = std::floor(value);
    g_fatCounterCarry = static_cast<float>(value - whole);
    if (whole >= 1.0) {
        *reinterpret_cast<uint32_t*>(kFatCounter) +=
            static_cast<uint32_t>(whole);
    }
}

// Replaces the twenty nine bytes of integer arithmetic. The original clobbered
// eax and edx and left the new total in eax, which the instruction at the
// return address overwrites straight away, so nothing has to be handed back.
// The exercise rate sat at `[esp+8]` before the call, which is `[esp+20]` once
// the return address and the two saved registers are on the stack.
__declspec(naked) void FatCounterThunk() {
    __asm {
        push ecx
        push edx
        mov ecx, dword ptr [esp + 0x14]
        push ecx
        push eax
        call FatCounterAdd
        add esp, 8
        pop edx
        pop ecx
        ret
    }
}

} // namespace

bool InstallFatCounterFix() {
    if (!InstallBranch(g_fatCounterPatch, kFatCounterMath, &FatCounterThunk,
                       kExpectedFatCounterMath.data(), 29, 0xE8)) {
        Log("Fat counter fix skipped: CStats::UpdateFatAndMuscleStats bytes do "
            "not match GTA SA 1.0 US.");
        return false;
    }
    g_fatCounterCarry = 0.0f;
    Log("Installed a fat counter that carries the discarded remainder.");
    return true;
}

} // namespace hff::player
