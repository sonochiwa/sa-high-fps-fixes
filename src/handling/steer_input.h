#pragma once

namespace hff::handling {

bool InstallSteerInputRateFix();
// The share of the gap to the stick a plane's control surfaces close this
// frame, with or without this fix.
float PlaneSteerShare();

} // namespace hff::handling
