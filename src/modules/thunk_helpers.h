#pragma once

#include "modules/prelude.h"

namespace hff {

extern uint8_t g_mapWheelUpEdge;
extern uint8_t g_mapWheelDownEdge;
extern uint8_t g_mapWheelUpPrev;
extern uint8_t g_mapWheelDownPrev;
void __cdecl SampleMapWheelEdges();
extern std::array<float, 3> g_fireEventCarries;
int32_t __cdecl FireEventTick(int32_t site);

constexpr size_t kFrameTickSlots = 1;
constexpr int32_t kFrameTickDrunkSteer = 0;

struct FrameTickSlot {
    uint32_t frame;
    int32_t decision;
    float carry;
};
extern std::array<FrameTickSlot, kFrameTickSlots> g_frameTicks;
void ResetFrameTicks();
int32_t __cdecl FrameTick(int32_t index);
extern float g_fatCounterCarry;
void __cdecl FatCounterAdd(uint32_t milliseconds, uint32_t rate);
float __cdecl GetDrunkCameraPhaseStep();
int __cdecl ConsumeBreakObjectLifetimeTicks(int32_t* lifetime);

} // namespace hff
