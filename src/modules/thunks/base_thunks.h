#pragma once

#include "modules/prelude.h"

namespace hff {

void FlightTimerThunk();
void EndTimerThunk();
void DrowningDamageThunk();
void BloodyFootprintCounterThunk();
void BloodyFootLandedSideThunk();
void BloodyFootprintShadowThunk();
void ChainsawStrikeRewindThunk();
void FightStrikeTraceThunk();
void ContinuousWeaponAmmoThunk();
void WheelFrictionCarDriveThunk();
void WheelFrictionCarBrakeThunk();
void WheelFrictionBikeBaseThunk();
void WheelFrictionBikeDriveThunk();
void WheelFrictionBikeBrakeThunk();
void FollowPedCameraRateThunk();
void FollowCarCameraRateThunk();
void AimWeaponFovStepThunk();
void AttachedEntitySpeedThunk();
void AiAircraftSteerRateThunk();
void StatTruncCarryThunk();
void MoneyStepThunk();
void ClimbSpeedClampThunk();
void PickUpAlignThunk();
void BuoyancyThresholdThunk();
void BuoyancyClampedStoreThunk();
void SwimResistanceThunk();
void AimingRifleWalkThunk();
void SkimmerResistanceThunk();
void BurnoutThunk();
void HeliRotorSlowThunk();
void HeliRotorFastThunk();
void RailWheelSpinThunk0();
void RailWheelSpinThunk1();
void RailWheelSpinThunk2();
void RailWheelSpinThunk3();
void PedPushCarThunk();
void CarRestThresholdThunk();
void BikeRestThresholdThunk();
void TrailerRestThresholdThunk();
void DrunkCameraPhaseThunk();

} // namespace hff
