#include "modules/modules.h"

// Some SCM scripts step a value once a frame with 30 FPS steps written into
// their literals. The player's freefall and canopy flight in
// `player_parachute.scm` turn by a share of the roll a frame, and ease roll,
// pitch, horizontal speed and sink rate towards their targets by a fixed share
// a frame, so above 30 FPS the jumper spins and settles many times faster. The
// burglary mission keeps a fixed share of its noise meter a frame, so above
// 30 FPS the meter drains before it can fill. Before the scripts run each
// frame, the running script's literals are set to the values that give the
// 30 FPS steps over the current timestep; at 30 FPS they are the stock values.

namespace hff {

bool g_parachuteFlight = false;
bool g_burglaryNoise = false;

namespace {

struct ScriptLiteralSet {
    const char* scriptName;
    const char* fixName;
    const char* scriptDescription;
    const ScriptLiteral* literals;
    size_t count;
    const bool* enabled;
    uintptr_t checkedBase;
    bool matches;
};

std::array<ScriptLiteralSet, 2> g_literalSets{{
    {kParachuteScriptName, "Parachute flight fix", "player_parachute.scm",
     kParachuteLiterals, std::size(kParachuteLiterals), &g_parachuteFlight},
    {kBurglaryScriptName, "Burglary noise fix", "the burglary mission",
     kBurglaryLiterals, std::size(kBurglaryLiterals), &g_burglaryNoise},
}};

// The instructions around each literal, checked once per script image so a
// modified script is left alone. The values themselves are not compared:
// they carry the previous frame's rewrite.
bool ScriptLiteralsMatch(const uint8_t* base, const ScriptLiteralSet& set) {
    for (size_t i = 0; i < set.count; ++i) {
        const ScriptLiteral& literal = set.literals[i];
        const uint8_t* at = base + literal.offset;
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
    message += set.matches ? " recognised."
                           : " does not match the stock script; left alone.";
    Log(message.c_str());
}

void UpdateScriptLiteralSet(ScriptLiteralSet& set, float ratio) {
    __try {
        const uintptr_t script = FindRunningScript(set.scriptName);
        if (!script) {
            return;
        }
        const auto base = *reinterpret_cast<const uintptr_t*>(script + kRunningScriptBaseIp);
        if (!base) {
            return;
        }
        if (base != set.checkedBase) {
            set.checkedBase = base;
            set.matches = ScriptLiteralsMatch(reinterpret_cast<const uint8_t*>(base), set);
            LogScriptCheck(set);
        }
        if (!set.matches) {
            return;
        }
        for (size_t i = 0; i < set.count; ++i) {
            const ScriptLiteral& literal = set.literals[i];
            const float value = ScriptLiteralValue(literal, ratio);
            std::memcpy(reinterpret_cast<uint8_t*>(base) + literal.offset
                            + kScriptLiteralValueOffset,
                        &value, sizeof(value));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        set.checkedBase = 0;
    }
}

bool HookScriptsProcess(const char* fixName) {
    if (g_scriptsProcessPatch.installed
        || InstallJump(g_scriptsProcessPatch, kScriptsProcess,
                       &ScriptsProcessThunk, kExpectedScriptsProcess)) {
        return true;
    }
    std::string message(fixName);
    message += " skipped: CTheScripts::Process bytes do not match GTA SA 1.0 US.";
    Log(message.c_str());
    return false;
}

} // namespace

bool InstallParachuteFlightFix() {
    if (!HookScriptsProcess("Parachute flight fix")) {
        return false;
    }
    g_parachuteFlight = true;
    Log("Installed parachute steering and easing at the original rate.");
    return true;
}

bool InstallBurglaryNoiseFix() {
    if (!HookScriptsProcess("Burglary noise fix")) {
        return false;
    }
    g_burglaryNoise = true;
    Log("Installed the burglary noise meter at the original rate.");
    return true;
}

void UpdateScriptLiterals() {
    const float ratio = TimeStepRatio();
    for (auto& set : g_literalSets) {
        if (*set.enabled) {
            UpdateScriptLiteralSet(set, ratio);
        }
    }
}

} // namespace hff
