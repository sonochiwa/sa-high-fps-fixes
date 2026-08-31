// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------

bool IsContinuousWeapon(int32_t weaponType) {
    constexpr int32_t kFlamethrower = 37;
    constexpr int32_t kSpraycan = 41;
    constexpr int32_t kExtinguisher = 42;
    return weaponType == kFlamethrower || weaponType == kSpraycan
        || weaponType == kExtinguisher;
}

bool IsWeaponFxEmitter(void* emitter, void** systemOut = nullptr) {
    if (!emitter) {
        return false;
    }
    __try {
        void* system = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(emitter) + 0x08);
        if (systemOut) {
            *systemOut = system;
        }
        return system && (*reinterpret_cast<uint8_t*>(
            reinterpret_cast<uintptr_t>(system) + 0x62) & 0x20) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

using FxCreateParticlesFn = void(__thiscall*)(void*, float, float);

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

using AimWeaponFn = void(__thiscall*)(void*, const Vec3&, float, float, float);

void __fastcall HookedProcessAimWeapon(void* cam, void*, const Vec3* target,
                                       float orientation, float speedVar,
                                       float speedVarWanted) {
    __try {
        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        g_aimTimeStep = std::isfinite(timeStep) && timeStep > 0.0f
                     && timeStep < 1.0f
            ? 1.0f
            : timeStep;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_aimTimeStep = 1.0f;
    }

    const Vec3 zero{};
    reinterpret_cast<AimWeaponFn>(g_aimWeaponPatch.gateway)(
        cam, target ? *target : zero, orientation, speedVar, speedVarWanted);
}

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

// Diagnostic for the cycle skill counter, which was reported in game as not
// levelling at all at 600 FPS with `skillProgress` on, and as levelling in
// three minutes with it off against two minutes at 30 FPS. Reading the code
// predicts the opposite: unpatched it should be several times faster at a high
// frame rate, not slower. That means the reading is wrong somewhere, and this
// logs the four numbers that separate the candidates rather than guessing
// again.
//
// `calls` is how many times a second the accumulate path is reached at all,
// which is the speed gate at `0x55C8E6`: if the bike is simply slower at a high
// frame rate the counter starves regardless of any truncation.
// `raw` against `added` separates the correction from the truncation.
constexpr uintptr_t kCycleSkillCounter = 0x00B794E0;
constexpr uintptr_t kCycleStaminaCounter = 0x00B794DC;
constexpr uintptr_t kCycleSkillLimit = 0x00B78FAC;

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

void TraceChainsaw();

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

// ---------------------------------------------------------------------------
// Direct particle emission rate
// ---------------------------------------------------------------------------

using FxAddParticleFn = void(__fastcall*)(void* self, void* edx, const void* pos,
                                          const void* vel, float timeSince,
                                          const void* mults, float rotZ,
                                          float lightMult, float lightMultLimit,
                                          int32_t createLocal);

bool g_particleRateGate = true;
uint32_t g_particleBudget = 0;

// One entry per call site, found by the return address of the call into
// `FxSystem_c::AddParticle`. Open addressing, and a full table fails open: a
// missed gate costs some particles, a wrong one costs the effect entirely.
constexpr uint32_t kParticleSiteSlots = 512;  // power of two
constexpr uint32_t kParticleSiteMask = kParticleSiteSlots - 1;

struct ParticleSiteState {
    uintptr_t site;
    uint32_t lastFrame;
    float carry;
    uint8_t open;
};
std::array<ParticleSiteState, kParticleSiteSlots> g_particleSites{};

void ResetParticleSites() {
    for (auto& slot : g_particleSites) {
        slot.site = 0;
        slot.lastFrame = 0;
        slot.carry = 0.0f;
        slot.open = 1;
    }
}

ParticleSiteState* FindParticleSite(uintptr_t site) {
    uint32_t key = static_cast<uint32_t>(site);
    key ^= key >> 4;
    key *= 2654435761u;
    uint32_t index = (key >> 8) & kParticleSiteMask;
    for (uint32_t probe = 0; probe < 32; ++probe) {
        ParticleSiteState& slot = g_particleSites[index];
        if (slot.site == site) {
            return &slot;
        }
        if (slot.site == 0) {
            slot.site = site;
            slot.lastFrame = 0;
            slot.carry = 0.0f;
            slot.open = 1;
            return &slot;
        }
        index = (index + 1) & kParticleSiteMask;
    }
    return nullptr;
}

// True when this call site may emit on this frame. The decision is taken once
// per frame counter value and reused, so every call the site makes within one
// frame agrees: the exhaust adds up to eight particles in its loop and either
// all of them belong to this frame or none do.
bool ParticleSiteOpen(uintptr_t site) {
    ParticleSiteState* slot = FindParticleSite(site);
    if (!slot) {
        return true;
    }
    const uint32_t frame = *reinterpret_cast<volatile uint32_t*>(kFrameCounter);
    if (slot->lastFrame == frame) {
        return slot->open != 0;
    }

    float ratio = TimeStepRatio();
    if (!std::isfinite(ratio) || ratio <= 0.0f) {
        ratio = 1.0f;
    }
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }

    const uint32_t elapsedFrames = frame - slot->lastFrame;
    if (slot->lastFrame == 0
        || static_cast<float>(elapsedFrames) * ratio >= 1.0f) {
        // The call site was idle for at least one original frame, so this is a
        // fresh event rather than a short stochastic gap in a stream. Never
        // drop the first particle of a fresh event.
        slot->carry = 0.0f;
        slot->open = 1;
    } else {
        slot->carry += ratio;
        if (slot->carry >= 1.0f) {
            slot->carry -= 1.0f;
            slot->open = 1;
        } else {
            slot->open = 0;
        }
    }
    slot->lastFrame = frame;
    return slot->open != 0;
}

