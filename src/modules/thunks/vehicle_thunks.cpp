#include "modules/modules.h"

namespace hff {

// Each replaces the single arithmetic instruction that changed the wheel speed,
// entered by a `call` with the speed alone on the FPU stack and expected to
// leave the new speed there. The replaced instruction is six bytes, so the call
// fits with one byte of padding.
__declspec(naked) void WheelSpinDecelThunk() {
    __asm {
        fld dword ptr ds:[0x00858B1C]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fsubp st(1), st
        ret
    }
}

__declspec(naked) void WheelSpinAccelThunk() {
    __asm {
        fld dword ptr ds:[0x00858C28]
        fmul dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        faddp st(1), st
        ret
    }
}

// Damping is a ratio applied per frame, so it takes the exponent rather than
// the product, the same shape the game itself uses for the chassis door.
__declspec(naked) void WheelSpinDampThunk() {
    __asm {
        fld dword ptr ds:[0x00858EF0]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmulp st(1), st
        ret
    }
}

// Scales the free front wheel's angular velocity into the current frame before
// the pitch angle integrates it. `esi` is the bike and the field offset is the
// one the replaced `fadd` used; the original `fstp` at the return address
// stores the result.
__declspec(naked) void BikeWheelPitchIntegrateThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        fadd dword ptr [esi + 0x750]
        ret
    }
}

// The four ramp steps. Each replaces one `fadd`/`fsub` against a constant with
// the same operation scaled into the current frame; `fchs` before the add
// avoids relying on `fsubp` operand order, which assemblers disagree about.
// The value being ramped is already on the stack and the original store
// follows at the return address.
__declspec(naked) void JetPackFxRampUpThunk() {
    __asm {
        fld dword ptr ds:[0x00858B1C]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        faddp st(1), st
        ret
    }
}

__declspec(naked) void JetPackFxRampDownThunk() {
    __asm {
        fld dword ptr ds:[0x00858B1C]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        fchs
        faddp st(1), st
        ret
    }
}

__declspec(naked) void HeadBopRampUpThunk() {
    __asm {
        fld dword ptr ds:[0x00858C28]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        faddp st(1), st
        ret
    }
}

__declspec(naked) void HeadBopRampDownThunk() {
    __asm {
        fld dword ptr ds:[0x00858C28]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        fchs
        faddp st(1), st
        ret
    }
}

// The base, the writable `0.95`, is already in st(0) when these run.
__declspec(naked) void BmxLeanLeftDecayThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmul dword ptr [esi + 0x654]
        ret
    }
}

__declspec(naked) void BmxLeanFwdDecayThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmul dword ptr [esi + 0x658]
        ret
    }
}

// The gap between the wheel's drawn position and where the suspension wants it
// is already in st(0).
__declspec(naked) void WheelSettleThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetWheelSettleWeight
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        ret
    }
}

// The same shape as the free wheel damping, against the 0.9 constant that
// `CVehicle::CanPedJumpOutCar` uses for both speeds.
__declspec(naked) void JumpOutDampThunk() {
    __asm {
        fld dword ptr ds:[0x00858C20]
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        call kPow
        fmulp st(1), st
        ret
    }
}

// Replaces the three `fld / fmul 0.99 / fstp` triples that damp
// `m_vecTurnSpeed`. `esi` is the physical and the x87 stack is empty here, so
// the factor comes back on it and the stores keep the original order and
// field offsets.
__declspec(naked) void TurnAirResistanceThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        push esi
        call GetTurnAirResistanceFactor
        add esp, 4
        pop edx
        pop ecx
        pop eax
        popfd
        fld st(0)
        fmul dword ptr [esi + 0x50]
        fstp dword ptr [esi + 0x50]
        fld st(0)
        fmul dword ptr [esi + 0x54]
        fstp dword ptr [esi + 0x54]
        fmul dword ptr [esi + 0x58]
        fstp dword ptr [esi + 0x58]
        jmp kTurnAirResistanceReturn
    }
}

// Each replaces the `fmul [esp+..] / fmul [0.2]` pair with `(target - raw)`
// alone on the x87 stack; the gain arrives on top of it and the product is
// left for the `fadd` / `fstp` at the return address.
__declspec(naked) void CarSteerInputAThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetCarSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kCarSteerInputAReturn
    }
}

__declspec(naked) void CarSteerInputBThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetCarSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kCarSteerInputBReturn
    }
}

__declspec(naked) void BikeSteerInputAThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetBikeSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kBikeSteerInputAReturn
    }
}

__declspec(naked) void BikeSteerInputBThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetBikeSteerInputGain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jmp kBikeSteerInputBReturn
    }
}

// Replaces `fld st(0) / fsub [ebp] / jne`. On entry st(0) is the gear band
// ratio and the flags are those of the `cmp bl,1` just before the site, so
// they are kept across the call and the original branch is re-created.
__declspec(naked) void TransmissionInertiaThunk() {
    __asm {
        fld st(0)
        fsub dword ptr [ebp]
        pushfd
        push eax
        push ecx
        push edx
        call GetTransmissionInertiaScale
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        jne cheat
        jmp kTransmissionInertiaReturn
    cheat:
        jmp kTransmissionInertiaCheatSkip
    }
}

// Replaces the smoother, `s = a * (1 - 0.85) + 0.85 * s`, with the same
// blend at the frame-rate-corrected fraction. On entry st(0) is `a` and
// `[esp+28h]` the pointer to `s`; on exit st(0) is the new `s` and the rest
// of the stack is untouched, as the original left it for the code after.
__declspec(naked) void TransmissionSmootherThunk() {
    __asm {
        mov eax, dword ptr [esp + 0x28]
        pushfd
        push eax
        push ecx
        push edx
        call GetTransmissionSmootherFrac
        pop edx
        pop ecx
        pop eax
        popfd
        fld st(0)
        fmul dword ptr [eax]
        fxch st(1)
        fchs
        fld1
        faddp st(1), st
        fmulp st(2), st
        faddp st(1), st
        jmp kTransmissionSmootherReturn
    }
}

// Replaces `fst [esp+18h] / jne`: st(0) is the lateral slip velocity and the
// flags are those of the `cmp` that decides driving against coasting. The
// slip is scaled to one original frame's worth, stored where the original
// stored it, and the branch re-created.
__declspec(naked) void WheelSlipRightThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call TimeStepRatio
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        fst dword ptr [esp + 0x18]
        jne coasting
        jmp kWheelSlipRightDriving
    coasting:
        jmp kWheelSlipRightCoasting
    }
}

// Replaces `fchs / fstp [esp+10h]`: st(0) is the coasting longitudinal slip
// before its sign flip; the `jne` after the site reads the flags of a
// `test` before it, so they are preserved across the call.
__declspec(naked) void WheelSlipCoastThunk() {
    __asm {
        fchs
        pushfd
        push eax
        push ecx
        push edx
        call TimeStepRatio
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        fstp dword ptr [esp + 0x10]
        jmp kWheelSlipCoastReturn
    }
}

// Replaces the step of a moving part from the timestep load to the clamp.
// `edi` is the vehicle, `ebx` its driver's pad and `[esp+0x1C]` the rate; the
// x87 stack is empty, and eax, ecx, edx and ebx are dead at the resume point,
// which clears ebx itself.
__declspec(naked) void MovingPartStepThunk() {
    __asm {
        push dword ptr [esp + 0x1C]
        push ebx
        push edi
        call StepMovingPartAngle
        add esp, 12
        jmp kMovingPartStepResume
    }
}

} // namespace hff
