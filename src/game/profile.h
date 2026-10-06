#pragma once

#include "game/addresses.h"

namespace hff {

// The profile of the running executable, or null for any other executable.
const GameProfile* DetectGameProfile();

} // namespace hff
