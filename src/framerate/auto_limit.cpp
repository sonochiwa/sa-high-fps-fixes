#include "framerate/auto_limit.h"

#include "core/config.h"
#include "core/conflicts.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "framerate/frame_limit.h"
#include "game/addresses.h"
#include "game/frame_hook.h"
#include "game/scripts.h"
#include "game/sites/framerate.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace hff::framerate {

namespace {

// The lowest limit an automatic case applies.
constexpr int kMinimumAutoLimit = 20;

// The FPS limit each automatic case applies, or 0 when the case is off.
struct AutoLimitCaps {
    int missions;
    int minigames;
    int schools;
    int cutscenes;
    int scriptedCutscenes;
    int pauseMenu;
};

SitePatch g_menuBackgroundPatch{};
AutoLimitCaps g_autoLimit{};
bool g_autoLimitTogglesGate{};
bool g_autoLimitActive{};
uint8_t g_savedFrameLimit{};
// What this plugin wrote when a case began, so the end of the case undoes only
// writes nothing else has changed since: a frame limit tool, CheatMenu or
// SA-MP's /fpslimit may change both in the meantime.
bool g_gateOpenedByCase{};
uint8_t g_frameLimitWrittenByCase{};

bool AnyAutoLimit() {
    return g_autoLimit.missions != 0 || g_autoLimit.minigames != 0
        || g_autoLimit.schools != 0 || g_autoLimit.cutscenes != 0
        || g_autoLimit.scriptedCutscenes != 0 || g_autoLimit.pauseMenu != 0;
}

// Each key holds the limit its case applies; the frame limiter is a single
// byte, and anything at or below zero leaves the case off. A limit under 20
// would make the case unplayable, so it is raised to 20.
int ReadAutoLimit(const char* key, int defaultLimit) {
    const int limit = ReadNumber("framerate", key, defaultLimit);
    if (limit <= 0) {
        return 0;
    }
    if (limit < kMinimumAutoLimit) {
        AddConfigWarning("framerate", key, "is below 20; using 20.");
        return kMinimumAutoLimit;
    }
    return std::min(limit, 255);
}

int PreferredScriptFpsLimit() {
    int preferred = 0;
    for (auto script = FirstRunningScript(); script;
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

uint8_t ReadGateOpcode() {
    __try {
        return *reinterpret_cast<const uint8_t*>(kFrameLimiterGate);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// Opens the gate only when it still holds the stock `jne`, and closes only a
// gate this plugin opened that nothing has rewritten since.
void OpenFrameLimiterGate() {
    constexpr uint8_t kJump = 0xEB;
    g_gateOpenedByCase = g_autoLimitTogglesGate
        && ReadGateOpcode() == kExpectedFrameLimiterGate[0]
        && WriteBytes(kFrameLimiterGate, &kJump, 1);
}

void CloseFrameLimiterGate() {
    if (g_gateOpenedByCase && ReadGateOpcode() == 0xEB) {
        WriteBytes(kFrameLimiterGate, &kExpectedFrameLimiterGate[0], 1);
    }
    g_gateOpenedByCase = false;
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
        OpenFrameLimiterGate();
    }
    const int outside = LimitOutsideCases();
    if (outside != 0 && outside < limit) {
        limit = outside;
    }
    g_frameLimitWrittenByCase = static_cast<uint8_t>(limit);
    WriteFrameLimit(g_frameLimitWrittenByCase);
}

void EndAutoLimit() {
    if (!g_autoLimitActive) {
        return;
    }
    if (ReadFrameLimit() == g_frameLimitWrittenByCase) {
        WriteFrameLimit(g_savedFrameLimit);
    }
    CloseFrameLimiterGate();
    g_autoLimitActive = false;
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

// Runs on every frame a menu is drawn: the front end, the pause menu, and
// the SA-MP menu, during which the game keeps running.
void __cdecl OnPauseMenuBackground() {
    PaceMenuFrame(g_autoLimit.pauseMenu);
}

__declspec(naked) void MenuBackgroundThunk() {
    __asm {
        pushfd
        pushad
        call OnPauseMenuBackground
        popad
        popfd
        jmp kMenuBackgroundTarget
    }
}

bool InstallAutoLimitCases() {
    if (!InstallFrameHook("Automatic FPS limit")) {
        return false;
    }
    // The cases write `RsGlobal.frameLimit`, which the engine reads only while
    // the limiter gate is open. With `fpsLimit` set the gate is open for good;
    // otherwise it is opened for the length of a case and closed after it, so
    // frames outside the cases stay as the game's own setting has them.
    constexpr std::array<uint8_t, 2> openGate{0xEB, 0x17};
    g_autoLimitTogglesGate = !FrameLimitHoldsGate()
                          && !MemoryMatches(kFrameLimiterGate, openGate);
    if (g_autoLimitTogglesGate
        && !MemoryMatches(kFrameLimiterGate, kExpectedFrameLimiterGate)) {
        g_autoLimitTogglesGate = false;
        Log("Automatic FPS limit skipped: the frame limiter gate does not "
            "match GTA SA 1.0 US.");
        return false;
    }
    AddFrameCallback(&ProcessAutoFpsLimit);
    return true;
}

} // namespace

void InstallAutoFpsLimit() {
    g_autoLimit = {};
    g_autoLimit.schools = ReadAutoLimit("forSchools", 0);
    g_autoLimit.missions = ReadAutoLimit("forMissions", 200);
    g_autoLimit.minigames = ReadAutoLimit("forMinigames", 30);
    g_autoLimit.cutscenes = ReadAutoLimit("forCutscenes", 200);
    g_autoLimit.scriptedCutscenes = ReadAutoLimit("forScriptedCutscenes", 200);
    g_autoLimit.pauseMenu = ReadAutoLimit("forPauseMenu", 200);
    if (!AnyAutoLimit() || !InstallAutoLimitCases()) {
        return;
    }
    if (g_autoLimit.pauseMenu != 0) {
        // plugin-sdk's `drawMenuBackgroundEvent` chains through this call, so
        // it is never contested.
        ExemptFromConflictGuard(kMenuBackground);
        if (!InstallJump(g_menuBackgroundPatch, kMenuBackground,
                         &MenuBackgroundThunk, kExpectedMenuBackground)) {
            Log("Automatic FPS limit installed without the pause menu case.");
            return;
        }
    }
    Log("Installed automatic FPS limiting for the configured game cases.");
}

} // namespace hff::framerate
