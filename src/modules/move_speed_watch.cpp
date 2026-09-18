#include "modules/modules.h"

namespace hff {

// The generic force helpers are shared by the whole engine, so the instruction
// that writes the field names `CPhysical::ApplyTurnForce` and says nothing
// about which system asked for the force. The stack above the trap is scanned
// for the first value that looks like a return address into the executable,
// that is, one whose preceding five bytes are a direct `call`. That is the
// caller, which is what the search is actually after.
uintptr_t FindCallerOnStack(uintptr_t esp) {
    for (size_t i = 0; i < 96; ++i) {
        const auto candidate =
            *reinterpret_cast<const uintptr_t*>(esp + i * sizeof(uintptr_t));
        if (candidate < kTextLow || candidate >= kTextHigh) {
            continue;
        }
        if (*reinterpret_cast<const uint8_t*>(candidate - 5) == 0xE8) {
            return candidate;
        }
    }
    return 0;
}

LONG CALLBACK MoveSpeedWatchHandler(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    CONTEXT* ctx = info->ContextRecord;
    if ((ctx->Dr6 & 1u) == 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    ctx->Dr6 = 0;

    const auto eip = static_cast<uintptr_t>(ctx->Eip);
    float delta = 0.0f;
    __try {
        const float value = *reinterpret_cast<const float*>(g_watchAddress);
        if (g_watchHaveLastValue) {
            delta = value - g_watchLastValue;
        }
        g_watchLastValue = value;
        g_watchHaveLastValue = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    uintptr_t caller = 0;
    __try {
        caller = FindCallerOnStack(static_cast<uintptr_t>(ctx->Esp));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        caller = 0;
    }

    while (InterlockedCompareExchange(&g_watchLock, 1, 0) != 0) {
        YieldProcessor();
    }
    ++g_watchTotalHits;
    for (size_t i = 0; i < kWatchSlots; ++i) {
        if (g_watchEips[i] == eip && g_watchCallers[i] == caller) {
            ++g_watchCounts[i];
            g_watchDeltaSum[i] += std::fabs(delta);
            g_watchSignedSum[i] += delta;
            break;
        }
        if (g_watchEips[i] == 0) {
            g_watchEips[i] = eip;
            g_watchCallers[i] = caller;
            g_watchCounts[i] = 1;
            g_watchDeltaSum[i] = std::fabs(delta);
            g_watchSignedSum[i] = delta;
            break;
        }
    }
    InterlockedExchange(&g_watchLock, 0);
    return EXCEPTION_CONTINUE_EXECUTION;
}

bool SetMoveSpeedWatch(uintptr_t address) {
    const DWORD threadId = g_gameThreadId;
    if (!threadId) {
        return false;
    }
    const HANDLE thread = OpenThread(
        THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
        FALSE,
        threadId);
    if (!thread) {
        return false;
    }
    bool ok = false;
    if (SuspendThread(thread) != static_cast<DWORD>(-1)) {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(thread, &ctx)) {
            ctx.Dr0 = address;
            ctx.Dr6 = 0;
            ctx.Dr7 = address ? kDr7WriteFourBytes : 0;
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            ok = SetThreadContext(thread, &ctx) != FALSE;
        }
        ResumeThread(thread);
    }
    CloseHandle(thread);
    return ok;
}

// The counters are copied out and cleared under the lock, and the formatting
// happens after it is released. Printing while holding it made the game thread
// spin inside the exception handler for the length of a buffered file write,
// once per report, which is not something the render thread can afford.
void ReportMoveSpeedWatch(FILE* file) {
    uintptr_t eips[kWatchSlots];
    uintptr_t callers[kWatchSlots];
    uint32_t counts[kWatchSlots];
    float absSum[kWatchSlots];
    float netSum[kWatchSlots];
    uint32_t hits = 0;

    while (InterlockedCompareExchange(&g_watchLock, 1, 0) != 0) {
        YieldProcessor();
    }
    hits = g_watchTotalHits;
    for (size_t i = 0; i < kWatchSlots; ++i) {
        eips[i] = g_watchEips[i];
        callers[i] = g_watchCallers[i];
        counts[i] = g_watchCounts[i];
        absSum[i] = g_watchDeltaSum[i];
        netSum[i] = g_watchSignedSum[i];
        g_watchEips[i] = 0;
        g_watchCallers[i] = 0;
        g_watchCounts[i] = 0;
        g_watchDeltaSum[i] = 0.0f;
        g_watchSignedSum[i] = 0.0f;
    }
    g_watchTotalHits = 0;
    g_watchHaveLastValue = false;
    InterlockedExchange(&g_watchLock, 0);

    std::fprintf(file, "# writers of +%u, %u hits total, ts=%.5f\n",
                 static_cast<uint32_t>(g_watchOffset), hits,
                 *reinterpret_cast<const float*>(kTimerTimeStep));
    for (size_t i = 0; i < kWatchSlots && eips[i]; ++i) {
        std::fprintf(file,
                     "# writer %08X from %08X x%u abs=%.6f net=%.6f\n",
                     static_cast<uint32_t>(eips[i]),
                     static_cast<uint32_t>(callers[i]),
                     counts[i], absSum[i], netSum[i]);
    }
    std::fflush(file);
}

float VectorLength(const float v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

bool SampleThePlayerVehicle(VehicleSample& out) {
    __try {
        const auto ped = *reinterpret_cast<uintptr_t*>(kWorldPlayers);
        if (!ped) {
            return false;
        }
        const auto vehicle = *reinterpret_cast<uintptr_t*>(ped + kPedVehicle);
        if (!vehicle) {
            return false;
        }

        out.vehicle = vehicle;
        out.time = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
        out.timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        out.timeScale = *reinterpret_cast<const float*>(kTimerTimeScale);

        const auto matrix = *reinterpret_cast<uintptr_t*>(vehicle + kEntityMatrix);
        out.posZ = matrix
            ? *reinterpret_cast<const float*>(matrix + kMatrixPosition + 8)
            : 0.0f;

        std::memcpy(out.move,
                    reinterpret_cast<const void*>(vehicle + kPhysicalMoveSpeed),
                    sizeof(out.move));
        std::memcpy(out.turn,
                    reinterpret_cast<const void*>(vehicle + kPhysicalTurnSpeed),
                    sizeof(out.turn));
        std::memcpy(out.force,
                    reinterpret_cast<const void*>(vehicle + kPhysicalForce),
                    sizeof(out.force));
        std::memcpy(out.torque,
                    reinterpret_cast<const void*>(vehicle + kPhysicalTorque),
                    sizeof(out.torque));

        std::memcpy(out.friction,
                    reinterpret_cast<const void*>(vehicle
                                                  + kPhysicalFrictionMoveSpeed),
                    sizeof(out.friction));

        out.physicalFlags =
            *reinterpret_cast<const uint32_t*>(vehicle + kPhysicalFlags);
        out.lastCollisionTime = *reinterpret_cast<const uint32_t*>(
            vehicle + kPhysicalLastCollisionTime);
        out.attachedTo =
            *reinterpret_cast<const uintptr_t*>(vehicle + kPhysicalAttachedTo);
        out.processCalls = g_bikeProcessCalls;
        out.gravityCalls = g_gravityCalls;
        out.leanWrites[0] = g_leanWrites[0];
        out.leanWrites[1] = g_leanWrites[1];
        out.leanWrites[2] = g_leanWrites[2];
        out.mvZAfterGravity = g_moveSpeedAfterGravity;
        out.rightZ = matrix
            ? *reinterpret_cast<const float*>(matrix + kMatrixRight + 8)
            : 0.0f;
        out.upZ = matrix
            ? *reinterpret_cast<const float*>(matrix + kMatrixUp + 8)
            : 0.0f;
        out.barSteer = *reinterpret_cast<const float*>(vehicle + kBikeBarSteerAngle);
        out.lean = *reinterpret_cast<const float*>(vehicle + kBikeLeanAngle);
        out.desiredLean =
            *reinterpret_cast<const float*>(vehicle + kBikeDesiredLeanAngle);
        out.brakePedal =
            *reinterpret_cast<const float*>(vehicle + kVehicleBrakePedal);
        out.gasPedal =
            *reinterpret_cast<const float*>(vehicle + kVehicleGasPedal);

        out.mass = *reinterpret_cast<const float*>(vehicle + kPhysicalMass);
        out.airResistance =
            *reinterpret_cast<const float*>(vehicle + kPhysicalAirResistance);
        out.entityFlags = *reinterpret_cast<const uint32_t*>(vehicle + kEntityFlags);
        out.fakePhysics =
            *reinterpret_cast<const uint8_t*>(vehicle + kFakePhysicsOffset);
        out.status = static_cast<uint8_t>(
            (*reinterpret_cast<const uint8_t*>(vehicle + kEntityTypeAndStatus) >> 3)
            & 0x1F);
        out.subClass = *reinterpret_cast<const uint8_t*>(vehicle + kVehicleSubClass);
        // Only meaningful for the bike classes.
        const bool isBike = out.subClass == 9 || out.subClass == 10;
        out.contactWheels = isBike
            ? *reinterpret_cast<const uint8_t*>(vehicle + kBikeContactWheels)
            : 0xFF;
        if (isBike) {
            std::memcpy(out.wheelAngularVelocity,
                        reinterpret_cast<const void*>(
                            vehicle + kBikeWheelAngularVelocity),
                        sizeof(out.wheelAngularVelocity));
            if (matrix) {
                const auto* right =
                    reinterpret_cast<const float*>(matrix + kMatrixRight);
                out.localPitchTurnSpeed = out.turn[0] * right[0]
                                        + out.turn[1] * right[1]
                                        + out.turn[2] * right[2];
            }
        }
        // Only the BMX classes carry the sprint lean angle.
        out.sprintLean = out.subClass == 10
            ? *reinterpret_cast<const float*>(vehicle + kBmxSprintLeanAngle)
            : 0.0f;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The pushing ped is on foot, so this cannot ride on the vehicle sample. It
// reports how much velocity the vehicle was handed per unit of real time, which
// is the quantity that has to match between a capped and an uncapped run.
// `traceWatchMode` picks what the watchpoint is looking for: mode 0 waits for a
// ridden bike to stand nearly still, mode 1 waits for the ped to start pushing
// a vehicle. Either way the candidate has to be the same object for three
// samples running before the breakpoint is armed on it.
void MaybeArmWatch(FILE* file) {
    static uintptr_t lastCandidate = 0;
    static uint32_t stableSamples = 0;
    static uint32_t armedSamples = 0;

    if (g_watchReported) {
        return;
    }
    if (!g_watchArmed) {
        const uintptr_t candidate = g_watchCandidate;
        if (candidate != 0 && candidate == lastCandidate) {
            ++stableSamples;
        } else {
            stableSamples = candidate != 0 ? 1 : 0;
            lastCandidate = candidate;
        }
        if (stableSamples >= g_watchArmDelay) {
            g_watchAddress = candidate + g_watchOffset;
            g_watchArmed = SetMoveSpeedWatch(g_watchAddress);
            if (g_watchArmed) {
                std::fprintf(file, "# watching %08X mode=%d offset=%u\n",
                             static_cast<uint32_t>(g_watchAddress), g_watchMode,
                             static_cast<uint32_t>(g_watchOffset));
                std::fflush(file);
                armedSamples = 0;
            } else {
                stableSamples = 0;
            }
        }
        return;
    }
    // Reported repeatedly rather than once, so a single session can cover more
    // than one frame rate: the timestep goes into every report line, and the
    // counters reset after each one.
    // The breakpoint stays on between reports. Taking it down and putting it
    // back up each window meant suspending the render thread once per report,
    // and it also left the field unwatched during the re-arm gap, which is
    // exactly where a short event such as a landing falls.
    const uintptr_t candidate = g_watchCandidate;
    if (candidate != 0 && candidate + g_watchOffset != g_watchAddress) {
        g_watchAddress = candidate + g_watchOffset;
        if (SetMoveSpeedWatch(g_watchAddress)) {
            std::fprintf(file, "# rewatching %08X offset=%u\n",
                         static_cast<uint32_t>(g_watchAddress),
                         static_cast<uint32_t>(g_watchOffset));
            std::fflush(file);
        }
    }
    if (++armedSamples >= g_watchSampleLimit ||
        g_watchTotalHits >= g_watchHitLimit) {
        armedSamples = 0;
        ReportMoveSpeedWatch(file);
        if (++g_watchReports >= g_watchReportLimit) {
            SetMoveSpeedWatch(0);
            g_watchArmed = false;
            g_watchReported = true;
        }
    }
}

} // namespace hff
