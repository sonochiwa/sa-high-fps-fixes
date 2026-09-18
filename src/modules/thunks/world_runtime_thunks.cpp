#include "modules/modules.h"

namespace hff {

// The x87 stack is empty here and the replaced load is reproduced before the
// return. The flags matter: a `test al,al` above and a `jge` below straddle
// this point.
__declspec(naked) void MapWheelSampleThunk() {
    __asm {
        pushfd
        pushad
        call SampleMapWheelEdges
        popad
        popfd
        fld dword ptr ds:[0x008653F4]
        ret
    }
}

// Entered with the elapsed pause-mode milliseconds in eax, replacing
// `cmp eax,14h / jbe skip`. Proceeds when the tick has passed, as before, or on
// a wheel notch.
__declspec(naked) void MapZoomInGateThunk() {
    __asm {
        cmp eax, 0x14
        ja proceed
        cmp byte ptr [g_mapWheelUpEdge], 0
        jne proceed
        jmp kMapZoomInSkip
    proceed:
        jmp kMapZoomInProceed
    }
}

__declspec(naked) void MapZoomOutGateThunk() {
    __asm {
        cmp eax, 0x14
        ja proceed
        cmp byte ptr [g_mapWheelDownEdge], 0
        jne proceed
        jmp kMapZoomOutSkip
    proceed:
        jmp kMapZoomOutProceed
    }
}

// Entered with the freshly drawn random number in eax, replacing
// `test al,<mask> / jne skip`. `FireEventTick` clobbers eax, ecx and edx, all
// of which the rand call immediately above already clobbered, so only the draw
// itself has to be preserved. `pop` leaves the flags alone, so the mask test
// after it is the original test unchanged.
__declspec(naked) void FireVehicleGateThunk() {
    __asm {
        push eax
        push 0
        call FireEventTick
        add esp, 4
        test eax, eax
        pop eax
        je skipped
        test al, 0x1F
        jne skipped
        jmp kFireVehicleResume
    skipped:
        jmp kFireVehicleSkip
    }
}

__declspec(naked) void FireSpreadGateThunk() {
    __asm {
        push eax
        push 1
        call FireEventTick
        add esp, 4
        test eax, eax
        pop eax
        je skipped
        test al, 0x7F
        jne skipped
        jmp kFireSpreadResume
    skipped:
        jmp kFireSpreadSkip
    }
}

__declspec(naked) void FireMergeGateThunk() {
    __asm {
        push eax
        push 2
        call FireEventTick
        add esp, 4
        test eax, eax
        pop eax
        je skipped
        test al, 0x0F
        jne skipped
        jmp kFireMergeResume
    skipped:
        jmp kFireMergeSkip
    }
}

// Replaces the shift loop's two set-up instructions. `ebx` is the pad and is
// callee-saved across the helper; eax, ecx and edx are dead here, and the two
// the loop needs are rebuilt before jumping into it.
__declspec(naked) void DrunkSteerShiftThunk() {
    __asm {
        push kFrameTickDrunkSteer
        call FrameTick
        add esp, 4
        test eax, eax
        je skipped
        lea eax, [ebx + 0x72]
        mov ecx, 9
        jmp kDrunkSteerShiftResume
    skipped:
        jmp kDrunkSteerShiftSkip
    }
}

// Replaces the twenty nine bytes of integer arithmetic. The original clobbered
// eax and edx and left the new total in eax, which the instruction at the
// return address overwrites straight away, so nothing has to be handed back.
// The exercise rate sat at `[esp+8]` before the call, which is `[esp+20]` once
// the return address and the two saved registers are on the stack.
__declspec(naked) void FatCounterThunk() {
    __asm {
        push ecx
        push edx
        mov ecx, dword ptr [esp + 0x14]
        push ecx
        push eax
        call FatCounterAdd
        add esp, 8
        pop edx
        pop ecx
        ret
    }
}

// `ang` is in st(0), the selected chassis apply rate is in st(1). Contact
// impulses can feed the same delta once per rendered frame, so normalize this
// chassis-only input to the original frame duration. The non-chassis path is
// deliberately left in the game for firetruck ladder movement.
__declspec(naked) void DoorForceChassisThunk() {
    __asm {
        fmul st, st(1)
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fadd dword ptr [esi + 0x14]
        jmp kDoorForceChassisReturn
    }
}

// `_CIpow` takes the base in st(1) and the exponent in st(0). Raising the
// stock damping factor to the timestep ratio preserves the fraction left over
// across frames; unlike the linear approximation in the source patch, this is
// stable at both very short and long timesteps.
__declspec(naked) void DoorDampingFiretruckThunk() {
    __asm {
        fld dword ptr ds:[0x00872314]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        jmp kDoorDampingFiretruckReturn
    }
}

__declspec(naked) void DoorDampingOtherThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmul dword ptr [esi + 0x14]
        fstp dword ptr [esi + 0x14]
        jmp kDoorDampingOtherReturn
    }
}

// Scale the angular velocity before the original addition integrates it.
__declspec(naked) void DoorIntegrationThunk() {
    __asm {
        fld dword ptr [esi + 0x14]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        mov ecx, ebx
        jmp kDoorIntegrationReturn
    }
}

// The two replaced instructions store the target and drop the leftover
// accumulator, so both are reproduced before the filter runs. After `pushfd`,
// `pushad` and one argument push, the caller frame starts 0x28 bytes up, which
// puts the target slot at `esp + 0x3C`.
__declspec(naked) void BikeLeanTargetThunk() {
    __asm {
        fstp dword ptr [esp + 0x14]
        fstp st(0)
        pushfd
        pushad
        push esi
        lea eax, [esp + 0x3C]
        push eax
        call FilterBikeLeanTarget
        add esp, 8
        popad
        popfd
        jmp kBikeLeanTargetReturn
    }
}

__declspec(naked) void ObjectFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov cl, byte ptr [esi + 0xB8]
        test eax, eax
        jz noTick
        inc cl
    noTick:
        jmp kObjectFakePhysicsReturn
    }
}

__declspec(naked) void CarFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov dl, byte ptr [esi + 0xB8]
        test eax, eax
        jz noTick
        inc dl
    noTick:
        jmp kCarFakePhysicsReturn
    }
}

__declspec(naked) void BikeFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov cl, byte ptr [esi + 0xB8]
        test eax, eax
        jz noTick
        inc cl
    noTick:
        jmp kBikeFakePhysicsReturn
    }
}

__declspec(naked) void TrailerFakePhysicsThunk() {
    __asm {
        pushfd
        pushad
        call ShouldTickFakePhysicsCounter
        mov dword ptr [esp + 28], eax
        popad
        popfd
        mov bl, byte ptr [esi + 0xB8]
        test eax, eax
        jz noTick
        inc bl
    noTick:
        jmp kTrailerFakePhysicsReturn
    }
}

} // namespace hff
