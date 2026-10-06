#include "harness.h"

#include "game.h"
#include "scenarios.h"

#include "MinHook.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace harness {
namespace {

constexpr wchar_t kScenariosVariable[] = L"HFF_AUTOTEST";
constexpr wchar_t kFpsVariable[] = L"HFF_AUTOTEST_FPS";
// Movies are skipped once they have played this long; skipping on their
// first frame hung the start-up in the Resource System autotest.
constexpr DWORD kMovieSkipDelayMs = 1000;
// Virtual seconds the world gets to stream in and come to rest before a
// scenario starts.
constexpr float kSettleSeconds = 2.0f;
// A scenario still running after this many virtual seconds is stopped.
constexpr float kScenarioTimeoutSeconds = 60.0f;
// The Los Santos airport apron: flat, open and away from traffic.
constexpr float kSiteX = 1690.0f;
constexpr float kSiteY = -2550.0f;
// The game's time scale, applied to the frame time as CTimer::Update does.
constexpr uintptr_t kTimeScale = 0xB7CB64;

using GameFn = void(__cdecl*)();
using UpdateVariablesFn = void(__cdecl*)(float);
using TimerFunction = uint64_t(__cdecl*)();

enum class Phase { kBoot, kSettle, kStart, kRun, kDone };

GameFn g_updatePads = nullptr;
GameFn g_gameProcess = nullptr;
GameFn g_timerUpdate = nullptr;
std::vector<const Scenario*> g_scenarios;
size_t g_current = 0;
int g_fps = 30;
Phase g_phase = Phase::kBoot;
uint32_t g_phaseFrames = 0;
FILE* g_results = nullptr;

std::wstring ReadVariable(const wchar_t* name) {
    wchar_t value[512] = {};
    const DWORD length = GetEnvironmentVariableW(name, value, 512);
    return length > 0 && length < 512 ? std::wstring(value, length) : std::wstring();
}

std::wstring ModulePath(const wchar_t* extension) {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ModulePath), &module);
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring result(path);
    const size_t dot = result.find_last_of(L'.');
    return (dot == std::wstring::npos ? result : result.substr(0, dot)) + extension;
}

bool ParseScenarios(const std::wstring& list) {
    size_t start = 0;
    while (start <= list.size()) {
        size_t end = list.find(L',', start);
        end = end == std::wstring::npos ? list.size() : end;
        std::string name;
        for (size_t i = start; i < end; ++i) {
            name.push_back(static_cast<char>(list[i]));
        }
        if (!name.empty()) {
            const Scenario* scenario = scenarios::Find(name.c_str());
            if (!scenario) {
                return false;
            }
            g_scenarios.push_back(scenario);
        }
        start = end + 1;
    }
    return !g_scenarios.empty();
}

bool MoviePlayedLongEnough(int32_t state) {
    static int32_t movie = -1;
    static DWORD since = 0;
    const DWORD now = GetTickCount();
    if (movie != state) {
        movie = state;
        since = now;
    }
    return now - since >= kMovieSkipDelayMs;
}

// The transitions WinMain makes on a key press during a movie, and a front
// end kept closed, so WinMain starts a new game.
void SkipToNewGame() {
    int32_t& state = game::At<int32_t>(game::kGameState);
    if (state == game::kStatePlayingLogo || state == game::kStatePlayingIntro) {
        if (MoviePlayedLongEnough(state)) {
            state = state == game::kStatePlayingLogo ? game::kStateTitle
                                                     : game::kStateFrontendLoading;
        }
    } else if (state == game::kStateFrontendIdle) {
        game::At<uint8_t>(game::kMenuActivateNextFrame) = 0;
        game::At<uint8_t>(game::kMenuActive) = 0;
    }
}

Frame CurrentFrame() {
    return {g_phaseFrames, g_fps, static_cast<float>(g_phaseFrames) / static_cast<float>(g_fps)};
}

void EnterPhase(Phase phase) {
    g_phase = phase;
    g_phaseFrames = 0;
}

void PlaceAtSite() {
    const float z = game::GroundZ(kSiteX, kSiteY);
    game::Teleport(game::Player(), {kSiteX, kSiteY, z + 1.0f});
    game::ShowPlayer();
}

void Finish() {
    Note("finished");
    EnterPhase(Phase::kDone);
    game::At<int32_t>(game::kQuit) = 1;
}

void Advance() {
    if (g_phase == Phase::kDone) {
        return;
    }
    game::SleepAllScripts();
    switch (g_phase) {
    case Phase::kBoot: {
        char line[64];
        std::snprintf(line, sizeof(line), "screen %dx%d", game::At<int32_t>(game::kScreenWidth),
                      game::At<int32_t>(game::kScreenHeight));
        Note(line);
        game::At<uint8_t>(game::kFrameLimiterPreference) = 0;
        PlaceAtSite();
        EnterPhase(Phase::kSettle);
        return;
    }
    case Phase::kSettle:
        if (CurrentFrame().seconds >= kSettleSeconds) {
            EnterPhase(Phase::kStart);
        }
        break;
    case Phase::kStart: {
        const Scenario* scenario = g_scenarios[g_current];
        std::string line = std::string("start ") + scenario->name;
        Note(line.c_str());
        if (!scenario->start()) {
            Record(scenario->name, "skipped", 1.0);
            ++g_current;
            if (g_current >= g_scenarios.size()) {
                Finish();
            } else {
                PlaceAtSite();
                EnterPhase(Phase::kSettle);
            }
            return;
        }
        EnterPhase(Phase::kRun);
        return;
    }
    case Phase::kRun: {
        const Scenario* scenario = g_scenarios[g_current];
        const Frame frame = CurrentFrame();
        bool done = scenario->step(frame);
        if (!done && frame.seconds >= kScenarioTimeoutSeconds) {
            Record(scenario->name, "timeout", 1.0);
            done = true;
        }
        if (done) {
            ++g_current;
            if (g_current >= g_scenarios.size()) {
                Finish();
                return;
            }
            PlaceAtSite();
            EnterPhase(Phase::kSettle);
            return;
        }
        break;
    }
    case Phase::kDone:
        return;
    }
    ++g_phaseFrames;
}

