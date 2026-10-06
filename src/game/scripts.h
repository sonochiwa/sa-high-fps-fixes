#pragma once

#include <cstdint>

namespace hff {

// Compares a running script's eight-character `SCRIPT_NAME` without regard to
// case.
bool ScriptNameMatches(const char* name, const char* expected);
// The active script with that name, or 0.
uintptr_t FindRunningScript(const char* name);
// The first active script, or 0; each script links to the next at offset 0.
uintptr_t FirstRunningScript();

} // namespace hff
