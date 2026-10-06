#include "game.h"

#include "MinHook.h"

#include <windows.h>

#include <array>
#include <cstring>

namespace game {
namespace {

struct Prologue {
    uintptr_t address;
    std::array<uint8_t, 6> bytes;
    size_t size;
};

// The first bytes of each function the test calls or hooks, as the 1.0 US
// executable has them; the Hoodlum build moves some bodies behind a jump.
constexpr Prologue kPrologues[] = {
    {kTimerUpdate, {0x8B, 0x0D, 0x28, 0xCB, 0xB7, 0x00}, 6},
    {kTimerUpdateVariables, {0x51, 0xA1, 0x2C, 0xCB, 0xB7, 0x00}, 6},
    {kUpdatePads, {0x56, 0x57, 0xE9}, 3},
    {kGameProcess, {0x83, 0xEC, 0x0C, 0x53, 0x56}, 5},
    {kFindPlayerPed, {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0}, 6},
    {kVehicleCheat, {0x90, 0xE9}, 2},
    {kSetPedInCarDirect, {0x90, 0xE9}, 2},
    {kLoadScene, {0x83, 0xEC, 0x1C, 0x55}, 4},
    {kFindGroundZ, {0x83, 0xEC, 0x38}, 3},
    {kCameraFade, {0xE9}, 1},
    {kCameraBehindPlayer, {0x56, 0x8B, 0xF1, 0x6A, 0xFF}, 5},
    {kGiveWeapon, {0x51, 0x53, 0x8B, 0x5C, 0x24, 0x0C}, 6},
    {kSetCurrentWeapon, {0x8B, 0x44, 0x24, 0x04, 0x56}, 5},
    {kStartFire, {0x51, 0x55, 0x8B, 0x6C, 0x24, 0x10}, 6},
    {kExtinguishPoint, {0x56, 0x57, 0x8B, 0xF1}, 4},
    {kRequestModel, {0x53, 0x8B, 0x5C, 0x24, 0x0C}, 5},
    {kLoadAllRequestedModels, {0xE9}, 1},
    {kRandomNumberInRange, {0x51, 0x56, 0xE8}, 3},
    {kAddProjectile, {0x6A, 0xFF, 0xE9}, 3},
    {kAddExplosion, {0x83, 0xEC, 0x1C, 0x53, 0x55}, 5},
    {kSetAeroplaneCompStatus, {0x8A, 0x44, 0x24, 0x04, 0x2C, 0x0C}, 6},
    {kAddClock, {0xE9}, 1},
    {kClearClock, {0xE9}, 1},
};

using FindPlayerPedFn = uint8_t*(__cdecl*)(int32_t);
using TeleportFn = void(__thiscall*)(void*, Vector, bool);
using LoadSceneFn = void(__cdecl*)(const Vector*);
using GroundFn = float(__cdecl*)(float, float);
using FadeFn = void(__thiscall*)(void*, float, uint16_t);
using CameraFn = void(__thiscall*)(void*);
using VehicleCheatFn = uint8_t*(__cdecl*)(int32_t);
using PedInCarFn = bool(__cdecl*)(void*, void*, int32_t, bool);
using GiveWeaponFn = int32_t(__thiscall*)(void*, int32_t, uint32_t, bool);
using SetWeaponFn = void(__thiscall*)(void*, int32_t);
using StartFireFn = void*(__thiscall*)(void*, Vector, float, uint8_t, void*, uint32_t, int8_t,
                                       uint8_t);
using ExtinguishFn = void(__thiscall*)(void*, Vector, float);
using RequestModelFn = void(__cdecl*)(int32_t, int32_t);
using LoadAllFn = void(__cdecl*)(bool);
using AddExplosionFn = void(__cdecl*)(void*, void*, int32_t, Vector, uint32_t, int32_t, float,
                                      int32_t);
using SetPartStatusFn = void(__thiscall*)(void*, int32_t, int32_t);
using AddProjectileFn = bool(__cdecl*)(void*, int32_t, Vector, float, const Vector*, void*);
using RandomRangeFn = int32_t(__cdecl*)(int32_t, int32_t);

RandomRangeFn g_randomRange = nullptr;
bool g_evenPercent = false;
uint32_t g_percentStep = 0;

// 37 and 100 share no factor, so the steps visit every value once a hundred.
int32_t __cdecl RandomRangeHook(int32_t low, int32_t high) {
    if (g_evenPercent && low == 0 && high == 100) {
        g_percentStep = (g_percentStep + 37) % 100;
        return static_cast<int32_t>(g_percentStep);
    }
    return g_randomRange(low, high);
}
using AddClockFn = void(__thiscall*)(void*, uint32_t, const char*, int32_t);
using ClearClockFn = void(__thiscall*)(void*, uint32_t);

template <typename T>
T Function(uintptr_t address) {
    return reinterpret_cast<T>(address);
}

void LoadScene(const Vector& position) {
    Function<LoadSceneFn>(kLoadScene)(&position);
}

// Streaming flag GAME_REQUIRED: kept loaded until released.
void LoadModel(int32_t model) {
    constexpr int32_t kGameRequired = 2;
    Function<RequestModelFn>(kRequestModel)(model, kGameRequired);
    Function<LoadAllFn>(kLoadAllRequestedModels)(false);
}

}  // namespace

bool VerifyFunctions() {
    for (const Prologue& prologue : kPrologues) {
        if (std::memcmp(reinterpret_cast<const void*>(prologue.address), prologue.bytes.data(),
                        prologue.size) != 0) {
            return false;
        }
    }
    return true;
}

int32_t GameState() {
    return At<int32_t>(kGameState);
}

uint8_t* Player() {
    return Function<FindPlayerPedFn>(kFindPlayerPed)(-1);
}

Vector MatrixRow(const void* entity, size_t row) {
    const uint8_t* matrix = Field<uint8_t*>(entity, kEntityMatrix);
    return matrix ? *reinterpret_cast<const Vector*>(matrix + row) : Vector{};
}

Vector Position(const void* entity) {
    return MatrixRow(entity, kMatrixPosition);
}

void Teleport(void* entity, const Vector& position) {
    const auto* vtable = Field<const uintptr_t*>(entity, 0);
    reinterpret_cast<TeleportFn>(vtable[kEntityTeleportSlot])(entity, position, false);
}

float GroundZ(float x, float y) {
    LoadScene({x, y, 0.0f});
    return Function<GroundFn>(kFindGroundZ)(x, y);
}

void ShowPlayer() {
    void* camera = reinterpret_cast<void*>(kTheCamera);
    Function<CameraFn>(kCameraBehindPlayer)(camera);
    Function<FadeFn>(kCameraFade)(camera, 0.0f, 0);
}

uint8_t* SpawnVehicle(int32_t model) {
    uint8_t* vehicle = Function<VehicleCheatFn>(kVehicleCheat)(model);
    if (vehicle) {
        Field<uint32_t>(vehicle, kVehicleCreatedBy) = kMissionVehicle;
    }
    return vehicle;
}

// Warping the player in leaves the vehicle under physics control (status 3);
// the game reads the pad only for a vehicle with the player status, 0, in
// the high five bits of the status byte.
void PutPlayerIn(void* vehicle) {
    Function<PedInCarFn>(kSetPedInCarDirect)(Player(), vehicle, 0, true);
    Field<uint8_t>(vehicle, kEntityStatus) &= 0x07;
}

void GiveWeapon(int32_t weapon, int32_t model, uint32_t ammo) {
    LoadModel(model);
    uint8_t* player = Player();
    Function<GiveWeaponFn>(kGiveWeapon)(player, weapon, ammo, false);
    Function<SetWeaponFn>(kSetCurrentWeapon)(player, weapon);
    if (uint8_t* data = Field<uint8_t*>(player, kPedPlayerData)) {
        Field<uint8_t>(data, kPlayerDataChosenWeapon) =
            Field<uint8_t>(player, kPedActiveWeaponSlot);
    }
}

void StartFire(const Vector& position, float size) {
    Function<StartFireFn>(kStartFire)(reinterpret_cast<void*>(kFireManager), position, size, 1,
                                      nullptr, 60000, 0, 0);
}

void ExtinguishFiresAround(const Vector& position, float radius) {
    Function<ExtinguishFn>(kExtinguishPoint)(reinterpret_cast<void*>(kFireManager), position,
                                             radius);
}

uint8_t* ActiveCam() {
    const uint8_t index = At<uint8_t>(kTheCamera + kCameraActiveCam);
    return reinterpret_cast<uint8_t*>(kTheCamera + kCameraCams + index * kCamSize);
}

void SleepAllScripts() {
    for (auto* script = At<uint8_t*>(kActiveScripts); script;
         script = Field<uint8_t*>(script, kScriptNext)) {
        Field<int32_t>(script, kScriptWakeTime) = INT32_MAX;
    }
}

void AddExplosion(int32_t type, const Vector& position) {
    Function<AddExplosionFn>(kAddExplosion)(nullptr, nullptr, type, position, 0, 0, 0.0f, 1);
}

void DropProjectile(int32_t weapon, int32_t model, const Vector& position) {
    LoadModel(model);
    Function<AddProjectileFn>(kAddProjectile)(Player(), weapon, position, 0.0f, nullptr,
                                              nullptr);
}

void EvenRandomPercent(bool on) {
    if (!g_randomRange) {
        void* original = nullptr;
        auto* target = reinterpret_cast<void*>(kRandomNumberInRange);
        if (MH_CreateHook(target, reinterpret_cast<void*>(&RandomRangeHook), &original) != MH_OK
            || MH_EnableHook(target) != MH_OK) {
            return;
        }
        g_randomRange = reinterpret_cast<RandomRangeFn>(original);
    }
    g_evenPercent = on;
    g_percentStep = 0;
}

void DamagePlanePart(void* plane, int32_t frame, int32_t state) {
    void* damage = reinterpret_cast<uint8_t*>(plane) + kAutomobileDamageManager;
    Function<SetPartStatusFn>(kSetAeroplaneCompStatus)(damage, frame, state);
}

void MidRangeRandom(uintptr_t site, bool on) {
    std::array<uint8_t, 5> bytes{0xB8, 0xFF, 0x3F, 0x00, 0x00};
    if (!on) {
        const auto offset = static_cast<int32_t>(kRand - (site + 5));
        bytes[0] = 0xE8;
        std::memcpy(&bytes[1], &offset, sizeof(offset));
    }
    auto* code = reinterpret_cast<void*>(site);
    DWORD protection = 0;
    if (VirtualProtect(code, bytes.size(), PAGE_EXECUTE_READWRITE, &protection)) {
        std::memcpy(code, bytes.data(), bytes.size());
        VirtualProtect(code, bytes.size(), protection, &protection);
        FlushInstructionCache(GetCurrentProcess(), code, bytes.size());
    }
}

void StartCountdown(uint32_t variable, int32_t milliseconds) {
    constexpr int32_t kCountDown = 1;
    At<int32_t>(kScriptSpace + variable) = milliseconds;
    Function<AddClockFn>(kAddClock)(reinterpret_cast<void*>(kOnscreenTimer), variable,
                                    nullptr, kCountDown);
}

void StopCountdown(uint32_t variable) {
    Function<ClearClockFn>(kClearClock)(reinterpret_cast<void*>(kOnscreenTimer), variable);
}

}  // namespace game
