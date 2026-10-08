#pragma once

#include <cstddef>
#include <cstdint>

// The game functions and data the autotest drives, GTA San Andreas 1.0 US.
// Functions are checked against their first bytes before the test starts.

namespace game {

struct Vector {
    float x;
    float y;
    float z;
};

// gGameState and the values the start-up passes through.
constexpr uintptr_t kGameState = 0xC8D4C0;
constexpr int32_t kStatePlayingLogo = 2;
constexpr int32_t kStateTitle = 3;
constexpr int32_t kStatePlayingIntro = 4;
constexpr int32_t kStateFrontendLoading = 5;
constexpr int32_t kStateFrontendIdle = 7;
constexpr int32_t kStatePlaying = 9;
// FrontEndMenuManager.m_bActivateMenuNextFrame and m_bMenuActive: cleared
// while the front end idles, WinMain starts a new game instead of the menu.
constexpr uintptr_t kMenuActivateNextFrame = 0xBA677B;
constexpr uintptr_t kMenuActive = 0xBA67A4;
// RsGlobal.maximumWidth, maximumHeight and quit.
constexpr uintptr_t kScreenWidth = 0xC17044;
constexpr uintptr_t kScreenHeight = 0xC17048;
constexpr uintptr_t kQuit = 0xC17050;
// ForegroundApp: WinMain sleeps instead of running frames while it is 0.
constexpr uintptr_t kForegroundApp = 0x8D621C;
// FrontEndMenuManager.m_bPrefsFrameLimiter.
constexpr uintptr_t kFrameLimiterPreference = 0xBA6794;

// CTimer.
constexpr uintptr_t kTimerUpdate = 0x561B10;
constexpr uintptr_t kTimerUpdateVariables = 0x5618D0;
constexpr uintptr_t kTimerFunction = 0xB7CB28;
constexpr uintptr_t kTimerDivider = 0xB7CB2C;
constexpr uintptr_t kTimerRenderStart = 0xB7CB38;
constexpr uintptr_t kTimerCodePause = 0xB7CB48;
constexpr uintptr_t kTimerUserPause = 0xB7CB49;
constexpr uintptr_t kTimerFrameCounter = 0xB7CB4C;
constexpr uintptr_t kTimerPreviousNonClipped = 0xB7CB68;
constexpr uintptr_t kTimerPPPPrevious = 0xB7CB6C;
constexpr uintptr_t kTimerPPPrevious = 0xB7CB70;
constexpr uintptr_t kTimerPPrevious = 0xB7CB74;
constexpr uintptr_t kTimerPrevious = 0xB7CB78;
constexpr uintptr_t kTimerPauseModeTime = 0xB7CB7C;
constexpr uintptr_t kTimerNonClipped = 0xB7CB80;
constexpr uintptr_t kTimerTime = 0xB7CB84;

// CPad::UpdatePads, CGame::Process.
constexpr uintptr_t kUpdatePads = 0x541DD0;
constexpr uintptr_t kGameProcess = 0x53BEE0;

// CPad::Pads[0]: NewState, the CControllerState the game reads after
// UpdatePads, then DisablePlayerControls.
constexpr uintptr_t kPad = 0xB73458;
constexpr size_t kPadLeftStickX = 0x00;
constexpr size_t kPadLeftStickY = 0x02;
constexpr size_t kPadRightStickY = 0x06;
constexpr size_t kPadRightShoulder1 = 0x0C;
constexpr size_t kPadButtonSquare = 0x1C;
constexpr size_t kPadButtonCross = 0x20;
constexpr size_t kPadButtonCircle = 0x22;
constexpr size_t kPadShockButtonL = 0x24;
constexpr size_t kPadDisablePlayerControls = 0x10E;
// bDisablePlayerEnterCar, Duck, FireWeapon, FireWeaponWithL1, CycleWeapon and
// Jump, one byte each, which the opening of a new game sets.
constexpr size_t kPadDisableFlags = 0x119;
constexpr size_t kPadDisableFlagCount = 6;
constexpr int16_t kButtonDown = 255;
// CPad::bHornHistory, the horn of the last five frames, which CPad::Update
// fills from the real pad before the scenario's input, and its newest entry.
constexpr size_t kPadHornHistory = 0x111;
constexpr size_t kPadHornHistoryIndex = 0x116;
constexpr size_t kPadHornHistorySize = 5;

// CTheScripts::pActiveScripts, the list of running scripts, and the fields
// of CRunningScript read and written: the next script, its name, its base
// and the time the script sleeps until.
constexpr uintptr_t kActiveScripts = 0xA8B42C;
constexpr size_t kScriptNext = 0x00;
constexpr size_t kScriptName = 0x08;
constexpr size_t kScriptNameSize = 8;
constexpr size_t kScriptBaseIp = 0x10;
constexpr size_t kScriptWakeTime = 0xCC;
// CTheScripts::MissionBlock and LocalVariablesForCurrentMission.
constexpr uintptr_t kMissionBlock = 0xA7A6A0;
constexpr size_t kMissionBlockSize = 69000;
constexpr uintptr_t kMissionLocals = 0xA48960;

// Entities.
constexpr size_t kEntityMatrix = 0x14;
constexpr size_t kEntityStatus = 0x36;
constexpr size_t kMatrixRight = 0x00;
constexpr size_t kMatrixForward = 0x10;
constexpr size_t kMatrixUp = 0x20;
constexpr size_t kMatrixPosition = 0x30;
constexpr size_t kPhysicalMoveSpeed = 0x44;
constexpr size_t kPhysicalTurnSpeed = 0x50;
constexpr size_t kEntityTeleportSlot = 14;
constexpr size_t kPedHealth = 0x540;
constexpr size_t kPedFire = 0x730;
constexpr size_t kPedCreatedBy = 0x484;
constexpr uint8_t kPedCreatedByGame = 1;
constexpr size_t kPedActiveWeaponSlot = 0x718;
// CPed::m_pPlayerData and its m_nChosenWeapon: the player fires only while the
// chosen slot is the active one.
constexpr size_t kPedPlayerData = 0x480;
constexpr size_t kPlayerDataChosenWeapon = 0x20;
constexpr size_t kVehicleCreatedBy = 0x4A4;
constexpr uint32_t kMissionVehicle = 2;
constexpr size_t kAutomobileMiscAngle = 0x86C;

// CCamera TheCamera: the active camera's index and the CCam array.
constexpr uintptr_t kTheCamera = 0xB6F028;
constexpr size_t kCameraActiveCam = 0x59;
constexpr size_t kCameraCams = 0x174;
constexpr size_t kCamSize = 0x238;
constexpr size_t kCamMode = 0x0C;
constexpr size_t kCamFov = 0xB4;

// gFireManager: 60 CFire of 0x28 bytes, flags (bit 0 active) at 0 and the
// position at 4.
constexpr uintptr_t kFireManager = 0xB71F80;
constexpr size_t kFireCount = 60;
constexpr size_t kFireSize = 0x28;
constexpr size_t kFireFlags = 0x00;
constexpr size_t kFirePosition = 0x04;

// CWaterCannons::aCannons: three cannons of 0x3CC bytes, each the vehicle it
// belongs to, a ring of 32 section points and a used flag per section.
constexpr uintptr_t kWaterCannons = 0xC80740;
constexpr size_t kWaterCannonCount = 3;
constexpr size_t kWaterCannonSize = 0x3CC;
constexpr size_t kWaterCannonId = 0x00;
constexpr size_t kWaterCannonPoints = 0x0C;
constexpr size_t kWaterCannonUsed = 0x30C;
constexpr size_t kWaterCannonSections = 32;

// CPhysical buoyancy and CVehicle state, and the damage manager of a CAutomobile.
constexpr size_t kPhysicalMass = 0x8C;
constexpr size_t kPhysicalBuoyancy = 0xA0;
constexpr size_t kVehicleDrowningByte = 0x42B;
constexpr uint8_t kVehicleDrowning = 0x40;
constexpr size_t kAutomobileDamageManager = 0x5A0;
constexpr size_t kHeliRotorSpeed = 0x84C;
constexpr float kHeliRotorFullSpeed = 0.22f;
// CAutoPilot's mission, its target entity and cruise speed, and the follow
// radius HELI_FOLLOW_ENTITY keeps in CHeli::m_fMinAltitude.
constexpr size_t kAutoPilotMission = 0x3BA;
constexpr uint8_t kMissionHeliFollowEntity = 39;
constexpr size_t kAutoPilotTarget = 0x41C;
constexpr size_t kHeliFollowRadius = 0x9B0;
// bSirenOrAlarm in the top bit, and m_HornCounter, which the siren code sets
// while the horn is held.
constexpr size_t kVehicleSirenByte = 0x42D;
constexpr uint8_t kVehicleSiren = 0x80;
constexpr size_t kVehicleHornCounter = 0x514;
constexpr size_t kVehicleHandlingFlags = 0x38C;
constexpr uint32_t kHydraulicsInstalled = 0x20000;
constexpr size_t kAutomobileGasPedalAudio = 0x964;
constexpr size_t kVehicleEngineByte = 0x428;
constexpr uint8_t kVehicleEngineOn = 0x10;

// CExplosion::aExplosions.
constexpr uintptr_t kExplosions = 0xC88950;
constexpr size_t kExplosionCount = 16;
constexpr size_t kExplosionSize = 0x7C;
constexpr size_t kExplosionType = 0x00;
constexpr size_t kExplosionActiveCounter = 0x28;

// CUserDisplay::OnscnTimer, the on-screen mission clock, which counts a
// script variable given by its byte offset in CTheScripts::ScriptSpace.
constexpr uintptr_t kOnscreenTimer = 0xBA1788;
constexpr uintptr_t kScriptSpace = 0xA49960;

// Functions.
constexpr uintptr_t kRand = 0x821B1E;
constexpr uintptr_t kRandomNumberInRange = 0x407180;
constexpr uintptr_t kAddProjectile = 0x737C80;
constexpr uintptr_t kAddExplosion = 0x736A50;
constexpr uintptr_t kSetAeroplaneCompStatus = 0x6C22D0;
constexpr uintptr_t kAddClock = 0x44CD50;
constexpr uintptr_t kClearClock = 0x44CE60;
constexpr uintptr_t kFindPlayerPed = 0x56E210;
constexpr uintptr_t kVehicleCheat = 0x43A0B0;
constexpr uintptr_t kSetPedInCarDirect = 0x650280;
constexpr uintptr_t kLoadScene = 0x40EB70;
constexpr uintptr_t kFindGroundZ = 0x569660;
constexpr uintptr_t kCameraFade = 0x50AC20;
constexpr uintptr_t kCameraBehindPlayer = 0x50BD40;
constexpr uintptr_t kGiveWeapon = 0x5E6080;
constexpr uintptr_t kSetCurrentWeapon = 0x5E6280;
constexpr uintptr_t kStartFire = 0x539F00;
constexpr uintptr_t kExtinguishPoint = 0x539450;
constexpr uintptr_t kRequestModel = 0x4087E0;
constexpr uintptr_t kLoadAllRequestedModels = 0x40EA10;
constexpr uintptr_t kTellHeliToGoToCoors = 0x6A2390;
constexpr uintptr_t kStartNewScript = 0x464C20;

template <typename T>
T& At(uintptr_t address) {
    return *reinterpret_cast<T*>(address);
}

template <typename T>
T& Field(const void* object, size_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(object) + offset);
}

