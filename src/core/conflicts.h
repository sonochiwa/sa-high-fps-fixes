#pragma once

#include <cstddef>
#include <cstdint>

namespace hff {

struct ForeignBranch {
    bool found{};
    // True when the foreign padding may run past the end of this plugin's
    // span, so the bytes after it need restoring too.
    bool mayOverrun{};
    uintptr_t target{};
};

// Records where the game and the plugin are mapped and whether
// `overrideConflictingHooks` lets the plugin win at contested sites.
void InitializeConflictGuard(bool overrideEnabled);
bool ConflictGuardEnabled();
// A site other plugins hook by design, such as a function entry they chain
// through. It is never taken over.
void ExemptFromConflictGuard(uintptr_t address);
bool SiteMayOverrideConflicts(uintptr_t address);
// Decides whether the `size` bytes at `address`, which no longer equal
// `reference`, differ from it only by a foreign hook this plugin may safely
// write over.
ForeignBranch AnalyzeForeignBranch(uintptr_t address, const uint8_t* reference,
                                   size_t size);
void LogSiteTakeover(uintptr_t address, uintptr_t target, const char* how);
// Called once a frame from the game thread once every fix is installed.
void __cdecl GuardInstalledSites();

} // namespace hff
