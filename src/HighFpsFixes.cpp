#define NOMINMAX
#include <windows.h>
#include <share.h>
#include <intrin.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <string>

namespace {

#include "modules/game_addresses.inl"
#include "modules/expected_bytes.inl"
#include "modules/game_profiles.inl"
#include "modules/configuration.inl"
#include "modules/patching.inl"
#include "modules/weapons_and_particles.inl"
#include "modules/player.inl"
#include "modules/vehicles.inl"
#include "modules/hud.inl"
#include "modules/diagnostics.inl"
#include "modules/bike_lean_filter.inl"
#include "modules/runtime_features.inl"
#include "modules/thunk_helpers.inl"
#include "modules/thunks.inl"
#include "modules/installers.inl"
#include "modules/bootstrap.inl"

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void* reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
        if (HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0,
                                         nullptr)) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH && reserved == nullptr) {
        Shutdown();
    }
    return TRUE;
}
