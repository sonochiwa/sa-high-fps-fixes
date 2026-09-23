#pragma once

#include "modules/prelude.h"

namespace hff {

struct SitePatch {
    uintptr_t address{};
    std::array<uint8_t, 48> original{};
    // The branch and padding this plugin wrote, so the guard can tell a site
    // that still holds the patch from one another plugin has written over.
    std::array<uint8_t, 48> written{};
    // The bytes that followed the site when it was patched. A foreign patch
    // that is longer than this one leaves its NOP padding over them, and they
    // are what the guard puts back.
    std::array<uint8_t, 16> tail{};
    size_t size{};
    uint8_t reasserted{};
    bool installed{};
};

struct BytePatch {
    uintptr_t address{};
    uint8_t original{};
    bool installed{};
};

struct RawPatch {
    uintptr_t address{};
    std::array<uint8_t, 8> original{};
    size_t size{};
    bool installed{};
};

struct DetourPatch {
    uintptr_t address{};
    std::array<uint8_t, 16> original{};
    size_t size{};
    void* gateway{};
    bool installed{};
};

// The lowest limit an automatic case applies.
constexpr int kMinimumAutoLimit = 20;

// The FPS limit each automatic case applies, or 0 when the case is off.
struct AutoLimitCaps {
    int missions;
    int minigames;
    int schools;
    int cutscenes;
    int scriptedCutscenes;
    int pauseMenu;

