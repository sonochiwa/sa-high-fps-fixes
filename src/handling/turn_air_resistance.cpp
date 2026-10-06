#include "handling/turn_air_resistance.h"

#include "core/config.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/handling.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace hff::handling {

namespace {

enum class TurnDamping : uint8_t {
    stock,
    groundVehicle,
    aircraft,
};

SitePatch g_turnAirResistancePatch{};
std::array<SitePatch, 3> g_flyingTurnResistancePatches{};
float g_turnAirResistanceStrength{1.0f};
bool g_groundVehicleTurnResistance{};
bool g_aircraftTurnResistance{};

// Which correction the vehicle the air resistance is applied to takes: a car
// or bike with a wheel on the ground, an aircraft, or none. A car in the air
// rotates as it did before these fixes, and boats and trains keep the stock
// damping.
TurnDamping ClassifyTurnDamping(const uint8_t* physical) {
    __try {
        if ((physical[kEntityTypeAndStatus] & 7) != kEntityTypeVehicle) {
            return TurnDamping::stock;
        }
        switch (*reinterpret_cast<const int32_t*>(physical + kVehicleSubClass)) {
        case kVehicleSubClassAutomobile:
        case kVehicleSubClassMonsterTruck:
        case kVehicleSubClassQuad:
        case kVehicleSubClassTrailer:
            return physical[kAutomobileContactWheels] > 0
                ? TurnDamping::groundVehicle : TurnDamping::stock;
        case kVehicleSubClassBike:
        case kVehicleSubClassBmx:
            return physical[kBikeContactWheels] > 0
                ? TurnDamping::groundVehicle : TurnDamping::stock;
        case kVehicleSubClassHeli:
        case kVehicleSubClassPlane:
            return TurnDamping::aircraft;
        default:
            return TurnDamping::stock;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return TurnDamping::stock;
    }
}

bool CorrectsTurnDamping(const uint8_t* physical) {
    switch (ClassifyTurnDamping(physical)) {
    case TurnDamping::groundVehicle:
        return g_groundVehicleTurnResistance;
    case TurnDamping::aircraft:
        return g_aircraftTurnResistance;
    default:
        return false;
    }
}

// The per-frame factor for `m_vecTurnSpeed`. A decay applied once per frame
// keeps the same fraction per second only when raised to the timestep ratio;
// the strength interpolates the exponent between the stock `1` and that
// ratio, so `0` is the untouched game and `100` is the 30 FPS vehicle. At
// 30 FPS the ratio is one and every strength returns the stock constant.
float __cdecl GetTurnAirResistanceFactor(const uint8_t* physical) {
    const float stock = ReadGameFloat(kTurnAirResistanceConstant, 0.99f);
    if (!(stock > 0.0f) || stock >= 1.0f || !CorrectsTurnDamping(physical)) {
        return stock;
    }
    const float exponent =
        1.0f + (TimeStepRatio() - 1.0f) * g_turnAirResistanceStrength;
    return std::pow(stock, exponent);
}

// The base FlyingControl raises to the timestep once more. Stock, it is
// `pow(TurnRes, ts) / (1 + speedTerm)`; for an aircraft the turn resistance is
// taken to the 30 FPS timestep instead, with the same strength as the `0.99`,
// so the resistance per second is what one original frame had.
float __cdecl FlyingTurnResistance(const uint8_t* vehicle,
                                   const float* turnResistance, float stock,
                                   float speedTerm) {
    const float divisor = 1.0f + speedTerm;
    if (!g_aircraftTurnResistance
        || ClassifyTurnDamping(vehicle) != TurnDamping::aircraft) {
        return stock / divisor;
    }
    __try {
        const float timeStep = ReadGameFloat(kTimerTimeStep, kOriginalTimeStep);
        const float base = *turnResistance;
        if (timeStep <= 0.0f || timeStep >= kOriginalTimeStep
            || !(base > 0.0f) || !std::isfinite(base)) {
            return stock / divisor;
        }
        const float exponent = timeStep
            + (kOriginalTimeStep - timeStep) * g_turnAirResistanceStrength;
        return std::pow(base, exponent) / divisor;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return stock / divisor;
    }
}

// Replaces the three `fld / fmul 0.99 / fstp` triples that damp
// `m_vecTurnSpeed`. `esi` is the physical and the x87 stack is empty here, so
// the factor comes back on it and the stores keep the original order and
// field offsets.
__declspec(naked) void TurnAirResistanceThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        push esi
        call GetTurnAirResistanceFactor
        add esp, 4
        pop edx
        pop ecx
        pop eax
        popfd
        fld st(0)
        fmul dword ptr [esi + 0x50]
        fstp dword ptr [esi + 0x50]
        fld st(0)
        fmul dword ptr [esi + 0x54]
        fstp dword ptr [esi + 0x54]
        fmul dword ptr [esi + 0x58]
        fstp dword ptr [esi + 0x58]
        jmp kTurnAirResistanceReturn
    }
}

