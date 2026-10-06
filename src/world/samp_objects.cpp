#include "world/samp_objects.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/frame_hook.h"
#include "game/sites/world.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstring>

namespace hff::world {

namespace {

SitePatch g_sampObjectRotationPatch{};
SitePatch g_sampObjectArrivalPatch{};
uintptr_t g_sampObjectRotationReturn{};
uintptr_t g_sampObjectArrivedReturn{};
uintptr_t g_sampObjectMovingReturn{};
// Set by the arrival thunk so it can branch after restoring the flags the
// helper call clobbered.
uint8_t g_sampObjectMoveExpired{};

// SA-MP's moving-object rotation is a slerp between the start and target
// quaternions whose parameter CObject::Process forms as
//
//     t = 1.0f - remainingDistance / m_fTotalDistance;
//
// where `remainingDistance` is measured from the object's real position. The
// caller replaces `remainingDistance` with a wall-clock estimate, so this
// returns the *remaining* fraction and the stock `1.0f - x` that follows keeps
// its original meaning. Returning the elapsed fraction here would run the slerp
// backwards from the target to the start pose.
float __cdecl SampObjectRotationRemainingFraction(float elapsedDistance,
                                                  float totalDistance) {
    if (!std::isfinite(totalDistance) || totalDistance <= 0.0f) {
        return 0.0f;
    }
    if (!std::isfinite(elapsedDistance) || elapsedDistance <= 0.0f) {
        return 1.0f;
    }
    if (elapsedDistance >= totalDistance) {
        return 0.0f;
    }
    return 1.0f - elapsedDistance / totalDistance;
}

// A move's scheduled duration is m_fTotalDistance / m_fSpeed, and the caller
// hands over elapsedSeconds * m_fSpeed, so `elapsed >= total` is exactly
// "the move's wall-clock time is up". Only ever forces the arrival CObject
// already reaches on its own; a move that is still within its duration keeps
// the stock overshoot test.
void __cdecl EvaluateSampObjectMoveExpiry(float elapsedDistance,
                                          float totalDistance) {
    g_sampObjectMoveExpired =
        (std::isfinite(totalDistance) && totalDistance > 0.0f
         && std::isfinite(elapsedDistance) && elapsedDistance >= totalDistance)
            ? 1
            : 0;
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

// Returns the single occurrence of `pattern` in the module's code section, or
// zero when it is absent or appears more than once. Ambiguity is treated as
// absence: a second match would mean the pattern no longer identifies the site
// it was written for.
uintptr_t FindUniqueCodePattern(HMODULE module, const uint8_t* pattern,
                                size_t size) {
    const auto* image = reinterpret_cast<const uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    const auto* headers =
        reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
    if (headers->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    const uint8_t* code = image + headers->OptionalHeader.BaseOfCode;
    const size_t length = headers->OptionalHeader.SizeOfCode;
    if (length < size) {
        return 0;
    }

    const uint8_t* found = nullptr;
    for (size_t i = 0; i + size <= length; ++i) {
        if (std::memcmp(code + i, pattern, size) != 0) {
            continue;
        }
        if (found) {
            return 0;
        }
        found = code + i;
        i += size - 1;
    }
    return reinterpret_cast<uintptr_t>(found);
}

bool InstallSampObjectPatches(HMODULE samp) {
    const uintptr_t arrival =
        FindUniqueCodePattern(samp, kSampObjectMoveArrivalPattern.data(),
                              kSampObjectMoveArrivalPattern.size());
    if (!arrival) {
        Log("SA-MP moving object rotation fix skipped: CObject::Process "
            "arrival test not found in samp.dll.");
        return false;
    }

    const uintptr_t rotation = arrival + kSampObjectRotationProgressOffset;
    int32_t stillMoving{};
    std::memcpy(&stillMoving,
                reinterpret_cast<const void*>(
                    arrival + kSampObjectMoveContinueDisplacement),
                sizeof(stillMoving));

    PatchSet patches("SA-MP moving object rotation fix");
    g_sampObjectRotationReturn = rotation + kSampObjectRotationProgressPatchSize;
    g_sampObjectArrivedReturn = arrival + kSampObjectMoveArrivedOffset;
    g_sampObjectMovingReturn = g_sampObjectArrivedReturn + stillMoving;
    if (!patches.Track(
            InstallJump(g_sampObjectRotationPatch, rotation,
                        &SampObjectRotationThunk, kExpectedSampObjectRotation),
            g_sampObjectRotationPatch)
        || !patches.Track(
            InstallBranch(g_sampObjectArrivalPatch, arrival,
                          &SampObjectArrivalThunk,
                          kSampObjectMoveArrivalPattern.data(),
                          kSampObjectArrivalPatchSize, 0xE9),
            g_sampObjectArrivalPatch)) {
        g_sampObjectRotationReturn = 0;
        g_sampObjectArrivedReturn = 0;
        g_sampObjectMovingReturn = 0;
        Log("SA-MP moving object rotation fix skipped: CObject::Process bytes "
            "around the arrival test are not the expected ones.");
        return false;
    }
    patches.Commit();
    Log("Installed wall-clock rotation progress and arrival for moving SA-MP "
        "objects.");
    return true;
}

// samp.dll is often loaded after this plugin has started, so it is looked for
// once a second from the game thread until it turns up.
uint32_t g_sampPollFrame{};
bool g_sampPolling{};

void PollForSampDll() {
    if (!g_sampPolling) {
        return;
    }
    const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
    if (frame - g_sampPollFrame < 60) {
        return;
    }
    g_sampPollFrame = frame;
    if (const HMODULE samp = GetModuleHandleA("samp.dll")) {
        g_sampPolling = false;
        InstallSampObjectPatches(samp);
    }
}

} // namespace

bool InstallSampObjectRotationFix() {
    if (const HMODULE samp = GetModuleHandleA("samp.dll")) {
        return InstallSampObjectPatches(samp);
    }
    if (!InstallFrameHook("SA-MP moving object rotation fix")) {
        return false;
    }
    g_sampPolling = true;
    AddFrameCallback(&PollForSampDll);
    Log("SA-MP moving object rotation fix waits for samp.dll.");
    return true;
}

} // namespace hff::world
