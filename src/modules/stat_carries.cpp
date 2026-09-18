#include "modules/modules.h"

namespace hff {

EmissionCarrySlot* FindEmissionCarrySlot(void* blueprint) {
    EmissionCarrySlot* empty{};
    for (auto& slot : g_weaponFxEmissionCarry) {
        if (slot.blueprint == blueprint) {
            return &slot;
        }
        if (!slot.blueprint && !empty) {
            empty = &slot;
        }
    }
    if (empty) {
        empty->blueprint = blueprint;
    }
    return empty;
}

AmmoConsumptionSlot& FindAmmoConsumptionSlot(void* weapon) {
    AmmoConsumptionSlot* oldest = &g_ammoConsumptionSlots.front();
    for (auto& slot : g_ammoConsumptionSlots) {
        if (slot.weapon == weapon) {
            return slot;
        }
        if (!slot.weapon) {
            return slot;
        }
        if (static_cast<int32_t>(slot.lastUpdate - oldest->lastUpdate) < 0) {
            oldest = &slot;
        }
    }
    return *oldest;
}

// The fraction the truncation would have thrown away, carried into the next
// frame. At 30 FPS the damage is exactly five per frame and the carry stays at
// zero, so a capped run is bit for bit what it was. There is one accumulator
// because `HandlePlayerBreath` is a `CPlayerPed` method and single player has
// one of those.
float g_drowningDamageCarry{};

int32_t __cdecl AccumulateDrowningDamage(float damage) {
    __try {
        if (!std::isfinite(damage) || damage <= 0.0f) {
            g_drowningDamageCarry = 0.0f;
            return 0;
        }
        g_drowningDamageCarry += damage;
        const float whole = std::floor(g_drowningDamageCarry);
        g_drowningDamageCarry -= whole;
        return static_cast<int32_t>(whole);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int32_t>(damage);
    }
}

// One carry per call site, indexed the same as `kStatTruncSites`. The lookup is
// a linear scan over twenty one entries and runs at most a handful of times a
// frame, so it costs nothing worth measuring.
std::array<float, kStatTruncSites.size()> g_statTruncCarries{};

bool g_traceCycleSkill{};
uint64_t g_cycleTraceLast{};
uint32_t g_cycleTraceCalls{};
double g_cycleTraceRaw{};
int64_t g_cycleTraceAdded{};

void TraceCycleSkill(int32_t site, double raw, int32_t added) {
    if (!g_traceCycleSkill) {
        return;
    }
    if (site == 0) {
        ++g_cycleTraceCalls;
    } else {
        g_cycleTraceRaw += raw;
        g_cycleTraceAdded += added;
    }

    const uint64_t now = GetTickCount64();
    if (g_cycleTraceLast == 0) {
        g_cycleTraceLast = now;
        return;
    }
    if (now - g_cycleTraceLast < 1000) {
        return;
    }
    g_cycleTraceLast = now;

    const float timeStep = ReadGameFloat(kTimerTimeStep, kOriginalTimeStep);
    const float limit = ReadGameFloat(kCycleSkillLimit, 0.0f);
    char line[256];
    std::snprintf(line, sizeof(line),
                  "cycle: fps~%.0f ts=%.4f calls=%u raw=%.1f added=%lld "
                  "skill=%u stamina=%u limit=%.0f",
                  timeStep > 0.0f ? 50.0f / timeStep : 0.0f, timeStep,
                  g_cycleTraceCalls, g_cycleTraceRaw,
                  static_cast<long long>(g_cycleTraceAdded),
                  *reinterpret_cast<volatile uint32_t*>(kCycleSkillCounter),
                  *reinterpret_cast<volatile uint32_t*>(kCycleStaminaCounter),
                  limit * 1000.0f);
    Log(line);

    g_cycleTraceCalls = 0;
    g_cycleTraceRaw = 0.0;
    g_cycleTraceAdded = 0;
}

int32_t __cdecl TruncateStatWithCarry(double value, uintptr_t site) {
    __try {
        size_t index = kStatTruncSites.size();
        for (size_t i = 0; i < kStatTruncSites.size(); ++i) {
            // The return address the wrapper sees is the instruction after the
            // call, and the table holds the address of the call itself.
            if (kStatTruncSites[i].address + 5 == site) {
                index = i;
                break;
            }
        }
        if (index == kStatTruncSites.size() || !std::isfinite(value)) {
            return static_cast<int32_t>(value);
        }

        const double total = value + g_statTruncCarries[index];
        const double whole = std::trunc(total);
        const double remainder = total - whole;
        g_statTruncCarries[index] = std::isfinite(remainder)
                                  ? static_cast<float>(remainder)
                                  : 0.0f;
        const int32_t result = static_cast<int32_t>(whole);
        if (g_traceCycleSkill) {
            if (kStatTruncSites[index].address == 0x0055C94B) {
                TraceCycleSkill(0, value, result);
            } else if (kStatTruncSites[index].address == 0x0055C972) {
                TraceCycleSkill(1, value, result);
            }
        }
        return result;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int32_t>(value);
    }
}

int32_t __cdecl ShouldConsumeContinuousWeaponAmmo(uintptr_t weapon) {
    if (!weapon) {
        return true;
    }

    __try {
        const int32_t weaponType = *reinterpret_cast<int32_t*>(weapon);
        if (!IsContinuousWeapon(weaponType)) {
            return true;
        }

        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep >= kOriginalTimeStep) {
            return true;
        }

        const uint32_t now = *reinterpret_cast<uint32_t*>(
            kTimerTimeInMilliseconds);
        auto& slot = FindAmmoConsumptionSlot(reinterpret_cast<void*>(weapon));
        if (slot.weapon != reinterpret_cast<void*>(weapon)
            || slot.weaponType != weaponType) {
            slot = {reinterpret_cast<void*>(weapon), weaponType, now, 0.0f};
            return true;
        }

        const uint32_t elapsed = now - slot.lastUpdate;
        slot.lastUpdate = now;
        constexpr uint32_t kNewBurstThresholdMs = 200;
        if (elapsed > kNewBurstThresholdMs) {
            slot.credit = 0.0f;
            return true;
        }

        slot.credit += static_cast<float>(elapsed)
                     * (kOriginalWeaponConsumptionRate / 1000.0f);
        if (slot.credit < 1.0f) {
            return false;
        }

        slot.credit -= std::floor(slot.credit);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
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
void __cdecl UpdateChainsawRewindOffset(void* anim, void* task) {
    g_chainsawRewindOffset = kChainsawStockRewind;
    if (!anim) {
        return;
    }

    __try {
        ++g_chainsawCalls;
        if (task) {
            g_chainsawCombo = *reinterpret_cast<const int8_t*>(
                reinterpret_cast<uintptr_t>(task) + 0x24);
            g_chainsawMove = *reinterpret_cast<const int8_t*>(
                reinterpret_cast<uintptr_t>(task) + 0x25);
        }
        g_chainsawAnimStep = *reinterpret_cast<const float*>(
            reinterpret_cast<uintptr_t>(anim) + 0x28);
        g_chainsawAnimTime = *reinterpret_cast<const float*>(
            reinterpret_cast<uintptr_t>(anim) + 0x20);

        const uint32_t now = *reinterpret_cast<uint32_t*>(
            kTimerTimeInMilliseconds);
        bool armed = true;
        if (anim != g_chainsawAnim
            || now - g_chainsawLastCall > kChainsawBurstGapMs) {
            g_chainsawAnim = anim;
            g_chainsawCredit = 0.0f;
        } else {
            g_chainsawCredit += static_cast<float>(now - g_chainsawLastCall);
            if (g_chainsawCredit >= kChainsawStrikePeriodMs) {
                g_chainsawCredit = std::min(
                    g_chainsawCredit - kChainsawStrikePeriodMs,
                    kChainsawStrikePeriodMs);
            } else {
                armed = false;
            }
        }
        g_chainsawLastCall = now;

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
        if (armed) {
            ++g_chainsawArms;
        }
        TraceChainsaw();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_chainsawRewindOffset = kChainsawStockRewind;
    }
}

void __cdecl RecordFightStrike(void* task) {
    ++g_chainsawStrikes;
    if (task) {
        __try {
            g_chainsawStrikeCombo = *reinterpret_cast<const int8_t*>(
                reinterpret_cast<uintptr_t>(task) + 0x24);
            g_chainsawStrikeMove = *reinterpret_cast<const int8_t*>(
                reinterpret_cast<uintptr_t>(task) + 0x25);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    TraceChainsaw();
}

// One line a second while `traceChainsaw` is on: how often the rewind site
// runs, how many of those passes armed a strike, how many strikes actually
// reached `FightStrike`, and which combo and move the task is in. A chainsaw
// combo is 12 and its moving attack is move 4.
void TraceChainsaw() {
    if (!g_traceChainsaw) {
        return;
    }
    const uint64_t now = GetTickCount64();
    if (g_chainsawTraceLast == 0) {
        g_chainsawTraceLast = now;
        return;
    }
    if (now - g_chainsawTraceLast < 1000) {
        return;
    }
    const double seconds = static_cast<double>(now - g_chainsawTraceLast)
                         / 1000.0;
    g_chainsawTraceLast = now;

    const float timeStep = ReadGameFloat(kTimerTimeStep, kOriginalTimeStep);
    char line[256];
    std::snprintf(line, sizeof(line),
                  "chainsaw: fps~%.0f rewinds/s=%.1f arms/s=%.1f strikes/s=%.1f "
                  "combo=%d move=%d strikeCombo=%d strikeMove=%d "
                  "animStep=%.4f animTime=%.4f",
                  timeStep > 0.0f ? 50.0f / timeStep : 0.0f,
                  g_chainsawCalls / seconds, g_chainsawArms / seconds,
                  g_chainsawStrikes / seconds, g_chainsawCombo, g_chainsawMove,
                  g_chainsawStrikeCombo, g_chainsawStrikeMove,
                  g_chainsawAnimStep, g_chainsawAnimTime);
    Log(line);

    g_chainsawCalls = 0;
    g_chainsawArms = 0;
    g_chainsawStrikes = 0;
}

void __fastcall HookedFxCreateParticles(void* emitter, void*, float currentTime,
                                        float deltaTime) {
    EmissionCarrySlot* carry{};
    float* intensity{};
    if (IsWeaponFxEmitter(emitter)) {
        __try {
            void* blueprint = *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(emitter) + 0x04);
            carry = FindEmissionCarrySlot(blueprint);
            intensity = reinterpret_cast<float*>(
                reinterpret_cast<uintptr_t>(emitter) + 0x10);
            if (carry && *intensity == 0.0f && carry->intensity > 0.0f
                && carry->intensity < 1.0f) {
                *intensity = carry->intensity;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            carry = nullptr;
            intensity = nullptr;
        }
    }

    reinterpret_cast<FxCreateParticlesFn>(g_fxCreateParticlesPatch.gateway)(
        emitter, currentTime, deltaTime);

    if (carry && intensity) {
        __try {
            carry->intensity = *intensity;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            carry->intensity = 0.0f;
        }
    }
}

} // namespace hff