bool VerifyFunctions();
int32_t GameState();
uint8_t* Player();
Vector Position(const void* entity);
Vector MatrixRow(const void* entity, size_t row);
void Teleport(void* entity, const Vector& position);
float GroundZ(float x, float y);
void ShowPlayer();
uint8_t* SpawnVehicle(int32_t model);
void PutPlayerIn(void* vehicle);
void GiveWeapon(int32_t weapon, int32_t model, uint32_t ammo);
void StartFire(const Vector& position, float size);
void ExtinguishFiresAround(const Vector& position, float radius);
uint8_t* ActiveCam();
void SleepAllScripts();
// An explosion of `type`, with no victim or creator, silent and visible.
void AddExplosion(int32_t type, const Vector& position);
// A projectile of `weapon`, whose model is `model`, dropped at `position` by
// the player.
void DropProjectile(int32_t weapon, int32_t model, const Vector& position);
// While on, every `GetRandomNumberInRange(0, 100)` walks 0 to 99 in a fixed
// order, so a roll below n passes exactly n times in a hundred.
void EvenRandomPercent(bool on);
// Sets the damage state, 0 to 2, of a plane's moving part `frame`.
void DamagePlanePart(void* plane, int32_t frame, int32_t state);
// Hands a helicopter to its autopilot, flying to `target` between the two
// altitudes, as HELI_GOTO_COORDS does.
void FlyHeliTo(void* heli, const Vector& target, float lowest, float highest);
// Makes the `call rand` at `site` yield `value`, or puts the call back.
void HoldRandom(uintptr_t site, int32_t value, bool on);
// Makes the `call rand` at `site` yield the middle of the range, so a random
// amount added there every frame adds nothing, or puts the call back.
void MidRangeRandom(uintptr_t site, bool on);
// Loads bytes [start, end) of main.scm into the mission block and starts a
// script named `name` there that sleeps for good, so what runs on a mission
// of that name sees its code without it running.
bool LoadSleepingMission(const char* name, uint32_t start, uint32_t end);
// Shows the mission clock counting `variable` down from `milliseconds`.
void StartCountdown(uint32_t variable, int32_t milliseconds);
void StopCountdown(uint32_t variable);

}  // namespace game
