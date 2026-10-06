#include "scripts/script_objects.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/sites/scripts.h"

namespace hff::scripts {

namespace {

SitePatch g_scriptSlideObjectPatch{};
SitePatch g_scriptRotateObjectPatch{};

// `SLIDE_OBJECT` takes its target at ScriptParams[1..3] and its three per-frame
// movement rates at [4..6]. Scale only those rates, then reproduce the two
// overwritten loads which select the object from its handle.
__declspec(naked) void ScriptSlideObjectThunk() {
    __asm {
        movss xmm0, dword ptr ds:[0x00A43C88]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr ds:[0x00A43C88], xmm0

        movss xmm0, dword ptr ds:[0x00A43C8C]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr ds:[0x00A43C8C], xmm0

        movss xmm0, dword ptr ds:[0x00A43C90]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr ds:[0x00A43C90], xmm0

        mov eax, dword ptr ds:[0x00A43C78]
        mov ecx, dword ptr ds:[0x00B7449C]
        jmp kScriptSlideObjectReturn
    }
}

// `ROTATE_OBJECT` takes the object handle in ScriptParams[0], a direction in
// [1], and its per-frame angular rate in [2].
__declspec(naked) void ScriptRotateObjectThunk() {
    __asm {
        movss xmm0, dword ptr ds:[0x00A43C80]
        mulss xmm0, dword ptr ds:[0x00B7CB5C]
        divss xmm0, dword ptr [g_originalTimeStepValue]
        movss dword ptr ds:[0x00A43C80], xmm0

        mov ecx, dword ptr ds:[0x00A43C78]
        push ecx
        mov ecx, dword ptr ds:[0x00B7449C]
        jmp kScriptRotateObjectReturn
    }
}

} // namespace

bool InstallScriptObjectSlideFix() {
    if (!InstallJump(g_scriptSlideObjectPatch, kScriptSlideObject,
                     &ScriptSlideObjectThunk, kExpectedScriptSlideObject)) {
        Log("Script object slide fix skipped: SLIDE_OBJECT bytes do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled SLIDE_OBJECT script rate.");
    return true;
}

bool InstallScriptRotateObjectFix() {
    if (!InstallJump(g_scriptRotateObjectPatch, kScriptRotateObject,
                     &ScriptRotateObjectThunk, kExpectedScriptRotateObject)) {
        Log("Script object rotate fix skipped: ROTATE_OBJECT bytes do not "
            "match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a timestep-scaled ROTATE_OBJECT script rate.");
    return true;
}

} // namespace hff::scripts
