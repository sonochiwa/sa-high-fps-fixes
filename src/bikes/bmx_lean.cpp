#include "bikes/bmx_lean.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/frame_steps.h"
#include "game/sites/bikes.h"

#include <array>
#include <cstdint>

namespace hff::bikes {

namespace {

SitePatch g_bmxSprintLeanPatch{};
std::array<SitePatch, 4> g_bmxLeanPatches{};

// The base, the writable `0.95`, is already in st(0) when these run. The
// registers `_CIpow` writes are kept across it.
__declspec(naked) void BmxLeanLeftDecayThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        pushfd
        push eax
        push ecx
        push edx
        call kPow
        pop edx
        pop ecx
        pop eax
        popfd
        fmul dword ptr [esi + 0x654]
        ret
    }
}

__declspec(naked) void BmxLeanFwdDecayThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        pushfd
        push eax
        push ecx
        push edx
        call kPow
        pop edx
        pop ecx
        pop eax
        popfd
        fmul dword ptr [esi + 0x658]
        ret
    }
}

} // namespace

bool InstallBmxSprintLeanFix() {
    if (!InstallBranch(g_bmxSprintLeanPatch, kBmxSprintLeanDecay,
                       &FrameStepDecayThunk, kExpectedFrameStepDecay.data(), 6,
                       0xE8)) {
        Log("BMX sprint lean fix skipped: CBmx::ProcessControl bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled BMX sprint lean decay.");
    return true;
}

bool InstallBmxLeanSettleFix() {
    PatchSet patches("BMX lean settle fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kBmxLeanLeftDecayA, &BmxLeanLeftDecayThunk,
         kExpectedBmxLeanLeftDecay.data()},
        {kBmxLeanFwdDecayA, &BmxLeanFwdDecayThunk,
         kExpectedBmxLeanFwdDecay.data()},
        {kBmxLeanLeftDecayB, &BmxLeanLeftDecayThunk,
         kExpectedBmxLeanLeftDecay.data()},
        {kBmxLeanFwdDecayB, &BmxLeanFwdDecayThunk,
         kExpectedBmxLeanFwdDecay.data()},
    };
    for (size_t i = 0; i < g_bmxLeanPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_bmxLeanPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE8),
                g_bmxLeanPatches[i])) {
            Log("BMX lean settle fix skipped: CBmx::ProcessDrivingAnims bytes "
                "do not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled BMX rider lean settle.");
    return true;
}

} // namespace hff::bikes
