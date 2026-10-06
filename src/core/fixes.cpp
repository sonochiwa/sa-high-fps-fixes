#include "core/fixes.h"

#include "core/config.h"
#include "core/log.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

namespace hff {

namespace {

struct InstallSummary {
    size_t installed{};
    size_t failed{};
    size_t disabled{};
};

InstallSummary g_installSummary{};
std::atomic<bool> g_fixesInstalled{};

// The handling switches added since 1.3.0. `classicHandling=1` keeps all of
// them off, whatever their own keys say, so a vehicle drives and flies as it
// did before 1.3.0: planted at a high frame rate rather than as at 30 FPS.
constexpr const char* kHandlingKeys[] = {
    "turnAirResistance", "aircraftTurnResistance", "steerInputRate",
    "gearChangeInertia", "gearChangeKick", "suspensionDampingLimit",
    "suspensionLoadLean", "wheelSlipRate",
};
bool g_classicHandling = false;

bool IsHandlingKey(const char* section, const char* key) {
    if (_stricmp(section, "vehicles") != 0) {
        return false;
    }
    for (const char* handlingKey : kHandlingKeys) {
        if (_stricmp(handlingKey, key) == 0) {
            return true;
        }
    }
    return false;
}

void LogDisabled(const char* name, const char* reason) {
    std::string message(name);
    message += reason;
    Log(message.c_str());
}

} // namespace

void ReadClassicHandling() {
    g_classicHandling = ReadSetting("vehicles", "classicHandling", false);
}

void InstallFix(const char* section, const char* key, const char* name,
                bool (*installer)(), bool defaultOn) {
    if (g_classicHandling && IsHandlingKey(section, key)) {
        // The key stays a recognized one, whatever it says.
        RegisterConfigKey(section, key);
        ++g_installSummary.disabled;
        LogDisabled(name, " disabled by classicHandling.");
        return;
    }
    if (ReadSetting(section, key, defaultOn)) {
        CountInstall(installer());
        return;
    }
    ++g_installSummary.disabled;
    LogDisabled(name, " disabled by configuration.");
}

void CountInstall(bool installed) {
    if (installed) {
        ++g_installSummary.installed;
    } else {
        ++g_installSummary.failed;
    }
}

void CountDisabled(size_t fixes, const char* message) {
    g_installSummary.disabled += fixes;
    Log(message);
}

void LogInstallSummary() {
    char summary[192];
    std::snprintf(summary, sizeof(summary),
                  "Installation summary: %u installed, %u failed, %u disabled by configuration.",
                  static_cast<unsigned>(g_installSummary.installed),
                  static_cast<unsigned>(g_installSummary.failed),
                  static_cast<unsigned>(g_installSummary.disabled));
    Log(summary);
}

void MarkFixesInstalled() {
    g_fixesInstalled.store(true, std::memory_order_release);
}

bool FixesInstalled() {
    return g_fixesInstalled.load(std::memory_order_acquire);
}

} // namespace hff
