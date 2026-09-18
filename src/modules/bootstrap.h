#pragma once

#include "modules/prelude.h"

namespace hff {

struct InstallSummary {
    size_t installed{};
    size_t failed{};
    size_t disabled{};
};
extern InstallSummary g_installSummary;

struct FixSpec {
    const char* section;
    const char* key;
    const char* name;
    bool (*installer)();
    bool defaultOn{true};
};
void InstallFix(const char* section, const char* key, const char* name, bool (*installer)(), bool defaultOn = true);

template <size_t Count>
void InstallFixes(const FixSpec (&fixes)[Count]) {
    for (const auto& fix : fixes) {
        InstallFix(fix.section, fix.key, fix.name, fix.installer,
                   fix.defaultOn);
    }
}
DWORD WINAPI Initialize(void*);
void Shutdown();

} // namespace hff
