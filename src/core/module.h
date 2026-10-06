#pragma once

#include <windows.h>

#include <string>

namespace hff {

extern HMODULE g_module;

bool PinPluginModule(HINSTANCE instance);
// The plugin's own path with its extension replaced, such as `.ini`.
std::string ModulePathWithExtension(const char* extension);

bool StartWorkerThread(LPTHREAD_START_ROUTINE entry);
// Waits up to `timeoutMilliseconds` and is true once the workers are asked to
// stop.
bool WorkerStopRequested(DWORD timeoutMilliseconds);
bool StopAllWorkerThreads();

} // namespace hff
