#pragma once

#include "modules/prelude.h"

// Literals inside SCM scripts that step a value once a frame: in the streamed
// script `player_parachute.scm` (script.img), which runs the player's freefall
// and canopy flight, and in the burglary mission of main.scm.

namespace hff {

constexpr size_t kRunningScriptBaseIp = 0x10;

// How a literal is used. A smoothing divisor or factor closes a share of the
// distance to a target each frame, and a decay factor keeps a share of a
// value each frame, so the share is raised to the timestep ratio; a rate
// divisor turns a value into a per-frame step, so the step is multiplied by
// it.
enum class ScriptLiteralKind : uint8_t {
    smoothingDivisor,
    smoothingFactor,
    decayFactor,
    rateDivisor,
};

// One `DIV_FLOAT_LVAR_BY_VAL` (0x0017) or `MULT_FLOAT_LVAR_BY_VAL` (0x0013)
// with a float literal: opcode, local variable type 3 with its index, float
// type 6 and the value, which starts six bytes into the instruction. The
// offset is from the script's base, which for a mission is the mission block.
struct ScriptLiteral {
    uint16_t offset;
    uint16_t opcode;
    uint16_t localVar;
    float stock;
    ScriptLiteralKind kind;
};

constexpr size_t kScriptLiteralValueOffset = 6;

// `SCRIPT_NAME` of the running script.
constexpr char kParachuteScriptName[] = "PLCHUTE";

constexpr ScriptLiteral kParachuteLiterals[] = {
    // Freefall: roll eases towards the stick by a twentieth a frame, heading
    // turns by a fifth of the roll a frame, pitch eases by a twentieth, and
    // the horizontal speed closes a hundredth of its gap to the target.
    {986, 0x0017, 21, 20.0f, ScriptLiteralKind::smoothingDivisor},
    {1012, 0x0017, 21, 5.0f, ScriptLiteralKind::rateDivisor},
    {1118, 0x0017, 22, 20.0f, ScriptLiteralKind::smoothingDivisor},
    {1499, 0x0013, 21, 0.01f, ScriptLiteralKind::smoothingFactor},
    {1541, 0x0013, 21, 0.01f, ScriptLiteralKind::smoothingFactor},
    // Canopy: roll eases by a twentieth, heading turns by a fifteenth of the
    // roll, and the sink rate eases by a twentieth towards the value the
    // stick asks for.
    {3228, 0x0017, 21, 20.0f, ScriptLiteralKind::smoothingDivisor},
    {3254, 0x0017, 21, 15.0f, ScriptLiteralKind::rateDivisor},
    {3462, 0x0017, 21, 20.0f, ScriptLiteralKind::smoothingDivisor},
    {3775, 0x0017, 21, 20.0f, ScriptLiteralKind::smoothingDivisor},
    {3934, 0x0017, 21, 20.0f, ScriptLiteralKind::smoothingDivisor},
    {4082, 0x0017, 21, 20.0f, ScriptLiteralKind::smoothingDivisor},
};

constexpr char kBurglaryScriptName[] = "BURGJB";

constexpr ScriptLiteral kBurglaryLiterals[] = {
    // The noise meter. Each frame the mission adds how far the loudness of
    // the sounds the player made rises above 20, then keeps 0.99 of the
    // total. A step is heard in one frame only, whatever the frame rate, so
    // above 30 FPS the meter drains many times faster than steps fill it.
    {6735, 0x0013, 41, 0.99f, ScriptLiteralKind::decayFactor},
};

} // namespace hff
