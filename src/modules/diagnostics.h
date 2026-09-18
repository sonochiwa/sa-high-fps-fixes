#pragma once

#include "modules/prelude.h"

namespace hff {

// Samples the vehicle the player is riding and writes one line per sample to
// `HighFpsFixes.trace.log`. This exists to diagnose frame-rate-dependent
// behavior that cannot be identified from the disassembly alone; it patches
// nothing and is disabled by default.

struct VehicleSample {
    uintptr_t vehicle;
    uint32_t time;
    float timeStep;
    float timeScale;
    float posZ;
    float move[3];
    float turn[3];
    float force[3];
    float torque[3];
    float friction[3];
    float mass;
    float airResistance;
    float barSteer;
    float lean;
    float desiredLean;
    float sprintLean;
    float brakePedal;
    float gasPedal;
    float wheelAngularVelocity[2];
    float localPitchTurnSpeed;
    uint32_t wheelTurnCalls;
    float wheelTurnPitchImpulse;
    uint32_t entityFlags;
    uint32_t physicalFlags;
    uint32_t lastCollisionTime;
    uintptr_t attachedTo;
    uint32_t processCalls;
    uint32_t gravityCalls;
    uint32_t leanWrites[3];
    float rightZ;
    float mvZAfterGravity;
    float upZ;
    uint8_t fakePhysics;
    uint8_t status;
    uint8_t subClass;
    uint8_t contactWheels;
};
extern volatile uint32_t g_bikeProcessCalls;
extern volatile uint32_t g_gravityCalls;
extern volatile float g_moveSpeedAfterGravity;
extern uint32_t g_leanWrites[3];
using ThisCallVoidFn = void(__thiscall*)(void*);
extern DetourPatch g_bikeProcessPatch;
extern DetourPatch g_applyGravityPatch;
extern DetourPatch g_abandonedBikeCollisionPatch;
extern DetourPatch g_abandonedBikeShiftPatch;
extern DetourPatch g_abandonedBikeRwFramePatch;
extern DetourPatch g_abandonedBikePreRenderPatch;
extern DetourPatch g_abandonedBikeRenderPatch;
extern std::array<SitePatch, 3> g_leanWritePatches;
extern bool g_abandonedBikePhysicsStepEnabled;
extern uint32_t g_abandonedBikePhysicsLastFrame;
extern float g_abandonedBikePhysicsCredit;
extern bool g_abandonedBikePhysicsTick;

struct AbandonedBikeRenderState {
    void* bike{};
    std::array<float, 12> previous{};
    std::array<float, 12> current{};
    bool valid{};
    bool previousCaptured{};
};
extern std::array<AbandonedBikeRenderState, 16> g_abandonedBikeRenderStates;
AbandonedBikeRenderState* FindAbandonedBikeRenderState(void* bike, bool create);
bool CopyMatrixTransform(uintptr_t matrix, std::array<float, 12>& out);
bool WriteMatrixTransform(uintptr_t matrix, const std::array<float, 12>& transform);
bool CopyBikeTransform(void* bike, std::array<float, 12>& out);
bool WriteBikeTransform(void* bike, const std::array<float, 12>& transform);
void NormalizeRenderVector(std::array<float, 12>& transform, size_t base);
std::array<float, 12> InterpolateBikeTransform( const AbandonedBikeRenderState& state, float alpha);
void BeginAbandonedBikePhysicsStep(void* bike);
void FinishAbandonedBikePhysicsStep(void* bike);
bool IsAbandonedBike(const void* entity);
bool ShouldRunAbandonedBikePhysicsStep();

class ScopedOriginalTimeStep {
public:
    ScopedOriginalTimeStep() {
        __try {
            m_saved = *reinterpret_cast<float*>(kTimerTimeStep);
            m_changed = std::isfinite(m_saved) && m_saved > 0.0f
                     && m_saved < kOriginalTimeStep;
            if (m_changed) {
                *reinterpret_cast<float*>(kTimerTimeStep) = kOriginalTimeStep;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            m_changed = false;
        }
    }

    ~ScopedOriginalTimeStep() {
        if (!m_changed) {
            return;
        }
        __try {
            *reinterpret_cast<float*>(kTimerTimeStep) = m_saved;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    ScopedOriginalTimeStep(const ScopedOriginalTimeStep&) = delete;
    ScopedOriginalTimeStep& operator=(const ScopedOriginalTimeStep&) = delete;

private:
    float m_saved{kOriginalTimeStep};
    bool m_changed{};
};
void CallAbandonedBikePhysicsStep(DetourPatch& patch, void* entity);

// One sample is published per final bike balance force. It is intentionally
// only diagnostic state: the hooks below reproduce the displaced instructions
// and pass the exact original vector to CPhysical::ApplyTurnForce.
struct BikeBalanceSample {
    uint32_t time{};
    float timeStep{};
    float local34{};
    float coefficientA{};
    float coefficientB{};
    float input68{};
    float input6C{};
    float input78{};
    float force[3]{};
    float turnY{};
    uint8_t contactWheels{};
};

struct BikeBalanceWindow {
    uint32_t time{};
    uint32_t calls{};
    float elapsed{};
    float peakInput68{};
    float sumInput68{};
    float sumInput6C{};
    float force[3]{};
};
extern BikeBalanceSample g_bikeBalanceSample;
extern volatile LONG g_bikeBalanceSequence;
extern BikeBalanceWindow g_bikeBalanceWindow;
extern BikeBalanceWindow g_bikeBalanceWindowSnapshot;
extern volatile LONG g_bikeBalanceWindowSequence;
extern SitePatch g_bikeBalanceInputPatch;
extern SitePatch g_bikeBalanceForcePatch;
extern SitePatch g_bikeWheelTurnTracePatch;
extern volatile uint32_t g_bikeWheelTurnCalls;
extern volatile float g_bikeWheelTurnPitchImpulse;
void __cdecl RecordBikeWheelTurnForce(uintptr_t bike, const float* arguments);
void BikeWheelTurnTraceThunk();
bool IsThePlayerVehicle(const void* entity);
void __fastcall HookedBikeProcessControl(void* bike, void*);
bool EnsureBikeProcessControlHook();
void __fastcall HookedPhysicalProcessCollision(void* physical, void*);
void __fastcall HookedPhysicalProcessShift(void* physical, void*);
void __fastcall HookedEntityUpdateRwFrame(void* entity, void*);
void CallAbandonedBikeRender(DetourPatch& patch, void* bike);
void __fastcall HookedBikePreRender(void* bike, void*);
void __fastcall HookedBikeRender(void* bike, void*);
void __fastcall HookedApplyGravity(void* physical, void*);
void __cdecl RecordBikeBalanceInputs(uintptr_t bike, const float* stack);
void __cdecl RecordBikeBalanceForce(uintptr_t bike, const float* force);
void BikeBalanceInputThunk();
void BikeBalanceForceThunk();
void LeanWriteSmootherThunk();
void LeanWriteBikeThunk();
void LeanWriteBmxThunk();

// Three separate hypotheses for the mid-air bike freeze were wrong, so instead
// of guessing which function wipes the move speed the CPU is asked directly. A
// hardware data breakpoint is armed on `m_vecMoveSpeed.z` of the bike the
// player is riding the moment the freeze is observed, and every instruction
// that writes it is recorded. Debug registers are per-thread, so the breakpoint
// goes on the game thread, which is identified by whichever thread calls the
// gravity hook.
constexpr size_t kWatchSlots = 48;
// The executable's code, used to tell a return address from stack garbage.
constexpr uintptr_t kTextLow = 0x00401000;
constexpr uintptr_t kTextHigh = 0x00857000;
constexpr uint32_t kWatchHitLimit = 4000;
extern uint32_t g_watchHitLimit;
extern uint32_t g_watchSampleLimit;
extern uint32_t g_watchArmDelay;
extern size_t g_watchOffset;
extern volatile uintptr_t g_watchAddress;
extern volatile uint32_t g_watchTotalHits;
extern uintptr_t g_watchEips[kWatchSlots];
extern uintptr_t g_watchCallers[kWatchSlots];
extern float g_watchDeltaSum[kWatchSlots];
extern float g_watchSignedSum[kWatchSlots];
extern float g_watchLastValue;
extern bool g_watchHaveLastValue;
extern uint32_t g_watchCounts[kWatchSlots];
extern volatile long g_watchLock;
extern void* g_watchHandler;
extern bool g_watchArmed;
extern bool g_watchReported;
extern uint32_t g_watchReports;
constexpr uint32_t kWatchReportLimit = 60;
extern uint32_t g_watchReportLimit;
uintptr_t FindCallerOnStack(uintptr_t esp);
LONG CALLBACK MoveSpeedWatchHandler(EXCEPTION_POINTERS* info);

// `Dr7` enables the first slot locally, as a four byte write watch:
// L0, RW0 = 01 (write) and LEN0 = 11 (four bytes).
constexpr DWORD kDr7WriteFourBytes = 0x1u | (0x1u << 16) | (0x3u << 18);
bool SetMoveSpeedWatch(uintptr_t address);
void ReportMoveSpeedWatch(FILE* file);
float VectorLength(const float v[3]);
bool SampleThePlayerVehicle(VehicleSample& out);
void MaybeArmWatch(FILE* file);
void DrainPushSamples(FILE* file);
void ReportPedPushTelemetry(FILE* file, uint32_t& lastReport);

// The on-foot counterpart of the vehicle trace. Written for the climbing death,
// where the question is what the player's move speed is on the frame the health
// drops, so it records speed and health together and nothing else.
struct PedSample {
    uint32_t time;
    float timeStep;
    float position[3];
    float move[3];
    float health;
    float armour;
};
bool SampleThePlayerPed(PedSample& out);
DWORD WINAPI PedTraceThread(void*);
DWORD WINAPI VehicleTraceThread(void*);

} // namespace hff
