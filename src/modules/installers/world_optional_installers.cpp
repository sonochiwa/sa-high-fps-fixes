#include "modules/modules.h"

namespace hff {

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

// Returns the single occurrence of `pattern` in the module's code section, or
// zero when it is absent or appears more than once. Ambiguity is treated as
// absence: a second match would mean the pattern no longer identifies the site
// it was written for.
uintptr_t FindUniqueCodePattern(HMODULE module, const uint8_t* pattern,
                                size_t size) {
    const auto* image = reinterpret_cast<const uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    const auto* headers =
        reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
    if (headers->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    const uint8_t* code = image + headers->OptionalHeader.BaseOfCode;
    const size_t length = headers->OptionalHeader.SizeOfCode;
    if (length < size) {
        return 0;
    }

    const uint8_t* found = nullptr;
    for (size_t i = 0; i + size <= length; ++i) {
        if (std::memcmp(code + i, pattern, size) != 0) {
            continue;
        }
        if (found) {
            return 0;
        }
        found = code + i;
        i += size - 1;
    }
    return reinterpret_cast<uintptr_t>(found);
}

bool InstallSampObjectRotationFix() {
    const HMODULE samp = GetModuleHandleA("samp.dll");
    if (!samp) {
        Log("SA-MP moving object rotation fix skipped: samp.dll is not loaded.");
        return false;
    }

    const uintptr_t arrival =
        FindUniqueCodePattern(samp, kSampObjectMoveArrivalPattern.data(),
                              kSampObjectMoveArrivalPattern.size());
    if (!arrival) {
        Log("SA-MP moving object rotation fix skipped: CObject::Process "
            "arrival test not found in samp.dll.");
        return false;
    }

    const uintptr_t rotation = arrival + kSampObjectRotationProgressOffset;
    int32_t stillMoving{};
    std::memcpy(&stillMoving,
                reinterpret_cast<const void*>(
                    arrival + kSampObjectMoveContinueDisplacement),
                sizeof(stillMoving));

    PatchSet patches("SA-MP moving object rotation fix");
    g_sampObjectRotationReturn = rotation + kSampObjectRotationProgressPatchSize;
    g_sampObjectArrivedReturn = arrival + kSampObjectMoveArrivedOffset;
    g_sampObjectMovingReturn = g_sampObjectArrivedReturn + stillMoving;
    if (!patches.Track(
            InstallJump(g_sampObjectRotationPatch, rotation,
                        &SampObjectRotationThunk, kExpectedSampObjectRotation),
            g_sampObjectRotationPatch)
        || !patches.Track(
            InstallBranch(g_sampObjectArrivalPatch, arrival,
                          &SampObjectArrivalThunk,
                          kSampObjectMoveArrivalPattern.data(),
                          kSampObjectArrivalPatchSize, 0xE9),
            g_sampObjectArrivalPatch)) {
        g_sampObjectRotationReturn = 0;
        g_sampObjectArrivedReturn = 0;
        g_sampObjectMovingReturn = 0;
        Log("SA-MP moving object rotation fix skipped: CObject::Process bytes "
            "around the arrival test are not the expected ones.");
        return false;
    }
    patches.Commit();
    Log("Installed wall-clock rotation progress and arrival for moving SA-MP "
        "objects.");
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

    if (!InstallJumpTable(patches, g_fakePhysicsPatches, addresses, thunks,
                          expected)) {
        Log("Physics sleep rate fix skipped: executable bytes do not match the active game profile.");
        return false;
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

bool InstallContinuousWeaponShotsFix() {
    PatchSet patches("Continuous weapon shot rate fix");
    if (!patches.Track(InstallCall(g_continuousShotPatches[0], kAreaEffectAddShotCall,
                                   &GatedAreaEffectAddShot,
                                   kExpectedAreaEffectAddShotCall),
                       g_continuousShotPatches[0])
        || !patches.Track(InstallCall(g_continuousShotPatches[1],
                                      kAreaEffectCreepingFireCall,
                                      &GatedAreaEffectCreepingFire,
                                      kExpectedAreaEffectCreepingFireCall),
                          g_continuousShotPatches[1])) {
        Log("Continuous weapon shot rate fix skipped: CWeapon::FireAreaEffect "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallCall(g_continuousShotPatches[2], kShotExtinguishCall,
                                   &ExtinguishShotWithWater,
                                   kExpectedShotExtinguishCall),
                       g_continuousShotPatches[2])) {
        Log("Continuous weapon shot rate fix skipped: CShotInfo::Update bytes "
            "do not match GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed spraycan, extinguisher and flamethrower shots at the "
        "original rate.");
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

bool InstallRadioFrameLockFix() {
    if (!InstallCall(g_radioFrameLockPatch, kFrameLimiterBeatCheck,
                     &BeatTrackHoldsFrameLimit, kExpectedFrameLimiterBeatCheck)) {
        Log("Radio frame lock fix skipped: the main loop bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a frame limiter that music holds only in the dance and "
        "lowrider minigames.");
    return true;
}

bool InstallAutoFpsLimit() {
    PatchSet patches("Automatic FPS limit");
    // The per-frame hook is shared with other fixes; whichever installs first
    // puts it in place, and a failure here must not remove theirs.
    if (!g_scriptsProcessPatch.installed
        && !patches.Track(
            InstallJump(g_scriptsProcessPatch, kScriptsProcess,
                        &ScriptsProcessThunk, kExpectedScriptsProcess),
            g_scriptsProcessPatch)) {
        Log("Automatic FPS limit skipped: CTheScripts::Process bytes do not match GTA SA 1.0 US.");
        return false;
    }
    // The cases write `RsGlobal.frameLimit`, which the engine reads only while
    // the limiter gate is open. With `fpsLimit` set the gate is open for good;
    // otherwise it is opened for the length of a case and closed after it, so
    // frames outside the cases stay as the game's own setting has them.
    constexpr std::array<uint8_t, 2> openGate{0xEB, 0x17};
    g_autoLimitTogglesGate = !g_frameLimiterGatePatch.installed
                          && !MemoryMatches(kFrameLimiterGate, openGate);
    if (g_autoLimitTogglesGate
        && !MemoryMatches(kFrameLimiterGate, kExpectedFrameLimiterGate)) {
        g_autoLimitTogglesGate = false;
        Log("Automatic FPS limit skipped: the frame limiter gate does not "
            "match GTA SA 1.0 US.");
        return false;
    }
    if (g_autoLimit.pauseMenu != 0
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

// Gets the guard its once-a-frame call. Other fixes use the same hook, so
// when one of them installed first the guard simply rides along.
bool InstallConflictingHookGuard() {
    if (g_scriptsProcessPatch.installed) {
        Log("Conflicting hook guard is sharing the CTheScripts::Process hook.");
        return true;
    }
    if (!InstallJump(g_scriptsProcessPatch, kScriptsProcess,
                     &ScriptsProcessThunk, kExpectedScriptsProcess)) {
        Log("Conflicting hook guard skipped: CTheScripts::Process bytes do not "
            "match GTA SA 1.0 US. Sites patched over by another plugin later "
            "in startup will not be reclaimed.");
        return false;
    }
    Log("Installed the conflicting hook guard.");
    return true;
}

} // namespace hff
