#include "core/timestep.h"

#include "game/addresses.h"

#include <windows.h>

#include <array>
#include <cmath>

namespace hff {

float g_originalTimeStepValue{kOriginalTimeStep};

namespace {

struct FrameTickState {
    uint32_t frame;
    int32_t decision;
    float carry;
};

std::array<FrameTickState, kFrameTickSlots> g_frameTicks{};
int g_originalTimeStepDepth{};

} // namespace

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

void ResetFrameTicks() {
    for (auto& slot : g_frameTicks) {
        slot.frame = 0xFFFFFFFFu;
        slot.decision = 1;
        slot.carry = 0.0f;
    }
}

int32_t __cdecl FrameTick(int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= g_frameTicks.size()
        || InsideOriginalTimeStep()) {
        return 1;
    }
    FrameTickState& slot = g_frameTicks[index];

    const uint32_t frame = *reinterpret_cast<volatile uint32_t*>(kFrameCounter);
    if (frame == slot.frame) {
        return slot.decision;
    }
    slot.frame = frame;

    const float ratio = TimeStepRatio();
    if (!std::isfinite(ratio) || ratio >= 1.0f || ratio <= 0.0f) {
        slot.carry = 0.0f;
        slot.decision = 1;
        return 1;
    }
    const float total = slot.carry + ratio;
    if (total >= 1.0f) {
        slot.carry = total - 1.0f;
        slot.decision = 1;
    } else {
        slot.carry = total;
        slot.decision = 0;
    }
    return slot.decision;
}

bool InsideOriginalTimeStep() {
    return g_originalTimeStepDepth > 0;
}

ScopedOriginalTimeStep::ScopedOriginalTimeStep() : m_saved(kOriginalTimeStep) {
    ++g_originalTimeStepDepth;
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

ScopedOriginalTimeStep::~ScopedOriginalTimeStep() {
    --g_originalTimeStepDepth;
    if (!m_changed) {
        return;
    }
    __try {
        *reinterpret_cast<float*>(kTimerTimeStep) = m_saved;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

} // namespace hff
