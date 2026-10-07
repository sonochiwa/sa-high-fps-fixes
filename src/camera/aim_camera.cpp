#include "camera/aim_camera.h"

#include "core/hook.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/camera.h"

#include "MinHook.h"

#include <windows.h>

#include <cstdint>

namespace hff::camera {

namespace {

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

using AimWeaponFn = void(__thiscall*)(void*, const Vec3&, float, float, float);
using CameraProcessFn = void(__thiscall*)(void*);

enum CamMode : int16_t {
    modeAimWeapon = 53,
    modeAimWeaponAttached = 65,
};

CameraProcessFn g_originalCameraProcess{};
AimWeaponFn g_originalAimWeapon{};
bool g_aimHooksCreated{};
int g_aimGuardDepth{};
float g_savedAimTimeStep{};
float g_savedAimTimeStepNonClipped{};
bool g_haveAimTimeStep{};
bool g_haveAimTimeStepNonClipped{};
SitePatch g_aimWeaponFovStepPatch{};

template <typename T>
bool SafeReadAimValue(uintptr_t address, T& value) {
    __try {
        value = *reinterpret_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool SafeWriteAimValue(uintptr_t address, const T& value) {
    __try {
        *reinterpret_cast<T*>(address) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool LooksLikeGameCode(uintptr_t address) {
    MEMORY_BASIC_INFORMATION memory{};
    if (!address
        || VirtualQuery(reinterpret_cast<const void*>(address), &memory,
                        sizeof(memory)) != sizeof(memory)) {
        return false;
    }
    constexpr DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ
                               | PAGE_EXECUTE_READWRITE
                               | PAGE_EXECUTE_WRITECOPY;
    return memory.State == MEM_COMMIT && (memory.Protect & executable) != 0
        && memory.AllocationBase == GetModuleHandleA(nullptr);
}

// Only the over-the-shoulder aim shakes. The scoped first-person modes, the
// sniper, camera and rocket launchers, already step by the real timestep;
// under the guard their zoom and turn would run the frame-rate ratio faster.
bool IsAimCameraMode(int16_t mode) {
    return mode == modeAimWeapon || mode == modeAimWeaponAttached;
}

bool IsPlayerOnFootForAimCamera() {
    uintptr_t ped{};
    if (!SafeReadAimValue(kPlayerPed, ped) || !ped) {
        return false;
    }
    uint8_t flags{};
    return SafeReadAimValue(ped + kPedFlagsInVehicle, flags)
        && (flags & 1) == 0;
}

bool IsAimCameraActive(void* cameraPointer) {
    const uintptr_t camera = reinterpret_cast<uintptr_t>(cameraPointer);
    if (camera != kTheCamera || !IsPlayerOnFootForAimCamera()) {
        return false;
    }

    // The queued player weapon mode is set while the player aims, before the
    // camera switches to it.
    int16_t weaponMode{};
    if (SafeReadAimValue(camera + kCameraWeaponMode, weaponMode)
        && IsAimCameraMode(weaponMode)) {
        return true;
    }

    uint8_t activeCam{};
    if (!SafeReadAimValue(camera + kCameraActiveCam, activeCam)
        || activeCam > 2) {
        return false;
    }
    for (uint8_t index = 0; index < 3; ++index) {
        int16_t mode{};
        const uintptr_t cam = camera + kCameraCams
                            + static_cast<uintptr_t>(index) * kCamSize;
        if (SafeReadAimValue(cam + kCamMode, mode)
            && IsAimCameraMode(mode)) {
            return true;
        }
    }
    return false;
}

void BeginAimTimeStepGuard() {
    if (g_aimGuardDepth++ != 0) {
        return;
    }

    g_haveAimTimeStep =
        SafeReadAimValue(kTimerTimeStep, g_savedAimTimeStep);
    g_haveAimTimeStepNonClipped = SafeReadAimValue(
        kTimerTimeStepNonClipped, g_savedAimTimeStepNonClipped);
    constexpr float minimumAimTimeStep = 1.0f;
    if (g_haveAimTimeStep && g_savedAimTimeStep > 0.00001f
        && g_savedAimTimeStep < minimumAimTimeStep) {
        SafeWriteAimValue(kTimerTimeStep, minimumAimTimeStep);
    }
    if (g_haveAimTimeStepNonClipped
        && g_savedAimTimeStepNonClipped > 0.00001f
        && g_savedAimTimeStepNonClipped < minimumAimTimeStep) {
        SafeWriteAimValue(kTimerTimeStepNonClipped, minimumAimTimeStep);
    }
}

void EndAimTimeStepGuard() {
    if (g_aimGuardDepth <= 0) {
        g_aimGuardDepth = 0;
        return;
    }
    if (--g_aimGuardDepth != 0) {
        return;
    }
    if (g_haveAimTimeStep) {
        SafeWriteAimValue(kTimerTimeStep, g_savedAimTimeStep);
    }
    if (g_haveAimTimeStepNonClipped) {
        SafeWriteAimValue(kTimerTimeStepNonClipped,
                          g_savedAimTimeStepNonClipped);
    }
}

// The guard is undone in `__finally`, so an exception a crash handler resumes
// from cannot leave the timestep pinned. CCamera::Process is never entered
// from inside itself, so a guard still open on entry was left by a frame that
// never finished; CTimer has replaced the pinned timestep since, so only the
// depth is cleared.
void __fastcall HookedCameraProcess(void* camera, void*) {
    g_aimGuardDepth = 0;
    const bool guard = IsAimCameraActive(camera);
    if (guard) {
        BeginAimTimeStepGuard();
    }
    __try {
        g_originalCameraProcess(camera);
    } __finally {
        if (guard) {
            EndAimTimeStepGuard();
        }
    }
}

void __fastcall HookedProcessAimWeapon(void* cam, void*, const Vec3* target,
                                       float orientation, float speedVar,
                                       float speedVarWanted) {
    BeginAimTimeStepGuard();
    const Vec3 zero{};
    __try {
        g_originalAimWeapon(cam, target ? *target : zero, orientation, speedVar,
                            speedVarWanted);
    } __finally {
        EndAimTimeStepGuard();
    }
}

// Replaces the timestep load ahead of the aim camera FOV step. The site runs
// inside the guarded CCam::Process_AimWeapon call, where the global holds the
// pinned 1.0, so the step takes the real frame duration from UnguardedTimeStep
// instead. The helper leaves it in st(0), exactly as the replaced `fld` did,
// and the `fmul` that follows scales it into the per-frame step.
__declspec(naked) void AimWeaponFovStepThunk() {
    __asm {
        pushfd
        push eax
        push ecx
        push edx
        call UnguardedTimeStep
        pop edx
        pop ecx
        pop eax
        popfd
        jmp kAimWeaponFovStepReturn
    }
}

} // namespace

float __cdecl UnguardedTimeStep() {
    if (g_aimGuardDepth > 0 && g_haveAimTimeStep) {
        return g_savedAimTimeStep;
    }
    float timeStep{};
    if (SafeReadAimValue(kTimerTimeStep, timeStep)) {
        return timeStep;
    }
    return kOriginalTimeStep;
}

bool InstallAimCameraShakeFix() {
    if (!LooksLikeGameCode(kCameraProcess)
        || !LooksLikeGameCode(kProcessAimWeapon)) {
        Log("Aim camera shake fix skipped: camera code is not executable game memory.");
        return false;
    }
    if (!MemoryMatches(kCameraProcess, kExpectedCameraProcess)
        || !MemoryMatches(kProcessAimWeapon, kExpectedProcessAimWeapon)) {
        Log("Aim camera shake fix skipped: camera entry bytes do not match GTA SA 1.0 US, "
            "or another plugin already hooks them.");
        return false;
    }
    // The guard pins the timestep the FOV step reads, so the zoom site goes
    // in with the hooks and comes out with them: one without the other would
    // either leave the zoom running at the frame rate or change nothing.
    if (!InstallJump(g_aimWeaponFovStepPatch, kAimWeaponFovStep,
                     &AimWeaponFovStepThunk, kExpectedAimWeaponFovStep)) {
        Log("Aim camera shake fix skipped: the aim FOV step bytes do not match GTA SA 1.0 US.");
        return false;
    }
    if (!InitializeMinHook()) {
        RestoreSite(g_aimWeaponFovStepPatch);
        Log("Aim camera shake fix skipped: MinHook initialization failed.");
        return false;
    }
    g_aimHooksCreated = true;
    const bool created =
        MH_CreateHook(reinterpret_cast<void*>(kCameraProcess),
                      &HookedCameraProcess,
                      reinterpret_cast<void**>(&g_originalCameraProcess))
                == MH_OK
        && MH_CreateHook(reinterpret_cast<void*>(kProcessAimWeapon),
                         &HookedProcessAimWeapon,
                         reinterpret_cast<void**>(&g_originalAimWeapon))
                == MH_OK;
    bool enabled = false;
    if (created) {
        ThreadFreezeLock lock;
        enabled = MH_EnableHook(reinterpret_cast<void*>(kCameraProcess)) == MH_OK
            && MH_EnableHook(reinterpret_cast<void*>(kProcessAimWeapon)) == MH_OK;
    }
    if (!enabled) {
        RemoveAimCameraHooks();
        RestoreSite(g_aimWeaponFovStepPatch);
        Log("Aim camera shake fix skipped: camera hooks could not be installed.");
        return false;
    }
    Log("Installed scoped aim-camera timestep guards around CCamera::Process and "
        "Process_AimWeapon, with the aim FOV zoom kept on the real timestep.");
    return true;
}

void RemoveAimCameraHooks() {
    if (!g_aimHooksCreated) {
        return;
    }
    ThreadFreezeLock lock;
    MH_DisableHook(reinterpret_cast<void*>(kCameraProcess));
    MH_DisableHook(reinterpret_cast<void*>(kProcessAimWeapon));
    MH_RemoveHook(reinterpret_cast<void*>(kCameraProcess));
    MH_RemoveHook(reinterpret_cast<void*>(kProcessAimWeapon));
    g_originalCameraProcess = nullptr;
    g_originalAimWeapon = nullptr;
    g_aimHooksCreated = false;
}

} // namespace hff::camera
