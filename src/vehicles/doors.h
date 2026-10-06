#pragma once

namespace hff::vehicles {

bool InstallDoorSwingFix();
// Puts back the chassis sway `disableSwingingCompletely` switched off.
void RestoreDoorSwinging();

} // namespace hff::vehicles
