#include "modules/modules.h"

namespace hff {

__declspec(naked) void SirenTapThunk() {
    __asm {
        pushfd
        pushad
        push esi
        call SelectSirenReturnAddress
        add esp, 4
        mov dword ptr [esp + 28], eax
        popad
        popfd
        jmp eax
    }
}

__declspec(naked) void ScriptsProcessThunk() {
    __asm {
        pushfd
        pushad
        call ProcessFrameHooks
        popad
        popfd
        mov al, byte ptr ds:[0x00A43088]
        jmp kScriptsProcessReturn
    }
}

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

// Replaces `fld [esp+28h]` / `fdiv [ebx+15Bh]`, which forms
// remainingDistance / totalDistance from the object's real position. The x87
// stack is empty here, so the helper's st0 return value is the whole
// replacement and no fdiv is needed.
//
// CObject::Process has pushed two temporaries at this point, so the wall-clock
// distance local (elapsedSeconds * m_fSpeed, written at +0x1E9) sits at
// [esp+1Ch] of the entry frame, which is [esp+38h] once the thunk has spilled
// its own registers. m_fTotalDistance is [ebx+15Bh].
__declspec(naked) void SampObjectRotationThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        push dword ptr [ebx + 0x15B]
        push dword ptr [esp + 0x38]
        call SampObjectRotationRemainingFraction
        add esp, 8
        pop edx
        pop ecx
        pop eax
        popfd
        jmp dword ptr [g_sampObjectRotationReturn]
    }
}

// Replaces the arrival test
//
//     fld [esp+24h] / fcomp [esp+20h] / fnstsw ax / test ah,1
//
// which asks whether this frame's step would overshoot what is left of the
// distance. That question is kept verbatim; the wall-clock expiry is an extra
// way to answer it yes. The x87 stack is empty here, and the flags the helper
// call disturbs are restored before the stock sequence re-derives its own.
__declspec(naked) void SampObjectArrivalThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        push dword ptr [ebx + 0x15B]
        push dword ptr [esp + 0x30]
        call EvaluateSampObjectMoveExpiry
        add esp, 8
        pop edx
        pop ecx
        pop eax
        popfd
        cmp byte ptr [g_sampObjectMoveExpired], 0
        jne arrived
        fld dword ptr [esp + 0x24]
        fcomp dword ptr [esp + 0x20]
        fnstsw ax
        test ah, 1
        jne moving
    arrived:
        jmp dword ptr [g_sampObjectArrivedReturn]
    moving:
        jmp dword ptr [g_sampObjectMovingReturn]
    }
}

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

__declspec(naked) void BreakObjectLifetimeThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [edi + eax + 0x70]
        push eax
        call ConsumeBreakObjectLifetimeTicks
        add esp, 4
        mov dword ptr [esp + 24], eax
        popad
        popfd
        mov edx, dword ptr [edi + eax + 0x70]
        lea eax, [edi + eax + 0x70]
        sub edx, ecx
        mov dword ptr [eax], edx
        jmp kBreakObjectLifetimeReturn
    }
}

__declspec(naked) void MenuBackgroundThunk() {
    __asm {
        pushfd
        pushad
        call OnPauseMenuBackground
        popad
        popfd
        jmp kMenuBackgroundTarget
    }
}

} // namespace hff
