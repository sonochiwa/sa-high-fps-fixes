// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void InstallFix(const char* section, const char* key, const char* name,
                bool (*installer)(), bool defaultOn = true) {
    if (ReadSetting(section, key, defaultOn)) {
        installer();
        return;
    }
    std::string message(name);
    message += " disabled by configuration.";
    Log(message.c_str());
}

DWORD WINAPI Initialize(void*) {
    g_iniPath = ModulePathWithExtension(".ini");
    g_logPath = ModulePathWithExtension(".log");
    CreateDefaultIniIfMissing();

    // `[general]` is not written to the canonical INI: the plugin is always on
    // and the log is off, so the file a player opens holds nothing but fix
    // switches. Every key below is still read when someone adds it by hand,
    // which is how a report gets turned into a log without shipping one.
    g_loggingEnabled = ReadSetting("general", "enableLogging", false);

    if (reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) != kImageBase) {
        Log("Initialization skipped: unexpected executable image base.");
        return 0;
    }

    Log("Initializing High FPS Fixes v0.9.2.");

    InstallFix("camera", "stuntJumpCamera", "Stunt jump camera fix",
               InstallStuntJumpCameraFix);
    InstallFix("camera", "aimCameraShake", "Aim camera shake fix",
               InstallAimCameraShakeFix);
    InstallFix("camera", "followCameraRate", "Follow camera rate fix",
               InstallFollowCameraRateFix);
    InstallFix("camera", "idleCameraTimer", "Idle camera timer fix",
               InstallIdleCameraTimerFix);
    InstallFix("player", "aimingRifleWalk", "Aiming rifle walk fix",
               InstallAimingRifleWalkFix);
    InstallFix("player", "swimmingMovement", "Swimming movement fix",
               InstallSwimmingMovementFix);
    InstallFix("player", "swimPitchRate", "Swim pitch rate fix",
               InstallSwimPitchRateFix);
    InstallFix("player", "pedPushVehicle", "Ped push vehicle fix",
               InstallPedPushVehicleFix);
    InstallFix("player", "drowningDamage", "Drowning damage fix",
               InstallDrowningDamageFix);
    InstallFix("player", "drunkSteerDelay", "Drunk steering delay fix",
               InstallDrunkSteerDelayFix);
    InstallFix("player", "jetPackFlame", "Jetpack flame ramp fix",
               InstallJetPackFxRampFix);
    InstallFix("player", "fatCounter", "Fat counter fix",
               InstallFatCounterFix);
    InstallFix("player", "waterBuoyancy", "Water buoyancy fix",
               InstallWaterBuoyancyFix);
    InstallFix("player", "climbSpeed", "Climb speed fix",
               InstallClimbSpeedFix);
    InstallFix("player", "skillProgress", "Skill progress fix",
               InstallSkillProgressFix);
    InstallFix("player", "stuntCounters", "Stunt counter fix",
               InstallStuntCountersFix);
    InstallFix("player", "taskTimers", "Ped task timer fix",
               InstallTaskTimersFix);
    InstallFix("vehicles", "restThreshold", "Vehicle rest threshold fix",
               InstallVehicleRestThresholdFix);
    InstallFix("vehicles", "bikeLeanTarget", "Bike lean target fix",
               InstallBikeLeanTargetFix);
    InstallFix("vehicles", "bikePitchExperiment", "Bike pitch experiment",
               InstallBikePitchExperiment, false);
    InstallFix("vehicles", "groundFriction", "Ground friction fix",
               InstallGroundFrictionFix);
    InstallFix("vehicles", "turnAirResistance", "Turn air resistance fix",
               InstallTurnAirResistanceFix);
    InstallFix("vehicles", "moveSpeedSnap", "Move speed snap fix",
               InstallMoveSpeedSnapFix);
    InstallFix("vehicles", "physicsSleepRate", "Physics sleep rate fix",
               InstallPhysicsSleepRateFix);
    InstallFix("vehicles", "wheelFriction", "Wheel friction fix",
               InstallWheelFrictionFix);
    InstallFix("vehicles", "abandonedBikePhysicsStep",
               "Abandoned bike physics step fix",
               InstallAbandonedBikePhysicsStepFix);
    InstallFix("vehicles", "railWheelSpin", "Rail wheel spin fix",
               InstallRailWheelSpinFix);
    InstallFix("vehicles", "burnout", "Burnout fix", InstallBurnoutFix);
    InstallFix("vehicles", "doorSwing", "Door swing fix",
               InstallDoorSwingFix);
    InstallFix("vehicles", "sirenTap", "Siren tap fix", InstallSirenTapFix);
    InstallFix("vehicles", "heliRotorSpeed", "Helicopter rotor fix",
               InstallHeliRotorSpeedFix);
    InstallFix("vehicles", "attachedEntitySpeed", "Attached entity speed fix",
               InstallAttachedEntitySpeedFix);
    InstallFix("vehicles", "aiAircraftSteer", "AI aircraft steering fix",
               InstallAiAircraftSteerFix);
    InstallFix("vehicles", "upsideDownTimer", "Upside down car timer fix",
               InstallUpsideDownTimerFix);
    InstallFix("vehicles", "vehicleTimers", "Vehicle timer fix",
               InstallVehicleTimersFix);
    InstallFix("vehicles", "burnTimers", "Vehicle burn timer fix",
               InstallBurnTimersFix);
    InstallFix("vehicles", "rollOntoWheels", "Roll onto wheels fix",
               InstallRollOntoWheelsFix);
    InstallFix("vehicles", "suspensionDampingLimit",
               "Suspension damping limit fix",
               InstallSuspensionDampingLimitFix);
    InstallFix("vehicles", "collisionPushOut", "Collision push-out fix",
               InstallCollisionPushOutFix);
    InstallFix("vehicles", "wheelSettle", "Wheel settle fix",
               InstallWheelSettleFix);
    InstallFix("vehicles", "wheelSpin", "Free wheel spin fix",
               InstallWheelSpinFix);
    InstallFix("vehicles", "boatEngineSpeed", "Boat engine speed fix",
               InstallBoatEngineSpeedFix);
    InstallFix("vehicles", "bmxSprintLean", "BMX sprint lean fix",
               InstallBmxSprintLeanFix);
    InstallFix("vehicles", "bmxLeanSettle", "BMX lean settle fix",
               InstallBmxLeanSettleFix);
    InstallFix("vehicles", "bikeWheelSpin", "Bike wheel spin fix",
               InstallBikeWheelSpinFix);
    InstallFix("vehicles", "headBopping", "Head bopping fix",
               InstallHeadBoppingFix);
    InstallFix("vehicles", "jumpOutCarSpeed", "Jump out car speed fix",
               InstallJumpOutCarSpeedFix);
    InstallFix("vehicles", "skimmerResistance", "Skimmer resistance fix",
               InstallSkimmerResistanceFix);

    if (ReadSetting("weapons", "continuousWeaponParticles", true)) {
        Sleep(500);
        InstallContinuousWeaponParticlesFix();
    } else {
        Log("Continuous weapon particle fix disabled by configuration.");
    }
    InstallFix("weapons", "continuousWeaponAmmo", "Continuous weapon ammo fix",
               InstallContinuousWeaponAmmoFix);
    InstallFix("weapons", "chainsawStrikeRate", "Chainsaw strike rate fix",
               InstallChainsawStrikeRateFix);
    // A hard ceiling on new particles a second, the way FxLimiter capped them.
    // Not written to the canonical INI: it trades effects away for frame time
    // rather than correcting a frame-rate dependence, and `emissionRate`
    // already restores the intended density. Off unless asked for by hand.
    g_particleBudget = static_cast<uint32_t>(
        std::clamp(ReadNumber("particles", "particlesPerSecond", 0), 0, 100000));
    g_particleRateGate = ReadSetting("particles", "emissionRate", true);
    if (g_particleRateGate || g_particleBudget != 0) {
        InstallParticleEmissionRateFix();
    } else {
        Log("Direct particle emission hook not needed by configuration.");
    }
    // One switch over three mechanisms: the flash clock, the money counter step
    // and the 46 timed-text accumulators. They are separate patches but one
    // symptom to a player, so they are configured together.
    if (ReadSetting("hud", "hudTiming", true)) {
        InstallMoneyCounterFix();
        InstallHudFlashRateFix();
        InstallHudTimersFix();
    } else {
        Log("HUD timing fixes disabled by configuration.");
    }
    InstallFix("world", "gangWarTimer", "Gang war timer fix",
               InstallGangWarTimerFix);
    InstallFix("world", "fireSpread", "Fire spread fix",
               InstallFireSpreadFix);
    InstallFix("world", "scriptObjectSlide", "Script object slide fix",
               InstallScriptObjectSlideFix);
    InstallFix("world", "scriptObjectRotate", "Script object rotate fix",
               InstallScriptRotateObjectFix);
    InstallFix("world", "fallingGlass", "Falling glass fix",
               InstallFallingGlassFix);
    InstallFix("world", "breakableObjectLifetime",
               "Breakable object lifetime fix", InstallBreakableObjectLifetimeFix);
    InstallFix("menu", "mapZoomWheel", "Map zoom wheel fix",
               InstallMapZoomWheelFix);

    g_fpsLimit = std::clamp(ReadNumber("framerate", "fpsLimit", 0), 0, 255);
    g_refreshRate = std::clamp(ReadNumber("framerate", "refreshRate", 0), 0,
                               255);
    if (g_fpsLimit > 0) {
        InstallFrameLimit(g_fpsLimit);
    }
    if (g_refreshRate > 0 && g_refreshRate != 60) {
        InstallRefreshRate(g_refreshRate);
    }
    g_traceCycleSkill = ReadSetting("general", "traceCycleSkill", false);
    g_traceChainsaw = ReadSetting("general", "traceChainsaw", false);
    if (g_traceChainsaw) {
        if (InstallBranch(g_fightStrikeTracePatch, kFightStrikeCall,
                          &FightStrikeTraceThunk,
                          kExpectedFightStrikeCall.data(),
                          kExpectedFightStrikeCall.size(), 0xE8)) {
            Log("Chainsaw trace: counting melee strikes.");
        } else {
            Log("Chainsaw trace: CTaskSimpleFight::ProcessPed bytes do not "
                "match GTA SA 1.0 US, strikes will read zero.");
        }
    }

    const bool traceVehicleState =
        ReadSetting("general", "traceVehicleState", false);
    const bool tracePlayerPed = ReadSetting("general", "tracePlayerPed", false);
    if (traceVehicleState || tracePlayerPed) {
        g_diagnosticPath = ModulePathWithExtension(".trace.log");
    }

    if (traceVehicleState) {
        InstallBikeBalanceTrace();
        if (EnsureBikeProcessControlHook()) {
            Log("Vehicle state trace: counting CBike::ProcessControl entries.");
        } else {
            Log("Vehicle state trace: CBike::ProcessControl counter unavailable.");
        }

        constexpr std::array<uint8_t, 6> expectedApplyGravity{
            0x83, 0xEC, 0x18, 0x56, 0x8B, 0xF1
        };
        if (InstallDetour(g_applyGravityPatch, kPhysicalApplyGravity,
                          &HookedApplyGravity, expectedApplyGravity.data(),
                          expectedApplyGravity.size())) {
            Log("Vehicle state trace: counting CPhysical::ApplyGravity calls.");
        } else {
            Log("Vehicle state trace: ApplyGravity counter unavailable.");
        }

        constexpr std::array<uint8_t, 6> expectedLeanWrite{
            0xD9, 0x9E, 0x48, 0x06, 0x00, 0x00
        };
        constexpr std::array<uintptr_t, 3> leanSites{
            kLeanWriteSmoother, kLeanWriteBike, kLeanWriteBmx
        };
        const std::array<const void*, 3> leanThunks{
            &LeanWriteSmootherThunk, &LeanWriteBikeThunk, &LeanWriteBmxThunk
        };
        size_t leanInstalled = 0;
        for (size_t i = 0; i < leanSites.size(); ++i) {
            if (InstallJump(g_leanWritePatches[i], leanSites[i], leanThunks[i],
                            expectedLeanWrite)) {
                ++leanInstalled;
            }
        }
        if (leanInstalled == leanSites.size()) {
            Log("Vehicle state trace: counting all three LeanAngle writers.");
        } else {
            Log("Vehicle state trace: some LeanAngle writers were not counted.");
        }
        // Four byte aligned and inside the object, or the default is kept. The range
// reaches past CPhysical so bike fields such as `DesiredLeanAngle` at 0x64C can
// be watched too.
        const int watchOffset =
            ReadNumber("general", "traceWatchOffset",
                       static_cast<int>(kPhysicalMoveSpeed + 8));
        if (watchOffset >= 0 && watchOffset <= 0x900 && (watchOffset % 4) == 0) {
            g_watchOffset = static_cast<size_t>(watchOffset);
        }
        g_watchMode = std::clamp(ReadNumber("general", "traceWatchMode", 0), 0, 2);
        g_watchHitLimit = static_cast<uint32_t>(std::clamp(
            ReadNumber("general", "traceWatchHits",
                       static_cast<int>(kWatchHitLimit)), 20, 100000));
        g_watchSampleLimit = static_cast<uint32_t>(
            std::clamp(ReadNumber("general", "traceWatchSamples", 100), 1, 10000));
        g_watchArmDelay = static_cast<uint32_t>(
            std::clamp(ReadNumber("general", "traceWatchArmDelay", 3), 1, 100));
        g_watchReportLimit = static_cast<uint32_t>(std::clamp(
            ReadNumber("general", "traceWatchReports",
                       static_cast<int>(kWatchReportLimit)), 1, 100000));

        g_watchHandler = AddVectoredExceptionHandler(1, &MoveSpeedWatchHandler);
        if (g_watchHandler) {
            Log("Vehicle state trace: write watchpoint ready.");
        } else {
            Log("Vehicle state trace: move speed write watchpoint unavailable.");
        }

        g_diagnosticActive = true;
        if (HANDLE trace = CreateThread(nullptr, 0, VehicleTraceThread, nullptr,
                                        0, nullptr)) {
            CloseHandle(trace);
            Log("Vehicle state trace enabled; writing HighFpsFixes.trace.log.");
        } else {
            g_diagnosticActive = false;
            Log("Vehicle state trace failed to start its worker thread.");
        }
    }

    if (tracePlayerPed) {
        g_diagnosticActive = true;
        if (HANDLE trace = CreateThread(nullptr, 0, PedTraceThread, nullptr, 0,
                                        nullptr)) {
            CloseHandle(trace);
            Log("Player ped trace enabled; writing HighFpsFixes.trace.log.");
        } else {
            Log("Player ped trace failed to start its worker thread.");
        }
    }

    g_autoLimit.value = 0;
    g_autoLimit.flags.forMissions =
        ReadSetting("autoLimitFps", "forMissions", false);
    g_autoLimit.flags.forMinigames =
        ReadSetting("autoLimitFps", "forMinigames", false);
    g_autoLimit.flags.forSchools =
        ReadSetting("autoLimitFps", "forSchools", false);
    g_autoLimit.flags.forCutscenes =
        ReadSetting("autoLimitFps", "forCutscenes", false);
    g_autoLimit.flags.forScriptedCutscenes =
        ReadSetting("autoLimitFps", "forScriptedCutscenes", false);
    g_autoLimit.flags.forPauseMenu =
        ReadSetting("autoLimitFps", "forPauseMenu", false);
    if (g_autoLimit.value != 0) {
        InstallAutoFpsLimit();
    }

    return 0;
}

