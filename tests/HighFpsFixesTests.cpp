// The High FPS Fixes autotest, loaded beside the plugin in a test copy of the
// game. With HFF_AUTOTEST naming scenarios and HFF_AUTOTEST_FPS a frame rate,
// the game skips its movies and menu, starts a new game with the mission
// scripts asleep and runs on a virtual clock of exactly that many frames a
// second. Each scenario sets something up, drives the pad and measures what
// the game does; the results go to HighFpsFixesTests.txt beside this file and
// the game quits. Comparing a run at 30 FPS with one at a high rate shows
// whether a behaviour depends on the frame rate. Without the variable the
// module does nothing.

#include "harness.h"

#include <windows.h>

namespace {

// Long enough for the plugin under test to finish its own start-up first:
// two plugins applying MinHook hooks at once can suspend each other.
constexpr DWORD kStartDelayMs = 3000;

DWORD WINAPI Start(void*) {
    Sleep(kStartDelayMs);
    harness::Install();
    return 0;
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        if (GetEnvironmentVariableW(L"HFF_AUTOTEST", nullptr, 0) > 0) {
            if (HANDLE thread = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr)) {
                CloseHandle(thread);
            }
        }
    }
    return TRUE;
}
