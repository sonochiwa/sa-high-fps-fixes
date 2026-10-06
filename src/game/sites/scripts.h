#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// `CTheScripts::Process`, run once a frame on the game thread before any
// script, and its first instruction, `mov al,[0A43088h]`.
constexpr uintptr_t kScriptsProcess = 0x0046A000;
constexpr std::array<uint8_t, 5> kExpectedScriptsProcess{
    0xA0, 0x88, 0x30, 0xA4, 0x00
};

// CRunningScript::ProcessCommands800To899, opcode 034E (SLIDE_OBJECT).
// The three movement rates are ScriptParams[4..6].
constexpr uintptr_t kScriptSlideObject = 0x00482342;
constexpr uintptr_t kScriptSlideObjectReturn = 0x0048234D;
constexpr std::array<uint8_t, 11> kExpectedScriptSlideObject{
    0xA1, 0x78, 0x3C, 0xA4, 0x00,
    0x8B, 0x0D, 0x9C, 0x44, 0xB7, 0x00
};
// CRunningScript::ProcessCommands800To899, opcode 034D (ROTATE_OBJECT).
// Its angular rate is ScriptParams[2].
constexpr uintptr_t kScriptRotateObject = 0x00481CAE;
constexpr uintptr_t kScriptRotateObjectReturn = 0x00481CBB;
constexpr std::array<uint8_t, 13> kExpectedScriptRotateObject{
    0x8B, 0x0D, 0x78, 0x3C, 0xA4, 0x00,
    0x51,
    0x8B, 0x0D, 0x9C, 0x44, 0xB7, 0x00
};

// CRunningScript flags. `ShutdownThisScript` clears m_IsActive;
// `StartNewStreamedScript` sets m_IsExternal.
constexpr size_t kRunningScriptIsActive = 0xC4;
constexpr size_t kRunningScriptIsExternal = 0xC7;

// CTheScripts::StreamedScripts: 82 entries of 0x20 bytes, each the loaded
// image (nulled when it is freed), the count of scripts running from it (the
// image is only freed at zero) and the image size, then the count of entries.
constexpr uintptr_t kStreamedScripts = 0x00A47B60;
constexpr size_t kStreamedScriptSlots = 82;
constexpr size_t kStreamedScriptStride = 0x20;
constexpr size_t kStreamedScriptUsers = 0x04;
constexpr size_t kStreamedScriptSize = 0x1C;
constexpr size_t kStreamedScriptCount = 0xA44;

// CTheScripts::MissionBlock, where the running mission is loaded.
constexpr uintptr_t kMissionBlock = 0x00A7A6A0;
constexpr size_t kMissionBlockSize = 69000;

// Literals inside SCM scripts that step a value once a frame: in the streamed
// script `player_parachute.scm` (script.img), which runs the player's freefall
// and canopy flight, and in the burglary mission of main.scm.

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

// The three instructions that end at the noise meter's literal, unique in the
// mission, so a main.scm whose missions were edited elsewhere still has its
// literal found: `$1792 -= 20.0`, `41@ += $1792` and the `41@ *=` the literal
// belongs to, which starts 18 bytes in.
constexpr std::array<uint8_t, 24> kBurglaryNoisePattern{
    0x0D, 0x00, 0x02, 0x00, 0x1C, 0x06, 0x00, 0x00, 0xA0, 0x41,
    0x5D, 0x00, 0x03, 0x29, 0x00, 0x02, 0x00, 0x1C,
    0x13, 0x00, 0x03, 0x29, 0x00, 0x06
};
constexpr size_t kBurglaryNoisePatternLiteral = 18;

} // namespace hff
