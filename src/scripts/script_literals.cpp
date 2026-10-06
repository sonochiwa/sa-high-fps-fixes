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
// burglary mission keeps a fixed share of its noise meter a frame, so above
// 30 FPS the meter drains before it can fill. Before the scripts run each
// frame, the running script's literals are set to the values that give the
// 30 FPS steps over the current timestep; at 30 FPS they are the stock values.
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
    bool enabled;
    // The image last checked and how far its literals sit from their stock
    // offsets, so the result is logged once per image.
    uintptr_t checkedBase;
    int32_t shift;
    bool matches;
};

std::array<ScriptLiteralSet, 2> g_literalSets{{
    {kParachuteScriptName, "Parachute flight fix", "player_parachute.scm",
     kParachuteLiterals, std::size(kParachuteLiterals), ScriptImage::streamed},
    {kBurglaryScriptName, "Burglary noise fix", "the burglary mission",
     kBurglaryLiterals, std::size(kBurglaryLiterals), ScriptImage::mission},
}};

// The end of the farthest literal of a set, which the image must reach.
size_t LiteralsEnd(const ScriptLiteralSet& set) {
    size_t end = 0;
    for (size_t i = 0; i < set.count; ++i) {
        end = (std::max)(end, set.literals[i].offset + set.shift
                                  + kScriptLiteralValueOffset + sizeof(float));
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
// compared: they carry the previous frame's rewrite.
bool ScriptLiteralsMatch(const uint8_t* base, const ScriptLiteralSet& set) {
    for (size_t i = 0; i < set.count; ++i) {
        const ScriptLiteral& literal = set.literals[i];
        const uint8_t* at = base + literal.offset + set.shift;
        uint16_t opcode{};
        uint16_t localVar{};
        std::memcpy(&opcode, at, sizeof(opcode));
        std::memcpy(&localVar, at + 3, sizeof(localVar));
        if (opcode != literal.opcode || at[2] != 3 || localVar != literal.localVar
            || at[5] != 6) {
            return false;
        }
    }
    return true;
}

// Where the noise meter's literal is in a mission edited so that it moved:
// the offset from its stock place, or 0 when the pattern is not found exactly
// once.
int32_t LocateBurglaryLiteral(const uint8_t* base) {
    const uint8_t* found = nullptr;
    for (size_t i = 0; i + kBurglaryNoisePattern.size() <= kMissionBlockSize; ++i) {
        if (std::memcmp(base + i, kBurglaryNoisePattern.data(),
                        kBurglaryNoisePattern.size()) != 0) {
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
    return static_cast<int32_t>(found - base + kBurglaryNoisePatternLiteral)
         - static_cast<int32_t>(kBurglaryLiterals[0].offset);
}

float ScriptLiteralValue(const ScriptLiteral& literal, float ratio) {
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
    }
    return literal.stock;
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

// Checks a newly seen image once: at the stock offsets, then, for the
// burglary mission, wherever its pattern finds the literal.
void CheckScriptImage(ScriptLiteralSet& set, uintptr_t base) {
    set.checkedBase = base;
    set.shift = 0;
    const auto* image = reinterpret_cast<const uint8_t*>(base);
    set.matches = ScriptLiteralsMatch(image, set);
    if (!set.matches && set.image == ScriptImage::mission) {
        set.shift = LocateBurglaryLiteral(image);
        set.matches = set.shift != 0 && LiteralsEnd(set) <= kMissionBlockSize
                   && ScriptLiteralsMatch(image, set);
    }
    LogScriptCheck(set);
}

void UpdateScriptLiteralSet(ScriptLiteralSet& set, float ratio) {
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
            const float value = ScriptLiteralValue(literal, ratio);
            std::memcpy(reinterpret_cast<uint8_t*>(base) + literal.offset + set.shift
                            + kScriptLiteralValueOffset,
                        &value, sizeof(value));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        set.checkedBase = 0;
    }
}

void UpdateScriptLiterals() {
    const float ratio = TimeStepRatio();
    for (auto& set : g_literalSets) {
        if (set.enabled) {
            UpdateScriptLiteralSet(set, ratio);
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
    if (!InstallLiteralSet(g_literalSets[1])) {
        return false;
    }
    Log("Installed the burglary noise meter at the original rate.");
    return true;
}

} // namespace hff::scripts
