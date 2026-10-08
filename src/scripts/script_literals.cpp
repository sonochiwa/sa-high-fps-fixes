#include "scripts/script_literals.h"

#include "core/log.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/frame_hook.h"
#include "game/scripts.h"
#include "game/sites/scripts.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>

// Some SCM scripts step a value once a frame with 30 FPS steps written into
// their literals. The player's freefall and canopy flight in
// `player_parachute.scm` turn by a share of the roll a frame, and ease roll,
// pitch, horizontal speed and sink rate towards their targets by a fixed share
// a frame, so above 30 FPS the jumper spins and settles many times faster. The
// burglary mission and Home Invasion keep a fixed share of their noise meters
// a frame, so above 30 FPS the meter drains before it can fill. Small Town
// Bank, Tanker Commander and Zeroing In add a fixed step a frame to how fast
// the hostages give up and how fast the car the player follows may drive.
// Catalyst fills its throw meter and counts a crate's contact a frame, and
// Interdiction pushes its attacking helicopters a frame and times the damage
// they do with a threshold that 30 FPS frames pass every 67 ms. Before the
// scripts run each frame, the running script's literals are set to the values
// that give the 30 FPS steps over the current timestep; at 30 FPS they are
// the stock values.
//
// A literal is only written into a script that is running from memory that is
// still its own: an active script whose image is a loaded streamed script or
// the mission block, with the instructions around every literal as expected.

