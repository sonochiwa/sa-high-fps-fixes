// GTA San Andreas ties a long list of behaviours to the rendered frame
// rather than to game time: swimming, vehicle physics, camera timers, HUD
// counters, particle emission and more were tuned at 30 FPS and drift above
// it. Each fix here patches one of those sites so it reads the real
// timestep, verifies the expected bytes first, and has its own switch in
// HighFpsFixes.ini. Behaviour at 30 FPS is unchanged. The modules under
// src\modules own their addresses, thunks and installers; this file only
// pins the module, starts the initializer off the loader lock and restores
// everything on unload.

#include "modules/modules.h"

using namespace hff;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void* reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
        // An ASI loader does not provide a pre-FreeLibrary shutdown callback.
        // Pin the module before the initializer can create hooks or workers so
        // no thread can return into an unloaded image. Process termination
        // reclaims the module and handles without running the restoration path.
        if (!PinPluginModule(instance)) {
            return FALSE;
        }
        if (HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0,
                                         nullptr)) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH && reserved == nullptr) {
        Shutdown();
    }
    return TRUE;
}
