#include "weapons/chainsaw.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/weapons.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace hff::weapons {

namespace {

// The strike schedule of one chainsaw animation, so two peds sawing at once
// each keep their own.
struct ChainsawSchedule {
    void* anim;
    uint32_t lastCall;
    float credit;
};

SitePatch g_chainsawStrikePatch{};
float g_chainsawRewindOffset{kChainsawStockRewind};
std::array<ChainsawSchedule, 8> g_chainsawSchedules{};

// The schedule of `anim`, or the one unused for longest, which a new
// animation starts afresh.
ChainsawSchedule& ChainsawScheduleFor(void* anim, uint32_t now) {
    ChainsawSchedule* oldest = &g_chainsawSchedules.front();
    for (auto& schedule : g_chainsawSchedules) {
        if (schedule.anim == anim) {
            return schedule;
        }
        if (now - schedule.lastCall > now - oldest->lastCall) {
            oldest = &schedule;
        }
    }
    *oldest = {anim, now - kChainsawBurstGapMs - 1, 0.0f};
    return *oldest;
}

// Decides, on the frame the chainsaw's moving attack has run past `chain`,
// whether the animation is rewound behind `hit` so it strikes again, or parked
// on `hit` so it does not. Rewinding it further than the game does cannot work:
// `hit` is only 0.0333 s into the animation, so there is not enough animation
// in front of it to hold a whole strike period at a high frame rate. Parking is
// the half of the loop that costs nothing: the strike needs `currentTime` to
// cross `hit` from below, and a `currentTime` of exactly `hit` is not below it,
// so the next frames advance without striking and come back here.
//
// The schedule is kept on the millisecond clock rather than on frames, and the
// credit carries across frames, so a strike is armed every 66.7 ms whatever the
// frame rate. At 30 FPS and below every call arms, which is the stock 0.01
// rewind on every pass and therefore the stock behaviour exactly.
void __cdecl UpdateChainsawRewindOffset(void* anim) {
    g_chainsawRewindOffset = kChainsawStockRewind;
    if (!anim) {
        return;
    }

    __try {
        const uint32_t now = *reinterpret_cast<uint32_t*>(
            kTimerTimeInMilliseconds);
        ChainsawSchedule& schedule = ChainsawScheduleFor(anim, now);
        bool armed = true;
        if (now - schedule.lastCall > kChainsawBurstGapMs) {
            schedule.credit = 0.0f;
        } else {
            schedule.credit += static_cast<float>(now - schedule.lastCall);
            if (schedule.credit >= kChainsawStrikePeriodMs) {
                schedule.credit = std::min(
                    schedule.credit - kChainsawStrikePeriodMs,
                    kChainsawStrikePeriodMs);
            } else {
                armed = false;
            }
        }
        schedule.lastCall = now;

        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep >= kOriginalTimeStep) {
            armed = true;
        }

        // A negative rewind parks the animation just past `hit`. The margin is
        // there so that `currentTime - m_fTimeStep`, which is how the strike
        // test reconstructs the previous frame, cannot round back below `hit`
        // and fire a strike; it stays well inside the 0.0033 s that separates
        // `hit` from `chain`.
        g_chainsawRewindOffset = armed ? kChainsawStockRewind
                                       : -kChainsawParkMargin;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_chainsawRewindOffset = kChainsawStockRewind;
    }
}

// `st(0)` holds `m_fHit` for the moving attack and `ecx` the animation the
// rewind is about to be written to, which is also the `this` the
// `CAnimBlendAssociation::SetCurrentTime` call four bytes later expects, so it
// has to survive the helper.
__declspec(naked) void ChainsawStrikeRewindThunk() {
    __asm {
        pushfd
        push eax
        push edx
        push ecx
        push ecx
        call UpdateChainsawRewindOffset
        add esp, 4
        pop ecx
        pop edx
        pop eax
        popfd
        fsub g_chainsawRewindOffset
        ret
    }
}

} // namespace

bool InstallChainsawStrikeRateFix() {
    if (!InstallBranch(g_chainsawStrikePatch, kChainsawStrikeRewind,
                       &ChainsawStrikeRewindThunk,
                       kExpectedChainsawStrikeRewind.data(),
                       kExpectedChainsawStrikeRewind.size(), 0xE8)) {
        Log("Chainsaw strike rate fix skipped: CTaskSimpleFight::ProcessPed "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a frame-rate independent chainsaw strike rate.");
    return true;
}

} // namespace hff::weapons