void Shutdown() {
    g_diagnosticActive = false;
    if (g_watchArmed) {
        SetMoveSpeedWatch(0);
        g_watchArmed = false;
    }
    if (g_watchHandler) {
        RemoveVectoredExceptionHandler(g_watchHandler);
        g_watchHandler = nullptr;
    }
    RestoreDetour(g_bikeProcessPatch);
    RestoreDetour(g_applyGravityPatch);
    RestoreSite(g_bikeBalanceForcePatch);
    RestoreSite(g_bikeBalanceInputPatch);
    for (auto& patch : g_leanWritePatches) {
        RestoreSite(patch);
    }
    g_hudFlashActive = false;
    for (auto& patch : g_hudFlashPatches) {
        RestoreRawPatch(patch);
    }
    if (g_swingingDisabled) {
        WriteProtectedGameFloat(kDoorApplyRateChassis,
                                kStockDoorApplyRateChassis);
        g_swingingDisabled = false;
    }
    RestoreSite(g_menuBackgroundPatch);
    RestoreSite(g_scriptsProcessPatch);
    RestoreSite(g_scriptSlideObjectPatch);
    RestoreSite(g_scriptRotateObjectPatch);
    for (auto& patch : g_fallingGlassPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_breakObjectLifetimePatch);
    RestoreByte(g_refreshRatePatch);
    RestoreByte(g_frameLimitStorePatch);
    RestoreByte(g_frameLimiterGatePatch);
    RestoreSite(g_drowningDamagePatch);
    RestoreSite(g_continuousAmmoPatch);
    RestoreSite(g_chainsawStrikePatch);
    RestoreSite(g_fightStrikeTracePatch);
    RestoreDetour(g_fxCreateParticlesPatch);
    RestoreDetour(g_fxAddParticlePatch);
    RestoreSite(g_sirenPatch);
    for (auto& patch : g_fakePhysicsPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_restThresholdPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_moveSpeedSnapPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_turnAirResistancePatch);
    RestoreSite(g_groundFrictionPatch);
    RestoreSite(g_bikeLeanTargetPatch);
    RestoreSite(g_bikePitchExperimentPatch);
    for (auto& patch : g_bmxRiderFallTracePatches) {
        RestoreSite(patch);
    }
    RestoreDetour(g_bikeDamageKnockOffPatch);
    RestoreDetour(g_bmxLaunchBunnyHopPatch);
    RestoreDetour(g_suspensionDampingPatch);
    for (auto& patch : g_heliRotorPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_skimmerResistancePatch);
    RestoreSite(g_burnoutPatch);
    for (auto& patch : g_railWheelSpinPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_wheelFrictionPatches) {
        RestoreSite(patch);
    }
    g_abandonedBikePhysicsStepEnabled = false;
    RestoreDetour(g_abandonedBikeRenderPatch);
    RestoreDetour(g_abandonedBikePreRenderPatch);
    RestoreDetour(g_abandonedBikeRwFramePatch);
    RestoreDetour(g_abandonedBikeCollisionPatch);
    RestoreDetour(g_abandonedBikeShiftPatch);
    RestoreSite(g_pedPushCarPatch);
    RestoreSite(g_swimmingPatch);
    RestoreSite(g_climbSpeedPatch);
    RestoreSite(g_moneyStepPatch);
    RestoreSite(g_followPedCameraPatch);
    RestoreSite(g_followCarCameraPatch);
    RestoreSite(g_attachedEntitySpeedPatch);
    RestoreSite(g_aiAircraftSteerPatch);
    RestoreSite(g_rollOntoWheelsTurnPatch);
    RestoreSite(g_rollOntoWheelsMovePatch);
    for (auto& patch : g_doorSwingPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_wheelSpinPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_boatEngineDampingPatch);
    RestoreSite(g_bmxSprintLeanPatch);
    for (auto& patch : g_bmxLeanPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_bikeWheelSpinPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_mapWheelSamplePatch);
    RestoreSite(g_mapZoomInGatePatch);
    RestoreSite(g_mapZoomOutGatePatch);
    for (auto& patch : g_fireGatePatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_drunkSteerPatch);
    RestoreSite(g_fatCounterPatch);
    for (auto& patch : g_jetPackFxPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_headBopPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_jumpOutDampPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_pushOutPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_wheelSettlePatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_swimPitchPatches) {
        RestoreSite(patch);
    }
    for (auto& patch : g_statTruncPatches) {
        RestoreSite(patch);
    }
    RestoreSite(g_buoyancyThresholdPatch);
    RestoreSite(g_buoyancyClampedStorePatch);
    RestoreSite(g_aimingRifleWalkPatch);
    RestoreDetour(g_aimWeaponPatch);
    RestoreAbsoluteOperandPatches(g_aimTimeStepPatches);
    RestoreSite(g_flightTimerPatch);
    RestoreSite(g_endTimerPatch);
}
