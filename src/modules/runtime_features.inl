// ---------------------------------------------------------------------------
// Physics sleep counter
// ---------------------------------------------------------------------------

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

using PadStateFn = bool(__thiscall*)(void*);

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

// ---------------------------------------------------------------------------
// Optional frame limiting
// ---------------------------------------------------------------------------

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
        if (g_autoLimit.flags.forMinigames
            && (ScriptNameMatches(name, "POOL2")
                || ScriptNameMatches(name, "GFSEX"))) {
            preferred = 30;
        } else if (g_autoLimit.flags.forMissions
                   && ScriptNameMatches(name, "DRUGS1")) {
            // Big Smoke sometimes stops walking indoors, which locks the mission.
            if (*reinterpret_cast<const int32_t*>(kGameCurrentArea) != 0) {
                preferred = 50;
            }
        } else if (g_autoLimit.flags.forSchools
                   && (ScriptNameMatches(name, "DSKOOL")
                       || ScriptNameMatches(name, "BOAT")
                       || ScriptNameMatches(name, "BSKOOL"))) {
            preferred = 80;
        }
    }
    return preferred;
}

void __cdecl ProcessAutoFpsLimit() {
    __try {
        if (g_isOnPauseMenu) {
            if (g_lastFpsLimit != 0) {
                WriteFrameLimit(static_cast<uint8_t>(g_lastFpsLimit));
                g_lastFpsLimit = 0;
            }
            g_isOnPauseMenu = false;
        }

        int preferred = 0;
        if (*reinterpret_cast<const int8_t*>(kCutsceneRunning) != 0) {
            if (g_autoLimit.flags.forCutscenes) {
                preferred = 60;
            }
        } else if (*reinterpret_cast<const uint8_t*>(kCameraWideScreenOn) != 0) {
            // Letterbox borders mark scripted scenes.
            if (g_autoLimit.flags.forScriptedCutscenes) {
                preferred = 80;
            }
        } else {
            preferred = PreferredScriptFpsLimit();
        }

        if (preferred != 0) {
            if (g_lastFpsLimit == 0) {
                g_lastFpsLimit = ReadFrameLimit();
            }
            if (g_lastFpsLimit != 0 && g_lastFpsLimit < preferred) {
                preferred = g_lastFpsLimit;
            }
            WriteFrameLimit(static_cast<uint8_t>(preferred));
        } else if (g_lastFpsLimit != 0) {
            WriteFrameLimit(static_cast<uint8_t>(g_lastFpsLimit));
            g_lastFpsLimit = 0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

void __cdecl OnPauseMenuBackground() {
    g_isOnPauseMenu = true;
    if (g_lastFpsLimit == 0) {
        g_lastFpsLimit = ReadFrameLimit();
    }
    WriteFrameLimit(60);
}

