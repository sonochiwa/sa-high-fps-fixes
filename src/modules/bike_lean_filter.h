#pragma once

#include "modules/prelude.h"

namespace hff {

// The replaced block is three copies of `fld [esi+n]; fmul 0.99; fstp [esi+n]`,
// so the factor is computed once and reused with the same `fld st(0)` shape the
// original uses for the move speed a few instructions above. The net effect on
// the x87 stack is zero, exactly as for the block it replaces.
// The value handed in is the engine's estimate of the lateral acceleration in
// g, measured across one call as `deltaSpeed / (timeStep * gravity)`. Standing
// still it reached 0.7451 at 500 FPS against 0.0134 at 30 FPS, while the bike's
// actual roll was the same to within a factor of 1.5, so the estimate is not
// reporting real motion.
//
// It cannot be repaired by accumulating the per-call deltas over a real-time
// window. Between calls the resting contact cancels the tangential speed, so a
// delta is not an exact difference and the sum keeps the contact chatter that
// the endpoints would have cancelled. Measured, that left the amplitude at
// 0.4353, barely better than the 0.7451 it started from.
//
// What works is a plain finite difference of the state: the velocity is
// measured from how far the bike actually travelled over one original frame of
// real time, and the difference between two such measurements is the
// acceleration across that interval. Standing still, both samples are zero and
// the estimate is zero. Cornering, the difference is the real change in
// velocity over 1/30 s, which is what the engine measures at 30 FPS.
//
// The velocity kept between windows is the whole vector. Keeping only its
// component along the bike's right axis looks equivalent and is not: those axes
// turn with the bike, so a steady corner holds that component roughly constant
// and its difference is zero. That version reported no lateral acceleration
// through corners and held the bike upright at any frame rate above 30, which
// is what the projection loses — the engine's numerator is a change of world
// velocity, and the projection happens after the difference, not before.
//
// The direction dotted against is the matrix right vector, which is what the
// original numerator uses at `0x6BBAC2` through `edi`, loaded from `[esi+0x14]`.
// At or below the original frame rate the engine's own value is passed through,
// so stock behavior is reproduced by construction rather than by arithmetic.
// `CBike::ProcessControl` runs for every bike in the world, not just the one
// the player is on, so a single set of globals is torn between them: the
// "different bike" branch fires on every call, the interval never accumulates
// and the filter degenerates into a pass-through. The state is therefore kept
// per bike, in a small table that evicts whichever entry has gone longest
// without being touched.
constexpr size_t kLeanSlots = 8;

struct LeanTargetState {
    uintptr_t bike;
    float position[3];
    float velocity[3];
    float elapsed;
    uint32_t frame;
    uint32_t lastFrame;
    bool primed;
    bool sampled;
    float held;
};
extern LeanTargetState g_leanStates[kLeanSlots];
void ResetLeanStateFor(uintptr_t bike);
LeanTargetState& LeanStateFor(uintptr_t bike, uint32_t frame);
void __cdecl FilterBikeLeanTarget(float* target, uintptr_t bike);

} // namespace hff
