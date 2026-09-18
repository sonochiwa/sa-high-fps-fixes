#pragma once

#include "modules/prelude.h"

namespace hff {

// Executable families are detected separately from patch addresses. Compact
// and Hoodlum 1.0 US have different entry stubs but share the address layout
// used by this plugin. A future layout gets a new profile and address set
// instead of scattering version checks through every installer.
enum class GameAddressLayout : uint8_t {
    gtaSa10Us,
};

struct GameProfile {
    const char* name;
    uintptr_t signatureAddress;
    uint32_t signature;
    GameAddressLayout layout;
};

constexpr std::array<GameProfile, 2> kGameProfiles{{
    {"GTA SA 1.0 US Compact", 0x00401000, 0x53EC8B55,
     GameAddressLayout::gtaSa10Us},
    {"GTA SA 1.0 US Hoodlum", 0x00401000, 0x16197BE9,
     GameAddressLayout::gtaSa10Us},
}};

extern const GameProfile* g_activeGameProfile;
const GameProfile* DetectGameProfile();

} // namespace hff