void __cdecl UpdatePadsHook() {
    SkipToNewGame();
    g_updatePads();
    if (game::GameState() != game::kStatePlaying || g_phase != Phase::kRun) {
        return;
    }
    auto* pad = reinterpret_cast<uint8_t*>(game::kPad);
    game::Field<uint16_t>(pad, game::kPadDisablePlayerControls) = 0;
    std::memset(pad + game::kPadDisableFlags, 0, game::kPadDisableFlagCount);
    const Scenario* scenario = g_scenarios[g_current];
    if (scenario->input) {
        scenario->input(CurrentFrame(), pad);
    }
}

void __cdecl GameProcessHook() {
    if (game::GameState() == game::kStatePlaying) {
        Advance();
    }
    g_gameProcess();
}

// CTimer::Update with the frame's length fixed at 1/fps of a second while
// the game is played, so a scenario sees exactly that frame rate whatever
// the machine draws. Everything else is done as the game does it.
void __cdecl TimerUpdateHook() {
    using game::At;
    if (game::GameState() != game::kStatePlaying || g_phase == Phase::kDone) {
        g_timerUpdate();
        return;
    }
    const auto timer = At<TimerFunction>(game::kTimerFunction);
    if (!timer) {
        return;
    }
    At<uint32_t>(game::kTimerPPPPrevious) = At<uint32_t>(game::kTimerPPPrevious);
    At<uint32_t>(game::kTimerPPPrevious) = At<uint32_t>(game::kTimerPPrevious);
    At<uint32_t>(game::kTimerPPrevious) = At<uint32_t>(game::kTimerPrevious);
    At<uint32_t>(game::kTimerPrevious) = At<uint32_t>(game::kTimerTime);
    At<uint32_t>(game::kTimerPreviousNonClipped) = At<uint32_t>(game::kTimerNonClipped);
    const uint64_t before = At<uint64_t>(game::kTimerRenderStart);
    const uint64_t now = timer();
    At<uint64_t>(game::kTimerRenderStart) = now;
    const auto divider = static_cast<float>(At<uint32_t>(game::kTimerDivider));
    At<uint32_t>(game::kTimerPauseModeTime) +=
        static_cast<uint32_t>(static_cast<float>(now - before) / divider);
    const bool paused = At<bool>(game::kTimerUserPause) || At<bool>(game::kTimerCodePause);
    const float delta =
        paused ? 0.0f : divider * 1000.0f / static_cast<float>(g_fps) * At<float>(kTimeScale);
    reinterpret_cast<UpdateVariablesFn>(game::kTimerUpdateVariables)(delta);
    ++At<uint32_t>(game::kTimerFrameCounter);
}

// The game only runs frames while it thinks it has the focus; a test runs
// in the background, so the flag is held up from outside the frame loop.
DWORD WINAPI KeepForeground(void*) {
    while (g_phase != Phase::kDone) {
        game::At<uint8_t>(game::kForegroundApp) = 1;
        Sleep(100);
    }
    return 0;
}

bool Hook(uintptr_t address, void* detour, GameFn* original) {
    return MH_CreateHook(reinterpret_cast<void*>(address), detour,
                         reinterpret_cast<void**>(original)) == MH_OK;
}

}  // namespace

void Record(const char* scenario, const char* metric, double value) {
    if (g_results) {
        std::fprintf(g_results, "%s %s %.6f\n", scenario, metric, value);
        std::fflush(g_results);
    }
}

void Note(const char* text) {
    if (g_results) {
        std::fprintf(g_results, "# %s\n", text);
        std::fflush(g_results);
    }
}

bool Install() {
    const std::wstring list = ReadVariable(kScenariosVariable);
    if (list.empty()) {
        return false;
    }
    g_results = _wfsopen(ModulePath(L".txt").c_str(), L"w", _SH_DENYNO);
    if (!ParseScenarios(list)) {
        Note("error unknown scenario");
        return false;
    }
    const int fps = _wtoi(ReadVariable(kFpsVariable).c_str());
    g_fps = fps > 0 ? fps : 30;
    if (!game::VerifyFunctions()) {
        Note("error unexpected game code");
        return false;
    }
    if (MH_Initialize() != MH_OK ||
        !Hook(game::kUpdatePads, reinterpret_cast<void*>(&UpdatePadsHook), &g_updatePads) ||
        !Hook(game::kGameProcess, reinterpret_cast<void*>(&GameProcessHook), &g_gameProcess) ||
        !Hook(game::kTimerUpdate, reinterpret_cast<void*>(&TimerUpdateHook), &g_timerUpdate) ||
        MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        Note("error hooks");
        return false;
    }
    char line[64];
    std::snprintf(line, sizeof(line), "fps %d", g_fps);
    Note(line);
    if (HANDLE thread = CreateThread(nullptr, 0, KeepForeground, nullptr, 0, nullptr)) {
        CloseHandle(thread);
    }
    return true;
}

}  // namespace harness
