#pragma once

#include <cstdint>

namespace hff {

// A vehicle's script handle plus one, or 0 when the pointer is not a live
// vehicle, so a vehicle created at a recycled address reads as a new one.
uint32_t VehicleIdentity(const void* vehicle);
// SA-MP drives other players' vehicles with the player status too, so only the
// driver tells the local player's vehicle apart.
bool IsDrivenByLocalPlayer(uintptr_t vehicle);

} // namespace hff
