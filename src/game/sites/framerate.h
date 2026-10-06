#pragma once

#include <array>
#include <cstdint>

namespace hff {

// San Andreas gates its whole frame limiter behind the menu preference
// `CMenuManager::m_bPrefsFrameLimiter`. With it off the limiter code is jumped
// over and `RsGlobal.frameLimit` is never consulted, so writing a limit alone
// does nothing. Turning the branch into an unconditional jump runs the limiter
// without touching the saved preference.
constexpr uintptr_t kFrameLimiterGate = 0x00748D68;
constexpr std::array<uint8_t, 2> kExpectedFrameLimiterGate{0x75, 0x17};
// The store of the default 30 into `RsGlobal.frameLimit`.
constexpr uintptr_t kFrameLimitStore = 0x00619620;
constexpr uintptr_t kFrameLimitStoreOperand = 0x00619626;
constexpr std::array<uint8_t, 10> kExpectedFrameLimitStore{
    0xC7, 0x05, 0x4C, 0x70, 0xC1, 0x00, 0x1E, 0x00, 0x00, 0x00
};

// Past the gate, the main loop still runs the limiter while
// `CAudioEngine::IsBeatInfoPresent` says the music playing has a beat track,
// which the radio of the car the player sits in does. A car ride is then held
// to `RsGlobal.frameLimit`, 30 FPS. The call is redirected so that only the
// minigames danced or bounced to the beat keep the limiter.
constexpr uintptr_t kFrameLimiterBeatCheck = 0x00748D78;
constexpr uintptr_t kAudioEngineIsBeatInfoPresent = 0x005071D0;
// call CAudioEngine::IsBeatInfoPresent, from the main loop
constexpr std::array<uint8_t, 5> kExpectedFrameLimiterBeatCheck{
    0xE8, 0x53, 0xE4, 0xDB, 0xFF
};

// The tail jump that draws the menu background, reached on every frame a menu
// is drawn: the front end, the pause menu and the SA-MP menu.
constexpr uintptr_t kMenuBackground = 0x0057C324;
constexpr uintptr_t kMenuBackgroundTarget = 0x0057B750;
constexpr std::array<uint8_t, 5> kExpectedMenuBackground{
    0xE9, 0x27, 0xF4, 0xFF, 0xFF
};

} // namespace hff
