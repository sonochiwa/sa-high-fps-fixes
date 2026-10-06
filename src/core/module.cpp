#include "core/module.h"

#include "core/log.h"

#include <windows.h>

#include <array>
#include <cstdio>

namespace hff {

HMODULE g_module{};

namespace {

HANDLE g_workerStopEvent{};
std::array<HANDLE, 4> g_workers{};
size_t g_workerCount{};

void LogLastError(const char* what) {
    char line[128];
    std::snprintf(line, sizeof(line), "%s failed with error %lu.", what,
                  GetLastError());
    Log(line);
}

} // namespace

bool PinPluginModule(HINSTANCE instance) {
    HMODULE pinned{};
    return GetModuleHandleExA(
               GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                   | GET_MODULE_HANDLE_EX_FLAG_PIN,
               reinterpret_cast<LPCSTR>(instance), &pinned)
        != FALSE;
}

std::string ModulePathWithExtension(const char* extension) {
    std::array<char, MAX_PATH> path{};
    const DWORD length = GetModuleFileNameA(g_module, path.data(),
                                            static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return {};
    }

    std::string result(path.data(), length);
    const size_t slash = result.find_last_of("\\/");
    const size_t dot = result.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        result.resize(dot);
    }
    result += extension;
    return result;
}

bool StartWorkerThread(LPTHREAD_START_ROUTINE entry) {
    if (g_workerCount == g_workers.size()) {
        return false;
    }
    if (!g_workerStopEvent) {
        g_workerStopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!g_workerStopEvent) {
            LogLastError("CreateEvent");
            return false;
        }
    }
    const HANDLE thread = CreateThread(nullptr, 0, entry, nullptr, 0, nullptr);
    if (!thread) {
        LogLastError("CreateThread");
        return false;
    }
    g_workers[g_workerCount++] = thread;
    return true;
}

bool WorkerStopRequested(DWORD timeoutMilliseconds) {
    return g_workerStopEvent
        && WaitForSingleObject(g_workerStopEvent, timeoutMilliseconds)
               == WAIT_OBJECT_0;
}

bool StopAllWorkerThreads() {
    if (g_workerStopEvent) {
        SetEvent(g_workerStopEvent);
    }
    if (g_workerCount != 0) {
        const DWORD wait = WaitForMultipleObjects(
            static_cast<DWORD>(g_workerCount), g_workers.data(), TRUE, 5000);
        if (wait != WAIT_OBJECT_0) {
            Log("Shutdown deferred: a worker thread did not stop safely.");
            return false;
        }
    }
    for (size_t i = 0; i < g_workerCount; ++i) {
        CloseHandle(g_workers[i]);
        g_workers[i] = nullptr;
    }
    g_workerCount = 0;
    if (g_workerStopEvent) {
        CloseHandle(g_workerStopEvent);
        g_workerStopEvent = nullptr;
    }
    return true;
}

} // namespace hff
