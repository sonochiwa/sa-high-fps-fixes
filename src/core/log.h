#pragma once

#include <string>

namespace hff {

// Starts the log afresh at `path` with the plugin name and version as its
// first line. Until it is called, Log writes nothing.
void OpenLog(const std::string& path);
void Log(const char* message);

} // namespace hff
