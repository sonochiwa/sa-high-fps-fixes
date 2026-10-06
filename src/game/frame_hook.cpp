#include "game/frame_hook.h"

#include "core/hook.h"
#include "core/log.h"
#include "core/memory.h"
#include "game/sites/scripts.h"

#include "MinHook.h"

#include <array>
#include <atomic>
#include <string>

namespace hff {

namespace {

using ScriptsProcessFn = void(__cdecl*)();

ScriptsProcessFn g_originalScriptsProcess{};
bool g_frameHookInstalled{};
std::array<FrameCallback, 8> g_frameCallbacks{};
// Fixes add callbacks from the initialization thread while the game thread
// may already be running them, so a callback is stored before it is counted.
std::atomic<size_t> g_frameCallbackCount{};

void __cdecl HookedScriptsProcess() {
    const size_t count = g_frameCallbackCount.load(std::memory_order_acquire);
    for (size_t i = 0; i < count; ++i) {
        g_frameCallbacks[i]();
    }
    g_originalScriptsProcess();
}

void LogFrameHookSkipped(const char* fixName, const char* reason) {
    std::string message(fixName);
    message += " skipped: ";
    message += reason;
    Log(message.c_str());
}

} // namespace

// Other plugins hook this entry for their own script processing, often before
// this one loads. MinHook relocates whatever branch they left in the first
// instruction into its trampoline, so this hook chains to theirs instead of
// refusing the site.
bool InstallFrameHook(const char* fixName) {
    if (g_frameHookInstalled) {
        return true;
    }
    if (!InitializeMinHook()) {
        LogFrameHookSkipped(fixName, "MinHook initialization failed.");
        return false;
    }
    const bool chained = !MemoryMatches(kScriptsProcess, kExpectedScriptsProcess);
    auto* target = reinterpret_cast<void*>(kScriptsProcess);
    if (MH_CreateHook(target, &HookedScriptsProcess,
                      reinterpret_cast<void**>(&g_originalScriptsProcess)) != MH_OK) {
        LogFrameHookSkipped(fixName, "CTheScripts::Process could not be hooked.");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        MH_RemoveHook(target);
        LogFrameHookSkipped(fixName, "CTheScripts::Process could not be hooked.");
        return false;
    }
    g_frameHookInstalled = true;
    if (chained) {
        Log("CTheScripts::Process was already hooked by another module; the "
            "per-frame hook chains to it.");
    }
    return true;
}

void AddFrameCallback(FrameCallback callback) {
    const size_t count = g_frameCallbackCount.load(std::memory_order_relaxed);
    for (size_t i = 0; i < count; ++i) {
        if (g_frameCallbacks[i] == callback) {
            return;
        }
    }
    if (count < g_frameCallbacks.size()) {
        g_frameCallbacks[count] = callback;
        g_frameCallbackCount.store(count + 1, std::memory_order_release);
    }
}

} // namespace hff
