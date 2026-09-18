#pragma once

#include "modules/prelude.h"

namespace hff {

void MapWheelSampleThunk();
void MapZoomInGateThunk();
void MapZoomOutGateThunk();
void FireVehicleGateThunk();
void FireSpreadGateThunk();
void FireMergeGateThunk();
void DrunkSteerShiftThunk();
void FatCounterThunk();
void DoorForceChassisThunk();
void DoorDampingFiretruckThunk();
void DoorDampingOtherThunk();
void DoorIntegrationThunk();
void BikeLeanTargetThunk();
void ObjectFakePhysicsThunk();
void CarFakePhysicsThunk();
void BikeFakePhysicsThunk();
void TrailerFakePhysicsThunk();
void SirenTapThunk();
void ScriptsProcessThunk();
void ScriptSlideObjectThunk();
void ScriptRotateObjectThunk();
void SampObjectRotationThunk();
void SampObjectArrivalThunk();
void FallingGlassMoveThunk();
void FallingGlassTurnAThunk();
void FallingGlassTurnBThunk();
void BreakObjectLifetimeThunk();
void MenuBackgroundThunk();

} // namespace hff
