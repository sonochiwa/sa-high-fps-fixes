#include "camera/follow_camera.h"

#include "camera/aim_camera.h"
#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/camera.h"

namespace hff::camera {

namespace {

SitePatch g_followPedCameraPatch{};
SitePatch g_followCarCameraPatch{};

// Both leave exactly one value on the FPU stack, the divisor, which is what
// either arm of the replaced branch did. These two sites sit inside
// CCam::Process, which the aim camera fix wraps with a raised timestep, so the
// divisor comes from UnguardedTimeStep rather than straight from the global.
// The helper returns it in st(0), which is the one value the contract allows.
__declspec(naked) void FollowPedCameraRateThunk() {
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
        jmp kFollowPedCameraRateReturn
    }
}

__declspec(naked) void FollowCarCameraRateThunk() {
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
        jmp kFollowCarCameraRateReturn
    }
}

} // namespace

bool InstallFollowCameraRateFix() {
    PatchSet patches("Follow camera rate fix");
    if (!patches.Track(InstallJump(g_followPedCameraPatch,
                                   kFollowPedCameraRate,
                                   &FollowPedCameraRateThunk,
                                   kExpectedCameraRateClamp),
                       g_followPedCameraPatch)) {
        Log("Follow camera rate fix skipped: CCam bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(InstallJump(g_followCarCameraPatch,
                                   kFollowCarCameraRate,
                                   &FollowCarCameraRateThunk,
                                   kExpectedCameraRateClamp),
                       g_followCarCameraPatch)) {
        Log("Follow camera rate fix skipped: the car camera site does not match.");
        return false;
    }
    patches.Commit();
    Log("Installed a timestep-following rate in both follow cameras.");
    return true;
}

} // namespace hff::camera
