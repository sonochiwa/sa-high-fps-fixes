// ---------------------------------------------------------------------------
// HUD flashing
// ---------------------------------------------------------------------------

// Ported from the standalone HUD Flash Rate Fix.
//
// GTA flashes HUD elements by testing a bit of the frame counter:
//
//     if (CHud::m_ItemToFlash == ITEM && CTimer::m_FrameCounter & 8) -> skip
//
// The same check hides the health bar below 10 health. The period is measured
// in frames rather than in time, so with the frame limiter off the radar,
// flashed by the tutorial scripts, and the low health bar turn into a strobe.
//
// Each flash site reads a single byte through an absolute address operand, so
// no code is rewritten: those six operands are repointed at a plugin counter
// that advances in real time at 25 ticks per second. The game then sees the
// bit pattern it would have seen at 25 FPS, and every other use of
// `CTimer::m_FrameCounter` is left alone.

constexpr std::array<uint8_t, 2> kHudTestPrefix{0xF6, 0x05}; // test byte, imm8
constexpr std::array<uint8_t, 2> kHudMovPrefix{0x8A, 0x1D};  // mov bl, byte

template <size_t Size>
bool HudFlashSiteMatches(uintptr_t operand,
                         const std::array<uint8_t, Size>& prefix) {
    __try {
        if (std::memcmp(reinterpret_cast<const void*>(operand - prefix.size()),
                        prefix.data(), prefix.size()) != 0) {
            return false;
        }
        return *reinterpret_cast<const uintptr_t*>(operand) == kFrameCounter;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

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
        Sleep(5);
    }
    return 0;
}
