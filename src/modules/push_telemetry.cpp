#include "modules/modules.h"

namespace hff {

void DrainPushSamples(FILE* file) {
    const uint32_t head = g_pushWriteIndex;
    if (head - g_pushReadIndex > kPushSampleSlots) {
        // Overrun; skip to what is still intact rather than report garbage.
        g_pushReadIndex = head - kPushSampleSlots;
    }
    bool wrote = false;
    while (g_pushReadIndex != head && g_pushSamplesLogged < kPushSampleLimit) {
        const PushSample& s = g_pushSamples[g_pushReadIndex % kPushSampleSlots];
        std::fprintf(file, "# imp f=%u dv=%.7f v=%.6f ts=%.5f\n",
                     s.frame, s.deltaV, s.carSpeed, s.timeStep);
        ++g_pushReadIndex;
        ++g_pushSamplesLogged;
        wrote = true;
    }
    if (wrote) {
        std::fflush(file);
    }
}

void ReportPedPushTelemetry(FILE* file, uint32_t& lastReport) {
    const uint32_t now = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
    if (now - lastReport < 500) {
        return;
    }
    const uint32_t applications = g_pushApplications;
    if (applications != 0) {
        const float moved[3]{
            g_pushLastPos[0] - g_pushFirstPos[0],
            g_pushLastPos[1] - g_pushFirstPos[1],
            g_pushLastPos[2] - g_pushFirstPos[2]
        };
        std::fprintf(file,
                     "# push dt=%u n=%u dv=%.5f vmax=%.5f ped=%.5f "
                     "dist=%.5f ts=%.5f\n",
                     now - lastReport, applications, g_pushDeltaVSum,
                     g_pushCarSpeedPeak, g_pushPedSpeed, VectorLength(moved),
                     *reinterpret_cast<const float*>(kTimerTimeStep));
        std::fflush(file);
        g_pushApplications = 0;
        g_pushDeltaVSum = 0.0f;
        g_pushCarSpeedPeak = 0.0f;
        g_pushHavePos = false;
    }
    lastReport = now;
}

bool SampleThePlayerPed(PedSample& out) {
    __try {
        const auto ped = *reinterpret_cast<const uintptr_t*>(kWorldPlayers);
        if (!ped) {
            return false;
        }
        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            ped + kEntityMatrix);
        if (!matrix) {
            return false;
        }
        out.time = *reinterpret_cast<const uint32_t*>(kTimerTimeInMilliseconds);
        out.timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        for (int i = 0; i < 3; ++i) {
            out.position[i] = reinterpret_cast<const float*>(matrix + 0x30)[i];
            out.move[i] = reinterpret_cast<const float*>(
                ped + kPhysicalMoveSpeed)[i];
        }
        out.health = *reinterpret_cast<const float*>(ped + kPedHealth);
        out.armour = *reinterpret_cast<const float*>(ped + kPedArmour);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

DWORD WINAPI PedTraceThread(void*) {
    FILE* file = _fsopen(g_diagnosticPath.c_str(), "a", _SH_DENYNO);
    if (!file) {
        return 0;
    }
    std::fprintf(file, "# ped session fpsLimit=%d\n", g_fpsLimit);
    std::fprintf(file, "time ts posX posY posZ mvX mvY mvZ mvLen health armour\n");

    uint32_t written = 0;
    uint32_t lastTime = 0;
    while (g_diagnosticActive && written < kDiagnosticLineLimit) {
        PedSample s{};
        if (SampleThePlayerPed(s) && s.time != lastTime) {
            lastTime = s.time;
            const float length = std::sqrt(s.move[0] * s.move[0]
                                           + s.move[1] * s.move[1]
                                           + s.move[2] * s.move[2]);
            std::fprintf(file,
                         "%u %.5f %.3f %.3f %.3f %.5f %.5f %.5f %.5f %.2f %.2f\n",
                         s.time, s.timeStep,
                         s.position[0], s.position[1], s.position[2],
                         s.move[0], s.move[1], s.move[2], length,
                         s.health, s.armour);
            if ((++written % 32) == 0) {
                std::fflush(file);
            }
        }
        if (WorkerStopRequested(kDiagnosticIntervalMs)) {
            break;
        }
    }
    std::fflush(file);
    std::fclose(file);
    return 0;
}

DWORD WINAPI VehicleTraceThread(void*) {
    FILE* file{};
    // Appended, not truncated, so a capped run and an uncapped run of the same
    // test can be compared against each other in one file. Opened with sharing
    // so the log can be read while the game is still running; the default
    // `fopen` on this toolchain locks it exclusively.
    file = _fsopen(g_diagnosticPath.c_str(), "a", _SH_DENYNO);
    if (!file) {
        return 0;
    }
    std::fprintf(file, "# session fpsLimit=%d\n", g_fpsLimit);
    std::fprintf(file,
                 "time ts scale posZ mvLen tnLen frcLen trqLen fricLen "
                 "barSteer lean desiredLean sprintLean "
                 "eFlags pFlags lastCol attached calls fake status sub wheels "
                  "grav lw0 lw1 lw2 mvZpostGrav rightZ upZ leanRaw leanHeld "
                  "brake gas wheel0 wheel1 localPitch "
                  "mvX mvY mvZ tnX tnY tnZ\n");

    uint32_t written = 0;
    uint32_t lastTime = 0;
    uint32_t lastCalls = 0;
    uint32_t lastGravity = 0;
    uint32_t lastLean[3]{};
    uint32_t lastPushReport = 0;
    LONG lastBikeBalanceSequence = 0;
    LONG lastBikeBalanceWindowSequence = 0;
    while (g_diagnosticActive && written < kDiagnosticLineLimit) {
        VehicleSample s{};
        DrainPushSamples(file);
        ReportPedPushTelemetry(file, lastPushReport);
        const LONG balanceBefore = g_bikeBalanceSequence;
        if (balanceBefore != 0 && balanceBefore != lastBikeBalanceSequence) {
            MemoryBarrier();
            BikeBalanceSample balance{};
            std::memcpy(&balance, &g_bikeBalanceSample, sizeof(balance));
            MemoryBarrier();
            if (balanceBefore == g_bikeBalanceSequence) {
                std::fprintf(file,
                             "# bike-balance t=%u ts=%.5f l34=%.6f "
                             "a=%.6f b=%.6f in68=%.6f in6c=%.6f in78=%.6f "
                             "force=%.6f,%.6f,%.6f turnY=%.6f wheels=%u\n",
                             balance.time, balance.timeStep, balance.local34,
                             balance.coefficientA, balance.coefficientB,
                             balance.input68, balance.input6C, balance.input78,
                             balance.force[0], balance.force[1], balance.force[2],
                             balance.turnY, balance.contactWheels);
                lastBikeBalanceSequence = balanceBefore;
            }
        }
        const LONG windowBefore = g_bikeBalanceWindowSequence;
        if (windowBefore != 0 && windowBefore != lastBikeBalanceWindowSequence) {
            MemoryBarrier();
            BikeBalanceWindow window{};
            std::memcpy(&window, &g_bikeBalanceWindowSnapshot, sizeof(window));
            MemoryBarrier();
            if (windowBefore == g_bikeBalanceWindowSequence) {
                std::fprintf(file,
                             "# bike-balance-window t=%u calls=%u elapsed=%.5f "
                             "peak68=%.6f sum68=%.6f sum6c=%.6f "
                             "sum-force=%.6f,%.6f,%.6f\n",
                             window.time, window.calls, window.elapsed,
                             window.peakInput68, window.sumInput68,
                             window.sumInput6C, window.force[0],
                             window.force[1], window.force[2]);
                lastBikeBalanceWindowSequence = windowBefore;
            }
        }
        if (SampleThePlayerVehicle(s) && s.time != lastTime) {
            lastTime = s.time;
            const uint32_t callDelta = s.processCalls - lastCalls;
            lastCalls = s.processCalls;
            const uint32_t gravityDelta = s.gravityCalls - lastGravity;
            lastGravity = s.gravityCalls;
            uint32_t leanDelta[3];
            for (int i = 0; i < 3; ++i) {
                leanDelta[i] = s.leanWrites[i] - lastLean[i];
                lastLean[i] = s.leanWrites[i];
            }
            std::fprintf(file,
                         "%u %.5f %.3f %.3f "
                         "%.5f %.5f %.5f %.5f %.5f "
                         "%.4f %.4f %.4f %.4f "
                         "%08X %08X %u %08X %u %u %u %u %u "
                          "%u %u %u %u %.6f %.5f %.5f "
                          "%.5f %.5f "
                          "%.4f %.4f %.6f %.6f %.6f "
                          "%.5f %.5f %.5f %.5f %.5f %.5f\n",
                         s.time, s.timeStep, s.timeScale, s.posZ,
                         VectorLength(s.move), VectorLength(s.turn),
                         VectorLength(s.force), VectorLength(s.torque),
                         VectorLength(s.friction),
                         s.barSteer, s.lean, s.desiredLean, s.sprintLean,
                         s.entityFlags, s.physicalFlags, s.lastCollisionTime,
                         static_cast<uint32_t>(s.attachedTo), callDelta,
                         s.fakePhysics, s.status, s.subClass, s.contactWheels,
                         gravityDelta, leanDelta[0], leanDelta[1], leanDelta[2],
                          s.mvZAfterGravity, s.rightZ, s.upZ,
                          g_leanTargetRaw, g_leanTargetHeld,
                          s.brakePedal, s.gasPedal,
                          s.wheelAngularVelocity[0],
                          s.wheelAngularVelocity[1],
                          s.localPitchTurnSpeed,
                          s.move[0], s.move[1], s.move[2],
                         s.turn[0], s.turn[1], s.turn[2]);
            if ((++written % 64) == 0) {
                std::fflush(file);
            }

            // Once a ridden bike has been near still for a moment, watch the
            // chosen field and record every instruction that writes it.
            const bool isBike = s.subClass == 9 || s.subClass == 10;
            if (g_watchMode == 0) {
                g_watchCandidate = isBike && VectorLength(s.move) < 0.006f
                    ? s.vehicle
                    : 0;
            } else if (g_watchMode == 2) {
                // Any ridden bike, moving or not. Mode 0 waits for it to stand
                // still, which disarms the breakpoint for the whole of a jump
                // and its landing.
                g_watchCandidate = isBike ? s.vehicle : 0;
            }
        }

        MaybeArmWatch(file);
        if (WorkerStopRequested(kDiagnosticIntervalMs)) {
            break;
        }
    }

    std::fflush(file);
    std::fclose(file);
    return 0;
}

} // namespace hff
