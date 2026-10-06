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
bool InstallMissionTimersFix();
// Parts of the explosion fix in world\explosions and the damaged plane control
// fix in vehicles\aircraft.
bool InstallExplosionFuelTimerCarry();
bool InstallPlaneDamageWaveCarry();

} // namespace hff::timers
