#include "game/profile.h"

#include "game/addresses.h"

#include <windows.h>

namespace hff {

const GameProfile* DetectGameProfile() {
    if (reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) != kImageBase) {
        return nullptr;
    }
    __try {
        for (const auto& profile : kGameProfiles) {
            if (*reinterpret_cast<const uint32_t*>(profile.signatureAddress)
                == profile.signature) {
                return &profile;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

} // namespace hff