// Optional hard ceiling on new particles per second, kept for parity with the
// standalone FxLimiter plugin. Off by default: the rate gate above already
// restores the density the game was drawn for, and a fixed ceiling is a
// performance knob rather than a fix.
double g_qpcToSeconds = 0.0;

double NowSeconds() {
    if (g_qpcToSeconds == 0.0) {
        LARGE_INTEGER frequency{};
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0) {
            return 0.0;
        }
        g_qpcToSeconds = 1.0 / static_cast<double>(frequency.QuadPart);
    }
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) * g_qpcToSeconds;
}

struct ParticleBudgetState {
    double lastRefill;
    double credit;
};
ParticleBudgetState g_generalBudget{};

bool ParticleBudgetAllows(ParticleBudgetState& state, uint32_t perSecond) {
    if (perSecond == 0) {
        return true;
    }
    const double now = NowSeconds();
    if (now == 0.0) {
        return true;
    }
    if (state.lastRefill == 0.0 || now < state.lastRefill) {
        state.lastRefill = now;
        state.credit = 1.0;
    } else {
        state.credit += (now - state.lastRefill) * static_cast<double>(perSecond);
        state.lastRefill = now;
        if (state.credit > 1.0) {
            state.credit = 1.0;
        }
    }
    if (state.credit >= 1.0) {
        state.credit -= 1.0;
        return true;
    }
    return false;
}

__declspec(noinline) void __fastcall HookedFxAddParticle(
    void* self, void* edx, const void* pos, const void* vel, float timeSince,
    const void* mults, float rotZ, float lightMult, float lightMultLimit,
    int32_t createLocal) {
    const uintptr_t site = reinterpret_cast<uintptr_t>(_ReturnAddress());

    // Always evaluated so the site's frame stamp stays current even when the
    // gate is switched off and only the budget or the multipliers are wanted.
    const bool open = ParticleSiteOpen(site);
    if (g_particleRateGate && !open) {
        return;
    }

    if (g_particleBudget != 0
        && !ParticleBudgetAllows(g_generalBudget, g_particleBudget)) {
        return;
    }

    reinterpret_cast<FxAddParticleFn>(g_fxAddParticlePatch.gateway)(
        self, edx, pos, vel, timeSince, mults, rotZ, lightMult, lightMultLimit,
        createLocal);
}
