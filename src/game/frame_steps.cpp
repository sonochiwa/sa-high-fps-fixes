#include "game/frame_steps.h"

#include "core/timestep.h"
#include "game/addresses.h"

namespace hff {

// The constant scaled into the current frame, which is an identity at 30 FPS.
__declspec(naked) void FrameStepDecrementThunk() {
    __asm {
        fld dword ptr ds:[0x00858B1C]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fsubp st(1), st
        ret
    }
}

__declspec(naked) void FrameStepIncrementThunk() {
    __asm {
        fld dword ptr ds:[0x00858C28]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        faddp st(1), st
        ret
    }
}

// Damping is a ratio applied per frame, so it takes the exponent rather than
// the product, the same shape the game itself uses for the chassis door.
// `_CIpow` writes eax, ecx and edx, which some callers still hold live values
// in, such as the status byte the boat reads after its coast down, so they
// and the flags are kept across it.
__declspec(naked) void FrameStepDecayThunk() {
    __asm {
        fld dword ptr ds:[0x00858EF0]
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
        fmulp st(1), st
        ret
    }
}

} // namespace hff
