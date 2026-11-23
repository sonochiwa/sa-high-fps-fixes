#pragma once

namespace hff::handling {

bool InstallTurnAirResistanceFix();
bool InstallAircraftTurnResistanceFix();
// The boat handling fix's share of the `0.99`: boats shed their turn at the
// 30 FPS rate per second.
bool InstallBoatTurnResistance();

} // namespace hff::handling
