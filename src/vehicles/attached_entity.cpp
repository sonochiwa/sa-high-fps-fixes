#include "vehicles/attached_entity.h"

#include "camera/aim_camera.h"
#include "core/log.h"
#include "core/patch.h"
#include "game/sites/vehicles.h"

namespace hff::vehicles {

namespace {

using camera::UnguardedTimeStep;

SitePatch g_attachedEntitySpeedPatch{};

// Leaves the FPU stack exactly as the replaced block did: the reciprocal goes
// to the same slot and the three deltas underneath it are untouched. The site
// can run inside the guarded CCamera::Process, so the divisor is the unguarded
// timestep, for the same reason as the follow cameras. The helper leaves it in
// st(0) and `fdivr` then divides the constant by it, matching the original
// operand order. Every push is balanced before the store, so [esp + 0x0C]
// still names the slot the original instruction wrote.
__declspec(naked) void AttachedEntitySpeedThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call UnguardedTimeStep
        pop edx
        pop ecx
        pop eax
        popfd
        fdivr dword ptr ds:[0x00858624]
        fstp dword ptr [esp + 0x0C]
        jmp kAttachedEntitySpeedReturn
    }
}

} // namespace

bool InstallAttachedEntitySpeedFix() {
    if (!InstallJump(g_attachedEntitySpeedPatch, kAttachedEntitySpeed,
                     &AttachedEntitySpeedThunk, kExpectedAttachedSpeedClamp)) {
        Log("Attached entity speed fix skipped: CPhysical::PositionAttachedEntity "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real timestep in the attached entity speed.");
    return true;
}

} // namespace hff::vehicles
