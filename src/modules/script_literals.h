#pragma once

#include "modules/prelude.h"

namespace hff {

extern bool g_parachuteFlight;
extern bool g_burglaryNoise;

bool InstallParachuteFlightFix();
bool InstallBurglaryNoiseFix();
void UpdateScriptLiterals();

} // namespace hff
