#include "modules/modules.h"

namespace hff {

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

// Replaces the store of the stepped value. `edx` carries what the original
// would have written and `esi` the player info, whose old value is still in
// place, so the helper can recover the step the game chose.
__declspec(naked) void MoneyStepThunk() {
    __asm {
        pushfd
        pushad
        push edx
        lea eax, [esi + 0xBC]
        push eax
        call ApplyMoneyStep
        add esp, 8
        popad
        popfd
        jmp kMoneyStepReturn
    }
}

// The clamp goes in where the sibling branch does its own, between the divide
// and the addition of the climbed entity's speed. `edi` holds the ped and `ecx`
// the address `CVector::operator+=` is about to be called on, both preserved by
// `pushad`, and the replaced instructions are reproduced around it.
__declspec(naked) void ClimbSpeedClampThunk() {
    __asm {
        add esp, 0x0C
        pushfd
        pushad
        lea eax, [edi + 0x44]
        push eax
        call ClampClimbMoveSpeed
        add esp, 4
        popad
        popfd
        lea edx, [esp + 0x48]
        push edx
        call kVectorAddAssign
        jmp kClimbSpeedClampReturn
    }
}

// The impulse is built with the original timestep so the comparison against
// `mass * moveSpeed.z` keeps its 30 FPS meaning, and the copy written into the
// output vector is scaled back to the current frame. `esi` addresses the
// buoyancy state, `eax` the entity and `ecx` the output vector; none are
// touched here.
__declspec(naked) void BuoyancyThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xBC]
        fmul dword ptr [esi + 0x6C]
        add esp, 0x0C
        fmul g_originalTimeStepValue
        fld st(0)
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fstp dword ptr [ecx + 8]
        fld dword ptr [eax + 0x8C]
        fmul dword ptr [eax + 0x4C]
        fld st(1)
        fmul dword ptr ds:[0x00858B90]
        jmp kBuoyancyThresholdReturn
    }
}

// The reduced impulse taken when the entity is already rising fast. It is
// computed from the original timestep value above, so it is scaled here too.
// The replaced span runs to the end of the function, so this returns directly.
__declspec(naked) void BuoyancyClampedStoreThunk() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fstp dword ptr [ecx + 8]
        mov al, 1
        pop esi
        add esp, 0x0C
        ret 0x0C
    }
}

// Wraps the single call to `CTaskSimpleSwim::ProcessSwimmingResistance`. The
// two replaced instructions set up its arguments, so they are reproduced
// between the scale and the restore. `esi` is the ped and `edi` the task, both
// preserved by `pushad`.
__declspec(naked) void SwimResistanceThunk() {
    __asm {
        pushfd
        pushad
        push esi
        call ScaleSwimAnimShift
        add esp, 4
        popad
        popfd

        push esi
        mov ecx, edi
        call kProcessSwimmingResistance

        pushfd
        pushad
        push esi
        call RestoreSwimAnimShift
        add esp, 4
        popad
        popfd
        jmp kSwimResistanceReturn
    }
}

__declspec(naked) void AimingRifleWalkThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetAimingRifleWalkStep
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kAimingRifleWalkReturn
    }
}

__declspec(naked) void SkimmerResistanceThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetSkimmerResistance
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kSkimmerResistanceReturn
    }
}

__declspec(naked) void BurnoutThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetBurnoutWheelSpeed
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kBurnoutReturn
    }
}

__declspec(naked) void HeliRotorSlowThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetHeliRotorSlowStep
        pop edx
        pop ecx
        pop eax
        popfd
        faddp st(1), st
        jmp kHeliRotorSlowReturn
    }
}

__declspec(naked) void HeliRotorFastThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetHeliRotorFastStep
        pop edx
        pop ecx
        pop eax
        popfd
        faddp st(1), st
        jmp kHeliRotorFastReturn
    }
}

// Wheels of a simple (on-rails) AI car turn by `-dot(forward, moveSpeed) / radius`
// once per frame with no timestep. The physics wheels integrate the same kind of
// speed as `GetTimeStep() * m_wheelSpeed`, so the raw timestep is applied here
// too, exactly as FramerateVigilante does, rather than the ratio: at 30 FPS the
// stock rotation is 1.67 times too slow next to a physics car and this brings
// the two into step at every frame rate.
__declspec(naked) void RailWheelSpinThunk0() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fadd dword ptr [esi + 0x828]
        jmp kRailWheelSpinReturn0
    }
}

__declspec(naked) void RailWheelSpinThunk1() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fadd dword ptr [esi + 0x82C]
        jmp kRailWheelSpinReturn1
    }
}

__declspec(naked) void RailWheelSpinThunk2() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fadd dword ptr [esi + 0x830]
        jmp kRailWheelSpinReturn2
    }
}

__declspec(naked) void RailWheelSpinThunk3() {
    __asm {
        fmul dword ptr ds:[0x00B7CB5C]
        fadd dword ptr [esi + 0x834]
        jmp kRailWheelSpinReturn3
    }
}

__declspec(naked) void PedPushCarThunk() {
    __asm {
        pushfd
        pushad
        push esi
        lea eax, [esp + 0x28]
        push eax
        call ScalePedPushCarForce
        add esp, 8
        popad
        popfd
        mov edx, dword ptr [esp + 0x20]
        mov eax, dword ptr [esp + 0x24]
        jmp kPedPushCarReturn
    }
}

// Each site is `mov r8, [esi+0xB8]` followed by `inc r8`, and every one of them
// overwrites AL on the very next instruction, so returning the tick flag in EAX
// is safe.
// `m_fMovingSpeed` is per-frame distance, so it is rescaled into the units the
// fixed limit was written for before the comparison. At 30 FPS this multiplies
// by exactly one.
__declspec(naked) void CarRestThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xD4]
        fmul g_originalTimeStepValue
        fdiv dword ptr ds:[0x00B7CB5C]
        jmp kCarRestThresholdReturn
    }
}

__declspec(naked) void BikeRestThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xD4]
        fmul g_originalTimeStepValue
        fdiv dword ptr ds:[0x00B7CB5C]
        jmp kBikeRestThresholdReturn
    }
}

__declspec(naked) void TrailerRestThresholdThunk() {
    __asm {
        fld dword ptr [esi + 0xD4]
        fmul g_originalTimeStepValue
        fdiv dword ptr ds:[0x00B7CB5C]
        jmp kTrailerRestThresholdReturn
    }
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

} // namespace hff
