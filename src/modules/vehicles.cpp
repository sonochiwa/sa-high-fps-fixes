#include "modules/modules.h"

namespace hff {

float __cdecl GetFrameIndependentWheelFriction() {
    return ReadGameFloat(kWheelFriction, 0.9f) * TimeStepRatio();
}

float __cdecl GetSkimmerResistance() {
    return ReadGameFloat(kSkimmerResistanceConstant, 30.0f) * TimeStepRatio();
}

float __cdecl GetBurnoutWheelSpeed() {
    return ReadGameFloat(kBurnoutConstant, 3000.0f) * TimeStepRatio();
}

float g_turnAirResistanceStrength{1.0f};

// Reads the wheel contact count of the vehicle the air resistance is being
// applied to, or -1 for anything the fix leaves alone: entities that are not
// vehicles, and vehicle classes other than cars, bikes and their variants.
// Aircraft and boats keep the stock damping so their handling stays what a
// high frame rate player knows; `CVehicle::FlyingControl` is already frame
// independent on its own.
int ReadTurnAirResistanceContactWheels(const uint8_t* physical) {
    __try {
        if ((physical[kEntityTypeAndStatus] & 7) != kEntityTypeVehicle) {
            return -1;
        }
        switch (*reinterpret_cast<const int32_t*>(physical + kVehicleSubClass)) {
        case kVehicleSubClassAutomobile:
        case kVehicleSubClassMonsterTruck:
        case kVehicleSubClassQuad:
        case kVehicleSubClassTrailer:
            return physical[kAutomobileContactWheels];
        case kVehicleSubClassBike:
        case kVehicleSubClassBmx:
            return physical[kBikeContactWheels];
        default:
            return -1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

// The per-frame factor for `m_vecTurnSpeed`. A decay applied once per frame
// keeps the same fraction per second only when raised to the timestep ratio;
// the strength interpolates the exponent between the stock `1` and that
// ratio, so `0` is the untouched game and `100` is the 30 FPS car. At 30 FPS
// the ratio is one and every strength returns the stock constant. A vehicle
// with no wheel on the ground gets the stock constant too, so a car in the
// air rotates as it did before this fix.
float __cdecl GetTurnAirResistanceFactor(const uint8_t* physical) {
    const float stock = ReadGameFloat(kTurnAirResistanceConstant, 0.99f);
    if (ReadTurnAirResistanceContactWheels(physical) <= 0
        || !(stock > 0.0f) || stock >= 1.0f) {
        return stock;
    }
    const float exponent =
        1.0f + (TimeStepRatio() - 1.0f) * g_turnAirResistanceStrength;
    return std::pow(stock, exponent);
}

// The fraction of the remaining steering error closed this frame. The game
// closes `k * timeStep` of it in one linear step; the exponential form keeps
// the same fraction per second at every frame rate and equals the stock step
// exactly at 30 FPS. Falls back to the stock step when the constant has been
// retuned outside the range where the exponential is defined.
float SteerInputGain(float k) {
    const float ratio = TimeStepRatio();
    const float base = 1.0f - k * kOriginalTimeStep;
    if (!(base > 0.0f) || base >= 1.0f) {
        return k * kOriginalTimeStep * ratio;
    }
    return 1.0f - std::pow(base, ratio);
}

// The move and turn speed of the entity whose frame is in progress, taken
// at the entry of CPhysical::ProcessControl, before gravity, air resistance
// and the springs have acted. The damper wrapper reads it to know how much
// of the contact speed it sees was made this frame.
namespace {

struct FrameStartSpeeds {
    const void* physical;
    float move[3];
    float turn[3];
};
FrameStartSpeeds g_frameStart{};

float Dot3(const float* a, const float* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Cross3(const float* a, const float* b, float* out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

// The damper's same-frame bite on the frame's own speed change at the
// contact point, capped like the stock damping by the spring force.
// `fraction` is what the stock arithmetic will remove of the whole contact
// speed after this.
void ApplyLoadLeanBite(void* physical, float stockFraction, float fraction,
                       float springForceLimit, const float* direction,
                       const float* collisionPoint, const float* collisionSpeed) {
    const float extra = stockFraction - fraction;
    if (!(extra > 0.0f) || physical != g_frameStart.physical) {
        return;
    }
    const auto base = reinterpret_cast<uintptr_t>(physical);
    const float* move = reinterpret_cast<const float*>(base + kPhysicalMoveSpeed);
    const float* turn = reinterpret_cast<const float*>(base + kPhysicalTurnSpeed);
    const float mass = *reinterpret_cast<const float*>(base + kPhysicalMass);
    const float turnMass = *reinterpret_cast<const float*>(base + kPhysicalTurnMass);
    const float* centre = reinterpret_cast<const float*>(base + kPhysicalCentreOfMass);
    const float* matrix = *reinterpret_cast<const float* const*>(base + kEntityMatrix);
    if (!matrix || !(mass > 0.0f) || !(turnMass > 0.0f)) {
        return;
    }
    // the contact point relative to the centre of mass, as GetSpeed sees it
    float centreWorld[3];
    for (int i = 0; i < 3; ++i) {
        centreWorld[i] = matrix[i] * centre[0] + matrix[4 + i] * centre[1]
                       + matrix[8 + i] * centre[2];
    }
    float distance[3];
    for (int i = 0; i < 3; ++i) {
        distance[i] = collisionPoint[i] - centreWorld[i];
    }
    // the speed change this frame has made at the contact point
    float turnDelta[3];
    for (int i = 0; i < 3; ++i) {
        turnDelta[i] = turn[i] - g_frameStart.turn[i];
    }
    float delta[3];
    Cross3(turnDelta, distance, delta);
    for (int i = 0; i < 3; ++i) {
        delta[i] += move[i] - g_frameStart.move[i];
    }
    // The game's no-reversal clamp is not applied: it compares the removal
    // with the whole contact speed, and this removal is a fraction of one
    // frame's change, which at a high frame rate is far below the residual
    // motion of the body. The stock damper's own clamp never binds either,
    // since it removes a fraction of the same speed it compares against.
    (void)collisionSpeed;
    const float removal = -extra * Dot3(delta, direction);
    if (removal == 0.0f || !std::isfinite(removal)) {
        return;
    }
    float lever[3];
    Cross3(distance, direction, lever);
    const float pointMass = 1.0f / (Dot3(lever, lever) / turnMass + 1.0f / mass);
    float force = pointMass * removal;
    const float cap = std::fabs(springForceLimit)
                    * *reinterpret_cast<const float*>(kDampingLimitOfSpringForce);
    if (force > cap) {
        force = cap;
    } else if (force < -cap) {
        force = -cap;
    }
    // both vectors go by value, six floats on the stack
    using ApplyForceFn = void(__thiscall*)(void*, float, float, float, float, float, float, bool);
    reinterpret_cast<ApplyForceFn>(kPhysicalApplyForce)(
        physical, force * direction[0], force * direction[1], force * direction[2],
        collisionPoint[0], collisionPoint[1], collisionPoint[2], true);
}

} // namespace

void __fastcall HookedPhysicalProcessControl(void* physical, void*) {
    __try {
        const auto base = reinterpret_cast<uintptr_t>(physical);
        g_frameStart.physical = physical;
        std::memcpy(g_frameStart.move, reinterpret_cast<const void*>(base + kPhysicalMoveSpeed),
                    sizeof(g_frameStart.move));
        std::memcpy(g_frameStart.turn, reinterpret_cast<const void*>(base + kPhysicalTurnSpeed),
                    sizeof(g_frameStart.turn));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_frameStart.physical = nullptr;
    }
    reinterpret_cast<void(__thiscall*)(void*)>(g_physicalProcessControlPatch.gateway)(physical);
}

// Two fixes share this wrapper. With `suspensionDampingLimit` the damping
// level is rewritten so the per-frame removal, after the game's own clamp,
// equals the 30 FPS removal spread over this frame. With
// `suspensionLoadLean` the damper first takes its 30 FPS bite of the speed
// change made earlier in the frame, which the stock arithmetic no longer
// does above 30 FPS. `physical` is read for the bMakeMassTwiceAsBig flag,
// which the game folds into the product before clamping.
bool __fastcall HookedSpringDampening(void* physical, void*, float dampingLevel,
                                      float springForceLimit, float* direction,
                                      float* collisionPoint, float* collisionSpeed) {
    using Fn = bool(__thiscall*)(void*, float, float, float*, float*, float*);
    float level = dampingLevel;
    __try {
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        const float limit = *reinterpret_cast<const float*>(kDampingLimitInFrame);
        if (physical && std::isfinite(timeStep) && timeStep > 0.0f
            && timeStep < kOriginalTimeStep && std::isfinite(dampingLevel)
            && dampingLevel > 0.0f && limit > 0.0f) {
            const float mass =
                (*reinterpret_cast<const uint8_t*>(
                     reinterpret_cast<uintptr_t>(physical) + kPhysicalFlags) & 0x01) != 0
                    ? 2.0f : 1.0f;
            const float stockFraction =
                std::min(kOriginalTimeStep * dampingLevel * mass, limit);
            if (stockFraction > 0.0f && stockFraction < 1.0f) {
                if (g_suspensionDampingLimit) {
                    const float fraction =
                        1.0f - std::pow(1.0f - stockFraction, timeStep / kOriginalTimeStep);
                    level = fraction / (timeStep * mass);
                }
                if (g_suspensionLoadLean) {
                    const float fraction = std::min(timeStep * level * mass, limit);
                    ApplyLoadLeanBite(physical, stockFraction, fraction, springForceLimit,
                                      direction, collisionPoint, collisionSpeed);
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        level = dampingLevel;
    }
    return reinterpret_cast<Fn>(g_suspensionDampingPatch.gateway)(
        physical, level, springForceLimit, direction, collisionPoint, collisionSpeed);
}

// The engine inertia term is a difference between two frames, so it is
// scaled to the difference one original frame would have seen.
float __cdecl GetTransmissionInertiaScale() {
    const float ratio = TimeStepRatio();
    return ratio > 0.0f ? 1.0f / ratio : 1.0f;
}

// The smoother keeps 0.85 of its value per frame; per rendered frame that
// is 0.85 raised to the timestep ratio.
float __cdecl GetTransmissionSmootherFrac() {
    const float frac = ReadGameFloat(kTransmissionSmootherConstant, 0.85f);
    if (!(frac > 0.0f) || frac >= 1.0f) {
        return frac;
    }
    return std::pow(frac, TimeStepRatio());
}

float __cdecl GetCarSteerInputGain() {
    return SteerInputGain(ReadGameFloat(kCarSteerInputConstant, 0.2f));
}

float __cdecl GetBikeSteerInputGain() {
    return SteerInputGain(ReadGameFloat(kBikeSteerInputConstant, 0.2f));
}

// A lerp weight, so what has to hold across frames is the fraction of the gap
// left over: `1 - weight` per original frame becomes `(1 - weight)` raised to
// the timestep ratio per rendered frame. The ratio is capped at one so 30 FPS
// and below keep the stock weight exactly.
float __cdecl GetWheelSettleWeight() {
    const float weight = ReadGameFloat(kWheelSettleConstant, 0.75f);
    const float ratio = std::min(TimeStepRatio(), 1.0f);
    if (!(weight > 0.0f) || weight >= 1.0f || ratio >= 1.0f) {
        return weight;
    }
    return 1.0f - std::pow(1.0f - weight, ratio);
}

// The rotor speed constant is reached through the original instruction operand
// so mods that repoint it keep working.
float ReadHeliRotorFinalSpeed() {
    __try {
        const auto operand = *reinterpret_cast<const uintptr_t*>(
            kHeliRotorSpeedOperand + 2);
        return ReadGameFloat(operand, 0.22f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0.22f;
    }
}

float __cdecl GetHeliRotorSlowStep() {
    return (ReadHeliRotorFinalSpeed() / kHeliRotorSpeedDivisor)
         * TimeStepRatio();
}

float __cdecl GetHeliRotorFastStep() {
    return (ReadHeliRotorFinalSpeed() / kHeliRotorSpeedDivisor) * 3.0f
         * TimeStepRatio();
}

bool NearlyEqual(float a, float b) {
    return std::fabs(a - b) < 0.002f;
}

void WriteGameFloat(uintptr_t address, float value) {
    *reinterpret_cast<float*>(address) = value;
}

bool WriteProtectedGameFloat(uintptr_t address, float value) {
    DWORD oldProtect{};
    if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(value),
                        PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    WriteGameFloat(address, value);
    DWORD ignored{};
    VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), oldProtect,
                   &ignored);
    return true;
}

} // namespace hff
