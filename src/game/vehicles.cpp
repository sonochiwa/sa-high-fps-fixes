#include "game/vehicles.h"

#include "game/addresses.h"

#include <windows.h>

namespace hff {

uint32_t VehicleIdentity(const void* vehicle) {
    __try {
        const auto pool = *reinterpret_cast<const uintptr_t*>(kVehiclePool);
        if (!vehicle || !pool) {
            return 0;
        }
        const auto storage = *reinterpret_cast<const uintptr_t*>(pool + kPoolStorage);
        const auto* states = *reinterpret_cast<const uint8_t* const*>(
            pool + kPoolSlotStates);
        const auto capacity = *reinterpret_cast<const int32_t*>(pool + kPoolCapacity);
        const auto address = reinterpret_cast<uintptr_t>(vehicle);
        if (address < storage || (address - storage) % kVehiclePoolElementSize != 0) {
            return 0;
        }
        const auto index = (address - storage) / kVehiclePoolElementSize;
        if (capacity <= 0 || index >= static_cast<uintptr_t>(capacity)
            || (states[index] & 0x80) != 0) {
            return 0;
        }
        return ((static_cast<uint32_t>(index) << 8) | states[index]) + 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool IsDrivenByLocalPlayer(uintptr_t vehicle) {
    const auto player = *reinterpret_cast<const uintptr_t*>(kWorldPlayers);
    return player
        && *reinterpret_cast<const uintptr_t*>(vehicle + kVehicleDriverOffset) == player;
}

} // namespace hff
