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
