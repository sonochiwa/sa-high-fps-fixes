// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

struct InstallSummary {
    size_t installed{};
    size_t failed{};
    size_t disabled{};
};

InstallSummary g_installSummary{};

struct FixSpec {
    const char* section;
    const char* key;
    const char* name;
    bool (*installer)();
    bool defaultOn{true};
};

void InstallFix(const char* section, const char* key, const char* name,
                bool (*installer)(), bool defaultOn = true) {
    if (ReadSetting(section, key, defaultOn)) {
        if (installer()) {
            ++g_installSummary.installed;
        } else {
            ++g_installSummary.failed;
        }
        return;
    }
    ++g_installSummary.disabled;
    std::string message(name);
    message += " disabled by configuration.";
    Log(message.c_str());
}

template <size_t Count>
void InstallFixes(const FixSpec (&fixes)[Count]) {
    for (const auto& fix : fixes) {
        InstallFix(fix.section, fix.key, fix.name, fix.installer,
                   fix.defaultOn);
    }
}

DWORD WINAPI Initialize(void*) {
    g_iniPath = ModulePathWithExtension(".ini");
    g_logPath = ModulePathWithExtension(".log");
    CreateDefaultIniIfMissing();
    RegisterConditionalConfigKeys();

    // `[general]` is not written to the canonical INI: the plugin is always on
    // and the log is off, so the file a player opens holds nothing but fix
    // switches. Every key below is still read when someone adds it by hand,
    // which is how a report gets turned into a log without shipping one.
    g_loggingEnabled = ReadSetting("general", "enableLogging", false);

    g_activeGameProfile = DetectGameProfile();
    if (!g_activeGameProfile) {
        g_loggingEnabled = true;
        Log("Initialization skipped: unsupported GTA executable profile. Run tools\\validate-game.ps1 against gta_sa.exe for details.");
        return 0;
    }

    Log("Initializing High FPS Fixes v0.9.2.");
    std::string profileMessage("Detected executable profile: ");
    profileMessage += g_activeGameProfile->name;
    profileMessage += ".";
    Log(profileMessage.c_str());

    const FixSpec coreFixes[] = {
        {"camera", "stuntJumpCamera", "Stunt jump camera fix",
               InstallStuntJumpCameraFix},
        {"camera", "aimCameraShake", "Aim camera shake fix",
               InstallAimCameraShakeFix},
        {"camera", "followCameraRate", "Follow camera rate fix",
               InstallFollowCameraRateFix},
        {"camera", "idleCameraTimer", "Idle camera timer fix",
               InstallIdleCameraTimerFix},
        {"player", "aimingRifleWalk", "Aiming rifle walk fix",
               InstallAimingRifleWalkFix},
        {"player", "swimmingMovement", "Swimming movement fix",
               InstallSwimmingMovementFix},
        {"player", "swimPitchRate", "Swim pitch rate fix",
               InstallSwimPitchRateFix},
        {"player", "pedPushVehicle", "Ped push vehicle fix",
               InstallPedPushVehicleFix},
        {"player", "drowningDamage", "Drowning damage fix",
               InstallDrowningDamageFix},
        {"player", "drunkSteerDelay", "Drunk steering delay fix",
               InstallDrunkSteerDelayFix},
        {"player", "jetPackFlame", "Jetpack flame ramp fix",
               InstallJetPackFxRampFix},
        {"player", "fatCounter", "Fat counter fix",
               InstallFatCounterFix},
        {"player", "waterBuoyancy", "Water buoyancy fix",
               InstallWaterBuoyancyFix},
        {"player", "climbSpeed", "Climb speed fix",
               InstallClimbSpeedFix},
        {"player", "skillProgress", "Skill progress fix",
               InstallSkillProgressFix},
        {"player", "stuntCounters", "Stunt counter fix",
               InstallStuntCountersFix},
        {"player", "taskTimers", "Ped task timer fix",
               InstallTaskTimersFix},
        {"vehicles", "restThreshold", "Vehicle rest threshold fix",
               InstallVehicleRestThresholdFix},
        {"vehicles", "bikeLeanTarget", "Bike lean target fix",
               InstallBikeLeanTargetFix},
        {"vehicles", "bikePitchExperiment", "Bike pitch experiment",
               InstallBikePitchExperiment, false},
        {"vehicles", "groundFriction", "Ground friction fix",
               InstallGroundFrictionFix},
        {"vehicles", "turnAirResistance", "Turn air resistance fix",
               InstallTurnAirResistanceFix},
        {"vehicles", "moveSpeedSnap", "Move speed snap fix",
               InstallMoveSpeedSnapFix},
        {"vehicles", "physicsSleepRate", "Physics sleep rate fix",
               InstallPhysicsSleepRateFix},
        {"vehicles", "wheelFriction", "Wheel friction fix",
               InstallWheelFrictionFix},
        {"vehicles", "abandonedBikePhysicsStep",
               "Abandoned bike physics step fix",
               InstallAbandonedBikePhysicsStepFix},
        {"vehicles", "railWheelSpin", "Rail wheel spin fix",
               InstallRailWheelSpinFix},
        {"vehicles", "burnout", "Burnout fix", InstallBurnoutFix},
        {"vehicles", "doorSwing", "Door swing fix",
               InstallDoorSwingFix},
        {"vehicles", "sirenTap", "Siren tap fix", InstallSirenTapFix},
        {"vehicles", "heliRotorSpeed", "Helicopter rotor fix",
               InstallHeliRotorSpeedFix},
        {"vehicles", "attachedEntitySpeed", "Attached entity speed fix",
               InstallAttachedEntitySpeedFix},
        {"vehicles", "aiAircraftSteer", "AI aircraft steering fix",
               InstallAiAircraftSteerFix},
        {"vehicles", "upsideDownTimer", "Upside down car timer fix",
               InstallUpsideDownTimerFix},
        {"vehicles", "vehicleTimers", "Vehicle timer fix",
               InstallVehicleTimersFix},
        {"vehicles", "burnTimers", "Vehicle burn timer fix",
               InstallBurnTimersFix},
        {"vehicles", "rollOntoWheels", "Roll onto wheels fix",
               InstallRollOntoWheelsFix},
        {"vehicles", "suspensionDampingLimit",
               "Suspension damping limit fix",
               InstallSuspensionDampingLimitFix},
        {"vehicles", "collisionPushOut", "Collision push-out fix",
               InstallCollisionPushOutFix},
        {"vehicles", "wheelSettle", "Wheel settle fix",
               InstallWheelSettleFix},
        {"vehicles", "wheelSpin", "Free wheel spin fix",
               InstallWheelSpinFix},
        {"vehicles", "boatEngineSpeed", "Boat engine speed fix",
               InstallBoatEngineSpeedFix},
        {"vehicles", "bmxSprintLean", "BMX sprint lean fix",
               InstallBmxSprintLeanFix},
        {"vehicles", "bmxLeanSettle", "BMX lean settle fix",
               InstallBmxLeanSettleFix},
        {"vehicles", "bikeWheelSpin", "Bike wheel spin fix",
               InstallBikeWheelSpinFix},
        {"vehicles", "headBopping", "Head bopping fix",
               InstallHeadBoppingFix},
        {"vehicles", "jumpOutCarSpeed", "Jump out car speed fix",
               InstallJumpOutCarSpeedFix},
        {"vehicles", "skimmerResistance", "Skimmer resistance fix",
               InstallSkimmerResistanceFix},
    };
    InstallFixes(coreFixes);

    if (ReadSetting("weapons", "continuousWeaponParticles", true)) {
        Sleep(500);
        if (InstallContinuousWeaponParticlesFix()) {
            ++g_installSummary.installed;
        } else {
            ++g_installSummary.failed;
        }
    } else {
        ++g_installSummary.disabled;
        Log("Continuous weapon particle fix disabled by configuration.");
    }
    const FixSpec weaponFixes[] = {
        {"weapons", "continuousWeaponAmmo", "Continuous weapon ammo fix",
         InstallContinuousWeaponAmmoFix},
        {"weapons", "chainsawStrikeRate", "Chainsaw strike rate fix",
         InstallChainsawStrikeRateFix},
    };
    InstallFixes(weaponFixes);
    // A hard ceiling on new particles a second, the way FxLimiter capped them.
    // Not written to the canonical INI: it trades effects away for frame time
    // rather than correcting a frame-rate dependence, and `emissionRate`
    // already restores the intended density. Off unless asked for by hand.
    g_particleBudget = static_cast<uint32_t>(
        std::clamp(ReadNumber("particles", "particlesPerSecond", 0), 0, 100000));
    g_particleRateGate = ReadSetting("particles", "emissionRate", true);
    if (g_particleRateGate || g_particleBudget != 0) {
        if (InstallParticleEmissionRateFix()) {
            ++g_installSummary.installed;
        } else {
            ++g_installSummary.failed;
        }
    } else {
        ++g_installSummary.disabled;
        Log("Direct particle emission hook not needed by configuration.");
    }
    // One switch over three mechanisms: the flash clock, the money counter step
    // and the 46 timed-text accumulators. They are separate patches but one
    // symptom to a player, so they are configured together.
    if (ReadSetting("hud", "hudTiming", true)) {
        const bool hudResults[] = {
            InstallMoneyCounterFix(),
            InstallHudFlashRateFix(),
            InstallHudTimersFix(),
        };
        for (const bool installed : hudResults) {
            installed ? ++g_installSummary.installed
                      : ++g_installSummary.failed;
        }
    } else {
        g_installSummary.disabled += 3;
        Log("HUD timing fixes disabled by configuration.");
    }
    const FixSpec worldAndMenuFixes[] = {
        {"world", "gangWarTimer", "Gang war timer fix",
         InstallGangWarTimerFix},
        {"world", "fireSpread", "Fire spread fix", InstallFireSpreadFix},
        {"world", "scriptObjectSlide", "Script object slide fix",
         InstallScriptObjectSlideFix},
        {"world", "scriptObjectRotate", "Script object rotate fix",
         InstallScriptRotateObjectFix},
        {"world", "fallingGlass", "Falling glass fix",
         InstallFallingGlassFix},
        {"world", "breakableObjectLifetime",
         "Breakable object lifetime fix", InstallBreakableObjectLifetimeFix},
        {"menu", "mapZoomWheel", "Map zoom wheel fix",
         InstallMapZoomWheelFix},
    };
    InstallFixes(worldAndMenuFixes);

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
        if (StartWorkerThread(g_vehicleTraceThread, VehicleTraceThread)) {
            Log("Vehicle state trace enabled; writing HighFpsFixes.trace.log.");
        } else {
            g_diagnosticActive = false;
            Log("Vehicle state trace failed to start its worker thread.");
        }
    }

    if (tracePlayerPed) {
        g_diagnosticActive = true;
        if (StartWorkerThread(g_pedTraceThread, PedTraceThread)) {
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

    ValidateUnknownConfigKeys();
    ReportConfigWarnings();
    char summary[192];
    std::snprintf(summary, sizeof(summary),
                  "Installation summary: %u installed, %u failed, %u disabled by configuration.",
                  static_cast<unsigned>(g_installSummary.installed),
                  static_cast<unsigned>(g_installSummary.failed),
                  static_cast<unsigned>(g_installSummary.disabled));
    Log(summary);

    return 0;
}

void Shutdown() {
    if (!StopAllWorkerThreads()) {
        return;
    }

    if (g_watchArmed) {
        SetMoveSpeedWatch(0);
        g_watchArmed = false;
    }
    if (g_watchHandler) {
        RemoveVectoredExceptionHandler(g_watchHandler);
        g_watchHandler = nullptr;
    }

    g_abandonedBikePhysicsStepEnabled = false;
    if (g_swingingDisabled) {
        WriteProtectedGameFloat(kDoorApplyRateChassis,
                                kStockDoorApplyRateChassis);
        g_swingingDisabled = false;
    }

    RestoreAllPatches();
}
