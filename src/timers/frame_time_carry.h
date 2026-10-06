#pragma once

namespace hff::timers {

// Each switch repoints its own group of the `_ftol` sites in
// game\sites\timers.h.
bool InstallSkillProgressFix();
bool InstallStuntCountersFix();
bool InstallUpsideDownTimerFix();
bool InstallTaskTimersFix();
bool InstallVehicleTimersFix();
bool InstallIdleCameraTimerFix();
bool InstallHudTimersFix();
bool InstallBurnTimersFix();
bool InstallGangWarTimerFix();

} // namespace hff::timers
