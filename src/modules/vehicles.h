#pragma once

#include "modules/prelude.h"

namespace hff {

float __cdecl GetFrameIndependentWheelFriction();
float __cdecl GetSkimmerResistance();
float __cdecl GetBurnoutWheelSpeed();
extern float g_turnAirResistanceStrength;
float __cdecl GetTurnAirResistanceFactor(const uint8_t* physical);
float __cdecl GetCarSteerInputGain();
float __cdecl GetTransmissionInertiaScale();
bool __fastcall HookedSpringDampening(void* physical, void*, float dampingLevel, float springForceLimit, float* direction, float* collisionPoint, float* collisionSpeed);
void __fastcall HookedPhysicalProcessControl(void* physical, void*);
float __cdecl GetTransmissionSmootherFrac();
float __cdecl GetBikeSteerInputGain();
float __cdecl GetWheelSettleWeight();
float ReadHeliRotorFinalSpeed();
float __cdecl GetHeliRotorSlowStep();
float __cdecl GetHeliRotorFastStep();
bool NearlyEqual(float a, float b);
void WriteGameFloat(uintptr_t address, float value);
bool WriteProtectedGameFloat(uintptr_t address, float value);
extern uintptr_t g_bikePitchExperimentBike;
extern float g_bikePitchExperimentBefore[3];
extern float g_bikePitchExperimentAxis[3];
extern float g_bikePitchExperimentStrength;
extern float g_bikePitchExperimentFrameCorrection;
extern bool g_bikePitchExperimentActive;
extern uintptr_t g_bmxLaunchCorrectionBike;

// Matched full-charge trajectories reach about 47.9 degrees of backward
// rotation at 500+ FPS versus 42.1 degrees at 30 FPS. Comparing synchronized
// samples after the first 250 ms shows excess angular speed; the earlier
// first-sample comparison was taken at different positions inside a 30 Hz
// frame and understated it. A 24% asymptotic correction produced the closest
// visual match in game. It fades to an exact no-op at the original timestep.
constexpr float kBmxLaunchPitchExcess = 0.24f;
constexpr float kBmxStockLandingPitchLimit = 0.105f;
constexpr float kBmxFalseLandingDamageLimit = 31.0f;
constexpr uint32_t kBmxLandingProtectionMaxMs = 3000;
constexpr uint32_t kBmxLandingProtectionGraceMs = 200;
extern uintptr_t g_bmxLandingProtectionBike;
extern uint32_t g_bmxLandingProtectionUntil;
extern bool g_bmxLandingWasAirborne;
extern bool g_bmxLandingContactSeen;
void __cdecl HookedBmxLaunchBunnyHop(void* association, void* data);
void UpdateBmxLandingProtection(void* vehicle);
void CorrectBmxLaunchPitch(void* vehicle);
bool __cdecl HookedBikeDamageKnockOffRider( void* vehicle, float damageIntensity, uint32_t pieceType, void* damager, const float* collisionPosition, const float* collisionImpactVelocity);
void __fastcall HookedBmxRiderFallEventAdd( void* eventGroup, void*, void* event, bool addToEventGroup);
void __cdecl BeginBikePitchExperiment(uintptr_t bike);
void __cdecl FinishBikePitchExperiment();
void BikePitchExperimentThunk();

} // namespace hff
