#include "modules/modules.h"

namespace hff {

bool InstallStuntJumpCameraFix() {
    PatchSet patches("Stunt jump camera fix");
    if (!patches.Track(InstallCall(g_endTimerPatch, kEndTimerCall,
                                   &EndTimerThunk, kExpectedEndTimerCall),
                       g_endTimerPatch)) {
        Log("Stunt jump camera fix skipped: camera restore timer bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallCall(g_flightTimerPatch, kFlightTimerCall,
                                   &FlightTimerThunk, kExpectedFlightTimerCall),
                       g_flightTimerPatch)) {
        Log("Stunt jump camera fix skipped: in-flight timer bytes do not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed fraction-preserving unique stunt jump timers.");
    return true;
}

bool InstallAimCameraShakeFix() {
    if (!LooksLikeGameCode(kCameraProcess)
        || !LooksLikeGameCode(kProcessAimWeapon)) {
        Log("Aim camera shake fix skipped: camera code is not executable game memory.");
        return false;
    }
    if (!MemoryMatches(kCameraProcess, kExpectedCameraProcess)
        || !MemoryMatches(kProcessAimWeapon, kExpectedProcessAimWeapon)) {
        Log("Aim camera shake fix skipped: camera entry bytes do not match GTA SA 1.0 US, "
            "or another plugin already hooks them.");
        return false;
    }
    // The guard pins the timestep the FOV step reads, so the zoom site goes
    // in with the hooks and comes out with them: one without the other would
    // either leave the zoom running at the frame rate or change nothing.
    if (!InstallJump(g_aimWeaponFovStepPatch, kAimWeaponFovStep,
                     &AimWeaponFovStepThunk, kExpectedAimWeaponFovStep)) {
        Log("Aim camera shake fix skipped: the aim FOV step bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (MH_Initialize() != MH_OK) {
        RestoreSite(g_aimWeaponFovStepPatch);
        Log("Aim camera shake fix skipped: MinHook initialization failed.");
        return false;
    }
    g_aimMinHookInitialized = true;
    const bool created =
        MH_CreateHook(reinterpret_cast<void*>(kCameraProcess),
                      &HookedCameraProcess,
                      reinterpret_cast<void**>(&g_originalCameraProcess))
                == MH_OK
        && MH_CreateHook(reinterpret_cast<void*>(kProcessAimWeapon),
                         &HookedProcessAimWeapon,
                         reinterpret_cast<void**>(&g_originalAimWeapon))
                == MH_OK;
    const bool enabled = created
        && MH_EnableHook(reinterpret_cast<void*>(kCameraProcess)) == MH_OK
        && MH_EnableHook(reinterpret_cast<void*>(kProcessAimWeapon)) == MH_OK;
    if (!enabled) {
        RemoveAimCameraHooks();
        RestoreSite(g_aimWeaponFovStepPatch);
        Log("Aim camera shake fix skipped: camera hooks could not be installed.");
        return false;
    }
    Log("Installed scoped aim-camera timestep guards around CCamera::Process and "
        "Process_AimWeapon, with the aim FOV zoom kept on the real timestep.");
    return true;
}

bool InstallDrunkCameraShakeFix() {
    if (!InstallJump(g_drunkCameraPhasePatch, kDrunkCameraPhase,
                     &DrunkCameraPhaseThunk, kExpectedDrunkCameraPhase)) {
        Log("Drunk camera shake fix skipped: CCamera::Process sway bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real-time drunk camera sway rate.");
    return true;
}

bool InstallAimingRifleWalkFix() {
    if (!InstallJump(g_aimingRifleWalkPatch, kAimingRifleWalkPatch,
                     &AimingRifleWalkThunk, kExpectedAimingRifleWalk)) {
        Log("Aiming rifle walk fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent aiming rifle walk speed.");
    return true;
}

bool InstallSwimmingMovementFix() {
    if (!SwimmingMovementCodeIsUnmodified()) {
        Log("Swimming movement fix skipped: another swimming FPS fix has "
            "modified CTaskSimpleSwim::ProcessSwimmingResistance.");
        return false;
    }
    if (!InstallJump(g_swimmingPatch, kSwimResistanceCall,
                     &SwimResistanceThunk, kExpectedSwimResistanceCall)) {
        Log("Swimming movement fix skipped: CTaskSimpleSwim bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent swimming speed.");
    return true;
}

bool InstallFollowCameraRateFix() {
    PatchSet patches("Follow camera rate fix");
    if (!patches.Track(InstallJump(g_followPedCameraPatch,
                                   kFollowPedCameraRate,
                                   &FollowPedCameraRateThunk,
                                   kExpectedCameraRateClamp),
                       g_followPedCameraPatch)) {
        Log("Follow camera rate fix skipped: CCam bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallJump(g_followCarCameraPatch,
                                   kFollowCarCameraRate,
                                   &FollowCarCameraRateThunk,
                                   kExpectedCameraRateClamp),
                       g_followCarCameraPatch)) {
        Log("Follow camera rate fix skipped: the car camera site does not match.");
        return false;
    }
    patches.Commit();
    Log("Installed a timestep-following rate in both follow cameras.");
    return true;
}

bool InstallAttachedEntitySpeedFix() {
    if (!InstallJump(g_attachedEntitySpeedPatch, kAttachedEntitySpeed,
                     &AttachedEntitySpeedThunk, kExpectedAttachedSpeedClamp)) {
        Log("Attached entity speed fix skipped: CPhysical::PositionAttachedEntity "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real timestep in the attached entity speed.");
    return true;
}

bool InstallAiAircraftSteerFix() {
    if (!InstallJump(g_aiAircraftSteerPatch, kAiAircraftSteerRate,
                     &AiAircraftSteerRateThunk, kExpectedCameraRateClamp)) {
        Log("AI aircraft steering fix skipped: the autopilot bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real timestep in the AI aircraft steering rate.");
    return true;
}

bool InstallMoneyCounterFix() {
    if (!InstallJump(g_moneyStepPatch, kMoneyStepStore, &MoneyStepThunk,
                     kExpectedMoneyStepStore)) {
        Log("Money counter fix skipped: CPlayerInfo::Process bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real-time HUD money counter.");
    return true;
}

bool InstallClimbSpeedFix() {
    if (!InstallJump(g_climbSpeedPatch, kClimbSpeedClamp, &ClimbSpeedClampThunk,
                     kExpectedClimbSpeedClamp)) {
        Log("Climb speed fix skipped: CTaskSimpleClimb bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a clamped climb move speed.");
    return true;
}

bool InstallWaterBuoyancyFix() {
    PatchSet patches("Water buoyancy fix");
    if (!MemoryMatches(kBuoyancyThreshold, kExpectedBuoyancyThreshold)
        || !MemoryMatches(kBuoyancyClampedStore,
                          kExpectedBuoyancyClampedStore)) {
        Log("Water buoyancy fix skipped: cBuoyancy bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallJump(g_buoyancyThresholdPatch,
                                   kBuoyancyThreshold,
                                   &BuoyancyThresholdThunk,
                                   kExpectedBuoyancyThreshold),
                       g_buoyancyThresholdPatch)) {
        Log("Water buoyancy fix failed while installing the threshold hook.");
        return false;
    }
    if (!patches.Track(InstallJump(g_buoyancyClampedStorePatch,
                                   kBuoyancyClampedStore,
                                   &BuoyancyClampedStoreThunk,
                                   kExpectedBuoyancyClampedStore),
                       g_buoyancyClampedStorePatch)) {
        Log("Water buoyancy fix failed while installing the clamped store hook.");
        return false;
    }
    patches.Commit();
    Log("Installed a timestep-normalized buoyancy cutoff.");
    return true;
}

bool InstallPedPushVehicleFix() {
    g_pedPushLastFrame = 0;
    g_pedPushCarry = 0.0f;
    g_pedPushOriginalRateFrame = true;
    if (!InstallJump(g_pedPushCarPatch, kPedPushCarPatch, &PedPushCarThunk,
                     kExpectedPedPushCar)) {
        Log("Ped push vehicle fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent ped push physics for cars and bikes.");
    return true;
}

bool InstallBloodyFootprintsFix() {
    PatchSet patches("Bloody footprints fix");
    ResetBloodyFootprintTickStates();
    g_bloodyFootprintHeightStates = {};
    if (!patches.Track(InstallJump(g_bloodyFootprintCounterPatch,
                                   kBloodyFootprintCounterPatch,
                                   &BloodyFootprintCounterThunk,
                                   kExpectedBloodyFootprintCounter),
                       g_bloodyFootprintCounterPatch)) {
        Log("Bloody footprints fix skipped: CPed::PlayFootSteps bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallCall(g_bloodyFootLandedSidePatch,
                                   kPlayFootStepsLandedCall,
                                   &BloodyFootLandedSideThunk,
                                   kExpectedPlayFootStepsLandedCall),
                       g_bloodyFootLandedSidePatch)
        || !patches.Track(InstallCall(g_bloodyFootprintShadowPatch,
                                      kBloodyFootprintShadowCall,
                                      &BloodyFootprintShadowThunk,
                                      kExpectedBloodyFootprintShadowCall),
                          g_bloodyFootprintShadowPatch)) {
        Log("Bloody footprints fix skipped: foot-side or shadow call bytes do not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed a real-time bloody-footprint countdown and right-foot projection stabilization.");
    return true;
}

bool InstallWheelFrictionFix() {
    PatchSet patches("Wheel friction fix");
    constexpr std::array<uintptr_t, 5> addresses{
        0x006D6E69, 0x006D6EA8, 0x006D767F, 0x006D76AB, 0x006D76CD
    };
    const std::array<const void*, 5> thunks{
        &WheelFrictionCarDriveThunk,
        &WheelFrictionCarBrakeThunk,
        &WheelFrictionBikeBaseThunk,
        &WheelFrictionBikeDriveThunk,
        &WheelFrictionBikeBrakeThunk,
    };

    if (!InstallJumpTable(patches, g_wheelFrictionPatches, addresses, thunks,
                          kExpectedWheelFriction)) {
        Log("Wheel friction fix skipped: executable bytes do not match the active game profile.");
        return false;
    }
    patches.Commit();
    Log("Installed timestep-scaled car and bike wheel friction.");
    return true;
}

} // namespace hff
