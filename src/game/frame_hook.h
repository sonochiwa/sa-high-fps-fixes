#pragma once

namespace hff {

using FrameCallback = void (*)();

// Hooks `CTheScripts::Process`, which runs once a frame on the game thread,
// for the callbacks below. The first fix that needs it installs it; `fixName`
// names that fix in the log when the hook cannot be installed.
bool InstallFrameHook(const char* fixName);
void AddFrameCallback(FrameCallback callback);

} // namespace hff