// Each replaces `fadd [1.0] / fdivr [esp+N]` with the speed term alone on the
// x87 stack and leaves the quotient there. The `pow` call after the return
// address writes eax, ecx and edx anyway, and nothing reads the flags. Once
// the speed term is stored, `[esp+N]` is four bytes further up.
__declspec(naked) void FlyingTurnResistanceXThunk() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        push dword ptr [esp + 0x20]
        lea eax, [edi + 0x40]
        push eax
        push esi
        call FlyingTurnResistance
        add esp, 16
        jmp kFlyingTurnResistanceXReturn
    }
}

__declspec(naked) void FlyingTurnResistanceYThunk() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        push dword ptr [esp + 0x24]
        lea eax, [edi + 0x44]
        push eax
        push esi
        call FlyingTurnResistance
        add esp, 16
        jmp kFlyingTurnResistanceYReturn
    }
}

__declspec(naked) void FlyingTurnResistanceZThunk() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        push dword ptr [esp + 0x28]
        lea eax, [edi + 0x48]
        push eax
        push esi
        call FlyingTurnResistance
        add esp, 16
        jmp kFlyingTurnResistanceZReturn
    }
}

// The `0.99` site serves both switches; the first of them to install puts it
// in place.
bool EnsureTurnAirResistanceSite(const char* fixName) {
    if (g_turnAirResistancePatch.installed) {
        return true;
    }
    g_turnAirResistanceStrength = static_cast<float>(std::clamp(
        ReadNumber("vehicles", "turnAirResistanceStrength", 100), 0, 100))
                                / 100.0f;
    if (InstallJump(g_turnAirResistancePatch, kTurnAirResistance,
                    &TurnAirResistanceThunk, kExpectedTurnAirResistance)) {
        return true;
    }
    char line[160];
    std::snprintf(line, sizeof(line),
                  "%s skipped: CPhysical::ApplyAirResistance bytes do not "
                  "match GTA SA 1.0 US.", fixName);
    Log(line);
    return false;
}

} // namespace

bool InstallTurnAirResistanceFix() {
    if (!EnsureTurnAirResistanceSite("Turn air resistance fix")) {
        return false;
    }
    g_groundVehicleTurnResistance = true;
    char installed[160];
    std::snprintf(installed, sizeof(installed),
                  "Installed %.0f%% timestep-scaled turn air resistance for "
                  "cars and bikes with a wheel on the ground.",
                  g_turnAirResistanceStrength * 100.0f);
    Log(installed);
    return true;
}

bool InstallAircraftTurnResistanceFix() {
    PatchSet patches("Aircraft turn resistance fix");
    const std::array<const void*, 3> thunks{
        &FlyingTurnResistanceXThunk,
        &FlyingTurnResistanceYThunk,
        &FlyingTurnResistanceZThunk,
    };
    const std::array<uintptr_t, 3> sites{
        kFlyingTurnResistanceX, kFlyingTurnResistanceY, kFlyingTurnResistanceZ
    };
    for (size_t i = 0; i < sites.size(); ++i) {
        if (!patches.Track(InstallJump(g_flyingTurnResistancePatches[i], sites[i],
                                       thunks[i], kExpectedFlyingTurnResistance[i]),
                           g_flyingTurnResistancePatches[i])) {
            Log("Aircraft turn resistance fix skipped: CVehicle::FlyingControl "
                "bytes do not match GTA SA 1.0 US.");
            return false;
        }
    }
    if (!EnsureTurnAirResistanceSite("Aircraft turn resistance fix")) {
        return false;
    }
    patches.Commit();
    g_aircraftTurnResistance = true;
    Log("Installed timestep-scaled turn resistance for aircraft.");
    return true;
}

} // namespace hff::handling
