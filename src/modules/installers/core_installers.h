#pragma once

#include "modules/prelude.h"

namespace hff {

bool InstallStuntJumpCameraFix();
bool InstallAimCameraShakeFix();
bool InstallDrunkCameraShakeFix();
bool InstallAimingRifleWalkFix();
bool InstallSwimmingMovementFix();
bool InstallFollowCameraRateFix();
bool InstallAttachedEntitySpeedFix();
bool InstallAiAircraftSteerFix();
bool InstallMoneyCounterFix();
bool InstallClimbSpeedFix();
bool InstallWaterBuoyancyFix();
bool InstallPedPushVehicleFix();
bool InstallBloodyFootprintsFix();
bool InstallWheelFrictionFix();
bool InstallAbandonedBikePhysicsStepFix();
bool InstallRailWheelSpinFix();
bool InstallBurnoutFix();
bool InstallSkimmerResistanceFix();
bool InstallHeliRotorSpeedFix();
bool InstallHudFlashRateFix();
bool InstallVehicleRestThresholdFix();
bool RepointCall(SitePatch& patch, uintptr_t address, uintptr_t expectedCallee, const void* replacement);
bool InstallBikeBalanceTrace();
bool InstallTruncCarryGroup(uint8_t group, const char* what);
bool InstallSkillProgressFix();
bool InstallStuntCountersFix();
bool InstallUpsideDownTimerFix();
bool InstallTaskTimersFix();
bool InstallVehicleTimersFix();
bool InstallIdleCameraTimerFix();
bool InstallHudTimersFix();
bool InstallBurnTimersFix();

} // namespace hff
