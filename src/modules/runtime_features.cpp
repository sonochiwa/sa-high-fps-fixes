#include "modules/modules.h"

namespace hff {

// SA-MP's moving-object rotation is a slerp between the start and target
// quaternions whose parameter CObject::Process forms as
//
//     t = 1.0f - remainingDistance / m_fTotalDistance;
//
// where `remainingDistance` is measured from the object's real position. The
// caller replaces `remainingDistance` with a wall-clock estimate, so this
// returns the *remaining* fraction and the stock `1.0f - x` that follows keeps
// its original meaning. Returning the elapsed fraction here would run the slerp
// backwards from the target to the start pose.
float __cdecl SampObjectRotationRemainingFraction(float elapsedDistance,
                                                  float totalDistance) {
    if (!std::isfinite(totalDistance) || totalDistance <= 0.0f) {
        return 0.0f;
    }
    if (!std::isfinite(elapsedDistance) || elapsedDistance <= 0.0f) {
        return 1.0f;
    }
    if (elapsedDistance >= totalDistance) {
        return 0.0f;
    }
    return 1.0f - elapsedDistance / totalDistance;
}

// Set by the arrival thunk so it can branch after restoring the flags the
// helper call clobbered.
uint8_t g_sampObjectMoveExpired{};

// A move's scheduled duration is m_fTotalDistance / m_fSpeed, and the caller
// hands over elapsedSeconds * m_fSpeed, so `elapsed >= total` is exactly
// "the move's wall-clock time is up". Only ever forces the arrival CObject
// already reaches on its own; a move that is still within its duration keeps
// the stock overshoot test.
void __cdecl EvaluateSampObjectMoveExpiry(float elapsedDistance,
                                          float totalDistance) {
    g_sampObjectMoveExpired =
        (std::isfinite(totalDistance) && totalDistance > 0.0f
         && std::isfinite(elapsedDistance) && elapsedDistance >= totalDistance)
            ? 1
            : 0;
}

// `CObject`, `CAutomobile`, `CBike` and `CTrailer` all end their per-frame
// physics with the same idea:
//
//     m_vecForce = (m_vecForce + m_vecMoveSpeed) / 2;
//     if (still moving) { m_nFakePhysics = 0; }
//     else if (++m_nFakePhysics > 10) {
//         m_nFakePhysics = 10;
//         ResetMoveSpeed(); ResetTurnSpeed(); skipPhysics = true;
//     }
//
// `m_nFakePhysics` counts rendered frames, not time. At 30 FPS an entity has to
// stay nearly still for 11 frames, about 0.37 s, before the engine parks it. At
// 300 FPS that is 0.037 s, so a bike that is momentarily slow at the apex of a
// jump has its speed zeroed and its physics skipped, and hangs in mid-air; and
// a parked car being pushed is put back to sleep between pushes, which is why
// it becomes hard to move.
//
// The counter is therefore stepped in real time at the original 30 FPS rate
// instead of once per rendered frame. The decision is made once per game frame
// and shared by every entity, so each entity keeps its own counter and the
// `> 10` comparisons are untouched. At 30 FPS and below every frame ticks, so
// the original behavior is reproduced exactly.
int32_t __cdecl ShouldTickFakePhysicsCounter() {
    __try {
        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        if (frame != g_fakePhysicsLastFrame) {
            g_fakePhysicsLastFrame = frame;
            g_fakePhysicsCarry += TimeStepRatio();
            if (g_fakePhysicsCarry >= 1.0f) {
                g_fakePhysicsCarry -= std::floor(g_fakePhysicsCarry);
                g_fakePhysicsTick = 1;
            } else {
                g_fakePhysicsTick = 0;
            }
        }
        return g_fakePhysicsTick;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

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

void WriteFrameLimit(uint8_t value) {
    WriteBytes(kFrameLimit, &value, 1);
}

uint8_t ReadFrameLimit() {
    __try {
        return *reinterpret_cast<const uint8_t*>(kFrameLimit);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool ScriptNameMatches(const char* name, const char* expected) {
    std::array<char, kRunningScriptNameSize + 1> buffer{};
    std::memcpy(buffer.data(), name, kRunningScriptNameSize);
    return _stricmp(buffer.data(), expected) == 0;
}

int PreferredScriptFpsLimit() {
    int preferred = 0;
    const auto queueHead = *reinterpret_cast<uintptr_t*>(kScriptQueueOperand);
    if (!queueHead) {
        return 0;
    }
    for (auto script = *reinterpret_cast<uintptr_t*>(queueHead); script;
         script = *reinterpret_cast<uintptr_t*>(script)) {
        const char* name = reinterpret_cast<const char*>(
            script + kRunningScriptNameOffset);
        if (g_autoLimit.minigames != 0
            && (ScriptNameMatches(name, "POOL2")
                || ScriptNameMatches(name, "GFSEX"))) {
            preferred = g_autoLimit.minigames;
        } else if (g_autoLimit.missions != 0
                   && ScriptNameMatches(name, "DRUGS1")) {
            // Big Smoke sometimes stops walking indoors, which locks the mission.
            if (*reinterpret_cast<const int32_t*>(kGameCurrentArea) != 0) {
                preferred = g_autoLimit.missions;
            }
        } else if (g_autoLimit.schools != 0
                   && (ScriptNameMatches(name, "DSKOOL")
                       || ScriptNameMatches(name, "BOAT")
                       || ScriptNameMatches(name, "BSKOOL"))) {
            preferred = g_autoLimit.schools;
        }
    }
    return preferred;
}

namespace {

bool g_autoLimitActive{};
uint8_t g_savedFrameLimit{};

void SetFrameLimiterGate(bool open) {
    if (g_autoLimitTogglesGate) {
        const uint8_t jump = open ? 0xEB : kExpectedFrameLimiterGate[0];
        WriteBytes(kFrameLimiterGate, &jump, 1);
    }
}

// The limit outside every case, or 0 when frames are not limited then: the
// game's own limiter is off and no `fpsLimit` holds the gate open.
int LimitOutsideCases() {
    if (g_autoLimitTogglesGate
        && *reinterpret_cast<const uint8_t*>(kFrameLimiterPreference) == 0) {
        return 0;
    }
    return g_savedFrameLimit;
}

void BeginAutoLimit(int limit) {
    if (!g_autoLimitActive) {
        g_savedFrameLimit = ReadFrameLimit();
        g_autoLimitActive = true;
        SetFrameLimiterGate(true);
    }
    const int outside = LimitOutsideCases();
    if (outside != 0 && outside < limit) {
        limit = outside;
    }
    WriteFrameLimit(static_cast<uint8_t>(limit));
}

// Holds a menu frame until 1/limit of a second has passed since the
// previous one. The game's own limiter does not hold menu frames: with its
// gate open and `RsGlobal.frameLimit` at 200 the pause menu still drew about
// 2700 frames a second, and the front end has no limiter at all. Waits the
// way that limiter does, by spinning, so the pace is exact at any limit.
void PaceMenuFrame(int limit) {
    static LARGE_INTEGER frequency{};
    static LONGLONG next = 0;
    if (frequency.QuadPart == 0 && !QueryPerformanceFrequency(&frequency)) {
        return;
    }
    const LONGLONG period = frequency.QuadPart / limit;
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    if (next != 0 && now.QuadPart < next && next - now.QuadPart <= period) {
        while (now.QuadPart < next) {
            SwitchToThread();
            QueryPerformanceCounter(&now);
        }
        next += period;
    } else {
        next = now.QuadPart + period;
    }
}

void EndAutoLimit() {
    if (!g_autoLimitActive) {
        return;
    }
    WriteFrameLimit(g_savedFrameLimit);
    SetFrameLimiterGate(false);
    g_autoLimitActive = false;
}

} // namespace

void __cdecl ProcessAutoFpsLimit() {
    __try {
        int preferred = 0;
        if (*reinterpret_cast<const int8_t*>(kCutsceneRunning) != 0) {
            preferred = g_autoLimit.cutscenes;
        } else if (*reinterpret_cast<const uint8_t*>(kCameraWideScreenOn) != 0) {
            // Letterbox borders mark scripted scenes.
            preferred = g_autoLimit.scriptedCutscenes;
        } else {
            preferred = PreferredScriptFpsLimit();
        }

        if (preferred != 0) {
            BeginAutoLimit(preferred);
        } else {
            EndAutoLimit();
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

// The one call this plugin gets each frame on the game thread, from the
// `CTheScripts::Process` hook. Everything that has to run per frame from
// inside the game rather than from a worker thread hangs off it.
// The audio engine's wait between two acceleration loops is a frame count;
// this keeps it the same third of a second at any frame rate. Rounded to
// the nearest frame and never below one, so 30 FPS reads back the stock
// ten exactly.
void UpdateAcLoopFrameCount() {
    const float ratio = TimeStepRatio();
    if (!(ratio > 0.0f)) {
        return;
    }
    const int32_t frames = std::max(
        1L, std::lround(static_cast<float>(kStockAcLoopFrameCount) / ratio));
    __try {
        auto* value = reinterpret_cast<int32_t*>(kAcLoopFrameCount);
        if (*value != frames) {
            *value = frames;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void __cdecl ProcessFrameHooks() {
    if (g_autoLimit.Any()) {
        ProcessAutoFpsLimit();
    }
    if (g_gearChangeKick) {
        UpdateAcLoopFrameCount();
    }
    GuardInstalledSites();
}

// Runs on every frame a menu is drawn: the front end, the pause menu, and
// the SA-MP menu, during which the game keeps running.
void __cdecl OnPauseMenuBackground() {
    if (g_autoLimit.pauseMenu != 0) {
        PaceMenuFrame(g_autoLimit.pauseMenu);
    }
}

} // namespace hff
