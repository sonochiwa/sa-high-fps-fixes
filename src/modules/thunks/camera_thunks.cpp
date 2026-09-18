#include "modules/modules.h"

namespace hff {

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

// Replaces the timestep load ahead of the aim camera FOV step. The site runs
// inside the guarded CCam::Process_AimWeapon call, where the global holds the
// pinned 1.0, so the step takes the real frame duration from UnguardedTimeStep
// instead. The helper leaves it in st(0), exactly as the replaced `fld` did,
// and the `fmul` that follows scales it into the per-frame step.
__declspec(naked) void AimWeaponFovStepThunk() {
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
        jmp kAimWeaponFovStepReturn
    }
}

// Leaves the FPU stack exactly as the replaced block did: the reciprocal goes
// to the same slot and the three deltas underneath it are untouched.
// Also reached from CCamera::Process, so the divisor is the unguarded timestep
// for the same reason as the follow cameras. The helper leaves it in st(0) and
// `fdivr` then divides the constant by it, matching the original operand order.
// Every push is balanced before the store, so [esp + 0x0C] still names the slot
// the original instruction wrote.
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

// The same contract as the follow camera thunks: the replaced block left the
// divisor alone on the FPU stack and so does this.
__declspec(naked) void AiAircraftSteerRateThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        jmp kAiAircraftSteerRateReturn
    }
}

} // namespace hff
