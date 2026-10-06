#pragma once

#include <cstdint>

namespace hff {

// The 30 FPS timestep as a variable, for thunks that divide by it from memory.
extern float g_originalTimeStepValue;

// 1.0 at the original 30 FPS timestep, shrinking proportionally as the frame
// rate rises.
float TimeStepRatio();
// Adds whole milliseconds to an integer timer and carries the fraction.
int AccumulateMilliseconds(float milliseconds, float& fraction);

// Decisions shared by every caller within one frame: true on the frames on
// which an original 30 FPS frame has elapsed, and on every frame at or below
// 30 FPS.
enum FrameTickSlot : int32_t {
    kFrameTickDrunkSteer,
    kFrameTickFireEvents,
    kFrameTickWaterCannon,
    kFrameTickExplosions,
    kFrameTickSwimSplash,
    kFrameTickTearGas,
    kFrameTickHydraulics,
    kFrameTickWeather,
    kFrameTickBoatWaves,
    kFrameTickSwatRopes,
    kFrameTickAmbience,
    kFrameTickSlots,
};
void ResetFrameTicks();
int32_t __cdecl FrameTick(int32_t slot);
// How many frames the slot has let through, a count of original 30 FPS frames
// for code that measures time in frames.
uint32_t __cdecl FrameTickCount(int32_t slot);

// True while an entity steps at the original rate inside
// ScopedOriginalTimeStep. Its step stands for a whole original frame, so the
// once-a-frame gates let it through and keep their shared state for the rest
// of the frame.
bool InsideOriginalTimeStep();

// Runs the enclosed game code with the 30 FPS timestep.
class ScopedOriginalTimeStep {
public:
    ScopedOriginalTimeStep();
    ~ScopedOriginalTimeStep();
    ScopedOriginalTimeStep(const ScopedOriginalTimeStep&) = delete;
    ScopedOriginalTimeStep& operator=(const ScopedOriginalTimeStep&) = delete;

private:
    float m_saved;
    bool m_changed{};
};

} // namespace hff