    bool Any() const {
        return missions != 0 || minigames != 0 || schools != 0
            || cutscenes != 0 || scriptedCutscenes != 0 || pauseMenu != 0;
    }
};
extern HMODULE g_module;
extern SitePatch g_endTimerPatch;
extern SitePatch g_flightTimerPatch;
extern SitePatch g_continuousAmmoPatch;
extern std::array<SitePatch, 2> g_continuousShotPatches;
extern SitePatch g_chainsawStrikePatch;
extern SitePatch g_fightStrikeTracePatch;
extern float g_chainsawRewindOffset;
extern void* g_chainsawAnim;
extern uint32_t g_chainsawLastCall;
extern float g_chainsawCredit;
extern bool g_traceChainsaw;
extern uint64_t g_chainsawTraceLast;
extern uint32_t g_chainsawCalls;
extern uint32_t g_chainsawArms;
extern uint32_t g_chainsawStrikes;
extern int32_t g_chainsawCombo;
extern int32_t g_chainsawMove;
extern int32_t g_chainsawStrikeCombo;
extern int32_t g_chainsawStrikeMove;
extern float g_chainsawAnimStep;
extern float g_chainsawAnimTime;
extern SitePatch g_drowningDamagePatch;
extern SitePatch g_drunkCameraPhasePatch;
extern SitePatch g_aimWeaponFovStepPatch;
extern SitePatch g_aimingRifleWalkPatch;
extern SitePatch g_pedPushCarPatch;
extern SitePatch g_bloodyFootprintCounterPatch;
extern SitePatch g_bloodyFootLandedSidePatch;
extern SitePatch g_bloodyFootprintShadowPatch;
extern SitePatch g_skimmerResistancePatch;
extern SitePatch g_burnoutPatch;
extern SitePatch g_sirenPatch;
extern std::array<SitePatch, 4> g_fakePhysicsPatches;
extern std::array<SitePatch, 3> g_restThresholdPatches;
extern SitePatch g_bikeLeanTargetPatch;
extern SitePatch g_bikePitchExperimentPatch;
extern std::array<SitePatch, 2> g_bmxRiderFallTracePatches;
extern DetourPatch g_bmxLaunchBunnyHopPatch;
extern DetourPatch g_bikeDamageKnockOffPatch;
extern SitePatch g_scriptsProcessPatch;
extern SitePatch g_scriptSlideObjectPatch;
extern SitePatch g_scriptRotateObjectPatch;
extern SitePatch g_sampObjectRotationPatch;
extern uintptr_t g_sampObjectRotationReturn;
extern SitePatch g_sampObjectArrivalPatch;
extern uintptr_t g_sampObjectArrivedReturn;
extern uintptr_t g_sampObjectMovingReturn;
extern std::array<SitePatch, 3> g_fallingGlassPatches;
extern SitePatch g_breakObjectLifetimePatch;
extern SitePatch g_menuBackgroundPatch;
extern std::array<SitePatch, 5> g_wheelFrictionPatches;
extern SitePatch g_turnAirResistancePatch;
extern std::array<SitePatch, 4> g_steerInputPatches;
extern std::array<SitePatch, 2> g_transmissionPatches;
extern SitePatch g_swimmingPatch;
extern SitePatch g_climbSpeedPatch;
extern SitePatch g_moneyStepPatch;
extern SitePatch g_followPedCameraPatch;
extern SitePatch g_followCarCameraPatch;
extern SitePatch g_attachedEntitySpeedPatch;
extern SitePatch g_aiAircraftSteerPatch;
extern std::array<SitePatch, kStatTruncSites.size()> g_statTruncPatches;
extern std::array<SitePatch, 4> g_doorSwingPatches;
extern std::array<SitePatch, 6> g_wheelSpinPatches;
extern SitePatch g_boatEngineDampingPatch;
extern std::array<SitePatch, 3> g_swimPitchPatches;
extern SitePatch g_bmxSprintLeanPatch;
extern std::array<SitePatch, 5> g_bikeWheelSpinPatches;
extern std::array<SitePatch, 2> g_jetPackFxPatches;
extern std::array<SitePatch, 2> g_headBopPatches;
extern std::array<SitePatch, 4> g_bmxLeanPatches;
extern std::array<SitePatch, 6> g_jumpOutDampPatches;
extern std::array<SitePatch, 6> g_wheelSettlePatches;
extern SitePatch g_mapWheelSamplePatch;
extern SitePatch g_mapZoomInGatePatch;
extern SitePatch g_mapZoomOutGatePatch;
extern std::array<SitePatch, 3> g_fireGatePatches;
extern SitePatch g_drunkSteerPatch;
extern SitePatch g_fatCounterPatch;
extern SitePatch g_buoyancyThresholdPatch;
extern SitePatch g_buoyancyClampedStorePatch;
extern std::array<SitePatch, 4> g_railWheelSpinPatches;
extern std::array<SitePatch, 2> g_heliRotorPatches;
extern BytePatch g_frameLimiterGatePatch;
extern BytePatch g_frameLimitStorePatch;
extern DetourPatch g_fxCreateParticlesPatch;
extern DetourPatch g_fxAddParticlePatch;
extern float g_endTimerFraction;
extern float g_flightTimerFraction;
extern bool g_endTimerActive;
extern bool g_flightTimerActive;
extern bool g_loggingEnabled;
extern float g_originalTimeStepValue;
extern bool g_swingingDisabled;
extern uint32_t g_breakLifetimeLastFrame;
extern float g_breakLifetimeCarry;
extern int g_breakLifetimeTicks;
extern volatile uint32_t g_hudFlashClock;
extern volatile uint32_t g_hudVisibleClock;
extern unsigned g_hudFlashIntervalMs;
extern bool g_hudDisableFlashing;
extern std::atomic_bool g_hudFlashActive;
extern std::array<RawPatch, 6> g_hudFlashPatches;
extern int g_watchMode;
extern volatile uintptr_t g_watchCandidate;
extern volatile float g_leanTargetRaw;
extern float g_leanTargetHeld;
extern volatile DWORD g_gameThreadId;
extern std::atomic_bool g_diagnosticActive;
extern std::string g_diagnosticPath;
extern uint32_t g_fakePhysicsLastFrame;
extern float g_fakePhysicsCarry;
extern int32_t g_fakePhysicsTick;

struct HornTapState {
    uint32_t pressLastTime{};
    bool hasPressed{};
};
extern std::array<HornTapState, 2> g_hornTapStates;
extern int g_fpsLimit;
extern bool g_autoLimitTogglesGate;
extern bool g_gearChangeKick;
extern bool g_suspensionDampingLimit;
extern DetourPatch g_suspensionDampingPatch;
extern bool g_suspensionLoadLean;
extern DetourPatch g_physicalProcessControlPatch;
extern std::array<SitePatch, 2> g_wheelSlipPatches;
extern AutoLimitCaps g_autoLimit;

struct EmissionCarrySlot {
    void* blueprint{};
    float intensity{};
};

struct AmmoConsumptionSlot {
    void* weapon{};
    int32_t weaponType{};
    uint32_t lastUpdate{};
    float credit{};
};
extern std::array<EmissionCarrySlot, 16> g_weaponFxEmissionCarry;
extern std::array<AmmoConsumptionSlot, 16> g_ammoConsumptionSlots;
extern std::string g_iniPath;
extern std::string g_logPath;

struct ConfigKey {
    const char* section;
    const char* key;
};
extern std::array<ConfigKey, 128> g_knownConfigKeys;
extern size_t g_knownConfigKeyCount;
extern std::array<std::string, 32> g_configWarnings;
extern size_t g_configWarningCount;
extern std::array<char, 8192> g_iniSectionBuffer;
extern std::array<char, 16384> g_iniEntryBuffer;

enum class RegisteredPatchKind : uint8_t {
    site,
    byte,
    raw,
    detour,
};

struct RegisteredPatch {
    void* patch;
    RegisteredPatchKind kind;
};
extern std::array<RegisteredPatch, 512> g_installedPatches;
extern size_t g_installedPatchCount;
extern HANDLE g_workerStopEvent;
extern HANDLE g_hudFlashThread;
extern HANDLE g_vehicleTraceThread;
extern HANDLE g_pedTraceThread;
bool PinPluginModule(HINSTANCE instance);
bool StartWorkerThread(HANDLE& slot, LPTHREAD_START_ROUTINE entry);
bool WorkerStopRequested(DWORD timeoutMilliseconds);
bool StopAllWorkerThreads();
bool RegisterInstalledPatch(void* patch, RegisteredPatchKind kind);
void UnregisterInstalledPatch(const void* patch);
std::string ModulePathWithExtension(const char* extension);
void Log(const char* message);
bool WriteBytes(uintptr_t address, const uint8_t* bytes, size_t size);
bool MemoryMatchesRaw(uintptr_t address, const uint8_t* expected, size_t size);
bool CopyMemoryForDiagnostics(uintptr_t address, uint8_t* destination, size_t size);
void ReportPatchMismatch(uintptr_t address, const uint8_t* expected, size_t size);

template <size_t Size>
bool MemoryMatches(uintptr_t address,
                   const std::array<uint8_t, Size>& expected) {
    return MemoryMatchesRaw(address, expected.data(), expected.size());
}
bool SwimmingMovementCodeIsUnmodified();

// Every byte range this plugin has taken, so two fixes cannot claim the same
// instruction.
//
// This exists because it happened. `heliSpinUp` was written on 2026-08-26 after
// reading `CHeli::ProcessFlyingCarStuff` and finding a timer whose decay
// carried the timestep while its rise did not. That reading was correct and the
// fix was a duplicate: `heliRotorSpeed` had been patching the same two
// addresses, `0x6C4F29` and `0x6C4F37`, since long before, and doing it better.
// The byte check caught it in game, because the first fix had already replaced
// the bytes the second was matching against, and nothing shipped broken. But
// the byte check only catches an overlap when the first patch happens to change
// the bytes the second expects, which is luck rather than a guarantee.
//
// Refusing the second claim outright makes it a rule instead. The log line
// names both fixes, which is the thing that turns a silent skip into an
// obvious mistake.
struct ClaimedRange {
    uintptr_t begin;
    uintptr_t end;
};
extern std::array<ClaimedRange, 384> g_claimedRanges;
extern size_t g_claimedRangeCount;
bool ClaimPatchRange(uintptr_t address, size_t size);
void ReleasePatchRange(uintptr_t address);
extern bool g_overrideConflictingHooks;

struct ImageRange {
    uintptr_t begin{};
    uintptr_t end{};
};
ImageRange ModuleImageRange(HMODULE module);
extern ImageRange g_gameImage;
extern ImageRange g_pluginImage;
bool AddressInRange(uintptr_t address, const ImageRange& range);
bool SiteMayOverrideConflicts(uintptr_t address);
std::string ModuleNameForAddress(uintptr_t address);

struct ForeignBranch {
    bool found{};
    // True when the foreign padding may run past the end of this plugin's
    // span, so the bytes after it need restoring too.
    bool mayOverrun{};
    uintptr_t target{};
};
ForeignBranch AnalyzeForeignBranch(uintptr_t address, const uint8_t* reference, size_t size);
void LogSiteTakeover(uintptr_t address, uintptr_t target, const char* how);
void RecordSiteTail(SitePatch& patch);
bool InstallBranch(SitePatch& patch, uintptr_t address, const void* target, const uint8_t* expected, size_t size, uint8_t opcode);

template <size_t Size>
bool InstallJump(SitePatch& patch, uintptr_t address, const void* target,
                 const std::array<uint8_t, Size>& expected) {
    return InstallBranch(patch, address, target, expected.data(),
                         expected.size(), 0xE9);
}

template <size_t Size>
bool InstallCall(SitePatch& patch, uintptr_t address, const void* target,
                 const std::array<uint8_t, Size>& expected) {
    return InstallBranch(patch, address, target, expected.data(),
                         expected.size(), 0xE8);
}
void RestoreSite(SitePatch& patch);

// A plugin that keeps rewriting a site every frame would otherwise be fought
// forever; after this many rounds the site is conceded and reported once.
constexpr uint8_t kSiteReassertLimit = 3;
void ReassertSite(SitePatch& patch);
void __cdecl GuardInstalledSites();
bool InstallByte(BytePatch& patch, uintptr_t address, uint8_t value);
void RestoreByte(BytePatch& patch);
bool InstallRawPatch(RawPatch& patch, uintptr_t address, const uint8_t* replacement, size_t size);
void RestoreRawPatch(RawPatch& patch);
bool InstallDetour(DetourPatch& patch, uintptr_t address, const void* target, const uint8_t* expected, size_t size);
void RestoreDetour(DetourPatch& patch);
void RestoreAllPatches();

// Collects every patch installed by one fix and restores them in reverse order
// unless Commit is reached. Installers therefore describe only their forward
// path; an early return cannot leave half of a multi-site fix active.
class PatchSet {
public:
    explicit PatchSet(const char* name) : m_name(name) {}

