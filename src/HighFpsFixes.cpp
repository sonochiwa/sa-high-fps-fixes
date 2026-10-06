// GTA San Andreas ties a long list of behaviours to the rendered frame
// rather than to game time: swimming, vehicle physics, camera timers, HUD
// counters, particle emission and more were tuned at 30 FPS and drift above
// it. Each fix patches one of those sites so it reads the real timestep,
// verifies the expected bytes first, and has its own switch in
// HighFpsFixes.ini. Behaviour at 30 FPS is unchanged.
//
// The fixes live in one directory per area, with their patch sites in
// game\sites. This file pins the module, installs every fix from a thread off
// the loader lock in the order of the INI sections, and restores everything
// on unload.

#include "audio/ambience.h"
#include "audio/engine_revs.h"
#include "bikes/abandoned_bike.h"
#include "bikes/bike_wheel_spin.h"
#include "bikes/bmx_lean.h"
#include "bikes/lean_target.h"
#include "bikes/takeoff_pitch.h"
#include "camera/aim_camera.h"
#include "camera/drunk_camera.h"
#include "camera/follow_camera.h"
#include "camera/stunt_jump_camera.h"
#include "core/config.h"
#include "core/conflicts.h"
#include "core/fixes.h"
#include "core/hook.h"
#include "core/log.h"
#include "core/module.h"
#include "core/patch.h"
#include "framerate/auto_limit.h"
#include "framerate/frame_limit.h"
#include "framerate/radio_lock.h"
#include "game/frame_hook.h"
#include "game/profile.h"
#include "handling/gearbox.h"
#include "handling/steer_input.h"
#include "handling/suspension.h"
#include "handling/turn_air_resistance.h"
#include "handling/wheel_slip.h"
#include "hud/flash_rate.h"
#include "hud/map_zoom.h"
#include "hud/money_counter.h"
#include "player/aiming_walk.h"
#include "player/bloody_footprints.h"
#include "player/buoyancy.h"
#include "player/climbing.h"
#include "player/drowning.h"
#include "player/drunk_steering.h"
#include "player/fat_counter.h"
#include "player/jetpack.h"
#include "player/object_pickup.h"
#include "player/ped_push.h"
#include "player/swimming.h"
#include "scripts/script_literals.h"
#include "scripts/script_objects.h"
#include "timers/frame_time_carry.h"
#include "vehicles/aircraft.h"
#include "vehicles/attached_entity.h"
#include "vehicles/boat_engine.h"
#include "vehicles/boat_waves.h"
#include "vehicles/doors.h"
#include "vehicles/head_bop.h"
#include "vehicles/hydraulics.h"
#include "vehicles/jump_out.h"
#include "vehicles/moving_parts.h"
#include "vehicles/rest.h"
#include "vehicles/sinking.h"
#include "vehicles/siren.h"
#include "vehicles/water_cannon.h"
#include "vehicles/wheels.h"
#include "weapons/area_shots.h"
#include "weapons/chainsaw.h"
#include "weapons/tear_gas.h"
#include "weapons/weapon_ammo.h"
#include "weapons/weapon_particles.h"
#include "world/breakable_objects.h"
#include "world/explosions.h"
#include "world/falling_glass.h"
#include "world/fire_spread.h"
#include "world/lightning.h"
#include "world/particle_emission.h"
#include "world/police.h"
#include "world/samp_objects.h"

#include <windows.h>

#include <string>

