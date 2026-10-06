#pragma once

namespace hff {

// MinHook is set up once for every fix that hooks through it and released at
// shutdown, after those hooks are removed.
bool InitializeMinHook();
void UninitializeMinHook();

} // namespace hff
