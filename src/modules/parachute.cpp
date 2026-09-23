#include "modules/modules.h"

// The player's freefall and canopy flight run in the streamed script
// `player_parachute.scm` once a frame, with 30 FPS steps written into its
// literals: the heading turns by a share of the roll a frame, and roll,
// pitch, horizontal speed and sink rate ease towards their targets by a
// fixed share a frame. Above 30 FPS the jumper spins and settles many times
// faster. Before the scripts run each frame, the running script's literals
// are set to the values that give the 30 FPS steps over the current
// timestep; at 30 FPS they are the stock values.

namespace hff {

bool g_parachuteFlight = false;

namespace {

uintptr_t FindRunningScript(const char* name) {
    const auto queueHead = *reinterpret_cast<const uintptr_t*>(kScriptQueueOperand);
    if (!queueHead) {
        return 0;
    }
    for (auto script = *reinterpret_cast<const uintptr_t*>(queueHead); script;
         script = *reinterpret_cast<const uintptr_t*>(script)) {
        if (ScriptNameMatches(reinterpret_cast<const char*>(
                                  script + kRunningScriptNameOffset),
                              name)) {
            return script;
        }
    }
    return 0;
}

// The instructions around each literal, checked once per script image so a
// modified script is left alone. The values themselves are not compared:
// they carry the previous frame's rewrite.
bool ParachuteScriptMatches(const uint8_t* base) {
    for (const auto& literal : kParachuteLiterals) {
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

float ParachuteLiteralValue(const ParachuteLiteral& literal, float ratio) {
    switch (literal.kind) {
    case ParachuteLiteralKind::smoothingDivisor: {
        const float share = 1.0f - std::pow(1.0f - 1.0f / literal.stock, ratio);
        return share > 0.0f ? 1.0f / share : literal.stock;
    }
    case ParachuteLiteralKind::smoothingFactor:
        return 1.0f - std::pow(1.0f - literal.stock, ratio);
    case ParachuteLiteralKind::rateDivisor:
        return literal.stock / ratio;
    }
    return literal.stock;
}

} // namespace

bool InstallParachuteFlightFix() {
    if (!g_scriptsProcessPatch.installed
        && !InstallJump(g_scriptsProcessPatch, kScriptsProcess,
                        &ScriptsProcessThunk, kExpectedScriptsProcess)) {
        Log("Parachute flight fix skipped: CTheScripts::Process bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    g_parachuteFlight = true;
    Log("Installed parachute steering and easing at the original rate.");
    return true;
}

void UpdateParachuteScript() {
    static uintptr_t checkedBase = 0;
    static bool checkedMatches = false;
    __try {
        const uintptr_t script = FindRunningScript(kParachuteScriptName);
        if (!script) {
            return;
        }
        const auto base = *reinterpret_cast<const uintptr_t*>(script + kRunningScriptBaseIp);
        if (!base) {
            return;
        }
        if (base != checkedBase) {
            checkedBase = base;
            checkedMatches = ParachuteScriptMatches(reinterpret_cast<const uint8_t*>(base));
            Log(checkedMatches
                    ? "Parachute flight fix: player_parachute.scm recognised."
                    : "Parachute flight fix: player_parachute.scm does not match "
                      "the stock script; left alone.");
        }
        if (!checkedMatches) {
            return;
        }
        const float ratio = TimeStepRatio();
        if (!(ratio > 0.0f)) {
            return;
        }
        for (const auto& literal : kParachuteLiterals) {
            const float value = ParachuteLiteralValue(literal, ratio);
            std::memcpy(reinterpret_cast<uint8_t*>(base) + literal.offset
                            + kParachuteLiteralValueOffset,
                        &value, sizeof(value));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        checkedBase = 0;
    }
}

} // namespace hff
