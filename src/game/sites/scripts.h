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
// and canopy flight, and in missions of main.scm.

// How a literal is used. A smoothing divisor or factor closes a share of the
// distance to a target each frame, and a decay factor keeps a share of a
// value each frame, so the share is raised to the timestep ratio; a rate
// divisor turns a value into a per-frame step, so the step is multiplied by
// it; a frame step is added once a frame, so it is multiplied by it too.
enum class ScriptLiteralKind : uint8_t {
    smoothingDivisor,
    smoothingFactor,
    decayFactor,
    rateDivisor,
    frameStep,
};

// The SCM operand types of the variable an instruction changes: a global,
// given by its byte offset, or a local, given by its index.
constexpr uint8_t kScriptGlobalVariable = 2;
constexpr uint8_t kScriptLocalVariable = 3;

// One arithmetic instruction on a float variable with a float literal, such
// as `ADD_VAL_TO_FLOAT_LVAR` (0x000B), `SUB_VAL_FROM_FLOAT_LVAR` (0x000F),
// `MULT_FLOAT_LVAR_BY_VAL` (0x0013), `DIV_FLOAT_LVAR_BY_VAL` (0x0017) or the
// global forms 0x0009 and 0x000D: opcode, the variable's type and number,
// float type 6 and the value, which starts six bytes into the instruction.
// The offset is from the script's base, which for a mission is the mission
// block.
struct ScriptLiteral {
    uint16_t offset;
    uint16_t opcode;
    uint16_t variable;
    float stock;
    ScriptLiteralKind kind;
    uint8_t variableType{kScriptLocalVariable};
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

// Home Invasion keeps its noise meter the same way: `166@ -= 20.0`,
// `167@ += 166@`, then `167@ *= 0.99` each frame, the last found by the same
// three instructions when the mission has moved.
constexpr char kHomeInvasionScriptName[] = "GUNS1";

constexpr ScriptLiteral kHomeInvasionLiterals[] = {
    {17328, 0x0013, 167, 0.99f, ScriptLiteralKind::decayFactor},
};

constexpr std::array<uint8_t, 24> kHomeInvasionNoisePattern{
    0x0F, 0x00, 0x03, 0xA6, 0x00, 0x06, 0x00, 0x00, 0xA0, 0x41,
    0x5B, 0x00, 0x03, 0xA7, 0x00, 0x03, 0xA6, 0x00,
    0x13, 0x00, 0x03, 0xA7, 0x00, 0x06
};
constexpr size_t kHomeInvasionNoisePatternLiteral = 18;

// Small Town Bank: while the hostages are held, the rate at which their
// hands-up bars drain grows by this much a frame. The bars themselves fill
// and drain by the timestep.
constexpr char kSmallTownBankScriptName[] = "CAT2";

constexpr ScriptLiteral kSmallTownBankLiterals[] = {
    {6633, 0x000B, 81, 0.000025f, ScriptLiteralKind::frameStep},
};

// Tanker Commander: the speed limit of the car the player follows rises by
// 0.2 a frame up to 60 while the player is more than 15 m away, and falls by
// 0.1 a frame down to 40 while closer.
constexpr char kTankerCommanderScriptName[] = "CAT3";

constexpr ScriptLiteral kTankerCommanderLiterals[] = {
    {10499, 0x000B, 321, 0.2f, ScriptLiteralKind::frameStep},
    {10537, 0x000F, 321, 0.1f, ScriptLiteralKind::frameStep},
};

// Zeroing In: the speed limit of the car the player hunts, global $7405,
// rises by 0.1 a frame up to 45 while the player is within 50 m of its
// driver, and falls by 0.1 a frame down to 30 otherwise.
constexpr char kZeroingInScriptName[] = "STEAL1";

constexpr ScriptLiteral kZeroingInLiterals[] = {
    {4264, 0x0009, 7405 * 4, 0.1f, ScriptLiteralKind::frameStep,
     kScriptGlobalVariable},
    {4302, 0x000D, 7405 * 4, 0.1f, ScriptLiteralKind::frameStep,
     kScriptGlobalVariable},
};

} // namespace hff
