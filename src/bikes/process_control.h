#pragma once

#include "core/patch.h"

namespace hff::bikes {

// The CBike::ProcessControl detour, shared by the abandoned bike step and the
// BMX launch correction. The first fix that needs it installs it.
bool EnsureBikeProcessControlHook();
DetourPatch& BikeProcessControlPatch();

} // namespace hff::bikes
