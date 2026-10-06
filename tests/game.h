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
constexpr size_t kPadRightStickY = 0x06;
constexpr size_t kPadRightShoulder1 = 0x0C;
constexpr size_t kPadButtonSquare = 0x1C;
constexpr size_t kPadButtonCross = 0x20;
constexpr size_t kPadButtonCircle = 0x22;
constexpr size_t kPadDisablePlayerControls = 0x10E;
// bDisablePlayerEnterCar, Duck, FireWeapon, FireWeaponWithL1, CycleWeapon and
// Jump, one byte each, which the opening of a new game sets.
constexpr size_t kPadDisableFlags = 0x119;
constexpr size_t kPadDisableFlagCount = 6;
constexpr int16_t kButtonDown = 255;

// CTheScripts::pActiveScripts, the list of running scripts, and the fields
// of CRunningScript read and written: the next script and the time the
// script sleeps until.
constexpr uintptr_t kActiveScripts = 0xA8B42C;
constexpr size_t kScriptNext = 0x00;
constexpr size_t kScriptWakeTime = 0xCC;

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

// Functions.
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

}  // namespace game
