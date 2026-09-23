#pragma once

#include "modules/prelude.h"

namespace hff {

void WheelSpinDecelThunk();
void WheelSpinAccelThunk();
void WheelSpinDampThunk();
void BikeWheelPitchIntegrateThunk();
void JetPackFxRampUpThunk();
void JetPackFxRampDownThunk();
void HeadBopRampUpThunk();
void HeadBopRampDownThunk();
void BmxLeanLeftDecayThunk();
void BmxLeanFwdDecayThunk();
void WheelSettleThunk();
void JumpOutDampThunk();
void TurnAirResistanceThunk();
void CarSteerInputAThunk();
void CarSteerInputBThunk();
void BikeSteerInputAThunk();
void BikeSteerInputBThunk();
void TransmissionInertiaThunk();
void TransmissionSmootherThunk();
void WheelSlipRightThunk();
void WheelSlipCoastThunk();
void MovingPartStepThunk();

} // namespace hff
