#include "camera/drunk_camera.h"

#include "camera/aim_camera.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/camera.h"

namespace hff::camera {

namespace {

SitePatch g_drunkCameraPhasePatch{};

// The sway is a rotation rate, so the per-frame step becomes a per-second one.
// The 5.0 is reached through the game constant so a mod that retunes it keeps
// working, and at 30 FPS the ratio is one and the step is the stock 5.0 exactly.
// The site runs inside CCamera::Process, where the aim guard may have pinned
// the timestep, so the step takes the real frame duration.
float __cdecl GetDrunkCameraPhaseStep() {
    return ReadGameFloat(kDrunkCameraPhaseStep, 5.0f)
         * (UnguardedTimeStep() / kOriginalTimeStep);
}

// Replaces the bare `fadd` that advanced the drunk camera sway by a fixed step
// every rendered frame. The helper returns the scaled step in st(0) and the
// add pops it back off, so the stack is left holding the new phase exactly as
// the original instruction left it.
__declspec(naked) void DrunkCameraPhaseThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetDrunkCameraPhaseStep
        pop edx
        pop ecx
        pop eax
        popfd
        faddp st(1), st
        jmp kDrunkCameraPhaseReturn
    }
}

} // namespace

bool InstallDrunkCameraShakeFix() {
    if (!InstallJump(g_drunkCameraPhasePatch, kDrunkCameraPhase,
                     &DrunkCameraPhaseThunk, kExpectedDrunkCameraPhase)) {
        Log("Drunk camera shake fix skipped: CCamera::Process sway bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed a real-time drunk camera sway rate.");
    return true;
}

} // namespace hff::camera
