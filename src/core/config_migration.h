#pragma once

#include <string>

namespace hff {

// Moves the settings of older releases to their current keys and values.
void MigrateIniLayout(const std::string& iniPath);

} // namespace hff