namespace {

using namespace hff;

constexpr FixSpec kCameraFixes[] = {
    {"camera", "stuntJumpCamera", "Stunt jump camera fix",
     camera::InstallStuntJumpCameraFix},
    {"camera", "aimCameraShake", "Aim camera shake fix",
     camera::InstallAimCameraShakeFix},
    {"camera", "followCameraRate", "Follow camera rate fix",
     camera::InstallFollowCameraRateFix},
    {"camera", "idleCameraTimer", "Idle camera timer fix",
     timers::InstallIdleCameraTimerFix},
    {"camera", "drunkCameraShake", "Drunk camera shake fix",
     camera::InstallDrunkCameraShakeFix},
};

constexpr FixSpec kPlayerFixes[] = {
    {"player", "aimingRifleWalk", "Aiming rifle walk fix",
     player::InstallAimingRifleWalkFix},
    {"player", "swimmingMovement", "Swimming movement fix",
     player::InstallSwimmingMovementFix},
    {"player", "swimPitchRate", "Swim pitch rate fix",
     player::InstallSwimPitchRateFix},
    {"player", "pedPushVehicle", "Ped push vehicle fix",
     player::InstallPedPushVehicleFix},
    {"player", "bloodyFootprints", "Bloody footprints fix",
     player::InstallBloodyFootprintsFix},
    {"player", "drowningDamage", "Drowning damage fix",
     player::InstallDrowningDamageFix},
    {"player", "drunkSteerDelay", "Drunk steering delay fix",
     player::InstallDrunkSteerDelayFix},
    {"player", "parachuteFlight", "Parachute flight fix",
     scripts::InstallParachuteFlightFix},
    {"player", "jetPackFlame", "Jetpack flame ramp fix",
     player::InstallJetPackFxRampFix},
    {"player", "fatCounter", "Fat counter fix", player::InstallFatCounterFix},
    {"player", "waterBuoyancy", "Water buoyancy fix",
     player::InstallWaterBuoyancyFix},
    {"player", "climbSpeed", "Climb speed fix", player::InstallClimbSpeedFix},
    {"player", "objectPickUp", "Object pickup fix",
     player::InstallObjectPickUpFix},
    {"player", "skillProgress", "Skill progress fix",
     timers::InstallSkillProgressFix},
    {"player", "stuntCounters", "Stunt counter fix",
     timers::InstallStuntCountersFix},
    {"player", "taskTimers", "Ped task timer fix",
     timers::InstallTaskTimersFix},
};

constexpr FixSpec kVehicleBodyFixes[] = {
    {"vehicles", "restThreshold", "Vehicle rest threshold fix",
     vehicles::InstallVehicleRestThresholdFix},
    {"vehicles", "bikeLeanTarget", "Bike lean target fix",
     bikes::InstallBikeLeanTargetFix},
    {"vehicles", "bikePitchExperiment", "Bike pitch experiment",
     bikes::InstallBikePitchExperiment},
    {"vehicles", "physicsSleepRate", "Physics sleep rate fix",
     vehicles::InstallPhysicsSleepRateFix},
    {"vehicles", "wheelFriction", "Wheel friction fix",
     vehicles::InstallWheelFrictionFix},
};

constexpr FixSpec kHandlingFixes[] = {
    {"vehicles", "turnAirResistance", "Turn air resistance fix",
     handling::InstallTurnAirResistanceFix},
    {"vehicles", "aircraftTurnResistance", "Aircraft turn resistance fix",
     handling::InstallAircraftTurnResistanceFix},
    {"vehicles", "steerInputRate", "Steer input rate fix",
     handling::InstallSteerInputRateFix},
    {"vehicles", "gearChangeInertia", "Gear change inertia fix",
     handling::InstallGearChangeInertiaFix},
    {"vehicles", "gearChangeKick", "Gear change kick fix",
     handling::InstallGearChangeKickFix},
    {"vehicles", "suspensionDampingLimit", "Suspension damping limit fix",
     handling::InstallSuspensionDampingLimitFix},
    {"vehicles", "suspensionLoadLean", "Suspension load lean fix",
     handling::InstallSuspensionLoadLeanFix},
    {"vehicles", "wheelSlipRate", "Wheel slip rate fix",
     handling::InstallWheelSlipRateFix},
};

constexpr FixSpec kVehicleFixes[] = {
    {"vehicles", "abandonedBikePhysicsStep", "Abandoned bike physics step fix",
     bikes::InstallAbandonedBikePhysicsStepFix},
    {"vehicles", "railWheelSpin", "Rail wheel spin fix",
     vehicles::InstallRailWheelSpinFix},
    {"vehicles", "burnout", "Burnout fix", vehicles::InstallBurnoutFix},
    {"vehicles", "doorSwing", "Door swing fix", vehicles::InstallDoorSwingFix},
    {"vehicles", "movingParts", "Moving parts fix",
     vehicles::InstallMovingPartsFix},
    {"vehicles", "waterCannon", "Water cannon fix",
     vehicles::InstallWaterCannonFix},
    {"vehicles", "sirenTap", "Siren tap fix", vehicles::InstallSirenTapFix},
    {"vehicles", "heliRotorSpeed", "Helicopter rotor fix",
     vehicles::InstallHeliRotorSpeedFix},
    {"vehicles", "attachedEntitySpeed", "Attached entity speed fix",
     vehicles::InstallAttachedEntitySpeedFix},
    {"vehicles", "aiAircraftSteer", "AI aircraft steering fix",
     vehicles::InstallAiAircraftSteerFix},
    {"vehicles", "upsideDownTimer", "Upside down car timer fix",
     timers::InstallUpsideDownTimerFix},
    {"vehicles", "vehicleTimers", "Vehicle timer fix",
     timers::InstallVehicleTimersFix},
    {"vehicles", "burnTimers", "Vehicle burn timer fix",
     timers::InstallBurnTimersFix},
    {"vehicles", "wheelSettle", "Wheel settle fix",
     vehicles::InstallWheelSettleFix},
    {"vehicles", "wheelSpin", "Free wheel spin fix",
     vehicles::InstallWheelSpinFix},
    {"vehicles", "boatEngineSpeed", "Boat engine speed fix",
     vehicles::InstallBoatEngineSpeedFix},
    {"vehicles", "bmxSprintLean", "BMX sprint lean fix",
     bikes::InstallBmxSprintLeanFix},
    {"vehicles", "bmxLeanSettle", "BMX lean settle fix",
     bikes::InstallBmxLeanSettleFix},
    {"vehicles", "bikeWheelSpin", "Bike wheel spin fix",
     bikes::InstallBikeWheelSpinFix},
    {"vehicles", "headBopping", "Head bopping fix",
     vehicles::InstallHeadBoppingFix},
    {"vehicles", "jumpOutCarSpeed", "Jump out car speed fix",
     vehicles::InstallJumpOutCarSpeedFix},
    {"vehicles", "skimmerResistance", "Skimmer resistance fix",
     vehicles::InstallSkimmerResistanceFix},
    {"vehicles", "vehicleSinking", "Vehicle sinking fix",
     vehicles::InstallVehicleSinkingFix},
    {"vehicles", "damagedPlaneControl", "Damaged plane control fix",
     vehicles::InstallDamagedPlaneControlFix},
    {"vehicles", "hydraulicStance", "Hydraulic stance fix",
     vehicles::InstallHydraulicStanceFix},
    {"vehicles", "boatWaves", "Boat wave fix", vehicles::InstallBoatWavesFix},
    {"vehicles", "swatRopes", "SWAT rope fix", vehicles::InstallSwatRopesFix},
};

constexpr FixSpec kWeaponFixes[] = {
    {"weapons", "continuousWeaponParticles", "Continuous weapon particle fix",
     weapons::InstallContinuousWeaponParticlesFix},
    {"weapons", "continuousWeaponAmmo", "Continuous weapon ammo fix",
     weapons::InstallContinuousWeaponAmmoFix},
    {"weapons", "continuousWeaponShots", "Continuous weapon shot rate fix",
     weapons::InstallContinuousWeaponShotsFix},
    {"weapons", "chainsawStrikeRate", "Chainsaw strike rate fix",
     weapons::InstallChainsawStrikeRateFix},
    {"weapons", "tearGas", "Tear gas fix", weapons::InstallTearGasFix},
    {"particles", "emissionRate", "Particle emission rate fix",
     world::InstallParticleEmissionRateFix},
};

constexpr FixSpec kAudioFixes[] = {
    {"audio", "engineRevs", "Engine revs fix", audio::InstallEngineRevsFix},
    {"audio", "ambientSounds", "Ambient sounds fix", audio::InstallAmbientSoundsFix},
};

constexpr FixSpec kWorldFixes[] = {
    {"world", "gangWarTimer", "Gang war timer fix",
     timers::InstallGangWarTimerFix},
    {"world", "missionTimers", "Mission timer fix",
     timers::InstallMissionTimersFix},
    {"world", "fireSpread", "Fire spread fix", world::InstallFireSpreadFix},
    {"world", "scriptObjectSlide", "Script object slide fix",
     scripts::InstallScriptObjectSlideFix},
    {"world", "scriptObjectRotate", "Script object rotate fix",
     scripts::InstallScriptRotateObjectFix},
    {"world", "sampObjectRotation", "SA-MP moving object rotation fix",
     world::InstallSampObjectRotationFix},
    {"world", "fallingGlass", "Falling glass fix",
     world::InstallFallingGlassFix},
    {"world", "breakableObjectLifetime", "Breakable object lifetime fix",
     world::InstallBreakableObjectLifetimeFix},
    {"world", "burglaryNoise", "Burglary noise fix",
     scripts::InstallBurglaryNoiseFix},
    {"world", "missionScripts", "Mission script fix",
     scripts::InstallMissionScriptsFix},
    {"world", "copCarExit", "Cop car exit fix", world::InstallCopCarExitFix},
    {"world", "explosions", "Explosion fix", world::InstallExplosionsFix},
    {"world", "lightning", "Lightning fix", world::InstallLightningFix},
    {"menu", "mapZoomWheel", "Map zoom wheel fix", hud::InstallMapZoomWheelFix},
};

// One switch over three mechanisms: the flash clock, the money counter step
// and the 46 timed-text accumulators. They are separate patches but one
// symptom to a player, so they are configured together.
void InstallHudFixes() {
    if (!ReadSetting("hud", "hudTiming", true)) {
        CountDisabled(3, "HUD timing fixes disabled by configuration.");
        return;
    }
    CountInstall(hud::InstallMoneyCounterFix());
    CountInstall(hud::InstallHudFlashRateFix());
    CountInstall(timers::InstallHudTimersFix());
}

void InstallFramerateFixes() {
    framerate::InstallFrameLimit();
    // Nobody wants a car ride held at 30 FPS, so this fix has no switch.
    CountInstall(framerate::InstallRadioFrameLockFix());
    framerate::InstallAutoFpsLimit();
}

// Gets the guard its once-a-frame call. Other fixes use the same hook, so
// when one of them installed first the guard simply rides along.
void InstallConflictingHookGuard() {
    if (!ConflictGuardEnabled()) {
        Log("Conflicting hook guard disabled by configuration.");
        return;
    }
    if (InstallFrameHook("Conflicting hook guard")) {
        AddFrameCallback(&GuardInstalledSites);
        Log("Installed the conflicting hook guard.");
    }
}

DWORD WINAPI Initialize(void*) {
    SetConfigPath(ModulePathWithExtension(".ini"));
    // Read before anything can log: `log=0` writes no file at all, whatever
    // goes wrong afterwards.
    if (ReadSetting("general", "log", false)) {
        OpenLog(ModulePathWithExtension(".log"));
    }
    PrepareConfig();

    const GameProfile* profile = DetectGameProfile();
    if (!profile) {
        Log("Initialization skipped: this is not a GTA SA 1.0 US executable.");
        return 0;
    }
    InitializeConflictGuard(
        ReadSetting("general", "overrideConflictingHooks", true));
    ReadClassicHandling();
    std::string profileMessage("Detected executable profile: ");
    profileMessage += profile->name;
    profileMessage += ".";
    Log(profileMessage.c_str());

    InstallFixes(kCameraFixes);
    InstallFixes(kPlayerFixes);
    InstallFixes(kVehicleBodyFixes);
    InstallFixes(kHandlingFixes);
    InstallFixes(kVehicleFixes);
    InstallFixes(kWeaponFixes);
    InstallHudFixes();
    InstallFixes(kAudioFixes);
    InstallFixes(kWorldFixes);
    InstallFramerateFixes();
    InstallConflictingHookGuard();

    ReportConfigWarnings();
    LogInstallSummary();
    MarkFixesInstalled();
    return 0;
}

void Shutdown() {
    if (!StopAllWorkerThreads()) {
        return;
    }
    bikes::DisableAbandonedBikePhysicsStep();
    vehicles::RestoreDoorSwinging();
    camera::RemoveAimCameraHooks();
    RestoreAllPatches();
    UninitializeMinHook();
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void* reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        hff::g_module = instance;
        DisableThreadLibraryCalls(instance);
        // An ASI loader does not provide a pre-FreeLibrary shutdown callback.
        // Pin the module before the initializer can create hooks or workers so
        // no thread can return into an unloaded image. Process termination
        // reclaims the module and handles without running the restoration path.
        if (!hff::PinPluginModule(instance)) {
            return FALSE;
        }
        if (HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0,
                                         nullptr)) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH && reserved == nullptr) {
        Shutdown();
    }
    return TRUE;
}
