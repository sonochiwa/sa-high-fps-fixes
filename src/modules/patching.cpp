#include "modules/modules.h"

namespace hff {

HMODULE g_module{};

SitePatch g_endTimerPatch{};
SitePatch g_flightTimerPatch{};
SitePatch g_continuousAmmoPatch{};
SitePatch g_chainsawStrikePatch{};
SitePatch g_fightStrikeTracePatch{};
float g_chainsawRewindOffset{kChainsawStockRewind};
void* g_chainsawAnim{};
uint32_t g_chainsawLastCall{};
float g_chainsawCredit{};
bool g_traceChainsaw{};
uint64_t g_chainsawTraceLast{};
uint32_t g_chainsawCalls{};
uint32_t g_chainsawArms{};
uint32_t g_chainsawStrikes{};
int32_t g_chainsawCombo{-1};
int32_t g_chainsawMove{-1};
int32_t g_chainsawStrikeCombo{-1};
int32_t g_chainsawStrikeMove{-1};
float g_chainsawAnimStep{};
float g_chainsawAnimTime{};
SitePatch g_drowningDamagePatch{};
SitePatch g_drunkCameraPhasePatch{};
SitePatch g_aimWeaponFovStepPatch{};
SitePatch g_aimingRifleWalkPatch{};
SitePatch g_pedPushCarPatch{};
SitePatch g_bloodyFootprintCounterPatch{};
SitePatch g_bloodyFootLandedSidePatch{};
SitePatch g_bloodyFootprintShadowPatch{};
SitePatch g_skimmerResistancePatch{};
SitePatch g_burnoutPatch{};
SitePatch g_sirenPatch{};
std::array<SitePatch, 4> g_fakePhysicsPatches{};
std::array<SitePatch, 3> g_restThresholdPatches{};
SitePatch g_bikeLeanTargetPatch{};
SitePatch g_bikePitchExperimentPatch{};
std::array<SitePatch, 2> g_bmxRiderFallTracePatches{};
DetourPatch g_bmxLaunchBunnyHopPatch{};
DetourPatch g_bikeDamageKnockOffPatch{};
SitePatch g_scriptsProcessPatch{};
SitePatch g_scriptSlideObjectPatch{};
SitePatch g_scriptRotateObjectPatch{};
SitePatch g_sampObjectRotationPatch{};
uintptr_t g_sampObjectRotationReturn{};
SitePatch g_sampObjectArrivalPatch{};
uintptr_t g_sampObjectArrivedReturn{};
uintptr_t g_sampObjectMovingReturn{};
std::array<SitePatch, 3> g_fallingGlassPatches{};
SitePatch g_breakObjectLifetimePatch{};
SitePatch g_menuBackgroundPatch{};
std::array<SitePatch, 5> g_wheelFrictionPatches{};
SitePatch g_turnAirResistancePatch{};
std::array<SitePatch, 4> g_steerInputPatches{};
std::array<SitePatch, 2> g_transmissionPatches{};
bool g_gearChangeKick = false;
bool g_suspensionDampingLimit = false;
DetourPatch g_suspensionDampingPatch{};
bool g_suspensionLoadLean = false;
DetourPatch g_physicalProcessControlPatch{};
std::array<SitePatch, 2> g_wheelSlipPatches{};
SitePatch g_swimmingPatch{};
SitePatch g_climbSpeedPatch{};
SitePatch g_moneyStepPatch{};
SitePatch g_followPedCameraPatch{};
SitePatch g_followCarCameraPatch{};
SitePatch g_attachedEntitySpeedPatch{};
SitePatch g_aiAircraftSteerPatch{};
std::array<SitePatch, kStatTruncSites.size()> g_statTruncPatches{};
std::array<SitePatch, 4> g_doorSwingPatches{};
std::array<SitePatch, 6> g_wheelSpinPatches{};
SitePatch g_boatEngineDampingPatch{};
std::array<SitePatch, 3> g_swimPitchPatches{};
SitePatch g_bmxSprintLeanPatch{};
std::array<SitePatch, 5> g_bikeWheelSpinPatches{};
std::array<SitePatch, 2> g_jetPackFxPatches{};
std::array<SitePatch, 2> g_headBopPatches{};
std::array<SitePatch, 4> g_bmxLeanPatches{};
std::array<SitePatch, 6> g_jumpOutDampPatches{};
std::array<SitePatch, 6> g_wheelSettlePatches{};
SitePatch g_mapWheelSamplePatch{};
SitePatch g_mapZoomInGatePatch{};
SitePatch g_mapZoomOutGatePatch{};
std::array<SitePatch, 3> g_fireGatePatches{};
SitePatch g_drunkSteerPatch{};
SitePatch g_fatCounterPatch{};
SitePatch g_buoyancyThresholdPatch{};
SitePatch g_buoyancyClampedStorePatch{};
std::array<SitePatch, 4> g_railWheelSpinPatches{};
std::array<SitePatch, 2> g_heliRotorPatches{};
BytePatch g_frameLimiterGatePatch{};
BytePatch g_frameLimitStorePatch{};
DetourPatch g_fxCreateParticlesPatch{};
DetourPatch g_fxAddParticlePatch{};

float g_endTimerFraction{};
float g_flightTimerFraction{};
bool g_endTimerActive{};
bool g_flightTimerActive{};
bool g_loggingEnabled{};
float g_originalTimeStepValue{kOriginalTimeStep};
bool g_swingingDisabled{};

uint32_t g_breakLifetimeLastFrame{0xFFFFFFFFu};
float g_breakLifetimeCarry{};
int g_breakLifetimeTicks{};

// Only the low byte is ever read by the patched instructions, but the counters
// are full dwords so that a dword read would also see a sane value.
volatile uint32_t g_hudFlashClock{};
volatile uint32_t g_hudVisibleClock{};
unsigned g_hudFlashIntervalMs{kDefaultHudFlashIntervalMs};
bool g_hudDisableFlashing{};
std::atomic_bool g_hudFlashActive{};
std::array<RawPatch, 6> g_hudFlashPatches{};

// 0 watches a stationary ridden bike, 1 watches the vehicle being pushed.
int g_watchMode = 0;
volatile uintptr_t g_watchCandidate{};
volatile float g_leanTargetRaw{};
float g_leanTargetHeld{};
volatile DWORD g_gameThreadId{};
std::atomic_bool g_diagnosticActive{};
std::string g_diagnosticPath;

uint32_t g_fakePhysicsLastFrame{0xFFFFFFFF};
float g_fakePhysicsCarry{};
int32_t g_fakePhysicsTick{1};

std::array<HornTapState, 2> g_hornTapStates{};

int g_fpsLimit{};
bool g_autoLimitTogglesGate{};
AutoLimitCaps g_autoLimit{};

std::array<EmissionCarrySlot, 16> g_weaponFxEmissionCarry{};
std::array<AmmoConsumptionSlot, 16> g_ammoConsumptionSlots{};
std::string g_iniPath;
std::string g_logPath;

std::array<ConfigKey, 128> g_knownConfigKeys{};
size_t g_knownConfigKeyCount{};
std::array<std::string, 32> g_configWarnings{};
size_t g_configWarningCount{};
std::array<char, 8192> g_iniSectionBuffer{};
std::array<char, 16384> g_iniEntryBuffer{};

std::array<RegisteredPatch, 512> g_installedPatches{};
size_t g_installedPatchCount{};

HANDLE g_workerStopEvent{};
HANDLE g_hudFlashThread{};
HANDLE g_vehicleTraceThread{};
HANDLE g_pedTraceThread{};

bool PinPluginModule(HINSTANCE instance) {
    HMODULE pinned{};
    return GetModuleHandleExA(
               GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                   | GET_MODULE_HANDLE_EX_FLAG_PIN,
               reinterpret_cast<LPCSTR>(instance), &pinned)
        != FALSE;
}

bool StartWorkerThread(HANDLE& slot, LPTHREAD_START_ROUTINE entry) {
    if (slot) {
        return false;
    }
    if (!g_workerStopEvent) {
        g_workerStopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!g_workerStopEvent) {
            return false;
        }
    }
    slot = CreateThread(nullptr, 0, entry, nullptr, 0, nullptr);
    return slot != nullptr;
}

