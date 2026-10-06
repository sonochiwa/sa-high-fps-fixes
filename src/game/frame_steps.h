#pragma once

namespace hff {

// Stand-ins for the three per-frame steps in game\addresses.h. Each is
// entered by a `call` that replaces the six byte instruction, with the value
// being stepped alone on the x87 stack, and leaves the stepped value there.
void FrameStepDecrementThunk();
void FrameStepIncrementThunk();
void FrameStepDecayThunk();

} // namespace hff
