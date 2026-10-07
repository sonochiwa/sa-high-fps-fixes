#include "core/hook.h"

#include "MinHook.h"

#include <windows.h>

#include <cwchar>

namespace hff {

namespace {

constexpr DWORD kFreezeLockWaitMs = 10000;

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
        ThreadFreezeLock lock;
        MH_Uninitialize();
        g_minHookInitialized = false;
    }
}

ThreadFreezeLock::ThreadFreezeLock() : mutex(nullptr), owned(false) {
    wchar_t name[64]{};
    swprintf_s(name, L"Local\\GtaSaMinHookFreeze-%lu", GetCurrentProcessId());
    mutex = CreateMutexW(nullptr, FALSE, name);
    if (mutex) {
        const DWORD wait = WaitForSingleObject(mutex, kFreezeLockWaitMs);
        owned = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
    }
}

ThreadFreezeLock::~ThreadFreezeLock() {
    if (owned) {
        ReleaseMutex(mutex);
    }
    if (mutex) {
        CloseHandle(mutex);
    }
}

} // namespace hff
