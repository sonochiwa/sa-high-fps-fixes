#include "timers/frame_time_carry.h"

#include "core/conflicts.h"
#include "core/fixes.h"
#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/frame_hook.h"
#include "game/sites/timers.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdio>

namespace hff::timers {

namespace {

// The last result of a site, by the frame and the value it was for.
struct FrameResult {
    uint32_t frame;
    double value;
    int32_t result;
};

std::array<SitePatch, kStatTruncSites.size()> g_statTruncPatches{};
// One carry per call site, indexed the same as `kStatTruncSites`.
std::array<float, kStatTruncSites.size()> g_statTruncCarries{};
std::array<FrameResult, kStatTruncSites.size()> g_statTruncResults{};

int32_t __cdecl TruncateStatWithCarry(double value, uintptr_t site) {
    __try {
        size_t index = kStatTruncSites.size();
        for (size_t i = 0; i < kStatTruncSites.size(); ++i) {
            // The return address the wrapper sees is the instruction after the
            // call, and the table holds the address of the call itself.
            if (kStatTruncSites[i].address + 5 == site) {
                index = i;
                break;
            }
        }
        if (index == kStatTruncSites.size() || !std::isfinite(value)) {
            return static_cast<int32_t>(value);
        }

        // Many sites run once per entity per frame: every AI car, every
        // driver, every burning vehicle hands over the same frame time. They
        // all get the whole number the first one got, so the carry advances
        // once a frame and each entity counts the time the frame took,
        // instead of some of them taking the rounding down every frame.
        FrameResult& last = g_statTruncResults[index];
        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        if (last.frame == frame && last.value == value) {
            return last.result;
        }
        const double total = value + g_statTruncCarries[index];
        const double whole = std::trunc(total);
        const double remainder = total - whole;
        g_statTruncCarries[index] = std::isfinite(remainder)
                                  ? static_cast<float>(remainder)
                                  : 0.0f;
        last = {frame, value, static_cast<int32_t>(whole)};
        return last.result;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int32_t>(value);
    }
}

// Stands in for `_ftol` at the stat sites. `_ftol` takes the value in st(0),
// pops it and returns the integer in edx:eax; this does the same, with the
// fraction kept. The return address doubles as the site identifier, which is
// why the wrapper reads it out of the frame rather than taking a parameter.
// ecx is preserved because the compiled helper is free to clobber it.
__declspec(naked) void StatTruncCarryThunk() {
    __asm {
        push ebp
        mov ebp, esp
        push ecx
        mov eax, [ebp + 4]
        push eax
        sub esp, 8
        fstp qword ptr [esp]
        call TruncateStatWithCarry
        add esp, 12
        pop ecx
        cdq
        pop ebp
        ret
    }
}

bool InstallTruncCarryGroup(uint8_t group, const char* what) {
    PatchSet patches(what);
    size_t installed = 0;
    for (size_t i = 0; i < kStatTruncSites.size(); ++i) {
        if (kStatTruncSites[i].group != group) {
            continue;
        }
        if (!patches.Track(
                RepointCall(g_statTruncPatches[i],
                            kStatTruncSites[i].address, kFtol,
                            &StatTruncCarryThunk),
                g_statTruncPatches[i])) {
            char skipped[128];
            std::snprintf(skipped, sizeof(skipped),
                          "%s skipped: executable bytes do not match GTA SA 1.0 US.",
                          what);
            Log(skipped);
            return false;
        }
        g_statTruncCarries[i] = 0.0f;
        g_statTruncResults[i] = {0xFFFFFFFFu, 0.0, 0};
        ++installed;
    }
    char message[128];
    std::snprintf(message, sizeof(message),
                  "Installed fractional carry in %zu %s truncations.",
                  installed, what);
    patches.Commit();
    Log(message);
    return true;
}

bool CallsFtol(uintptr_t address) {
    __try {
        return *reinterpret_cast<const uint8_t*>(address) == 0xE8
            && address + 5 + static_cast<uintptr_t>(
                   *reinterpret_cast<const int32_t*>(address + 1)) == kFtol;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool g_scriptTimerPending{};

// SilentPatch and other plugins can load after this one has started, so the
// script timer call is examined on the first frame after this plugin has
// installed everything, once every plugin has patched the executable, and
// taken only while it is still the game's own.
void InstallScriptTimerCarry() {
    if (!g_scriptTimerPending || !FixesInstalled()) {
        return;
    }
    g_scriptTimerPending = false;
    if (!CallsFtol(kScriptTimerTruncCall)) {
        Log("Script timers left as they are: another plugin, such as "
            "SilentPatch, already handles their frame time.");
        return;
    }
    InstallTruncCarryGroup(kTruncGroupScript, "script timer");
}

} // namespace

bool InstallSkillProgressFix() {
    return InstallTruncCarryGroup(kTruncGroupStats, "stat counter");
}

bool InstallStuntCountersFix() {
    return InstallTruncCarryGroup(kTruncGroupStunt, "stunt counter");
}

bool InstallUpsideDownTimerFix() {
    return InstallTruncCarryGroup(kTruncGroupUpsideDown, "upside down car timer");
}

bool InstallTaskTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupTask, "ped task timer");
}

bool InstallVehicleTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupVehicle, "vehicle timer");
}

bool InstallIdleCameraTimerFix() {
    return InstallTruncCarryGroup(kTruncGroupIdleCam, "idle camera timer");
}

bool InstallHudTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupHud, "HUD timer");
}

bool InstallBurnTimersFix() {
    return InstallTruncCarryGroup(kTruncGroupBurn, "vehicle burn timer");
}

bool InstallGangWarTimerFix() {
    return InstallTruncCarryGroup(kTruncGroupWorld, "gang war timer");
}

bool InstallExplosionFuelTimerCarry() {
    return InstallTruncCarryGroup(kTruncGroupExplosion, "explosion fuel timer");
}

bool InstallPlaneDamageWaveCarry() {
    return InstallTruncCarryGroup(kTruncGroupPlaneDamage, "plane engine sputter");
}

bool InstallMissionTimersFix() {
    if (!InstallTruncCarryGroup(kTruncGroupMission, "mission countdown")) {
        return false;
    }
    // Never written back over another plugin's carry at the same call.
    ExemptFromConflictGuard(kScriptTimerTruncCall);
    if (InstallFrameHook("Mission timer fix")) {
        g_scriptTimerPending = true;
        AddFrameCallback(&InstallScriptTimerCarry);
    }
    return true;
}

} // namespace hff::timers
