#include "modules/modules.h"

namespace hff {

bool IsThePlayerVehicle(const void* entity) {
    __try {
        const auto ped = *reinterpret_cast<uintptr_t*>(kWorldPlayers);
        if (!ped) {
            return false;
        }
        return *reinterpret_cast<uintptr_t*>(ped + kPedVehicle)
            == reinterpret_cast<uintptr_t>(entity);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void __fastcall HookedBikeProcessControl(void* bike, void*) {
    if (IsThePlayerVehicle(bike)) {
        ++g_bikeProcessCalls;
    }
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(bike)) {
        if (!ShouldRunAbandonedBikePhysicsStep()) {
            // Physics remains at the last complete 30 Hz state, but refresh
            // the RenderWare hierarchy with an interpolated transform.
            reinterpret_cast<ThisCallVoidFn>(kEntityUpdateRwFrame)(bike);
            return;
        }
        BeginAbandonedBikePhysicsStep(bike);
        CallAbandonedBikePhysicsStep(g_bikeProcessPatch, bike);
        return;
    }
    if (g_abandonedBikePhysicsStepEnabled) {
        if (auto* state = FindAbandonedBikeRenderState(bike, false)) {
            // Release the slot as soon as the entity stops being an
            // abandoned bike. Keeping the pointer around made the fixed-size
            // cache retain destroyed bikes and allowed a recycled address to
            // inherit a stale render transform.
            *state = {};
        }
    }
    reinterpret_cast<ThisCallVoidFn>(g_bikeProcessPatch.gateway)(bike);
    CorrectBmxLaunchPitch(bike);
    UpdateBmxLandingProtection(bike);
}

bool EnsureBikeProcessControlHook() {
    return g_bikeProcessPatch.installed
        || InstallDetour(g_bikeProcessPatch, kBikeProcessControl,
                         &HookedBikeProcessControl,
                         kExpectedBikeProcessControl.data(),
                         kExpectedBikeProcessControl.size());
}

void __fastcall HookedPhysicalProcessCollision(void* physical, void*) {
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(physical)) {
        if (!g_abandonedBikePhysicsTick) {
            __try {
                // Prevent CWorld from retrying the deliberately skipped step.
                *reinterpret_cast<uint8_t*>(
                    reinterpret_cast<uintptr_t>(physical) + kEntityFlags)
                    |= 0x20; // m_bIsInSafePosition
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
            return;
        }
        CallAbandonedBikePhysicsStep(g_abandonedBikeCollisionPatch, physical);
        FinishAbandonedBikePhysicsStep(physical);
        return;
    }
    reinterpret_cast<ThisCallVoidFn>(
        g_abandonedBikeCollisionPatch.gateway)(physical);
}

void __fastcall HookedPhysicalProcessShift(void* physical, void*) {
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(physical)) {
        if (!g_abandonedBikePhysicsTick) {
            __try {
                *reinterpret_cast<uint8_t*>(
                    reinterpret_cast<uintptr_t>(physical) + kEntityFlags)
                    |= 0x20; // m_bIsInSafePosition
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
            return;
        }
        CallAbandonedBikePhysicsStep(g_abandonedBikeShiftPatch, physical);
        FinishAbandonedBikePhysicsStep(physical);
        return;
    }
    reinterpret_cast<ThisCallVoidFn>(g_abandonedBikeShiftPatch.gateway)(
        physical);
}

void __fastcall HookedEntityUpdateRwFrame(void* entity, void*) {
    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(entity)) {
        auto* state = FindAbandonedBikeRenderState(entity, false);
        if (state && state->valid) {
            std::array<float, 12> physicalTransform{};
            if (CopyBikeTransform(entity, physicalTransform)) {
                const auto renderTransform = InterpolateBikeTransform(
                    *state, g_abandonedBikePhysicsCredit);
                if (WriteBikeTransform(entity, renderTransform)) {
                    // UpdateRwMatrix is already detoured by the modpack. Call
                    // its public entry so that compatibility hook still runs.
                    reinterpret_cast<ThisCallVoidFn>(
                        kEntityUpdateRwMatrix)(entity);
                    WriteBikeTransform(entity, physicalTransform);
                }
            }
        }
    }
    reinterpret_cast<ThisCallVoidFn>(g_abandonedBikeRwFramePatch.gateway)(
        entity);
}

// The clump already receives the interpolated transform in UpdateRwFrame, but
// bike lights are generated later from CBike::m_mLeanMatrix. That matrix is a
// render cache built from the collision matrix and is consequently left at the
// last 30 Hz physics state. Give PreRender/Render the same interpolated entity
// transform, force their lean cache to be rebuilt, then restore both matrices
// before returning to gameplay code.
void CallAbandonedBikeRender(DetourPatch& patch, void* bike) {
    std::array<float, 12> physicalTransform{};
    std::array<float, 12> savedLeanTransform{};
    uint8_t savedLeanCalculated{};
    bool swapped = false;

    if (g_abandonedBikePhysicsStepEnabled && IsAbandonedBike(bike)) {
        if (auto* state = FindAbandonedBikeRenderState(bike, false);
            state && state->valid) {
            const auto address = reinterpret_cast<uintptr_t>(bike);
            const auto leanMatrix = address + kBikeLeanMatrix;
            if (CopyBikeTransform(bike, physicalTransform)
                && CopyMatrixTransform(leanMatrix, savedLeanTransform)) {
                const auto renderTransform = InterpolateBikeTransform(
                    *state, g_abandonedBikePhysicsCredit);
                __try {
                    savedLeanCalculated = *reinterpret_cast<const uint8_t*>(
                        address + kBikeLeanMatrixCalculated);
                    if (WriteBikeTransform(bike, renderTransform)) {
                        *reinterpret_cast<uint8_t*>(
                            address + kBikeLeanMatrixCalculated) = 0;
                        swapped = true;
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    swapped = false;
                }
            }
        }
    }

    reinterpret_cast<ThisCallVoidFn>(patch.gateway)(bike);

    if (swapped) {
        const auto address = reinterpret_cast<uintptr_t>(bike);
        WriteBikeTransform(bike, physicalTransform);
        WriteMatrixTransform(address + kBikeLeanMatrix, savedLeanTransform);
        __try {
            *reinterpret_cast<uint8_t*>(address + kBikeLeanMatrixCalculated) =
                savedLeanCalculated;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

void __fastcall HookedBikePreRender(void* bike, void*) {
    CallAbandonedBikeRender(g_abandonedBikePreRenderPatch, bike);
}

void __fastcall HookedBikeRender(void* bike, void*) {
    CallAbandonedBikeRender(g_abandonedBikeRenderPatch, bike);
}

// Gravity is called every frame during the freeze, so the interesting value is
// what the move speed looks like the instant gravity returns. If it is non-zero
// there and zero by sample time, something downstream is wiping it.
void __fastcall HookedApplyGravity(void* physical, void*) {
    g_gameThreadId = GetCurrentThreadId();
    const bool isPlayer = IsThePlayerVehicle(physical);
    if (isPlayer) {
        ++g_gravityCalls;
    }
    reinterpret_cast<ThisCallVoidFn>(g_applyGravityPatch.gateway)(physical);
    if (isPlayer) {
        __try {
            g_moveSpeedAfterGravity = *reinterpret_cast<const float*>(
                reinterpret_cast<uintptr_t>(physical) + kPhysicalMoveSpeed + 8);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

void __cdecl RecordBikeBalanceInputs(uintptr_t bike, const float* stack) {
    if (!IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
        return;
    }

    __try {
        g_bikeBalanceSample.time = *reinterpret_cast<const uint32_t*>(
            kTimerTimeInMilliseconds);
        g_bikeBalanceSample.timeStep = *reinterpret_cast<const float*>(
            kTimerTimeStep);
        g_bikeBalanceSample.local34 = stack[0x34 / sizeof(float)];
        g_bikeBalanceSample.coefficientA = stack[0x3C / sizeof(float)];
        g_bikeBalanceSample.coefficientB = stack[0x40 / sizeof(float)];
        g_bikeBalanceSample.input68 = stack[0x68 / sizeof(float)];
        g_bikeBalanceSample.input6C = stack[0x6C / sizeof(float)];
        g_bikeBalanceSample.input78 = stack[0x78 / sizeof(float)];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void __cdecl RecordBikeBalanceForce(uintptr_t bike, const float* force) {
    if (!IsThePlayerVehicle(reinterpret_cast<const void*>(bike))) {
        return;
    }

    __try {
        std::memcpy(g_bikeBalanceSample.force, force, sizeof(g_bikeBalanceSample.force));
        g_bikeBalanceSample.turnY = *reinterpret_cast<const float*>(
            bike + kPhysicalTurnSpeed + sizeof(float));
        g_bikeBalanceSample.contactWheels = *reinterpret_cast<const uint8_t*>(
            bike + kBikeContactWheels);

        const float timeStep = g_bikeBalanceSample.timeStep;
        if (std::isfinite(timeStep) && timeStep > 0.0f
            && timeStep <= kOriginalTimeStep * 1.1f) {
            g_bikeBalanceWindow.time = g_bikeBalanceSample.time;
            ++g_bikeBalanceWindow.calls;
            g_bikeBalanceWindow.elapsed += timeStep;
            g_bikeBalanceWindow.peakInput68 = std::max(
                g_bikeBalanceWindow.peakInput68,
                std::fabs(g_bikeBalanceSample.input68));
            g_bikeBalanceWindow.sumInput68 += g_bikeBalanceSample.input68;
            g_bikeBalanceWindow.sumInput6C += g_bikeBalanceSample.input6C;
            for (size_t i = 0; i < 3; ++i) {
                g_bikeBalanceWindow.force[i] += g_bikeBalanceSample.force[i];
            }
            if (g_bikeBalanceWindow.elapsed >= kOriginalTimeStep) {
                g_bikeBalanceWindowSnapshot = g_bikeBalanceWindow;
                g_bikeBalanceWindow.elapsed -= kOriginalTimeStep;
                g_bikeBalanceWindow.calls = 0;
                g_bikeBalanceWindow.peakInput68 = 0.0f;
                g_bikeBalanceWindow.sumInput68 = 0.0f;
                g_bikeBalanceWindow.sumInput6C = 0.0f;
                std::memset(g_bikeBalanceWindow.force, 0,
                            sizeof(g_bikeBalanceWindow.force));
                InterlockedIncrement(&g_bikeBalanceWindowSequence);
            }
        }
        InterlockedIncrement(&g_bikeBalanceSequence);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// `kBikeBalanceInput` starts with `fld [esp+68h]; fmul [esp+3Ch]`. The hook is
// entered with an empty x87 stack, records the completed local coefficients,
// then runs those exact instructions before continuing the stock calculation.
__declspec(naked) void BikeBalanceInputThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x24]
        push eax
        mov eax, esi
        push eax
        call RecordBikeBalanceInputs
        add esp, 8
        popad
        popfd
        fld dword ptr [esp + 0x68]
        fmul dword ptr [esp + 0x3C]
        jmp kBikeBalanceInputReturn
    }
}

// This is entered through the original five-byte call. The force argument is
// at `[esp+4]` before saving registers and `[esp+28h]` afterwards.
__declspec(naked) void BikeBalanceForceThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [esp + 0x28]
        push eax
        mov eax, esi
        push eax
        call RecordBikeBalanceForce
        add esp, 8
        popad
        popfd
        jmp kApplyTurnForce
    }
}

// Each site is exactly `fstp dword ptr [esi+0x648]`, so the store is reproduced
// and only a counter is added.
__declspec(naked) void LeanWriteSmootherThunk() {
    __asm {
        fstp dword ptr [esi + 0x648]
        pushfd
        inc dword ptr [g_leanWrites]
        popfd
        jmp kLeanWriteSmootherReturn
    }
}

__declspec(naked) void LeanWriteBikeThunk() {
    __asm {
        fstp dword ptr [esi + 0x648]
        pushfd
        inc dword ptr [g_leanWrites + 4]
        popfd
        jmp kLeanWriteBikeReturn
    }
}

__declspec(naked) void LeanWriteBmxThunk() {
    __asm {
        fstp dword ptr [esi + 0x648]
        pushfd
        inc dword ptr [g_leanWrites + 8]
        popfd
        jmp kLeanWriteBmxReturn
    }
}
// `traceWatchHits` and `traceWatchSamples` shrink the accumulation window. A
// landing lasts a few hundred milliseconds, and at the default window it shares
// a report with the seconds of flight and standing on either side of it.
uint32_t g_watchHitLimit = kWatchHitLimit;
uint32_t g_watchSampleLimit = 100;
// Samples the candidate has to stay the same before the breakpoint goes back on.
// Every report disarms it, so this is dead time between windows: worth lowering
// when the event under study is short.
uint32_t g_watchArmDelay = 3;

// Which CPhysical field the watchpoint arms on, as an offset from the vehicle.
// Defaults to `m_vecMoveSpeed.z`; `traceWatchOffset` in `[general]` moves it, so
// one build can name the writers of any four byte field.
size_t g_watchOffset = kPhysicalMoveSpeed + 8;
volatile uintptr_t g_watchAddress{};
volatile uint32_t g_watchTotalHits{};
uintptr_t g_watchEips[kWatchSlots]{};
uintptr_t g_watchCallers[kWatchSlots]{};
// How much each caller actually moved the field. The trap fires after the
// write, so consecutive traps bracket every change exactly, and the call count
// alone cannot say which caller dominates.
float g_watchDeltaSum[kWatchSlots]{};
// Signed as well as absolute: a force that swings both ways can add a large
// absolute total while its net effect is nearly nothing, and it is the net that
// holds a bike upright.
float g_watchSignedSum[kWatchSlots]{};
float g_watchLastValue{};
bool g_watchHaveLastValue{};
uint32_t g_watchCounts[kWatchSlots]{};
volatile long g_watchLock{};
void* g_watchHandler{};
bool g_watchArmed{};
bool g_watchReported{};
uint32_t g_watchReports{};
uint32_t g_watchReportLimit = kWatchReportLimit;

} // namespace hff