bool WorkerStopRequested(DWORD timeoutMilliseconds) {
    return g_workerStopEvent
        && WaitForSingleObject(g_workerStopEvent, timeoutMilliseconds)
               == WAIT_OBJECT_0;
}

bool StopAllWorkerThreads() {
    g_hudFlashActive = false;
    g_diagnosticActive = false;
    if (g_workerStopEvent) {
        SetEvent(g_workerStopEvent);
    }

    HANDLE handles[] = {
        g_hudFlashThread, g_vehicleTraceThread, g_pedTraceThread
    };
    HANDLE waiting[3]{};
    DWORD count{};
    for (const HANDLE handle : handles) {
        if (handle) {
            waiting[count++] = handle;
        }
    }
    if (count != 0) {
        const DWORD wait = WaitForMultipleObjects(count, waiting, TRUE, 5000);
        if (wait != WAIT_OBJECT_0) {
            Log("Shutdown deferred: a worker thread did not stop safely.");
            return false;
        }
    }
    for (HANDLE* slot : {&g_hudFlashThread, &g_vehicleTraceThread,
                         &g_pedTraceThread}) {
        if (*slot) {
            CloseHandle(*slot);
            *slot = nullptr;
        }
    }
    if (g_workerStopEvent) {
        CloseHandle(g_workerStopEvent);
        g_workerStopEvent = nullptr;
    }
    return true;
}

bool RegisterInstalledPatch(void* patch, RegisteredPatchKind kind) {
    for (size_t i = 0; i < g_installedPatchCount; ++i) {
        if (g_installedPatches[i].patch == patch) {
            return true;
        }
    }
    if (g_installedPatchCount == g_installedPatches.size()) {
        Log("Patch installation refused: restoration registry is full.");
        return false;
    }
    g_installedPatches[g_installedPatchCount++] = {patch, kind};
    return true;
}

void UnregisterInstalledPatch(const void* patch) {
    for (size_t i = 0; i < g_installedPatchCount; ++i) {
        if (g_installedPatches[i].patch != patch) {
            continue;
        }
        for (size_t move = i + 1; move < g_installedPatchCount; ++move) {
            g_installedPatches[move - 1] = g_installedPatches[move];
        }
        --g_installedPatchCount;
        return;
    }
}

} // namespace hff
