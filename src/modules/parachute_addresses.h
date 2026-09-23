#pragma once

#include "modules/prelude.h"

// Sites of the parachute flight fix, inside the streamed script
// `player_parachute.scm` (script.img), which runs the player's freefall and
// canopy flight once a frame.

namespace hff {

// `SCRIPT_NAME` of the running script.
constexpr char kParachuteScriptName[] = "PLCHUTE";

constexpr size_t kRunningScriptBaseIp = 0x10;

// How a literal is used. A smoothing divisor or factor closes a share of
// the distance to a target each frame, so the share is raised to the
// timestep ratio; a rate divisor turns a value into a per-frame step, so the
// step is multiplied by it.
enum class ParachuteLiteralKind : uint8_t {
    smoothingDivisor,
    smoothingFactor,
    rateDivisor,
};

// One `DIV_FLOAT_LVAR_BY_VAL` (0x0017) or `MULT_FLOAT_LVAR_BY_VAL` (0x0013)
// with a float literal: opcode, local variable type 3 with its index, float
// type 6 and the value, which starts six bytes into the instruction.
struct ParachuteLiteral {
    uint16_t offset;
    uint16_t opcode;
    uint16_t localVar;
    float stock;
    ParachuteLiteralKind kind;
};

constexpr size_t kParachuteLiteralValueOffset = 6;

constexpr ParachuteLiteral kParachuteLiterals[] = {
    // Freefall: roll eases towards the stick by a twentieth a frame, heading
    // turns by a fifth of the roll a frame, pitch eases by a twentieth, and
    // the horizontal speed closes a hundredth of its gap to the target.
    {986, 0x0017, 21, 20.0f, ParachuteLiteralKind::smoothingDivisor},
    {1012, 0x0017, 21, 5.0f, ParachuteLiteralKind::rateDivisor},
    {1118, 0x0017, 22, 20.0f, ParachuteLiteralKind::smoothingDivisor},
    {1499, 0x0013, 21, 0.01f, ParachuteLiteralKind::smoothingFactor},
    {1541, 0x0013, 21, 0.01f, ParachuteLiteralKind::smoothingFactor},
    // Canopy: roll eases by a twentieth, heading turns by a fifteenth of the
    // roll, and the sink rate eases by a twentieth towards the value the
    // stick asks for.
    {3228, 0x0017, 21, 20.0f, ParachuteLiteralKind::smoothingDivisor},
    {3254, 0x0017, 21, 15.0f, ParachuteLiteralKind::rateDivisor},
    {3462, 0x0017, 21, 20.0f, ParachuteLiteralKind::smoothingDivisor},
    {3775, 0x0017, 21, 20.0f, ParachuteLiteralKind::smoothingDivisor},
    {3934, 0x0017, 21, 20.0f, ParachuteLiteralKind::smoothingDivisor},
    {4082, 0x0017, 21, 20.0f, ParachuteLiteralKind::smoothingDivisor},
};

} // namespace hff
