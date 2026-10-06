#pragma once

namespace hff::framerate {

// Reads the per-case limits of [framerate] and installs the automatic limit
// when any case is on.
void InstallAutoFpsLimit();

} // namespace hff::framerate
