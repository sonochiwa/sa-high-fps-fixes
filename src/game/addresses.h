#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Engine globals, struct offsets and functions of GTA SA 1.0 US that several
// areas use. The patch sites of each area, with the bytes expected there, are
// in game\sites.

namespace hff {

constexpr uintptr_t kImageBase = 0x00400000;

// Compact and Hoodlum 1.0 US have different entry stubs but share the address
// layout this plugin patches.
struct GameProfile {
    const char* name;
    uintptr_t signatureAddress;
    uint32_t signature;
};

constexpr std::array<GameProfile, 2> kGameProfiles{{
    {"GTA SA 1.0 US Compact", 0x00401000, 0x53EC8B55},
    {"GTA SA 1.0 US Hoodlum", 0x00401000, 0x16197BE9},
}};

// CTimer.
constexpr uintptr_t kTimerTimeStep = 0x00B7CB5C;
constexpr uintptr_t kTimerTimeStepNonClipped = 0x00B7CB58;
constexpr uintptr_t kTimerTimeInMilliseconds = 0x00B7CB84;
constexpr uintptr_t kFrameCounter = 0x00B7CB4C;
constexpr float kOriginalTimeStep = 50.0f / 30.0f;

// World, players and pads.
constexpr uintptr_t kPlayerPed = 0x00B6F5F0;
constexpr uintptr_t kTheCamera = 0x00B6F028;
constexpr uintptr_t kWorldPlayers = 0x00B7CD98;
constexpr uintptr_t kPads = 0x00B73458;
constexpr uintptr_t kCutsceneRunning = 0x00B5F851;
constexpr uintptr_t kCameraWideScreenOn = 0x00B6F065;
constexpr uintptr_t kGameCurrentArea = 0x00B72914;
constexpr uintptr_t kFrameLimit = 0x00C1704C;
// `FrontEndMenuManager.m_bPrefsFrameLimiter`.
constexpr uintptr_t kFrameLimiterPreference = 0x00BA6794;
constexpr size_t kPlayerInfoSize = 0x190;
constexpr size_t kPadSize = 0x134;

// Scripts. The operand of `mov [CTheScripts::pActiveScripts],ebx` holds the
// address of the list head.
constexpr uintptr_t kScriptQueueOperand = 0x00468D76;
constexpr size_t kRunningScriptNameOffset = 0x08;
constexpr size_t kRunningScriptNameSize = 8;
constexpr size_t kRunningScriptBaseIp = 0x10;

// CEntity, CPhysical and CMatrix.
constexpr size_t kEntityMatrix = 0x14;
constexpr size_t kEntityFlags = 0x1C;
constexpr size_t kEntityTypeAndStatus = 0x36;
constexpr uint8_t kEntityTypeVehicle = 2;
constexpr size_t kMatrixRight = 0x00;
constexpr size_t kMatrixForward = 0x10;
constexpr size_t kMatrixUp = 0x20;
constexpr size_t kMatrixPosition = 0x30;
constexpr size_t kPhysicalFlags = 0x40;
constexpr size_t kPhysicalMoveSpeed = 0x44;
constexpr size_t kPhysicalTurnSpeed = 0x50;
constexpr size_t kPhysicalMass = 0x8C;
constexpr size_t kPhysicalTurnMass = 0x90;
constexpr size_t kPhysicalCentreOfMass = 0xA4;
constexpr size_t kPhysicalAttachedTo = 0xFC;

// CPed.
constexpr size_t kPedAnimMovingShift = 0x4D8;
constexpr size_t kPedFlagsInVehicle = 0x46D;

// CVehicle and its subclasses.
constexpr size_t kVehicleDriverOffset = 0x460;
constexpr size_t kVehicleSubClass = 0x594;
constexpr size_t kAutomobileContactWheels = 0x960;
constexpr size_t kBikeContactWheels = 0x804;
constexpr int32_t kVehicleSubClassAutomobile = 0;
constexpr int32_t kVehicleSubClassMonsterTruck = 1;
constexpr int32_t kVehicleSubClassQuad = 2;
constexpr int32_t kVehicleSubClassHeli = 3;
constexpr int32_t kVehicleSubClassPlane = 4;
constexpr int32_t kVehicleSubClassBike = 9;
constexpr int32_t kVehicleSubClassBmx = 10;
constexpr int32_t kVehicleSubClassTrailer = 11;

// `CPools::ms_pVehiclePool` and the CPool fields that give a vehicle its
// script handle: the storage, one state byte per slot (bit 7 free, the low
// seven bits a count of the slot's allocations) and the capacity. Vehicles
// are `sizeof(CHeli)` apart.
constexpr uintptr_t kVehiclePool = 0x00B74494;
constexpr size_t kPoolStorage = 0x00;
constexpr size_t kPoolSlotStates = 0x04;
constexpr size_t kPoolCapacity = 0x08;
constexpr size_t kVehiclePoolElementSize = 0xA18;

// `_CIpow`: the base in st(1), the exponent in st(0), the result in st(0).
constexpr uintptr_t kPow = 0x00822130;
// The C runtime `rand`, and `CGeneral::GetRandomNumberInRange(int, int)`.
constexpr uintptr_t kRand = 0x00821B1E;
constexpr uintptr_t kRandomNumberInRange = 0x00407180;
// `FxManager_c::CreateFxSystem(const char*, const CVector&, RwMatrix*, bool)`,
// a thiscall on `g_fxMan` that returns null when no system was created.
constexpr uintptr_t kCreateFxSystem = 0x004A9BE0;

// Three per-frame steps the game repeats across unrelated functions, each
// against a shared constant, and replaced by the scaled steps in
// game\frame_steps.h.
// fsub dword ptr ds:[00858B1Ch]   (0.1)
constexpr std::array<uint8_t, 6> kExpectedFrameStepDecrement{
    0xD8, 0x25, 0x1C, 0x8B, 0x85, 0x00
};
// fadd dword ptr ds:[00858C28h]   (0.05)
constexpr std::array<uint8_t, 6> kExpectedFrameStepIncrement{
    0xD8, 0x05, 0x28, 0x8C, 0x85, 0x00
};
// fmul dword ptr ds:[00858EF0h]   (0.95)
constexpr std::array<uint8_t, 6> kExpectedFrameStepDecay{
    0xD8, 0x0D, 0xF0, 0x8E, 0x85, 0x00
};

// `max(1.0f, CTimer::GetTimeStep())`, used as a divisor by the follow cameras
// and the AI aircraft autopilot:
// fld 1.0 / fcomp ms_fTimeStep / fnstsw / test / jne / fld 1.0 / jmp / fld ts
constexpr std::array<uint8_t, 33> kExpectedCameraRateClamp{
    0xD9, 0x05, 0x24, 0x86, 0x85, 0x00,
    0xD8, 0x1D, 0x5C, 0xCB, 0xB7, 0x00,
    0xDF, 0xE0,
    0xF6, 0xC4, 0x41,
    0x75, 0x08,
    0xD9, 0x05, 0x24, 0x86, 0x85, 0x00,
    0xEB, 0x06,
    0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00
};

} // namespace hff
