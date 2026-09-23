#pragma once

#include "modules/prelude.h"

// Sites where the game truncates a frame's worth of time to a whole number,
// repointed at one wrapper that carries the fraction: stat, stunt, task,
// vehicle, idle camera, HUD, burn and gang war counters.

namespace hff {

// Skill progress. Every counter that levels a stat through use is advanced by
// `CStats::UpdateStatsWhen*` with the same three instructions:
//
//     fld ms_fTimeStep / fmul 0.02 / fmul 1000.0 / call _ftol
//
// which is milliseconds of frame time, `1000 / FPS`, truncated to an integer.
// At 30 FPS that is 33 and almost nothing is lost. At 144 FPS it is 6.94
// truncated to 6, losing 13 per cent of every second; at 400 FPS 2.5 becomes 2
// and a fifth is gone; at 600 FPS 1.67 becomes 1 and two fifths are gone; above
// 1000 FPS the value truncates to zero and the counter stops advancing
// altogether. Several of the functions truncate a second time, when they store
// `oldTotal + milliseconds * rate` back into the counter, and that store loses
// its fraction every frame as well.
//
// Twenty one call sites, all of them `call _ftol`, are repointed at one wrapper
// that carries the discarded fraction into the next frame. The wrapper
// identifies the site by its return address, so each counter gets its own
// carry.
constexpr uintptr_t kFtol = 0x00821B40;

// Every site carries the fraction and does nothing else.
struct StatTruncSite {
    uintptr_t address;
    uint8_t group;
};

// Which fix owns a site, so the three groups can be switched on separately
// even though they share one wrapper and one carry array.
constexpr uint8_t kTruncGroupStats = 0;
constexpr uint8_t kTruncGroupStunt = 1;
constexpr uint8_t kTruncGroupUpsideDown = 2;
constexpr uint8_t kTruncGroupTask = 3;
constexpr uint8_t kTruncGroupVehicle = 4;
constexpr uint8_t kTruncGroupIdleCam = 5;
constexpr uint8_t kTruncGroupHud = 6;
constexpr uint8_t kTruncGroupBurn = 7;
constexpr uint8_t kTruncGroupWorld = 8;

// The counter each site feeds is named so the next reader does not have to
// chase the global back through the disassembly.
constexpr std::array<StatTruncSite, 103> kStatTruncSites{{
    {0x0055C5C3, kTruncGroupStats},  // m_FatCounter, milliseconds
    {0x0055C64E, kTruncGroupStats},  // m_MaxHealthCounter
    {0x0055C6DE, kTruncGroupStats},  // m_SprintStaminaCounter
    {0x0055C76E, kTruncGroupStats},  // m_RunningCounter
    {0x0055C838, kTruncGroupStats},  // m_CycleStaminaCounter
    {0x0055C94B, kTruncGroupStats},  // m_CycleSkillCounter, milliseconds
    {0x0055C972, kTruncGroupStats},  // m_CycleSkillCounter, the increment
    {0x0055CA26, kTruncGroupStats},  // m_SwimStaminaCounter
    {0x0055CAA8, kTruncGroupStats},  // m_SwimUnderWaterCounter
    {0x0055CB91, kTruncGroupStats},  // m_DrivingCounter, milliseconds
    {0x0055CBB2, kTruncGroupStats},  // m_DrivingCounter, the store
    {0x0055CBD2, kTruncGroupStats},  // m_DrivingCounter, milliseconds, second branch
    {0x0055CBF3, kTruncGroupStats},  // m_DrivingCounter, the store, second branch
    {0x0055CCE4, kTruncGroupStats},  // m_FlyingCounter, milliseconds
    {0x0055CD05, kTruncGroupStats},  // m_FlyingCounter, the store
    {0x0055CD25, kTruncGroupStats},  // m_FlyingCounter, milliseconds, second branch
    {0x0055CD46, kTruncGroupStats},  // m_FlyingCounter, the store, second branch
    {0x0055CE33, kTruncGroupStats},  // m_BikeCounter, milliseconds
    {0x0055CE54, kTruncGroupStats},  // m_BikeCounter, the store
    {0x0055CE76, kTruncGroupStats},  // m_BikeCounter, milliseconds, second branch
    {0x0055CE97, kTruncGroupStats},   // m_BikeCounter, the store, second branch

    // Stunt counters in `CPlayerInfo::Process`. Each stunt keeps a millisecond
    // counter and a grace buffer that lets the stunt survive a brief
    // interruption, and both are advanced by the same truncated frame time.
    {0x0056F9E7, kTruncGroupStunt},        // m_nCarLess3WheelCounter += ms
    {0x0056FA7B, kTruncGroupStunt},        // m_nCarTwoWheelCounter += ms
    {0x0056FAC8, kTruncGroupStunt},        // two wheels, grace buffer decay
    {0x0056FBAA, kTruncGroupStunt},        // m_nCarTwoWheelCounter, branch 2
    {0x0056FBF7, kTruncGroupStunt},        // two wheels, decay, branch 2
    {0x0056FC2D, kTruncGroupStunt},        // two wheels, the buffer store
    {0x0056FC67, kTruncGroupStunt},        // two wheels, grace buffer refill
    {0x0056FE0D, kTruncGroupStunt},        // m_nBikeRearWheelCounter += ms
    {0x0056FE5A, kTruncGroupStunt},        // wheelie, grace buffer decay
    {0x0056FE90, kTruncGroupStunt},        // wheelie, the buffer store
    {0x0056FFA4, kTruncGroupStunt},        // wheelie, grace buffer refill
    {0x00570003, kTruncGroupStunt},        // m_nBikeFrontWheelCounter += ms
    {0x00570050, kTruncGroupStunt},        // stoppie, grace buffer decay

    // `CUpsideDownCarCheck::UpdateTimers` reads the frame time once and adds it
    // to the timer of every car it is watching that is currently on its roof.
    {0x004655F9, kTruncGroupUpsideDown},

    // Ped and player task timers. Each one is `counter += frame milliseconds`
    // with an else branch that resets the counter to zero, and each is compared
    // against a threshold in milliseconds a few instructions later.
    {0x0060D3AD, kTruncGroupTask},    // CPlayerPed::EvaluateTarget, +0x8A vs 1200 ms
    {0x0060D47C, kTruncGroupTask},    // CPlayerPed::EvaluateTarget, +0x88 vs 1200 ms
    {0x006299B6, kTruncGroupTask},    // CTaskSimpleStealthKill::ManageAnim, +0x12
    {0x006809A2, kTruncGroupTask},    // CTaskSimpleInAir::ProcessPed, +0x28
    {0x0068132B, kTruncGroupTask},    // CTaskSimpleClimb::ProcessPed, +0x28 vs 1000 ms
    {0x0068790A, kTruncGroupTask},    // PlayerControlFighter, +0x10

    // Vehicle timers of the same shape.
    {0x0041F28D, kTruncGroupVehicle}, // CCarCtrl::UpdateCarAI, +0x4DC
    {0x006D9702, kTruncGroupVehicle}, // CVehicle::FlyingControl, +0x9A0

    // How long the player has been idle before the camera starts drifting.
    {0x0050A22F, kTruncGroupIdleCam},  // CIdleCam::ProcessIdleCamTicker, +0x94

    // Every timer behind the HUD's timed text and bars. All 46 are the same
    // accumulation of the truncated frame time, some into a global, some into
    // a register that is stored a few instructions later, some negated first
    // because they count down. None of them compares rather than accumulates,
    // which is why the whole block can be carried without picking through it
    // site by site.
    {0x0058AB42, kTruncGroupHud},
    {0x0058ABC4, kTruncGroupHud},
    {0x0058AC26, kTruncGroupHud},
    {0x0058ACFA, kTruncGroupHud},
    {0x0058AF5F, kTruncGroupHud},
    {0x0058AFAD, kTruncGroupHud},
    {0x0058AFD4, kTruncGroupHud},
    {0x0058B05F, kTruncGroupHud},
    {0x0058B894, kTruncGroupHud},
    {0x0058B8E6, kTruncGroupHud},
    {0x0058B938, kTruncGroupHud},
    {0x0058BA22, kTruncGroupHud},
    {0x0058C938, kTruncGroupHud},
    {0x0058C9C4, kTruncGroupHud},
    {0x0058CB5C, kTruncGroupHud},
    {0x0058CFCD, kTruncGroupHud},
    {0x0058D0B5, kTruncGroupHud},
    {0x0058D31B, kTruncGroupHud},
    {0x0058D388, kTruncGroupHud},
    {0x0058D655, kTruncGroupHud},
    {0x0058D6B1, kTruncGroupHud},
    {0x0058D713, kTruncGroupHud},
    {0x0058DA32, kTruncGroupHud},
    {0x0058DA7A, kTruncGroupHud},
    {0x0058DAF0, kTruncGroupHud},
    {0x0058DB8F, kTruncGroupHud},
    {0x0058DBC5, kTruncGroupHud},
    {0x0058DC3B, kTruncGroupHud},
    {0x0058ECB2, kTruncGroupHud},
    {0x0058ECF8, kTruncGroupHud},
    {0x0058ED52, kTruncGroupHud},
    {0x0058ED96, kTruncGroupHud},
    {0x0058EDCA, kTruncGroupHud},
    {0x0058EE24, kTruncGroupHud},
    {0x0058F237, kTruncGroupHud},
    {0x0058F293, kTruncGroupHud},
    {0x0058F2F5, kTruncGroupHud},
    {0x0058F38A, kTruncGroupHud},
    {0x0058F3D4, kTruncGroupHud},
    {0x0058F436, kTruncGroupHud},
    {0x0058F69F, kTruncGroupHud},
    {0x0058F6E7, kTruncGroupHud},
    {0x0058F75D, kTruncGroupHud},
    {0x0058F7FD, kTruncGroupHud},
    {0x0058F833, kTruncGroupHud},
    {0x0058F8A9, kTruncGroupHud},

    // Vehicle burn timers. Each adds the frame time to a float counter and
    // compares it against the threshold at 0x86CD78, which is how long a
    // burning vehicle has before it explodes. The first of the three car sites
    // scales by 0.2 first, so that state burns down more slowly.
    {0x006A7138, kTruncGroupBurn},   // CAutomobile::ProcessCarOnFireAndExplode, +0x8E4
    {0x006A7250, kTruncGroupBurn},   // the same counter, scaled by 0.2
    {0x006A7281, kTruncGroupBurn},   // the same counter, third branch
    {0x006BBD16, kTruncGroupBurn},   // CBike::ProcessControl, +0x7BC
    {0x006F1E9A, kTruncGroupBurn},   // CBoat::ProcessControl, +0x608

    // More ped task timers of the shape already covered by taskTimers.
    {0x00624C06, kTruncGroupTask},   // CTaskSimpleFightingControl::CalcMoveCommand, +0x1C
    {0x006298AA, kTruncGroupTask},   // CTaskSimpleStealthKill::ManageAnim, +0x1C vs 10000 ms
    {0x0062D855, kTruncGroupTask},   // CTaskComplexKillPedOnFootArmed, +0x16 counting down
    {0x00644595, kTruncGroupTask},   // CTaskSimpleCarDrive::ProcessPed, +0x4C vs 2000 ms
    {0x006944DA, kTruncGroupTask},   // CTaskSimpleDuck::ProcessPed, +0x0E from +0x0C

    // One more vehicle timer.
    {0x006C938F, kTruncGroupVehicle}, // CPlane::ProcessControl, +0x9FC

    // Gang war countdown, both branches of the same global at 0x96AB44.
    {0x00446BB4, kTruncGroupWorld},
    {0x00446CDC, kTruncGroupWorld}

}};

} // namespace hff
