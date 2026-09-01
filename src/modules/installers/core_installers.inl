// ---------------------------------------------------------------------------
// Installers
// ---------------------------------------------------------------------------

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
    if (!InstallAimTimeStepOperands()) {
        Log("Aim camera shake fix skipped: Process_AimWeapon timestep bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallDetour(g_aimWeaponPatch, kProcessAimWeapon,
                       &HookedProcessAimWeapon,
                       kExpectedProcessAimWeapon.data(),
                       kExpectedProcessAimWeapon.size())) {
        RestoreAbsoluteOperandPatches(g_aimTimeStepPatches);
        Log("Aim camera shake fix failed at Process_AimWeapon entry.");
        return false;
    }
    Log("Installed local aim-camera timestep normalization without global timer writes.");
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

bool InstallAbandonedBikePhysicsStepFix() {
    PatchSet patches("Abandoned bike physics step");
    g_abandonedBikePhysicsStepEnabled = false;
    g_abandonedBikePhysicsLastFrame = 0;
    g_abandonedBikePhysicsCredit = 0.0f;
    g_abandonedBikePhysicsTick = false;
    g_abandonedBikeRenderStates = {};

    if (!patches.Track(
            InstallDetour(g_abandonedBikeCollisionPatch,
                          kPhysicalProcessCollision,
                          &HookedPhysicalProcessCollision,
                          kExpectedPhysicalProcessCollision.data(),
                          kExpectedPhysicalProcessCollision.size()),
            g_abandonedBikeCollisionPatch)) {
        Log("Abandoned bike physics step skipped: ProcessCollision entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            InstallDetour(g_abandonedBikeShiftPatch, kPhysicalProcessShift,
                          &HookedPhysicalProcessShift,
                          kExpectedPhysicalProcessShift.data(),
                          kExpectedPhysicalProcessShift.size()),
            g_abandonedBikeShiftPatch)) {
        Log("Abandoned bike physics step skipped: ProcessShift entry does not "
            "match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            InstallDetour(g_abandonedBikeRwFramePatch, kEntityUpdateRwFrame,
                          &HookedEntityUpdateRwFrame,
                          kExpectedEntityUpdateRwFrame.data(),
                          kExpectedEntityUpdateRwFrame.size()),
            g_abandonedBikeRwFramePatch)) {
        Log("Abandoned bike physics step skipped: UpdateRwFrame entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    const bool bikeHookWasInstalled = g_bikeProcessPatch.installed;
    if (!EnsureBikeProcessControlHook()
        || (!bikeHookWasInstalled
            && !patches.Track(true, g_bikeProcessPatch))) {
        Log("Abandoned bike physics step skipped: CBike::ProcessControl entry "
            "does not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();

    PatchSet renderPatches("Abandoned bike light interpolation");
    const bool renderInstalled = renderPatches.Track(
        InstallDetour(g_abandonedBikePreRenderPatch, kBikePreRender,
                      &HookedBikePreRender, kExpectedBikePreRender.data(),
                      kExpectedBikePreRender.size()),
        g_abandonedBikePreRenderPatch) && renderPatches.Track(
        InstallDetour(g_abandonedBikeRenderPatch, kBikeRender,
                      &HookedBikeRender, kExpectedBikeRender.data(),
                      kExpectedBikeRender.size()),
        g_abandonedBikeRenderPatch);
    if (!renderInstalled) {
        Log("Abandoned bike light interpolation skipped: CBike render entries "
            "do not match GTA SA 1.0 US.");
    } else {
        renderPatches.Commit();
    }

    g_abandonedBikePhysicsStepEnabled = true;
    Log("Installed complete 30 Hz physics steps with render interpolation for "
        "abandoned bikes.");
    if (renderInstalled) {
        Log("Installed interpolated abandoned-bike render matrices for lights.");
    }
    return true;
}

bool InstallRailWheelSpinFix() {
    PatchSet patches("Rail wheel spin fix");
    constexpr std::array<uintptr_t, 4> addresses{
        0x006B523F, 0x006B524F, 0x006B525D, 0x006B5269
    };
    const std::array<const void*, 4> thunks{
        &RailWheelSpinThunk0,
        &RailWheelSpinThunk1,
        &RailWheelSpinThunk2,
        &RailWheelSpinThunk3,
    };

    if (!InstallJumpTable(patches, g_railWheelSpinPatches, addresses, thunks,
                          kExpectedRailWheelSpin)) {
        Log("Rail wheel spin fix skipped: executable bytes do not match the active game profile.");
        return false;
    }
    patches.Commit();
    Log("Installed frame-independent on-rails wheel rotation.");
    return true;
}

bool InstallBurnoutFix() {
    if (!InstallJump(g_burnoutPatch, kBurnoutPatch, &BurnoutThunk,
                     kExpectedBurnout)) {
        Log("Burnout fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent burnout wheel speed.");
    return true;
}

bool InstallSkimmerResistanceFix() {
    if (!InstallJump(g_skimmerResistancePatch, kSkimmerResistancePatch,
                     &SkimmerResistanceThunk, kExpectedSkimmerResistance)) {
        Log("Skimmer resistance fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed frame-independent skimmer water resistance.");
    return true;
}

bool InstallHeliRotorSpeedFix() {
    PatchSet patches("Helicopter rotor fix");
    constexpr std::array<uintptr_t, 2> addresses{0x006C4F29, 0x006C4F37};
    const std::array<std::array<uint8_t, 6>, 2> expected{
        kExpectedHeliRotorSlow, kExpectedHeliRotorFast
    };
    const std::array<const void*, 2> thunks{
        &HeliRotorSlowThunk, &HeliRotorFastThunk
    };

    if (!MemoryMatches(kHeliRotorSpeedOperand, kExpectedHeliRotorOperand)) {
        Log("Helicopter rotor fix skipped: rotor speed operand does not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallJumpTable(patches, g_heliRotorPatches, addresses, thunks,
                          expected)) {
        Log("Helicopter rotor fix skipped: executable bytes do not match the active game profile.");
        return false;
    }
    patches.Commit();
    Log("Installed frame-independent helicopter rotor acceleration.");
    return true;
}

bool InstallHudFlashRateFix() {
    PatchSet patches("HUD flash rate fix");
    if (!HudFlashTestSiteMatches(kHudArmorBarOperand, 8)
        || !HudFlashTestSiteMatches(kHudBreathBarOperand, 8)
        || !HudFlashSiteMatches(kHudHealthBarOperand, kHudMovPrefix)
        || !HudFlashTestSiteMatches(kHudRadarOperand, 8)
        || !HudFlashTestSiteMatches(kHudWantedActiveOperand, 4)
        || !HudFlashTestSiteMatches(kHudWantedEmptyOperand, 4)) {
        Log("HUD flash rate fix skipped: flash sites do not match GTA SA 1.0 US.");
        return false;
    }

    // 320 ms on and 320 ms off is what `frameCounter & 8` produced at the 25 FPS
    // the HUD was drawn for, so there is nothing here for a player to tune.
    g_hudFlashIntervalMs = kDefaultHudFlashIntervalMs;
    g_hudDisableFlashing = ReadSetting("hud", "disableFlashing", false);

    // Seed the clock before the game can read it, then redirect the reads.
    g_hudFlashClock = 0;
    constexpr std::array<uintptr_t, 6> operands{
        kHudArmorBarOperand, kHudBreathBarOperand, kHudHealthBarOperand,
        kHudRadarOperand, kHudWantedActiveOperand, kHudWantedEmptyOperand
    };
    const std::array<const volatile uint32_t*, 6> counters{
        &g_hudFlashClock,
        &g_hudFlashClock,
        g_hudDisableFlashing ? &g_hudVisibleClock : &g_hudFlashClock,
        g_hudDisableFlashing ? &g_hudVisibleClock : &g_hudFlashClock,
        &g_hudFlashClock,
        &g_hudFlashClock,
    };
    for (size_t i = 0; i < operands.size(); ++i) {
        if (!patches.Track(
                RepointHudFlashOperand(g_hudFlashPatches[i], operands[i],
                                       counters[i]),
                g_hudFlashPatches[i])) {
            Log("HUD flash rate fix failed while redirecting HUD clocks.");
            return false;
        }
    }

    g_hudFlashActive = true;
    if (!StartWorkerThread(g_hudFlashThread, HudFlashThread)) {
        g_hudFlashActive = false;
        Log("HUD flash rate fix failed to start its worker thread.");
        return false;
    }
    patches.Commit();

    Log(g_hudDisableFlashing
            ? "Installed the HUD flash rate fix with radar and health flashing disabled."
            : "Installed a real-time HUD flash clock at the original 25 FPS rate.");
    return true;
}

bool InstallVehicleRestThresholdFix() {
    PatchSet patches("Vehicle rest threshold fix");
    constexpr std::array<uintptr_t, 3> addresses{
        kCarRestThreshold, kBikeRestThreshold, kTrailerRestThreshold
    };
    const std::array<const void*, 3> thunks{
        &CarRestThresholdThunk,
        &BikeRestThresholdThunk,
        &TrailerRestThresholdThunk,
    };

    for (const auto address : addresses) {
        if (!MemoryMatches(address, kExpectedRestThreshold)) {
            Log("Vehicle rest threshold fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!patches.Track(InstallJump(g_restThresholdPatches[i], addresses[i],
                                       thunks[i], kExpectedRestThreshold),
                           g_restThresholdPatches[i])) {
            Log("Vehicle rest threshold fix failed while installing hooks.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-normalized at-rest threshold for cars, bikes and trailers.");
    return true;
}

// The five bytes differ between sites because each holds its own relative
// displacement, so instead of comparing bytes this verifies the opcode and
// resolves the displacement to check the call really does land on the expected
// callee. That is a stronger check than a byte match, not a weaker one.
bool RepointCall(SitePatch& patch, uintptr_t address, uintptr_t expectedCallee,
                 const void* replacement) {
    std::array<uint8_t, 5> expected{};
    __try {
        if (*reinterpret_cast<const uint8_t*>(address) != 0xE8) {
            return false;
        }
        const auto original = *reinterpret_cast<const int32_t*>(address + 1);
        if (address + 5 + static_cast<uintptr_t>(original) != expectedCallee) {
            return false;
        }
        std::memcpy(expected.data(), reinterpret_cast<const void*>(address),
                    expected.size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return InstallBranch(patch, address, replacement, expected.data(),
                         expected.size(), 0xE8);
}

bool InstallBikeBalanceTrace() {
    PatchSet patches("Bike balance trace");
    constexpr std::array<uint8_t, 8> expectedInput{
        0xD9, 0x44, 0x24, 0x68, 0xD8, 0x4C, 0x24, 0x3C
    };
    if (!patches.Track(InstallJump(g_bikeBalanceInputPatch, kBikeBalanceInput,
                                   &BikeBalanceInputThunk, expectedInput),
                       g_bikeBalanceInputPatch)) {
        Log("Vehicle state trace: bike balance input site does not match.");
        return false;
    }
    if (!patches.Track(RepointCall(g_bikeBalanceForcePatch,
                                   kBikeBalanceForceCall, kApplyTurnForce,
                                   &BikeBalanceForceThunk),
                       g_bikeBalanceForcePatch)) {
        Log("Vehicle state trace: bike balance force call does not match.");
        return false;
    }
    patches.Commit();
    Log("Vehicle state trace: recording stock bike balance inputs and force.");
    return true;
}


bool InstallTruncCarryGroup(uint8_t group, const char* what) {
    PatchSet patches(what);
    size_t installed = 0;
    for (size_t i = 0; i < kStatTruncSites.size(); ++i) {
        if (kStatTruncSites[i].group != group) {
            continue;
        }
        if (!patches.Track(
                RepointCall(g_statTruncPatches[i],
                            kStatTruncSites[i].address, kFtol,
                            &StatTruncCarryThunk),
                g_statTruncPatches[i])) {
            char skipped[128];
            std::snprintf(skipped, sizeof(skipped),
                          "%s skipped: executable bytes do not match GTA SA 1.0 US.",
                          what);
            Log(skipped);
            return false;
        }
        g_statTruncCarries[i] = 0.0f;
        ++installed;
    }
    char message[128];
    std::snprintf(message, sizeof(message),
                  "Installed fractional carry in %zu %s truncations.",
                  installed, what);
    patches.Commit();
    Log(message);
    return true;
}

bool InstallSkillProgressFix() {
    return InstallTruncCarryGroup(kTruncGroupStats, "stat counter");
}

bool InstallStuntCountersFix() {
    return InstallTruncCarryGroup(kTruncGroupStunt, "stunt counter");
}

bool InstallUpsideDownTimerFix() {
    return InstallTruncCarryGroup(kTruncGroupUpsideDown, "upside down car timer");
}

bool InstallTaskTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupTask, "ped task timer");
}

bool InstallVehicleTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupVehicle, "vehicle timer");
}

bool InstallIdleCameraTimerFix() {
    return InstallTruncCarryGroup(kTruncGroupIdleCam, "idle camera timer");
}

bool InstallHudTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupHud, "HUD timer");
}

bool InstallBurnTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupBurn, "vehicle burn timer");
}
