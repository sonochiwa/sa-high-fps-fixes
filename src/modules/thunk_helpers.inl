// C++ calculations called by the naked x86 bridge functions. Keeping these
// outside the thunk modules makes the ABI boundary explicit: thunks only move
// registers/stack values and select a continuation address.

uint8_t g_mapWheelUpEdge = 0;
uint8_t g_mapWheelDownEdge = 0;
uint8_t g_mapWheelUpPrev = 0;
uint8_t g_mapWheelDownPrev = 0;

void __cdecl SampleMapWheelEdges() {
    const uint8_t up = *reinterpret_cast<volatile uint8_t*>(kMouseWheelUpFlag);
    const uint8_t down =
        *reinterpret_cast<volatile uint8_t*>(kMouseWheelDownFlag);
    g_mapWheelUpEdge = (up && !g_mapWheelUpPrev) ? 1 : 0;
    g_mapWheelDownEdge = (down && !g_mapWheelDownPrev) ? 1 : 0;
    g_mapWheelUpPrev = up;
    g_mapWheelDownPrev = down;
}

std::array<float, 3> g_fireEventCarries{};

int32_t __cdecl FireEventTick(int32_t site) {
    if (site < 0 || static_cast<size_t>(site) >= g_fireEventCarries.size()) {
        return 1;
    }
    const float ratio = TimeStepRatio();
    if (!std::isfinite(ratio) || ratio >= 1.0f || ratio <= 0.0f) {
        return 1;
    }
    const float total = g_fireEventCarries[site] + ratio;
    if (total >= 1.0f) {
        g_fireEventCarries[site] = total - 1.0f;
        return 1;
    }
    g_fireEventCarries[site] = total;
    return 0;
}

constexpr size_t kFrameTickSlots = 1;
constexpr int32_t kFrameTickDrunkSteer = 0;

struct FrameTickSlot {
    uint32_t frame;
    int32_t decision;
    float carry;
};

std::array<FrameTickSlot, kFrameTickSlots> g_frameTicks{};

void ResetFrameTicks() {
    for (auto& slot : g_frameTicks) {
        slot.frame = 0xFFFFFFFFu;
        slot.decision = 1;
        slot.carry = 0.0f;
    }
}

int32_t __cdecl FrameTick(int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= g_frameTicks.size()) {
        return 1;
    }
    FrameTickSlot& slot = g_frameTicks[index];

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

float g_fatCounterCarry = 0.0f;

void __cdecl FatCounterAdd(uint32_t milliseconds, uint32_t rate) {
    const double product =
        static_cast<double>(static_cast<uint64_t>(milliseconds) * rate);
    const double value = product / 10.0 + static_cast<double>(g_fatCounterCarry);
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    const double whole = std::floor(value);
    g_fatCounterCarry = static_cast<float>(value - whole);
    if (whole >= 1.0) {
        *reinterpret_cast<uint32_t*>(kFatCounter) +=
            static_cast<uint32_t>(whole);
    }
}

void __cdecl ScaleRollOntoWheelsForce(float* force) {
    __try {
        const float ratio = TimeStepRatio();
        if (!std::isfinite(ratio) || ratio <= 0.0f || ratio >= 1.0f) {
            return;
        }
        force[0] *= ratio;
        force[1] *= ratio;
        force[2] *= ratio;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
}

int __cdecl ConsumeBreakObjectLifetimeTicks(int32_t* lifetime) {
    if (!lifetime || *lifetime <= 0) {
        return 0;
    }
    const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
    if (frame != g_breakLifetimeLastFrame) {
        g_breakLifetimeLastFrame = frame;
        g_breakLifetimeCarry += *reinterpret_cast<const float*>(kTimerTimeStep)
                              / g_originalTimeStepValue;
        g_breakLifetimeTicks = static_cast<int>(g_breakLifetimeCarry);
        g_breakLifetimeCarry -= static_cast<float>(g_breakLifetimeTicks);
    }
    return std::min(g_breakLifetimeTicks, *lifetime);
}
