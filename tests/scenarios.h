#pragma once

#include "harness.h"

#include <cstddef>

// The scenarios the autotest can run, looked up by the names HFF_AUTOTEST
// lists.
namespace scenarios {

const harness::Scenario* Find(const char* name);

}  // namespace scenarios
