#pragma once

#include "modules/prelude.h"

namespace hff {

bool IsContinuousWeapon(int32_t weaponType);
bool IsWeaponFxEmitter(void* emitter, void** systemOut = nullptr);

using FxCreateParticlesFn = void(__thiscall*)(void*, float, float);

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

using AimWeaponFn = void(__thiscall*)(void*, const Vec3&, float, float, float);
using CameraProcessFn = void(__thiscall*)(void*);
extern CameraProcessFn g_originalCameraProcess;
extern AimWeaponFn g_originalAimWeapon;
extern bool g_aimMinHookInitialized;
extern int g_aimGuardDepth;
extern float g_savedAimTimeStep;
extern float g_savedAimTimeStepNonClipped;
extern bool g_haveAimTimeStep;
extern bool g_haveAimTimeStepNonClipped;

enum CamMode : int16_t {
    modeSniper = 7,
    modeRocketLauncher = 8,
    modeM16FirstPerson = 34,
    modeSniperRunabout = 39,
    modeRocketLauncherRunabout = 40,
    modeFirstPersonRunabout = 41,
    modeM16FirstPersonRunabout = 42,
    modeFightCamRunabout = 43,
    modeHeliCannonFirstPerson = 45,
    modeCamera = 46,
    modeRocketLauncherHs = 51,
    modeRocketLauncherRunaboutHs = 52,
    modeAimWeapon = 53,
    modeAimWeaponAttached = 65,
};

template <typename T>
bool SafeReadAimValue(uintptr_t address, T& value) {
    __try {
        value = *reinterpret_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool SafeWriteAimValue(uintptr_t address, const T& value) {
    __try {
        *reinterpret_cast<T*>(address) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool LooksLikeGameCode(uintptr_t address);
bool IsAimCameraMode(int16_t mode);
bool IsPlayerOnFootForAimCamera();
bool IsAimCameraActive(void* cameraPointer);
void BeginAimTimeStepGuard();
void EndAimTimeStepGuard();
float __cdecl UnguardedTimeStep();
void __fastcall HookedCameraProcess(void* camera, void*);
void __fastcall HookedProcessAimWeapon(void* cam, void*, const Vec3* target, float orientation, float speedVar, float speedVarWanted);
void RemoveAimCameraHooks();
EmissionCarrySlot* FindEmissionCarrySlot(void* blueprint);
AmmoConsumptionSlot& FindAmmoConsumptionSlot(void* weapon);
extern float g_drowningDamageCarry;
int32_t __cdecl AccumulateDrowningDamage(float damage);
extern std::array<float, kStatTruncSites.size()> g_statTruncCarries;

// Diagnostic for the cycle skill counter, which was reported in game as not
// levelling at all at 600 FPS with `skillProgress` on, and as levelling in
// three minutes with it off against two minutes at 30 FPS. Reading the code
// predicts the opposite: unpatched it should be several times faster at a high
// frame rate, not slower. That means the reading is wrong somewhere, and this
// logs the four numbers that separate the candidates rather than guessing
// again.
//
// `calls` is how many times a second the accumulate path is reached at all,
// which is the speed gate at `0x55C8E6`: if the bike is simply slower at a high
// frame rate the counter starves regardless of any truncation.
// `raw` against `added` separates the correction from the truncation.
constexpr uintptr_t kCycleSkillCounter = 0x00B794E0;
constexpr uintptr_t kCycleStaminaCounter = 0x00B794DC;
constexpr uintptr_t kCycleSkillLimit = 0x00B78FAC;
extern bool g_traceCycleSkill;
extern uint64_t g_cycleTraceLast;
extern uint32_t g_cycleTraceCalls;
extern double g_cycleTraceRaw;
extern int64_t g_cycleTraceAdded;
void TraceCycleSkill(int32_t site, double raw, int32_t added);
int32_t __cdecl TruncateStatWithCarry(double value, uintptr_t site);
int32_t __cdecl ShouldConsumeContinuousWeaponAmmo(uintptr_t weapon);

// A CVector passed by value to the game's cdecl functions.
struct ShotVector {
    float x;
    float y;
    float z;
};
bool __cdecl GatedAreaEffectAddShot(void* creator, int32_t weaponType, ShotVector origin, ShotVector target);
bool __fastcall ExtinguishShotWithWater(void* fireManager, void*, ShotVector point, float radius, float strength);
bool __cdecl GatedAreaEffectCreepingFire(ShotVector position, int32_t generations, int32_t allowSpread, int32_t scriptFire, float zDistance);
void __cdecl UpdateChainsawRewindOffset(void* anim, void* task);
void __cdecl RecordFightStrike(void* task);
void TraceChainsaw();
void __fastcall HookedFxCreateParticles(void* emitter, void*, float currentTime, float deltaTime);

using FxAddParticleFn = void(__fastcall*)(void* self, void* edx, const void* pos,
                                          const void* vel, float timeSince,
                                          const void* mults, float rotZ,
                                          float lightMult, float lightMultLimit,
                                          int32_t createLocal);
extern bool g_particleRateGate;
extern uint32_t g_particleBudget;

// One entry per call site, found by the return address of the call into
// `FxSystem_c::AddParticle`. Open addressing, and a full table fails open: a
// missed gate costs some particles, a wrong one costs the effect entirely.
constexpr uint32_t kParticleSiteSlots = 512;  // power of two
constexpr uint32_t kParticleSiteMask = kParticleSiteSlots - 1;

struct ParticleSiteState {
    uintptr_t site;
    uint32_t lastFrame;
    float carry;
    uint8_t open;
};
extern std::array<ParticleSiteState, kParticleSiteSlots> g_particleSites;
void ResetParticleSites();
ParticleSiteState* FindParticleSite(uintptr_t site);
bool ParticleSiteOpen(uintptr_t site);
extern double g_qpcToSeconds;
double NowSeconds();

struct ParticleBudgetState {
    double lastRefill;
    double credit;
};
extern ParticleBudgetState g_generalBudget;
bool ParticleBudgetAllows(ParticleBudgetState& state, uint32_t perSecond);
__declspec(noinline) void __fastcall HookedFxAddParticle( void* self, void* edx, const void* pos, const void* vel, float timeSince, const void* mults, float rotZ, float lightMult, float lightMultLimit, int32_t createLocal);

} // namespace hff
