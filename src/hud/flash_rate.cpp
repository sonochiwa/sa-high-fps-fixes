#include "hud/flash_rate.h"

#include "core/config.h"
#include "core/log.h"
#include "core/module.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/hud.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstring>

// The HUD flashes by testing a bit of the frame counter, so with the frame
// limiter off the radar, flashed by the tutorial scripts, and the low health
// bar turn into a strobe. Each flash site reads a single byte through an
// absolute address operand, so no code is rewritten: those six operands are
// repointed at a plugin counter that advances in real time at 25 ticks per
// second. The game then sees the bit pattern it would have seen at 25 FPS,
// and every other use of `CTimer::m_FrameCounter` is left alone.

namespace hff::hud {

namespace {

// Only the low byte is ever read by the patched instructions, but the counters
// are full dwords so that a dword read would also see a sane value.
volatile uint32_t g_hudFlashClock{};
volatile uint32_t g_hudVisibleClock{};
std::array<RawPatch, 6> g_hudFlashPatches{};

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
    constexpr uint32_t tickMs = kDefaultHudFlashIntervalMs / kHudTicksPerFlash;
    do {
        // Advance off the game clock rather than wall time, so flashing freezes
        // while the game is paused, exactly as the frame counter does.
        __try {
            g_hudFlashClock =
                *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds)
                / tickMs;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    } while (!WorkerStopRequested(5));
    return 0;
}

} // namespace

bool InstallHudFlashRateFix() {
    PatchSet patches("HUD flash rate fix");
    if (!HudFlashTestSiteMatches(kHudArmorBarOperand, 8)
        || !HudFlashTestSiteMatches(kHudBreathBarOperand, 8)
        || !HudFlashSiteMatches(kHudHealthBarOperand, kHudMovPrefix)
        || !HudFlashTestSiteMatches(kHudRadarOperand, 8)
        || !HudFlashTestSiteMatches(kHudWantedActiveOperand, 4)
        || !HudFlashTestSiteMatches(kHudWantedEmptyOperand, 4)) {
        Log("HUD flash rate fix skipped: flash sites do not match GTA SA 1.0 US.");
        return false;
    }

    const bool disableFlashing = ReadSetting("hud", "disableFlashing", false);

    // Seed the clock before the game can read it, then redirect the reads.
    g_hudFlashClock = 0;
    constexpr std::array<uintptr_t, 6> operands{
        kHudArmorBarOperand, kHudBreathBarOperand, kHudHealthBarOperand,
        kHudRadarOperand, kHudWantedActiveOperand, kHudWantedEmptyOperand
    };
    const std::array<const volatile uint32_t*, 6> counters{
        &g_hudFlashClock,
        &g_hudFlashClock,
        disableFlashing ? &g_hudVisibleClock : &g_hudFlashClock,
        disableFlashing ? &g_hudVisibleClock : &g_hudFlashClock,
        &g_hudFlashClock,
        &g_hudFlashClock,
    };
    for (size_t i = 0; i < operands.size(); ++i) {
        if (!patches.Track(
                RepointHudFlashOperand(g_hudFlashPatches[i], operands[i],
                                       counters[i]),
                g_hudFlashPatches[i])) {
            Log("HUD flash rate fix failed while redirecting HUD clocks.");
            return false;
        }
    }

    if (!StartWorkerThread(HudFlashThread)) {
        Log("HUD flash rate fix failed to start its worker thread.");
        return false;
    }
    patches.Commit();

    Log(disableFlashing
            ? "Installed the HUD flash rate fix with radar and health flashing disabled."
            : "Installed a real-time HUD flash clock at the original 25 FPS rate.");
    return true;
}

} // namespace hff::hud
