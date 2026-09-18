#include "modules/modules.h"

namespace hff {

__declspec(naked) void FlightTimerThunk() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        call AccumulateFlightTimer
        add esp, 4
        ret
    }
}

__declspec(naked) void EndTimerThunk() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        call AccumulateEndTimer
        add esp, 4
        ret
    }
}

// Replaces the multiply, the truncation and the two argument pushes the
// optimizer moved in front of it. The two pushes are reproduced afterwards, so
// the six arguments `CWeapon::GenerateDamageEvent` is about to receive sit in
// the original order. `eax` carries the damage into the `push eax` at the
// return address, exactly as `_ftol` left it.
__declspec(naked) void DrowningDamageThunk() {
    __asm {
        fmul dword ptr ds:[0x00858B3C]
        sub esp, 4
        fstp dword ptr [esp]
        call AccumulateDrowningDamage
        add esp, 4
        push 0
        push 3
        jmp kDrowningDamageReturn
    }
}

// Entry contract: esi is CPed, ecx is the current countdown and edx contains
// the ped flags. The stock zero test after the replaced block clears the
// bloody-footprint flag when the scaled counter reaches zero.
__declspec(naked) void BloodyFootprintCounterThunk() {
    __asm {
        push eax
        push edx
        push ecx
        push esi
        call UpdateBloodyFootprintCounter
        add esp, 8
        mov ecx, eax
        pop edx
        pop eax
        mov dword ptr [esi + 0x750], ecx
        test ecx, ecx
        jmp kBloodyFootprintCounterReturn
    }
}

__declspec(naked) void BloodyFootLandedSideThunk() {
    __asm {
        pushfd
        pushad
        mov eax, dword ptr [esp + 0x28]
        push eax
        push ecx
        call SelectBloodyFootprintSide
        add esp, 8
        popad
        popfd
        jmp kDoFootLanded
    }
}

// Keep the game's cdecl AddPermanentShadow call intact. Only its stack-local
// position vector may be adjusted before tail-calling the original function.
__declspec(naked) void BloodyFootprintShadowThunk() {
    __asm {
        pushfd
        pushad
        mov eax, dword ptr [esp + 0x30]
        push eax
        call StabilizeBloodyFootprintHeight
        add esp, 4
        popad
        popfd
        jmp kAddPermanentShadow
    }
}

// `st(0)` holds `m_fHit` for the moving attack and `ecx` the animation the
// rewind is about to be written to, which is also the `this` the
// `CAnimBlendAssociation::SetCurrentTime` call four bytes later expects, so it
// has to survive the helper.
__declspec(naked) void ChainsawStrikeRewindThunk() {
    __asm {
        pushfd
        push eax
        push edx
        push ecx
        push esi
        push ecx
        call UpdateChainsawRewindOffset
        add esp, 8
        pop ecx
        pop edx
        pop eax
        popfd
        fsub g_chainsawRewindOffset
        ret
    }
}

// Counts melee strikes for `traceChainsaw`, then falls through into the
// function the replaced call was going to reach.
__declspec(naked) void FightStrikeTraceThunk() {
    __asm {
        pushfd
        pushad
        push ecx
        call RecordFightStrike
        add esp, 4
        popad
        popfd
        jmp kFightStrike
    }
}

__declspec(naked) void ContinuousWeaponAmmoThunk() {
    __asm {
        pushfd
        pushad
        push esi
        call ShouldConsumeContinuousWeaponAmmo
        add esp, 4
        mov dword ptr [esp + 28], eax
        popad
        popfd
        test eax, eax
        jz skipConsumption
        mov eax, dword ptr [esi + 8]
        test eax, eax
        jmp kContinuousAmmoConsume
    skipConsumption:
        jmp kContinuousAmmoSkip
    }
}

__declspec(naked) void WheelFrictionCarDriveThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionCarDriveReturn
    }
}

__declspec(naked) void WheelFrictionCarBrakeThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionCarBrakeReturn
    }
}

__declspec(naked) void WheelFrictionBikeBaseThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionBikeBaseReturn
    }
}

__declspec(naked) void WheelFrictionBikeDriveThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionBikeDriveReturn
    }
}

__declspec(naked) void WheelFrictionBikeBrakeThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetFrameIndependentWheelFriction
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kWheelFrictionBikeBrakeReturn
    }
}

} // namespace hff
