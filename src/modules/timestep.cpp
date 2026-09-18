#include "modules/modules.h"

namespace hff {

// This ratio is 1.0 at the original 30 FPS timestep and shrinks
// proportionally as the frame rate rises.
float TimeStepRatio() {
    __try {
        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f) {
            return 1.0f;
        }
        return timeStep / kOriginalTimeStep;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1.0f;
    }
}

float ReadGameFloat(uintptr_t address, float fallback) {
    __try {
        const float value = *reinterpret_cast<const float*>(address);
        return std::isfinite(value) ? value : fallback;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return fallback;
    }
}

int AccumulateMilliseconds(float milliseconds, float& fraction) {
    if (!std::isfinite(milliseconds) || milliseconds <= 0.0f) {
        fraction = 0.0f;
        return 0;
    }

    const float total = milliseconds + fraction;
    const int whole = static_cast<int>(total);
    fraction = total - static_cast<float>(whole);
    return whole;
}

int __cdecl AccumulateFlightTimer(float milliseconds) {
    if (!g_flightTimerActive) {
        g_flightTimerFraction = 0.0f;
        g_flightTimerActive = true;
    }
    g_endTimerActive = false;
    return AccumulateMilliseconds(milliseconds, g_flightTimerFraction);
}

int __cdecl AccumulateEndTimer(float milliseconds) {
    if (!g_endTimerActive) {
        g_endTimerFraction = 0.0f;
        g_endTimerActive = true;
    }
    g_flightTimerActive = false;
    return AccumulateMilliseconds(milliseconds, g_endTimerFraction);
}

} // namespace hff
