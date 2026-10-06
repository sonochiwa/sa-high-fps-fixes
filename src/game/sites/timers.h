#pragma once

#include <array>
#include <cstdint>

// Sites where the game truncates a frame's worth of time to a whole number,
// repointed at one wrapper that carries the fraction: stat, stunt, task,
// vehicle, idle camera, HUD, burn, gang war and mission counters.

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
constexpr uint8_t kTruncGroupMission = 9;
constexpr uint8_t kTruncGroupScript = 10;
constexpr uint8_t kTruncGroupExplosion = 11;
constexpr uint8_t kTruncGroupPlaneDamage = 12;

// `CTheScripts::Process` adds the truncated frame time to TIMERA and TIMERB of
// every script. SilentPatch replaces this call with its own carry of the same
// fraction, so the site is only taken while it still calls `_ftol`.
constexpr uintptr_t kScriptTimerTruncCall = 0x0046A036;

// The counter each site feeds is named so the next reader does not have to
// chase the global back through the disassembly.
constexpr std::array<StatTruncSite, 118> kStatTruncSites{{
    // m_FatCounter, milliseconds
    {0x0055C5C3, kTruncGroupStats},
    // m_MaxHealthCounter
    {0x0055C64E, kTruncGroupStats},
    // m_SprintStaminaCounter
    {0x0055C6DE, kTruncGroupStats},
    // m_RunningCounter
    {0x0055C76E, kTruncGroupStats},
    // m_CycleStaminaCounter
    {0x0055C838, kTruncGroupStats},
    // m_CycleSkillCounter, milliseconds
    {0x0055C94B, kTruncGroupStats},
    // m_CycleSkillCounter, the increment
    {0x0055C972, kTruncGroupStats},
    // m_SwimStaminaCounter
    {0x0055CA26, kTruncGroupStats},
    // m_SwimUnderWaterCounter
    {0x0055CAA8, kTruncGroupStats},
    // m_DrivingCounter, milliseconds
    {0x0055CB91, kTruncGroupStats},
    // m_DrivingCounter, the store
    {0x0055CBB2, kTruncGroupStats},
    // m_DrivingCounter, milliseconds, second branch
    {0x0055CBD2, kTruncGroupStats},
    // m_DrivingCounter, the store, second branch
    {0x0055CBF3, kTruncGroupStats},
    // m_FlyingCounter, milliseconds
    {0x0055CCE4, kTruncGroupStats},
    // m_FlyingCounter, the store
    {0x0055CD05, kTruncGroupStats},
    // m_FlyingCounter, milliseconds, second branch
    {0x0055CD25, kTruncGroupStats},
    // m_FlyingCounter, the store, second branch
    {0x0055CD46, kTruncGroupStats},
    // m_BikeCounter, milliseconds
    {0x0055CE33, kTruncGroupStats},
    // m_BikeCounter, the store
    {0x0055CE54, kTruncGroupStats},
    // m_BikeCounter, milliseconds, second branch
    {0x0055CE76, kTruncGroupStats},
    // m_BikeCounter, the store, second branch
    {0x0055CE97, kTruncGroupStats},

    // Stunt counters in `CPlayerInfo::Process`. Each stunt keeps a millisecond
    // counter and a grace buffer that lets the stunt survive a brief
    // interruption, and both are advanced by the same truncated frame time.
    // m_nCarLess3WheelCounter += ms
    {0x0056F9E7, kTruncGroupStunt},
    // m_nCarTwoWheelCounter += ms
    {0x0056FA7B, kTruncGroupStunt},
    // two wheels, grace buffer decay
    {0x0056FAC8, kTruncGroupStunt},
    // m_nCarTwoWheelCounter, branch 2
    {0x0056FBAA, kTruncGroupStunt},
    // two wheels, decay, branch 2
    {0x0056FBF7, kTruncGroupStunt},
    // two wheels, the buffer store
    {0x0056FC2D, kTruncGroupStunt},
    // two wheels, grace buffer refill
    {0x0056FC67, kTruncGroupStunt},
    // m_nBikeRearWheelCounter += ms
    {0x0056FE0D, kTruncGroupStunt},
    // wheelie, grace buffer decay
    {0x0056FE5A, kTruncGroupStunt},
    // wheelie, the buffer store
    {0x0056FE90, kTruncGroupStunt},
    // wheelie, grace buffer refill
    {0x0056FFA4, kTruncGroupStunt},
    // m_nBikeFrontWheelCounter += ms
    {0x00570003, kTruncGroupStunt},
    // stoppie, grace buffer decay
    {0x00570050, kTruncGroupStunt},

    // `CUpsideDownCarCheck::UpdateTimers` reads the frame time once and adds it
    // to the timer of every car it is watching that is currently on its roof.
    {0x004655F9, kTruncGroupUpsideDown},

    // Ped and player task timers. Each one is `counter += frame milliseconds`
    // with an else branch that resets the counter to zero, and each is compared
    // against a threshold in milliseconds a few instructions later.
    // CPlayerPed::EvaluateTarget, +0x8A vs 1200 ms
    {0x0060D3AD, kTruncGroupTask},
    // CPlayerPed::EvaluateTarget, +0x88 vs 1200 ms
    {0x0060D47C, kTruncGroupTask},
    // CTaskSimpleStealthKill::ManageAnim, +0x12
    {0x006299B6, kTruncGroupTask},
    // CTaskSimpleInAir::ProcessPed, +0x28
    {0x006809A2, kTruncGroupTask},
    // CTaskSimpleClimb::ProcessPed, +0x28 vs 1000 ms
    {0x0068132B, kTruncGroupTask},
    // PlayerControlFighter, +0x10
    {0x0068790A, kTruncGroupTask},

    // Vehicle timers of the same shape.
    // CCarCtrl::UpdateCarAI, +0x4DC
    {0x0041F28D, kTruncGroupVehicle},
    // CVehicle::FlyingControl, +0x9A0
    {0x006D9702, kTruncGroupVehicle},

    // How long the player has been idle before the camera starts drifting.
    // CIdleCam::ProcessIdleCamTicker, +0x94
    {0x0050A22F, kTruncGroupIdleCam},

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
    // CAutomobile::ProcessCarOnFireAndExplode, +0x8E4
    {0x006A7138, kTruncGroupBurn},
    // the same counter, scaled by 0.2
    {0x006A7250, kTruncGroupBurn},
    // the same counter, third branch
    {0x006A7281, kTruncGroupBurn},
    // CBike::ProcessControl, +0x7BC
    {0x006BBD16, kTruncGroupBurn},
    // CBoat::ProcessControl, +0x608
    {0x006F1E9A, kTruncGroupBurn},

    // More ped task timers of the shape already covered by taskTimers.
    // CTaskSimpleFightingControl::CalcMoveCommand, +0x1C
    {0x00624C06, kTruncGroupTask},
    // CTaskSimpleStealthKill::ManageAnim, +0x1C vs 10000 ms
    {0x006298AA, kTruncGroupTask},
    // CTaskComplexKillPedOnFootArmed, +0x16 counting down
    {0x0062D855, kTruncGroupTask},
    // CTaskSimpleCarDrive::ProcessPed, +0x4C vs 2000 ms
    {0x00644595, kTruncGroupTask},
    // CTaskSimpleDuck::ProcessPed, +0x0E from +0x0C
    {0x006944DA, kTruncGroupTask},

    // One more vehicle timer.
    // CPlane::ProcessControl, +0x9FC
    {0x006C938F, kTruncGroupVehicle},

    // Gang war countdown, both branches of the same global at 0x96AB44.
    {0x00446BB4, kTruncGroupWorld},
    {0x00446CDC, kTruncGroupWorld},
    // How long until rival gangs next attack a territory.
    // CGangWars::Update
    {0x00446DEC, kTruncGroupWorld},

    // Ped task timers that count down.
    // CTaskSimpleFall::ProcessPed, +0x1C, how long a fallen ped stays down
    {0x0067FB47, kTruncGroupTask},
    // the same timer, second branch
    {0x0067FBB3, kTruncGroupTask},
    // CTaskSimpleChoking::ProcessPed, +0x10
    {0x00620532, kTruncGroupTask},
    // CTaskSimpleGunControl::ProcessPed, +0x28
    {0x00625664, kTruncGroupTask},

    // Vehicle timers that scale the frame time by 16.67 instead of 20.
    // CVehicle::ProcessDelayedExplosion, +0x4DE, a car bomb's fuse
    {0x006D135C, kTruncGroupVehicle},
    // CVehicle::ProcessCarAlarm, +0x45C, compared with the time left
    {0x006D221B, kTruncGroupVehicle},
    // the same timer, then subtracted from it
    {0x006D223F, kTruncGroupVehicle},
    // CCarAI::UpdateCarAI, +0x4DC, how long a ramming or blocking police car
    // has waited, two more branches of the timer at 0x41F28D
    {0x0041DE6A, kTruncGroupVehicle},
    {0x0041E1D5, kTruncGroupVehicle},

    // Mission countdowns on screen.
    // COnscreenTimerEntry::Process
    {0x0044CB31, kTruncGroupMission},
    // TIMERA and TIMERB of every script.
    {kScriptTimerTruncCall, kTruncGroupScript},

    // How long an explosion has burnt fuel; the burning fuel is drawn for its
    // first 200 ms.
    // CExplosion::Update, +0x0C, negated
    {0x00737A09, kTruncGroupExplosion},

    // The phase of a damaged plane's engine sputter, the frame time scaled by
    // a random factor and truncated twice.
    // CPlane::ProcessFlyingCarStuff, m_planeDamageWave
    {0x006CB833, kTruncGroupPlaneDamage},
    {0x006CB84C, kTruncGroupPlaneDamage}

}};

} // namespace hff