    ~PatchSet() {
        if (!m_committed) {
            Rollback();
        }
    }

    bool Track(bool installed, SitePatch& patch) {
        return installed && Add(&patch, &RestoreSiteEntry);
    }

    bool Track(bool installed, DetourPatch& patch) {
        return installed && Add(&patch, &RestoreDetourEntry);
    }

    bool Track(bool installed, BytePatch& patch) {
        return installed && Add(&patch, &RestoreByteEntry);
    }

    bool Track(bool installed, RawPatch& patch) {
        return installed && Add(&patch, &RestoreRawEntry);
    }

    bool Commit() {
        m_committed = true;
        return true;
    }

private:
    struct Entry {
        void* patch;
        void (*restore)(void*);
    };

    static void RestoreSiteEntry(void* patch) {
        RestoreSite(*static_cast<SitePatch*>(patch));
    }

    static void RestoreDetourEntry(void* patch) {
        RestoreDetour(*static_cast<DetourPatch*>(patch));
    }

    static void RestoreByteEntry(void* patch) {
        RestoreByte(*static_cast<BytePatch*>(patch));
    }

    static void RestoreRawEntry(void* patch) {
        RestoreRawPatch(*static_cast<RawPatch*>(patch));
    }

    bool Add(void* patch, void (*restore)(void*)) {
        if (m_count == m_entries.size()) {
            restore(patch);
            char line[160];
            std::snprintf(line, sizeof(line),
                          "%s refused: patch transaction is too large.",
                          m_name);
            Log(line);
            return false;
        }
        m_entries[m_count++] = {patch, restore};
        return true;
    }

