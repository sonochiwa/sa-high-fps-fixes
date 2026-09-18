#include "modules/modules.h"

namespace hff {

bool HudFlashTestSiteMatches(uintptr_t operand, uint8_t mask) {
    if (!HudFlashSiteMatches(operand, kHudTestPrefix)) {
        return false;
    }
    __try {
        return *reinterpret_cast<const uint8_t*>(operand + sizeof(uintptr_t))
            == mask;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool RepointHudFlashOperand(RawPatch& patch, uintptr_t operand,
                            const volatile uint32_t* counter) {
    const auto address = reinterpret_cast<uintptr_t>(counter);
    return InstallRawPatch(patch, operand,
                           reinterpret_cast<const uint8_t*>(&address),
                           sizeof(address));
}

DWORD WINAPI HudFlashThread(void*) {
    const uint32_t tickMs = g_hudFlashIntervalMs / kHudTicksPerFlash;
    while (g_hudFlashActive) {
        // Advance off the game clock rather than wall time, so flashing freezes
        // while the game is paused, exactly as the frame counter does.
        __try {
            g_hudFlashClock =
                *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds)
                / tickMs;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        if (WorkerStopRequested(5)) {
            break;
        }
    }
    return 0;
}

} // namespace hff