namespace hff::scripts {

namespace {

enum class ScriptImage : uint8_t {
    streamed,
    mission,
};

struct ScriptLiteralSet {
    const char* scriptName;
    const char* fixName;
    const char* scriptDescription;
    const ScriptLiteral* literals;
    size_t count;
    ScriptImage image;
    // Instructions that end at the first literal and are unique in the
    // mission, so it is still found when the mission has moved, or null.
    const uint8_t* pattern;
    size_t patternSize;
    size_t patternLiteral;
    bool enabled;
    // The image last checked and how far its literals sit from their stock
    // offsets, so the result is logged once per image.
    uintptr_t checkedBase;
    int32_t shift;
    bool matches;
};

constexpr float kOriginalFrameMilliseconds = 1000.0f / 30.0f;

std::array<ScriptLiteralSet, 8> g_literalSets{{
    {kParachuteScriptName, "Parachute flight fix", "player_parachute.scm",
     kParachuteLiterals, std::size(kParachuteLiterals), ScriptImage::streamed,
     nullptr, 0, 0},
    {kBurglaryScriptName, "Burglary noise fix", "the burglary mission",
     kBurglaryLiterals, std::size(kBurglaryLiterals), ScriptImage::mission,
     kBurglaryNoisePattern.data(), kBurglaryNoisePattern.size(),
     kBurglaryNoisePatternLiteral},
    {kHomeInvasionScriptName, "Burglary noise fix", "Home Invasion",
     kHomeInvasionLiterals, std::size(kHomeInvasionLiterals),
     ScriptImage::mission, kHomeInvasionNoisePattern.data(),
     kHomeInvasionNoisePattern.size(), kHomeInvasionNoisePatternLiteral},
    {kSmallTownBankScriptName, "Mission script fix", "Small Town Bank",
     kSmallTownBankLiterals, std::size(kSmallTownBankLiterals),
     ScriptImage::mission, nullptr, 0, 0},
    {kTankerCommanderScriptName, "Mission script fix", "Tanker Commander",
     kTankerCommanderLiterals, std::size(kTankerCommanderLiterals),
     ScriptImage::mission, nullptr, 0, 0},
    {kZeroingInScriptName, "Mission script fix", "Zeroing In",
     kZeroingInLiterals, std::size(kZeroingInLiterals), ScriptImage::mission,
     nullptr, 0, 0},
    {kCatalystScriptName, "Mission script fix", "Catalyst",
     kCatalystLiterals, std::size(kCatalystLiterals), ScriptImage::mission,
     nullptr, 0, 0},
    {kInterdictionScriptName, "Mission script fix", "Interdiction",
     kInterdictionLiterals, std::size(kInterdictionLiterals),
     ScriptImage::mission, nullptr, 0, 0},
}};

// What one frame's literals are computed from: the timestep ratio and
// whether an original 30 FPS frame has passed.
struct FrameRate {
    float ratio;
    int32_t ticks;
};

size_t OperandSize(uint8_t type) {
    switch (type) {
    case kScriptInt8:
        return sizeof(int8_t);
    case kScriptLocalVariable:
        return sizeof(uint16_t);
    default:
        return sizeof(float);
    }
}

// The end of the farthest literal of a set, which the image must reach.
size_t LiteralsEnd(const ScriptLiteralSet& set) {
    size_t end = 0;
    for (size_t i = 0; i < set.count; ++i) {
        end = (std::max)(end, set.literals[i].offset + set.shift
                                  + kScriptLiteralValueOffset
                                  + OperandSize(set.literals[i].operandType));
    }
    return end;
}

// Whether `base` is the loaded image of a streamed script that at least one
// running script uses and that holds every literal.
bool IsLiveStreamedImage(uintptr_t base, size_t needed) {
    const auto count = static_cast<size_t>(
        *reinterpret_cast<const int16_t*>(kStreamedScripts + kStreamedScriptCount));
    for (size_t i = 0; i < count && i < kStreamedScriptSlots; ++i) {
        const uintptr_t entry = kStreamedScripts + i * kStreamedScriptStride;
        if (*reinterpret_cast<const uintptr_t*>(entry) == base) {
            return *reinterpret_cast<const uint8_t*>(entry + kStreamedScriptUsers) != 0
                && *reinterpret_cast<const int32_t*>(entry + kStreamedScriptSize)
                       >= static_cast<int32_t>(needed);
        }
    }
    return false;
}

bool ImageIsLive(const ScriptLiteralSet& set, uintptr_t script, uintptr_t base) {
    if (*reinterpret_cast<const uint8_t*>(script + kRunningScriptIsActive) == 0) {
        return false;
    }
    if (set.image == ScriptImage::mission) {
        return base == kMissionBlock && LiteralsEnd(set) <= kMissionBlockSize;
    }
    return *reinterpret_cast<const uint8_t*>(script + kRunningScriptIsExternal) != 0
        && IsLiveStreamedImage(base, LiteralsEnd(set));
}

// The instructions around each literal. The values themselves are not
// compared: they carry the previous frame's rewrite. A step kept in a local
// must name that local of the running mission.
bool ScriptLiteralsMatch(const uint8_t* base, const ScriptLiteralSet& set) {
    for (size_t i = 0; i < set.count; ++i) {
        const ScriptLiteral& literal = set.literals[i];
        const uint8_t* at = base + literal.offset + set.shift;
        uint16_t opcode{};
        uint16_t variable{};
        std::memcpy(&opcode, at, sizeof(opcode));
        std::memcpy(&variable, at + 3, sizeof(variable));
        if (opcode != literal.opcode || at[2] != literal.variableType
            || variable != literal.variable || at[5] != literal.operandType) {
            return false;
        }
        if (literal.operandType == kScriptLocalVariable) {
            uint16_t local{};
            std::memcpy(&local, at + kScriptLiteralValueOffset, sizeof(local));
            if (set.image != ScriptImage::mission || local != literal.operandLocal
                || local >= kMissionLocalVariableCount) {
                return false;
            }
        }
    }
    return true;
}

// Where the first literal is in a mission edited so that it moved: the offset
// from its stock place, or 0 when the pattern is not found exactly once.
int32_t LocateMovedLiteral(const uint8_t* base, const ScriptLiteralSet& set) {
    const uint8_t* found = nullptr;
    for (size_t i = 0; i + set.patternSize <= kMissionBlockSize; ++i) {
        if (std::memcmp(base + i, set.pattern, set.patternSize) != 0) {
            continue;
        }
        if (found) {
            return 0;
        }
        found = base + i;
    }
    if (!found) {
        return 0;
    }
    return static_cast<int32_t>(found - base + set.patternLiteral)
         - static_cast<int32_t>(set.literals[0].offset);
}

float ScriptLiteralValue(const ScriptLiteral& literal, const FrameRate& rate) {
    const float ratio = rate.ratio;
    switch (literal.kind) {
    case ScriptLiteralKind::smoothingDivisor: {
        const float share = 1.0f - std::pow(1.0f - 1.0f / literal.stock, ratio);
        return share > 0.0f ? 1.0f / share : literal.stock;
    }
    case ScriptLiteralKind::smoothingFactor:
        return 1.0f - std::pow(1.0f - literal.stock, ratio);
    case ScriptLiteralKind::decayFactor:
        return std::pow(literal.stock, ratio);
    case ScriptLiteralKind::rateDivisor:
        return literal.stock / ratio;
    case ScriptLiteralKind::frameStep:
        return literal.stock * ratio;
    case ScriptLiteralKind::frameTick:
        return literal.stock * static_cast<float>(rate.ticks);
    case ScriptLiteralKind::timerThreshold: {
        // At 30 FPS the timer passes on the first whole frame beyond the
        // threshold; half a frame short of that period it passes on the
        // frame nearest it, which at 30 FPS is the stock threshold.
        const float frames = std::floor(literal.stock / kOriginalFrameMilliseconds) + 1.0f;
        return frames * kOriginalFrameMilliseconds
             - 0.5f * kOriginalFrameMilliseconds * ratio;
    }
    }
    return literal.stock;
}

void WriteScriptLiteral(uint8_t* at, const ScriptLiteral& literal, float value) {
    switch (literal.operandType) {
    case kScriptInt8: {
        const long rounded = std::clamp(std::lround(value), -128L, 127L);
        at[kScriptLiteralValueOffset] = static_cast<uint8_t>(static_cast<int8_t>(rounded));
        break;
    }
    case kScriptLocalVariable:
        reinterpret_cast<int32_t*>(kMissionLocalVariables)[literal.operandLocal] =
            static_cast<int32_t>(std::lround(value));
        break;
    default:
        std::memcpy(at + kScriptLiteralValueOffset, &value, sizeof(value));
        break;
    }
}

void LogScriptCheck(const ScriptLiteralSet& set) {
    std::string message(set.fixName);
    message += ": ";
    message += set.scriptDescription;
    if (!set.matches) {
        message += " does not match the stock script; left alone.";
    } else if (set.shift != 0) {
        message += " recognised at a moved offset.";
    } else {
        message += " recognised.";
    }
    Log(message.c_str());
}

// Checks a newly seen image once: at the stock offsets, then, for a mission
// with a pattern, wherever the pattern finds the literal.
void CheckScriptImage(ScriptLiteralSet& set, uintptr_t base) {
    set.checkedBase = base;
    set.shift = 0;
    const auto* image = reinterpret_cast<const uint8_t*>(base);
    set.matches = ScriptLiteralsMatch(image, set);
    if (!set.matches && set.image == ScriptImage::mission && set.pattern) {
        set.shift = LocateMovedLiteral(image, set);
        set.matches = set.shift != 0 && LiteralsEnd(set) <= kMissionBlockSize
                   && ScriptLiteralsMatch(image, set);
    }
    LogScriptCheck(set);
}

void UpdateScriptLiteralSet(ScriptLiteralSet& set, const FrameRate& rate) {
    __try {
        const uintptr_t script = FindRunningScript(set.scriptName);
        if (!script) {
            return;
        }
        const auto base = *reinterpret_cast<const uintptr_t*>(script + kRunningScriptBaseIp);
        if (!base || !ImageIsLive(set, script, base)) {
            return;
        }
        if (base != set.checkedBase) {
            CheckScriptImage(set, base);
        }
        // Checked every frame, so memory reused for another script is never
        // written into.
        if (!set.matches
            || !ScriptLiteralsMatch(reinterpret_cast<const uint8_t*>(base), set)) {
            return;
        }
        for (size_t i = 0; i < set.count; ++i) {
            const ScriptLiteral& literal = set.literals[i];
            WriteScriptLiteral(reinterpret_cast<uint8_t*>(base) + literal.offset + set.shift,
                               literal, ScriptLiteralValue(literal, rate));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        set.checkedBase = 0;
    }
}

void UpdateScriptLiterals() {
    const FrameRate rate{TimeStepRatio(), FrameTick(kFrameTickScripts)};
    for (auto& set : g_literalSets) {
        if (set.enabled) {
            UpdateScriptLiteralSet(set, rate);
        }
    }
}

bool InstallLiteralSet(ScriptLiteralSet& set) {
    if (!InstallFrameHook(set.fixName)) {
        return false;
    }
    AddFrameCallback(&UpdateScriptLiterals);
    set.enabled = true;
    return true;
}

} // namespace

bool InstallParachuteFlightFix() {
    if (!InstallLiteralSet(g_literalSets[0])) {
        return false;
    }
    Log("Installed parachute steering and easing at the original rate.");
    return true;
}

bool InstallBurglaryNoiseFix() {
    if (!InstallLiteralSet(g_literalSets[1]) || !InstallLiteralSet(g_literalSets[2])) {
        return false;
    }
    Log("Installed the burglary and Home Invasion noise meters at the original "
        "rate.");
    return true;
}

bool InstallMissionScriptsFix() {
    for (size_t i = 3; i < g_literalSets.size(); ++i) {
        if (!InstallLiteralSet(g_literalSets[i])) {
            return false;
        }
    }
    Log("Installed the Small Town Bank hostages, the Tanker Commander and "
        "Zeroing In chase speeds, the Catalyst throw meter and the Interdiction "
        "helicopters at the original rate.");
    return true;
}

} // namespace hff::scripts
