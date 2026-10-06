#include "world/falling_glass.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/world.h"

#include <array>
#include <cstdint>
#include <iterator>

namespace hff::world {

namespace {

std::array<SitePatch, 3> g_fallingGlassPatches{};

// The glass pane stores a displacement and two angular displacements in stack
// locals. All three are per-frame quantities, whereas the integration below is
// a plain add into the pane's position/orientation.
__declspec(naked) void FallingGlassMoveThunk() {
    __asm {
        movss xmm0, dword ptr [esp + 0x20]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [esp + 0x20], xmm0
        movss xmm0, dword ptr [esp + 0x24]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [esp + 0x24], xmm0
        movss xmm0, dword ptr [esp + 0x28]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [esp + 0x28], xmm0
        fld dword ptr [esp + 0x20]
        fadd dword ptr [esi]
        jmp kFallingGlassMoveReturn
    }
}

__declspec(naked) void FallingGlassTurnAThunk() {
    __asm {
        movss xmm0, dword ptr [eax]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax], xmm0
        movss xmm0, dword ptr [eax + 0x04]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax + 0x04], xmm0
        movss xmm0, dword ptr [eax + 0x08]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax + 0x08], xmm0
        mov ecx, dword ptr [eax]
        mov dword ptr [esp + 0x2C], ecx
        jmp kFallingGlassTurnAReturn
    }
}

__declspec(naked) void FallingGlassTurnBThunk() {
    __asm {
        movss xmm0, dword ptr [eax]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax], xmm0
        movss xmm0, dword ptr [eax + 0x04]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax + 0x04], xmm0
        movss xmm0, dword ptr [eax + 0x08]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr [eax + 0x08], xmm0
        mov edx, dword ptr [eax]
        mov dword ptr [esp + 0x38], edx
        jmp kFallingGlassTurnBReturn
    }
}

} // namespace

bool InstallFallingGlassFix() {
    PatchSet patches("Falling glass fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kFallingGlassMove, &FallingGlassMoveThunk,
         kExpectedFallingGlassMove.data()},
        {kFallingGlassTurnA, &FallingGlassTurnAThunk,
         kExpectedFallingGlassTurnA.data()},
        {kFallingGlassTurnB, &FallingGlassTurnBThunk,
         kExpectedFallingGlassTurnB.data()}
    };
    for (size_t i = 0; i < std::size(sites); ++i) {
        if (!patches.Track(
                InstallBranch(g_fallingGlassPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE9),
                g_fallingGlassPatches[i])) {
            Log("Falling glass fix skipped: FallingGlassPane::Update bytes do "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed timestep-scaled falling glass motion and rotation.");
    return true;
}

} // namespace hff::world
