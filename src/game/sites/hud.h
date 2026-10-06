#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// HUD flashing. GTA flashes HUD elements by testing a bit of the frame counter:
//
//     if (CHud::m_ItemToFlash == ITEM && CTimer::m_FrameCounter & 8) -> skip
//
// Each address is the 4 byte absolute operand of the instruction that reads
// CTimer::m_FrameCounter to decide whether the element is hidden this frame:
// CHud::RenderArmorBar, RenderBreathBar, RenderHealthBar, DrawRadar and the
// two in DrawWanted.
constexpr uintptr_t kHudArmorBarOperand = 0x005890AF;
constexpr uintptr_t kHudBreathBarOperand = 0x0058919F;
constexpr uintptr_t kHudHealthBarOperand = 0x0058927E;
constexpr uintptr_t kHudRadarOperand = 0x0058A363;
constexpr uintptr_t kHudWantedActiveOperand = 0x0058DDBC;
constexpr uintptr_t kHudWantedEmptyOperand = 0x0058DE69;
// The instructions in front of the operands: `test byte ptr [m], imm8` and
// `mov bl, byte ptr [m]`.
constexpr std::array<uint8_t, 2> kHudTestPrefix{0xF6, 0x05};
constexpr std::array<uint8_t, 2> kHudMovPrefix{0x8A, 0x1D};
// The HUD flashes on `frameCounter & 8`, i.e. every 8 frames, which is 320 ms
// on and 320 ms off at the 25 FPS the game was tuned for.
constexpr unsigned kHudTicksPerFlash = 8;
constexpr unsigned kDefaultHudFlashIntervalMs = 320;

// HUD money counter. `CPlayerInfo::Process` walks `m_nDisplayMoney` toward
// `m_nMoney` by a fixed step chosen from how far apart they are, 12345, 1234,
// 123, 42 or 1, and applies it once per rendered frame with no timestep. At a
// high frame rate the counter runs through the difference that many times
// faster. The store is hooked rather than the step, so the size bands and their
// thresholds are the game's own.
constexpr uintptr_t kMoneyStepStore = 0x00570155;
constexpr uintptr_t kMoneyStepReturn = 0x0057015B;
constexpr size_t kPlayerInfoDisplayMoney = 0xBC;
// mov dword ptr [esi+0BCh],edx
constexpr std::array<uint8_t, 6> kExpectedMoneyStepStore{
    0x89, 0x96, 0xBC, 0x00, 0x00, 0x00
};

// Pause menu map zoom. The map input block gates every zoom and pan step behind
// a 20 ms tick:
//
//     panDelayPassed = GetTimeInMSPauseMode() - m_LastActionTime > 20;
//     ...
//     if (wheelUp || PgUp || shoulder) {
//         if (panDelayPassed) { m_fMapZoom += 7.0f; if (wheelUp) += 21.0f; }
//     }
//
// For a held key that is exactly right: the repeat rate is 50 Hz whatever the
// frame rate. The mouse wheel is not a held key. `isMouseWheelMovedUp` is a
// level flag rebuilt every frame from the DirectInput wheel delta, which is the
// movement since the previous poll, so one notch sets it for exactly one frame
// and it is clear on every other frame.
//
// At 30 FPS a frame is 33 ms, the tick has always passed, and every notch lands.
// At 60 FPS half of them are already lost. At 2000 FPS the flag is up for 0.5 ms
// out of every 20 ms tick, so roughly one notch in forty does anything and the
// zoom crawls.
//
// The fix lets a wheel notch through the gate regardless of the tick, and only a
// wheel notch: held keys and the shoulder buttons keep the 50 Hz repeat they
// were designed around, and panning is untouched. The bypass fires on the rising
// edge of the flag rather than its level, so one notch is always exactly one
// step even if a free-spinning wheel keeps the delta non-zero across several
// polls. At or below 30 FPS this is a no-op, because there the tick has already
// passed on every frame the flag is up.
//
// Site A is sampled once per frame from the map bounds block, which runs before
// either branch. It sits between a `test al,al` at 0x57741E and a `jge` at
// 0x57747D, so the thunk has to preserve the flags as well as the registers.
constexpr uintptr_t kMapWheelSample = 0x00577445;
constexpr uintptr_t kMapZoomInGate = 0x00577656;
constexpr uintptr_t kMapZoomInProceed = 0x0057765F;
constexpr uintptr_t kMapZoomInSkip = 0x00577918;
constexpr uintptr_t kMapZoomOutGate = 0x00577989;
constexpr uintptr_t kMapZoomOutProceed = 0x0057798E;
constexpr uintptr_t kMapZoomOutSkip = 0x005779FE;
constexpr uintptr_t kMouseWheelUpFlag = 0x00B7341B;
constexpr uintptr_t kMouseWheelDownFlag = 0x00B7341C;
// fld dword ptr ds:[008653F4h]
constexpr std::array<uint8_t, 6> kExpectedMapWheelSample{
    0xD9, 0x05, 0xF4, 0x53, 0x86, 0x00
};
// cmp eax,14h / jbe 00577918
constexpr std::array<uint8_t, 9> kExpectedMapZoomInGate{
    0x83, 0xF8, 0x14, 0x0F, 0x86, 0xB9, 0x02, 0x00, 0x00
};
// cmp eax,14h / jbe 005779FE
constexpr std::array<uint8_t, 5> kExpectedMapZoomOutGate{
    0x83, 0xF8, 0x14, 0x76, 0x70
};

} // namespace hff
