bool InstallGangWarTimerFix() {
    return InstallTruncCarryGroup(kTruncGroupWorld, "gang war timer");
}

bool InstallScriptObjectSlideFix() {
    if (!InstallJump(g_scriptSlideObjectPatch, kScriptSlideObject,
                     &ScriptSlideObjectThunk, kExpectedScriptSlideObject)) {
        Log("Script object slide fix skipped: SLIDE_OBJECT bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled SLIDE_OBJECT script rate.");
    return true;
}

bool InstallScriptRotateObjectFix() {
    if (!InstallJump(g_scriptRotateObjectPatch, kScriptRotateObject,
                     &ScriptRotateObjectThunk, kExpectedScriptRotateObject)) {
        Log("Script object rotate fix skipped: ROTATE_OBJECT bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled ROTATE_OBJECT script rate.");
    return true;
}

bool InstallFallingGlassFix() {
    PatchSet patches("Falling glass fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kFallingGlassMove, &FallingGlassMoveThunk,
         kExpectedFallingGlassMove.data()},
        {kFallingGlassTurnA, &FallingGlassTurnAThunk,
         kExpectedFallingGlassTurnA.data()},
        {kFallingGlassTurnB, &FallingGlassTurnBThunk,
         kExpectedFallingGlassTurnB.data()}
    };
    for (size_t i = 0; i < std::size(sites); ++i) {
        if (!patches.Track(
                InstallBranch(g_fallingGlassPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE9),
                g_fallingGlassPatches[i])) {
            Log("Falling glass fix skipped: FallingGlassPane::Update bytes do "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed timestep-scaled falling glass motion and rotation.");
    return true;
}

bool InstallBreakableObjectLifetimeFix() {
    if (!InstallJump(g_breakObjectLifetimePatch, kBreakObjectLifetime,
                     &BreakObjectLifetimeThunk,
                     kExpectedBreakObjectLifetime)) {
        Log("Breakable object lifetime fix skipped: BreakObject_c::Update "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    g_breakLifetimeLastFrame = 0xFFFFFFFFu;
    g_breakLifetimeCarry = 0.0f;
    g_breakLifetimeTicks = 0;
    Log("Installed real-time breakable object lifetime counters.");
    return true;
}

bool InstallBikeLeanTargetFix() {
    if (!InstallJump(g_bikeLeanTargetPatch, kBikeLeanTarget,
                     &BikeLeanTargetThunk, kExpectedBikeLeanTarget)) {
        Log("Bike lean target fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real-time bike lean target derivative.");
    return true;
}

bool InstallBikePitchExperiment() {
    PatchSet patches("Bike pitch experiment");
    g_bmxLaunchCorrectionBike = 0;
    g_bmxLandingProtectionBike = 0;
    g_bmxLandingProtectionUntil = 0;
    g_bmxLandingWasAirborne = false;
    g_bmxLandingContactSeen = false;
    g_bikePitchExperimentStrength = static_cast<float>(std::clamp(
        ReadNumber("vehicles", "bikePitchExperimentStrength", 100), 0, 100))
                                  / 100.0f;
    if (!patches.Track(
            RepointCall(g_bikePitchExperimentPatch, kBikeWheelTurnForceCall,
                        kApplyTurnForce, &BikePitchExperimentThunk),
            g_bikePitchExperimentPatch)) {
        Log("Bike pitch experiment skipped: wheel-contact ApplyTurnForce call "
            "does not match GTA SA 1.0 US.");
        return false;
    }
    const bool bikeHookWasInstalled = g_bikeProcessPatch.installed;
    if (!EnsureBikeProcessControlHook()
        || (!bikeHookWasInstalled
            && !patches.Track(true, g_bikeProcessPatch))) {
        Log("Bike pitch experiment skipped: CBike::ProcessControl entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            InstallDetour(g_bmxLaunchBunnyHopPatch, kBmxLaunchBunnyHop,
                          &HookedBmxLaunchBunnyHop,
                          kExpectedBmxLaunchBunnyHop.data(),
                          kExpectedBmxLaunchBunnyHop.size()),
            g_bmxLaunchBunnyHopPatch)) {
        Log("Bike pitch experiment skipped: BMX launch callback entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            InstallDetour(g_bikeDamageKnockOffPatch,
                          kBikeDamageKnockOffRider,
                          &HookedBikeDamageKnockOffRider,
                          kExpectedBikeDamageKnockOffRider.data(),
                          kExpectedBikeDamageKnockOffRider.size()),
            g_bikeDamageKnockOffPatch)) {
        Log("Bike pitch experiment skipped: DamageKnockOffRider entry does "
            "not match GTA SA 1.0 US.");
        return false;
    }
    for (size_t i = 0; i < kBikeRiderFallEventAddCalls.size(); ++i) {
        if (!patches.Track(
                RepointCall(g_bmxRiderFallTracePatches[i],
                            kBikeRiderFallEventAddCalls[i], kEventGroupAdd,
                            &HookedBmxRiderFallEventAdd),
                g_bmxRiderFallTracePatches[i])) {
            Log("Bike pitch experiment skipped: rider-fall event call does "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    char installed[128];
    std::snprintf(installed, sizeof(installed),
                  "Installed experimental %.0f%% correction of positive bike "
                  "pitch while climbing off a ramp above 30 FPS.",
                  g_bikePitchExperimentStrength * 100.0f);
    patches.Commit();
    Log(installed);
    return true;
}

bool InstallGroundFrictionFix() {
    if (!InstallJump(g_groundFrictionPatch, kGroundFrictionClamp,
                     &GroundFrictionClampThunk, kExpectedGroundFriction)) {
        Log("Ground friction fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-normalized ground friction budget for vehicles.");
    return true;
}

bool InstallTurnAirResistanceFix() {
    if (!InstallJump(g_turnAirResistancePatch, kTurnAirResistance,
                     &TurnAirResistanceThunk, kExpectedTurnAirResistance)) {
        Log("Turn air resistance fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed timestep-normalized turn speed air resistance.");
    return true;
}

bool InstallMoveSpeedSnapFix() {
    PatchSet patches("Move speed snap fix");
    const std::array<const void*, 6> thunks{
        &MoveSpeedSnapCarXThunk,
        &MoveSpeedSnapCarYThunk,
        &MoveSpeedSnapCarZThunk,
        &MoveSpeedSnapBikeXThunk,
        &MoveSpeedSnapBikeYThunk,
        &MoveSpeedSnapBikeZThunk,
    };

    for (const auto address : kMoveSpeedSnapSites) {
        if (!MemoryMatches(address, kExpectedMoveSpeedSnap)) {
            Log("Move speed snap fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < kMoveSpeedSnapSites.size(); ++i) {
        if (!patches.Track(
                InstallJump(g_moveSpeedSnapPatches[i], kMoveSpeedSnapSites[i],
                            thunks[i], kExpectedMoveSpeedSnap),
                g_moveSpeedSnapPatches[i])) {
            Log("Move speed snap fix failed while installing hooks.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-normalized move speed snap limit for cars and bikes.");
    return true;
}

bool InstallPhysicsSleepRateFix() {
    PatchSet patches("Physics sleep rate fix");
    constexpr std::array<uintptr_t, 4> addresses{
        kObjectFakePhysics, kCarFakePhysics, kBikeFakePhysics,
        kTrailerFakePhysics
    };
    const std::array<std::array<uint8_t, 8>, 4> expected{
        kExpectedObjectFakePhysics,
        kExpectedCarFakePhysics,
        kExpectedBikeFakePhysics,
        kExpectedTrailerFakePhysics,
    };
    const std::array<const void*, 4> thunks{
        &ObjectFakePhysicsThunk,
        &CarFakePhysicsThunk,
        &BikeFakePhysicsThunk,
        &TrailerFakePhysicsThunk,
    };

    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!MemoryMatches(addresses[i], expected[i])) {
            Log("Physics sleep rate fix skipped: executable bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (!patches.Track(InstallJump(g_fakePhysicsPatches[i], addresses[i],
                                       thunks[i], expected[i]),
                           g_fakePhysicsPatches[i])) {
            Log("Physics sleep rate fix failed while installing hooks.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a real-time physics sleep counter for objects, cars, bikes and trailers.");
    return true;
}

bool InstallSirenTapFix() {
    if (!MemoryMatches(kSirenAnchor, kExpectedSirenAnchor)) {
        Log("Siren tap fix skipped: ProcessSirenAndHorn layout does not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallJump(g_sirenPatch, kSirenPatch, &SirenTapThunk,
                     kExpectedSiren)) {
        Log("Siren tap fix skipped: executable bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed wall-clock siren tap detection.");
    return true;
}

bool InstallParticleEmissionRateFix() {
    ResetParticleSites();
    g_generalBudget = {};
    if (!InstallDetour(g_fxAddParticlePatch, kFxAddParticle,
                       &HookedFxAddParticle, kExpectedFxAddParticle.data(),
                       kExpectedFxAddParticle.size())) {
        Log("Particle emission rate fix skipped: FxSystem_c::AddParticle bytes "
            "do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a frame-rate independent direct particle emission rate.");
    return true;
}

bool InstallContinuousWeaponParticlesFix() {
    if (!InstallDetour(g_fxCreateParticlesPatch, kFxCreateParticles,
                       &HookedFxCreateParticles,
                       kExpectedFxCreateParticles.data(),
                       kExpectedFxCreateParticles.size())) {
        Log("Continuous weapon emission carry failed at FxEmitter::CreateParticles.");
        return false;
    }
    Log("Installed fractional emission carry for continuous weapon FX systems.");
    return true;
}

bool InstallDrowningDamageFix() {
    if (!InstallJump(g_drowningDamagePatch, kDrowningDamage,
                     &DrowningDamageThunk, kExpectedDrowningDamage)) {
        Log("Drowning damage fix skipped: CPlayerPed::HandlePlayerBreath bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed fraction-preserving drowning damage.");
    return true;
}

bool InstallContinuousWeaponAmmoFix() {
    if (!InstallJump(g_continuousAmmoPatch, kContinuousAmmoPatch,
                     &ContinuousWeaponAmmoThunk, kExpectedContinuousAmmo)) {
        Log("Continuous weapon ammo fix skipped: CWeapon::Fire bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed time-based ammo consumption for continuous area-effect weapons.");
    return true;
}

bool InstallChainsawStrikeRateFix() {
    if (!InstallBranch(g_chainsawStrikePatch, kChainsawStrikeRewind,
                       &ChainsawStrikeRewindThunk,
                       kExpectedChainsawStrikeRewind.data(),
                       kExpectedChainsawStrikeRewind.size(), 0xE8)) {
        Log("Chainsaw strike rate fix skipped: CTaskSimpleFight::ProcessPed "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a frame-rate independent chainsaw strike rate.");
    return true;
}

bool InstallFrameLimit(int limit) {
    PatchSet patches("Frame limit");
    if (!MemoryMatches(kFrameLimiterGate, kExpectedFrameLimiterGate)
        || !MemoryMatches(kFrameLimitStore, kExpectedFrameLimitStore)) {
        Log("Frame limit skipped: frame limiter bytes do not match GTA SA 1.0 US.");
        return false;
    }
    // 0x75 is `jne`, 0xEB the unconditional `jmp` with the same displacement.
    if (!patches.Track(
            InstallByte(g_frameLimiterGatePatch, kFrameLimiterGate, 0xEB),
            g_frameLimiterGatePatch)
        || !patches.Track(
            InstallByte(g_frameLimitStorePatch, kFrameLimitStoreOperand,
                        static_cast<uint8_t>(limit)),
            g_frameLimitStorePatch)) {
        Log("Frame limit failed while patching the frame limiter.");
        return false;
    }
    WriteFrameLimit(static_cast<uint8_t>(limit));
    patches.Commit();
    Log("Installed the configured frame limit.");
    return true;
}

bool InstallRefreshRate(int refreshRate) {
    if (!MemoryMatches(kRefreshRateCompare, kExpectedRefreshRate)) {
        Log("Refresh rate skipped: mode selection bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!InstallByte(g_refreshRatePatch, kRefreshRateOperand,
                     static_cast<uint8_t>(refreshRate))) {
        Log("Refresh rate failed while patching mode selection.");
        return false;
    }
    Log("Installed the configured minimum display refresh rate.");
    return true;
}

bool InstallAutoFpsLimit() {
    PatchSet patches("Automatic FPS limit");
    if (!patches.Track(
            InstallJump(g_scriptsProcessPatch, kScriptsProcess,
                        &ScriptsProcessThunk, kExpectedScriptsProcess),
            g_scriptsProcessPatch)) {
        Log("Automatic FPS limit skipped: CTheScripts::Process bytes do not match GTA SA 1.0 US.");
        return false;
    }
    // The cases below write `RsGlobal.frameLimit`, which the engine only reads
    // once the limiter gate is open, so this needs it too.
    constexpr std::array<uint8_t, 2> openGate{0xEB, 0x17};
    if (!g_frameLimiterGatePatch.installed
        && !MemoryMatches(kFrameLimiterGate, openGate)) {
        if (!MemoryMatches(kFrameLimiterGate, kExpectedFrameLimiterGate)
            || !patches.Track(
                InstallByte(g_frameLimiterGatePatch, kFrameLimiterGate, 0xEB),
                g_frameLimiterGatePatch)) {
            Log("Automatic FPS limit skipped: the frame limiter gate could not "
                "be opened.");
            return false;
        }
    }
    if (g_autoLimit.flags.forPauseMenu
        && !patches.Track(
            InstallJump(g_menuBackgroundPatch, kMenuBackground,
                        &MenuBackgroundThunk, kExpectedMenuBackground),
            g_menuBackgroundPatch)) {
        patches.Commit();
        Log("Automatic FPS limit installed without the pause menu case.");
        return true;
    }
    patches.Commit();
    Log("Installed automatic FPS limiting for the configured game cases.");
    return true;
}
