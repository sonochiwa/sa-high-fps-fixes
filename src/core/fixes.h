#pragma once

#include <cstddef>

namespace hff {

struct FixSpec {
    const char* section;
    const char* key;
    const char* name;
    bool (*installer)();
    bool defaultOn{true};
};

// Reads `classicHandling`. With it on, the handling switches added since
// 1.3.0 install nothing, whatever their own keys say.
void ReadClassicHandling();
// Installs the fix when its key is on and counts the result.
void InstallFix(const char* section, const char* key, const char* name,
                bool (*installer)(), bool defaultOn = true);

template <size_t Count>
void InstallFixes(const FixSpec (&fixes)[Count]) {
    for (const auto& fix : fixes) {
        InstallFix(fix.section, fix.key, fix.name, fix.installer,
                   fix.defaultOn);
    }
}

// Counts fixes installed outside InstallFix.
void CountInstall(bool installed);
void CountDisabled(size_t fixes, const char* message);
void LogInstallSummary();
// Set once the initialization thread has installed every fix, so a fix that
// patches later from the game thread does not race it.
void MarkFixesInstalled();
bool FixesInstalled();

} // namespace hff
