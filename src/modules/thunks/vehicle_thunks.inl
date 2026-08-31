// The comparison itself is preserved exactly; only the limit it is measured
// against is rescaled into the same per-frame units as the value. The 0.005 is
// read back through the original operand, so a mod that repoints it keeps
// working. At 30 FPS the timestep and the normalizer cancel and the limit is
// bit-exact 0.005.
//
// On entry `st(0)` holds the absolute value of one move speed component, and
// the original `fcomp` pops it. Pushing the scaled limit and storing it back
// out leaves the stack exactly as the replaced instruction found it.
#define MOVE_SPEED_SNAP_THUNK(name, returnAddress)                  \
    __declspec(naked) void name() {                                 \
        __asm {                                                     \
            __asm fld dword ptr ds:[0x00858B4C]                     \
            __asm fmul dword ptr ds:[0x00B7CB5C]                    \
            __asm fdiv g_originalTimeStepValue                      \
            __asm fstp g_scaledMoveSpeedSnap                        \
            __asm fcomp g_scaledMoveSpeedSnap                       \
            __asm jmp returnAddress                                 \
        }                                                           \
    }

MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapCarXThunk, kMoveSpeedSnapCarXReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapCarYThunk, kMoveSpeedSnapCarYReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapCarZThunk, kMoveSpeedSnapCarZReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapBikeXThunk, kMoveSpeedSnapBikeXReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapBikeYThunk, kMoveSpeedSnapBikeYReturn)
MOVE_SPEED_SNAP_THUNK(MoveSpeedSnapBikeZThunk, kMoveSpeedSnapBikeZReturn)

#undef MOVE_SPEED_SNAP_THUNK

// The replaced block is three copies of `fld [esi+n]; fmul 0.99; fstp [esi+n]`,
// so the factor is computed once and reused with the same `fld st(0)` shape the
// original uses for the move speed a few instructions above. The net effect on
// the x87 stack is zero, exactly as for the block it replaces.
// The value handed in is the engine's estimate of the lateral acceleration in
// g, measured across one call as `deltaSpeed / (timeStep * gravity)`. Standing
// still it reached 0.7451 at 500 FPS against 0.0134 at 30 FPS, while the bike's
// actual roll was the same to within a factor of 1.5, so the estimate is not
// reporting real motion.
//
// It cannot be repaired by accumulating the per-call deltas over a real-time
// window. Between calls the resting contact cancels the tangential speed, so a
// delta is not an exact difference and the sum keeps the contact chatter that
// the endpoints would have cancelled. Measured, that left the amplitude at
// 0.4353, barely better than the 0.7451 it started from.
//
// What works is a plain finite difference of the state: the velocity is
// measured from how far the bike actually travelled over one original frame of
// real time, and the difference between two such measurements is the
// acceleration across that interval. Standing still, both samples are zero and
// the estimate is zero. Cornering, the difference is the real change in
// velocity over 1/30 s, which is what the engine measures at 30 FPS.
//
// The velocity kept between windows is the whole vector. Keeping only its
// component along the bike's right axis looks equivalent and is not: those axes
// turn with the bike, so a steady corner holds that component roughly constant
// and its difference is zero. That version reported no lateral acceleration
// through corners and held the bike upright at any frame rate above 30, which
// is what the projection loses — the engine's numerator is a change of world
// velocity, and the projection happens after the difference, not before.
//
// The direction dotted against is the matrix right vector, which is what the
// original numerator uses at `0x6BBAC2` through `edi`, loaded from `[esi+0x14]`.
// At or below the original frame rate the engine's own value is passed through,
// so stock behavior is reproduced by construction rather than by arithmetic.
// `CBike::ProcessControl` runs for every bike in the world, not just the one
// the player is on, so a single set of globals is torn between them: the
// "different bike" branch fires on every call, the interval never accumulates
// and the filter degenerates into a pass-through. The state is therefore kept
// per bike, in a small table that evicts whichever entry has gone longest
// without being touched.
// Both are entered by a `call`, so the force vector the caller pushed starts at
// `[esp+4]`, which `pushfd` and `pushad` move to `[esp+0x28]`. The tail is a
// `jmp` so the real function returns straight to the original caller, and
// `ecx`, which carries `this`, is restored by `popad`.
__declspec(naked) void RollOntoWheelsTurnForceThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x28]
        push eax
        call ScaleRollOntoWheelsForce
        add esp, 4
        popad
        popfd
        jmp kApplyTurnForce
    }
}

__declspec(naked) void RollOntoWheelsMoveForceThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x28]
        push eax
        call ScaleRollOntoWheelsForce
        add esp, 4
        popad
        popfd
        jmp kApplyMoveForce
    }
}

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

// The same shape as the free wheel damping, against the 0.9 constant that
// `CVehicle::CanPedJumpOutCar` uses for both speeds.
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

// The shift component is already in st(0); both thunks replace the multiply
// that scaled it by the stock constant.
__declspec(naked) void PushOutMainThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetPushOutScaleMain
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        ret
    }
}

__declspec(naked) void PushOutAltThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call GetPushOutScaleAlt
        pop edx
        pop ecx
        pop eax
        popfd
        fmulp st(1), st
        ret
    }
}

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