    void Rollback() {
        while (m_count != 0) {
            Entry& entry = m_entries[--m_count];
            entry.restore(entry.patch);
        }
    }

    const char* m_name;
    std::array<Entry, 128> m_entries{};
    size_t m_count{};
    bool m_committed{};
};

// Declarative installer for the common "N addresses, N thunks" patch shape.
// The transaction owns rollback, so callers only describe the patch table and
// the user-facing result. Two overloads cover a shared signature and a unique
// signature per site.
template <size_t Count, size_t Size>
bool InstallJumpTable(PatchSet& transaction,
                      std::array<SitePatch, Count>& patches,
                      const std::array<uintptr_t, Count>& addresses,
                      const std::array<const void*, Count>& targets,
                      const std::array<uint8_t, Size>& expected) {
    for (size_t i = 0; i < Count; ++i) {
        if (!transaction.Track(
                InstallJump(patches[i], addresses[i], targets[i], expected),
                patches[i])) {
            return false;
        }
    }
    return true;
}

template <size_t Count, size_t Size>
bool InstallJumpTable(
    PatchSet& transaction, std::array<SitePatch, Count>& patches,
    const std::array<uintptr_t, Count>& addresses,
    const std::array<const void*, Count>& targets,
    const std::array<std::array<uint8_t, Size>, Count>& expected) {
    for (size_t i = 0; i < Count; ++i) {
        if (!transaction.Track(
                InstallJump(patches[i], addresses[i], targets[i], expected[i]),
                patches[i])) {
            return false;
        }
    }
    return true;
}
float TimeStepRatio();
float ReadGameFloat(uintptr_t address, float fallback);
int AccumulateMilliseconds(float milliseconds, float& fraction);
int __cdecl AccumulateFlightTimer(float milliseconds);
int __cdecl AccumulateEndTimer(float milliseconds);

} // namespace hff
