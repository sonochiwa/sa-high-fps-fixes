#include "modules/modules.h"

namespace hff {

bool IsContinuousWeapon(int32_t weaponType) {
    constexpr int32_t kFlamethrower = 37;
    constexpr int32_t kSpraycan = 41;
    constexpr int32_t kExtinguisher = 42;
    return weaponType == kFlamethrower || weaponType == kSpraycan
        || weaponType == kExtinguisher;
}

bool IsWeaponFxEmitter(void* emitter, void** systemOut) {
    if (!emitter) {
        return false;
    }
    __try {
        void* system = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(emitter) + 0x08);
        if (systemOut) {
            *systemOut = system;
        }
        return system && (*reinterpret_cast<uint8_t*>(
            reinterpret_cast<uintptr_t>(system) + 0x62) & 0x20) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

CameraProcessFn g_originalCameraProcess{};
AimWeaponFn g_originalAimWeapon{};
bool g_aimMinHookInitialized{};
int g_aimGuardDepth{};
float g_savedAimTimeStep{};
float g_savedAimTimeStepNonClipped{};
bool g_haveAimTimeStep{};
bool g_haveAimTimeStepNonClipped{};

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

bool IsAimCameraMode(int16_t mode) {
    switch (mode) {
    case modeSniper:
    case modeRocketLauncher:
    case modeM16FirstPerson:
    case modeSniperRunabout:
    case modeRocketLauncherRunabout:
    case modeFirstPersonRunabout:
    case modeM16FirstPersonRunabout:
    case modeFightCamRunabout:
    case modeHeliCannonFirstPerson:
    case modeCamera:
    case modeRocketLauncherHs:
    case modeRocketLauncherRunaboutHs:
    case modeAimWeapon:
    case modeAimWeaponAttached:
        return true;
    default:
        return false;
    }
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

    // A queued player weapon mode is only ever set while the player is aiming,
    // so any non-zero value here already means an aim camera is coming up or
    // running; the mode table is consulted for the per-CCam slots below.
    int16_t weaponMode{};
    if (SafeReadAimValue(camera + kCameraWeaponMode, weaponMode)
        && weaponMode != 0) {
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

// The aim guard pins both camera timesteps to the 50 FPS minimum for the whole
// of CCamera::Process. Three of this plugin's own corrections -- the two follow
// camera rates and the attached entity speed -- are patched into CCam methods
// that run inside that same call, and reading the pinned value would leave them
// short by exactly the frame-rate ratio they exist to remove. They read the
// real frame duration through here instead. Outside the guard this is just
// CTimer::ms_fTimeStep.
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

void __fastcall HookedCameraProcess(void* camera, void*) {
    const bool guard = IsAimCameraActive(camera);
    if (guard) {
        BeginAimTimeStepGuard();
    }
    g_originalCameraProcess(camera);
    if (guard) {
        EndAimTimeStepGuard();
    }
}

void __fastcall HookedProcessAimWeapon(void* cam, void*, const Vec3* target,
                                       float orientation, float speedVar,
                                       float speedVarWanted) {
    BeginAimTimeStepGuard();
    const Vec3 zero{};
    g_originalAimWeapon(cam, target ? *target : zero, orientation, speedVar,
                        speedVarWanted);
    EndAimTimeStepGuard();
}

void RemoveAimCameraHooks() {
    if (!g_aimMinHookInitialized) {
        return;
    }
    MH_DisableHook(reinterpret_cast<void*>(kCameraProcess));
    MH_DisableHook(reinterpret_cast<void*>(kProcessAimWeapon));
    MH_RemoveHook(reinterpret_cast<void*>(kCameraProcess));
    MH_RemoveHook(reinterpret_cast<void*>(kProcessAimWeapon));
    MH_Uninitialize();
    g_originalCameraProcess = nullptr;
    g_originalAimWeapon = nullptr;
    g_aimMinHookInitialized = false;
}

} // namespace hff
