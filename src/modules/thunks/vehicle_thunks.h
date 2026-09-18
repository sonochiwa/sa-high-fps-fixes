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

} // namespace hff
