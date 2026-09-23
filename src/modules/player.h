#pragma once

#include "modules/prelude.h"

namespace hff {

// CPed::PlayFootSteps stores the remaining bloody-footprint lifetime as an
// integer in m_nDeathTimeMS and subtracts one every time this path runs. At the
// original 30 Hz that produces the intended duration, but at a high frame rate
// all 200-300 ticks can disappear between two animation footsteps. Keep a
// fractional tick per ped so the field still changes at the original cadence.
// A gap in calls or an externally replaced counter resets the fraction, which
// also makes pool-slot reuse harmless.
struct BloodyFootprintTickState {
    uintptr_t ped{};
    uint32_t lastFrame{};
    uint32_t lastCounter{};
    float carry{};
};
extern std::array<BloodyFootprintTickState, 160> g_bloodyFootprintTickStates;
void ResetBloodyFootprintTickStates();
uint32_t __cdecl UpdateBloodyFootprintCounter(uintptr_t ped, uint32_t counter);
extern uintptr_t g_currentBloodyFootprintPed;
extern bool g_currentBloodyFootprintIsLeft;

struct BloodyFootprintHeightState {
    uintptr_t ped{};
    uint32_t leftTime{};
    float leftZ{};
};
extern std::array<BloodyFootprintHeightState, 160> g_bloodyFootprintHeightStates;
void __cdecl SelectBloodyFootprintSide(uintptr_t ped, uint32_t leftFoot);
void __cdecl StabilizeBloodyFootprintHeight(float* position);
float __cdecl GetAimingRifleWalkStep();
extern float g_moneyStepCarry;
void __cdecl ApplyMoneyStep(int32_t* field, int32_t proposed);
void __cdecl ClampClimbMoveSpeed(float* speed);
float __cdecl PickUpAlignStep(float offset);
extern float g_swimShiftSaved[2];
extern bool g_swimShiftScaled;
void __cdecl ScaleSwimAnimShift(uintptr_t ped);
void __cdecl RestoreSwimAnimShift(uintptr_t ped);
extern volatile uint32_t g_pushApplications;
extern volatile float g_pushDeltaVSum;
extern volatile float g_pushCarSpeedPeak;
extern volatile float g_pushPedSpeed;
extern float g_pushFirstPos[3];
extern float g_pushLastPos[3];
extern bool g_pushHavePos;

// Individual impulses, so the per-frame convergence fraction can be read off
// their decay instead of being guessed. While the ped walks at a steady speed
// the target is constant, so the remaining difference, and with it each
// impulse, shrinks by the same factor every frame. That factor is `1 - a`, and
// `a` is the only thing needed to convert this into a real-time rate.
struct PushSample {
    uint32_t frame;
    float deltaV;
    float carSpeed;
    float timeStep;
};
constexpr size_t kPushSampleSlots = 512;
constexpr uint32_t kPushSampleLimit = 900;
extern PushSample g_pushSamples[kPushSampleSlots];
extern volatile uint32_t g_pushWriteIndex;
extern uint32_t g_pushReadIndex;
extern uint32_t g_pushSamplesLogged;
extern uint32_t g_pedPushLastFrame;
extern float g_pedPushCarry;
extern bool g_pedPushOriginalRateFrame;
bool ShouldApplyOriginalRatePedPush();
void __cdecl ScalePedPushCarForce(uintptr_t stackFrame, uintptr_t vehicle);

} // namespace hff
