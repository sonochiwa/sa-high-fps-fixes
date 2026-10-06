#pragma once

#include <cstdint>

namespace hff::framerate {

// Applies `fpsLimit` when it is set.
void InstallFrameLimit();
// Whether `fpsLimit` holds the frame limiter gate open for good.
bool FrameLimitHoldsGate();
void WriteFrameLimit(uint8_t value);
uint8_t ReadFrameLimit();

} // namespace hff::framerate
