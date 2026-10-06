#include "core/hook.h"

#include "MinHook.h"

namespace hff {

namespace {

bool g_minHookInitialized{};

} // namespace

bool InitializeMinHook() {
    if (!g_minHookInitialized) {
        g_minHookInitialized = MH_Initialize() == MH_OK;
    }
    return g_minHookInitialized;
}

void UninitializeMinHook() {
    if (g_minHookInitialized) {
        MH_Uninitialize();
        g_minHookInitialized = false;
    }
}

} // namespace hff
