#pragma once

#include "modules/prelude.h"

namespace hff {

bool InstallGangWarTimerFix();
bool InstallScriptObjectSlideFix();
bool InstallScriptRotateObjectFix();
uintptr_t FindUniqueCodePattern(HMODULE module, const uint8_t* pattern, size_t size);
bool InstallSampObjectRotationFix();
bool InstallFallingGlassFix();
bool InstallBreakableObjectLifetimeFix();
bool InstallBikeLeanTargetFix();
bool InstallBikePitchExperiment();
bool InstallPhysicsSleepRateFix();
bool InstallSirenTapFix();
bool InstallParticleEmissionRateFix();
bool InstallContinuousWeaponParticlesFix();
bool InstallDrowningDamageFix();
bool InstallContinuousWeaponAmmoFix();
bool InstallChainsawStrikeRateFix();
bool InstallFrameLimit(int limit);
bool InstallRefreshRate(int refreshRate);
bool InstallAutoFpsLimit();
bool InstallConflictingHookGuard();

} // namespace hff
