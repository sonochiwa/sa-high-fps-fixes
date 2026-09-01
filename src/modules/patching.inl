// ---------------------------------------------------------------------------
// Patch bookkeeping
// ---------------------------------------------------------------------------

struct SitePatch {
    uintptr_t address{};
    std::array<uint8_t, 48> original{};
    size_t size{};
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

struct AbsoluteOperandPatch {
    uintptr_t instruction{};
    std::array<uint8_t, 6> expected{};
    bool installed{};
};

union AutoLimitFlags {
    uint32_t value;
    struct {
        uint32_t forMissions : 1;
        uint32_t forMinigames : 1;
        uint32_t forSchools : 1;
        uint32_t forCutscenes : 1;
        uint32_t forScriptedCutscenes : 1;
        uint32_t forPauseMenu : 1;
    } flags;
};

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
std::array<SitePatch, 6> g_moveSpeedSnapPatches{};
SitePatch g_turnAirResistancePatch{};
SitePatch g_groundFrictionPatch{};
SitePatch g_bikeLeanTargetPatch{};
SitePatch g_bikePitchExperimentPatch{};
std::array<SitePatch, 2> g_bmxRiderFallTracePatches{};
DetourPatch g_bmxLaunchBunnyHopPatch{};
DetourPatch g_bikeDamageKnockOffPatch{};
DetourPatch g_suspensionDampingPatch{};
// Scratch for the six move speed snap thunks. The game is single threaded
// through vehicle processing, and each thunk writes it and reads it back before
// the next instruction.
float g_scaledMoveSpeedSnap{};
SitePatch g_scriptsProcessPatch{};
SitePatch g_scriptSlideObjectPatch{};
SitePatch g_scriptRotateObjectPatch{};
std::array<SitePatch, 3> g_fallingGlassPatches{};
SitePatch g_breakObjectLifetimePatch{};
SitePatch g_menuBackgroundPatch{};
std::array<SitePatch, 5> g_wheelFrictionPatches{};
SitePatch g_swimmingPatch{};
SitePatch g_climbSpeedPatch{};
SitePatch g_moneyStepPatch{};
SitePatch g_followPedCameraPatch{};
SitePatch g_followCarCameraPatch{};
SitePatch g_attachedEntitySpeedPatch{};
SitePatch g_aiAircraftSteerPatch{};
std::array<SitePatch, kStatTruncSites.size()> g_statTruncPatches{};
SitePatch g_rollOntoWheelsTurnPatch{};
SitePatch g_rollOntoWheelsMovePatch{};
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
std::array<SitePatch, 6> g_pushOutPatches{};
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
BytePatch g_refreshRatePatch{};
DetourPatch g_fxCreateParticlesPatch{};
DetourPatch g_fxAddParticlePatch{};
DetourPatch g_aimWeaponPatch{};

float g_endTimerFraction{};
float g_flightTimerFraction{};
bool g_endTimerActive{};
bool g_flightTimerActive{};
bool g_loggingEnabled{true};
float g_aimTimeStep{1.0f};
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

struct HornTapState {
    uint32_t pressLastTime{};
    bool hasPressed{};
};

std::array<HornTapState, 2> g_hornTapStates{};

int g_fpsLimit{};
int g_refreshRate{};
int g_lastFpsLimit{};
bool g_isOnPauseMenu{};
AutoLimitFlags g_autoLimit{};

std::array<AbsoluteOperandPatch, 13> g_aimTimeStepPatches{{
    {0x0052167A, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x00521752, {0xD8, 0x1D, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x00521765, {0xD8, 0x25, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005217C9, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005217DE, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x0052191B, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x00521F40, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x0052210A, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x0052233B, {0xD8, 0x0D, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x00522369, {0xD8, 0x0D, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005223AA, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005224E4, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
    {0x005226CF, {0xD9, 0x05, 0x5C, 0xCB, 0xB7, 0x00}},
}};

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

std::array<EmissionCarrySlot, 16> g_weaponFxEmissionCarry{};
std::array<AmmoConsumptionSlot, 16> g_ammoConsumptionSlots{};
std::string g_iniPath;
std::string g_logPath;

struct ConfigKey {
    const char* section;
    const char* key;
};

std::array<ConfigKey, 128> g_knownConfigKeys{};
size_t g_knownConfigKeyCount{};
std::array<std::string, 32> g_configWarnings{};
size_t g_configWarningCount{};
std::array<char, 8192> g_iniSectionBuffer{};
std::array<char, 16384> g_iniEntryBuffer{};

void Log(const char* message);

enum class RegisteredPatchKind : uint8_t {
    site,
    byte,
    raw,
    detour,
    absoluteOperand,
};

struct RegisteredPatch {
    void* patch;
    RegisteredPatchKind kind;
};

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
            g_loggingEnabled = true;
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
        g_loggingEnabled = true;
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

// ---------------------------------------------------------------------------
// Infrastructure
// ---------------------------------------------------------------------------

std::string ModulePathWithExtension(const char* extension) {
    std::array<char, MAX_PATH> path{};
    const DWORD length = GetModuleFileNameA(g_module, path.data(),
                                            static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return {};
    }

    std::string result(path.data(), length);
    const size_t slash = result.find_last_of("\\/");
    const size_t dot = result.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        result.resize(dot);
    }
    result += extension;
    return result;
}

void Log(const char* message) {
    if (!g_loggingEnabled || g_logPath.empty()) {
        return;
    }

    FILE* file{};
    if (fopen_s(&file, g_logPath.c_str(), "a") == 0 && file) {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        std::fprintf(file, "[%02u:%02u:%02u] %s\n", time.wHour, time.wMinute,
                     time.wSecond, message);
        std::fclose(file);
    }
}

bool CreateDefaultIniIfMissing() {
    if (g_iniPath.empty()) {
        return false;
    }
    if (GetFileAttributesA(g_iniPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }

    HANDLE file = CreateFileA(g_iniPath.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written{};
    constexpr DWORD size = static_cast<DWORD>(sizeof(kDefaultIni) - 1);
    const bool ok = WriteFile(file, kDefaultIni, size, &written, nullptr) != FALSE
                 && written == size;
    CloseHandle(file);
    return ok;
}

struct IniCompletionResult {
    size_t added{};
    bool complete{true};
};

// Add only settings represented by the embedded canonical INI. Profile writes
// insert a key into its existing section (or append a missing section) without
// replacing the file, so user values, ordering, blank lines, comments and
// non-canonical diagnostic settings survive an upgrade. Comments from the
// template are deliberately not restored: deleting one is a harmless user edit
// and must not make every launch rewrite the file.
IniCompletionResult CompleteIniWithMissingDefaults() {
    IniCompletionResult result{};
    if (g_iniPath.empty()) {
        result.complete = false;
        return result;
    }

    constexpr char missingValue[] = "\x1Dhigh-fps-fixes-missing\x1D";
    std::string section;
    const char* cursor = kDefaultIni;
    while (*cursor) {
        const char* newline = std::strchr(cursor, '\n');
        const size_t length = newline
                                ? static_cast<size_t>(newline - cursor)
                                : std::strlen(cursor);
        std::string line(cursor, length);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.size() >= 3 && line.front() == '[' && line.back() == ']') {
            section.assign(line.data() + 1, line.size() - 2);
        } else if (!section.empty() && !line.empty()
                   && line.front() != '#' && line.front() != ';') {
            const size_t equals = line.find('=');
            if (equals != std::string::npos && equals != 0) {
                const std::string key = line.substr(0, equals);
                const std::string defaultValue = line.substr(equals + 1);
                std::array<char, 128> existing{};
                GetPrivateProfileStringA(
                    section.c_str(), key.c_str(), missingValue,
                    existing.data(), static_cast<DWORD>(existing.size()),
                    g_iniPath.c_str());
                if (std::strcmp(existing.data(), missingValue) == 0) {
                    if (WritePrivateProfileStringA(
                            section.c_str(), key.c_str(),
                            defaultValue.c_str(), g_iniPath.c_str())) {
                        ++result.added;
                    } else {
                        result.complete = false;
                    }
                }
            }
        }

        if (!newline) {
            break;
        }
        cursor = newline + 1;
    }

    if (result.added != 0) {
        // The all-null form only flushes the profile API cache. Some Windows
        // versions return zero for this form even when every preceding write
        // succeeded, so it must not turn a successful migration into a warning.
        WritePrivateProfileStringA(nullptr, nullptr, nullptr,
                                   g_iniPath.c_str());
    }
    return result;
}

void RegisterConfigKey(const char* section, const char* key) {
    for (size_t i = 0; i < g_knownConfigKeyCount; ++i) {
        if (_stricmp(g_knownConfigKeys[i].section, section) == 0
            && _stricmp(g_knownConfigKeys[i].key, key) == 0) {
            return;
        }
    }
    if (g_knownConfigKeyCount < g_knownConfigKeys.size()) {
        g_knownConfigKeys[g_knownConfigKeyCount++] = {section, key};
    }
}

void AddConfigWarning(const char* section, const char* key,
                      const char* reason) {
    if (g_configWarningCount >= g_configWarnings.size()) {
        return;
    }
    std::string warning("Configuration warning: [");
    warning += section;
    warning += "] ";
    warning += key;
    warning += " ";
    warning += reason;
    g_configWarnings[g_configWarningCount++] = warning;
}

bool ReadSetting(const char* section, const char* key, bool defaultValue) {
    RegisterConfigKey(section, key);
    std::array<char, 64> value{};
    GetPrivateProfileStringA(section, key, "", value.data(),
                             static_cast<DWORD>(value.size()),
                             g_iniPath.c_str());
    if (value[0] == '\0') {
        return defaultValue;
    }
    if (std::strcmp(value.data(), "0") == 0) {
        return false;
    }
    if (std::strcmp(value.data(), "1") == 0) {
        return true;
    }
    AddConfigWarning(section, key, "must be 0 or 1; using its default.");
    return defaultValue;
}

int ReadNumber(const char* section, const char* key, int defaultValue) {
    RegisterConfigKey(section, key);
    std::array<char, 64> value{};
    GetPrivateProfileStringA(section, key, "", value.data(),
                             static_cast<DWORD>(value.size()),
                             g_iniPath.c_str());
    if (value[0] == '\0') {
        return defaultValue;
    }
    char* end{};
    const long parsed = std::strtol(value.data(), &end, 10);
    while (end && *end == ' ') {
        ++end;
    }
    if (!end || *end != '\0'
        || parsed < std::numeric_limits<int>::min()
        || parsed > std::numeric_limits<int>::max()) {
        AddConfigWarning(section, key,
                         "must be an integer; using its default.");
        return defaultValue;
    }
    return static_cast<int>(parsed);
}

bool IsKnownConfigKey(const char* section, const char* key) {
    for (size_t i = 0; i < g_knownConfigKeyCount; ++i) {
        if (_stricmp(g_knownConfigKeys[i].section, section) == 0
            && _stricmp(g_knownConfigKeys[i].key, key) == 0) {
            return true;
        }
    }
    return false;
}

void RegisterConditionalConfigKeys() {
    constexpr ConfigKey keys[] = {
        {"vehicles", "bikePitchExperimentStrength"},
        {"vehicles", "disableSwingingCompletely"},
        {"particles", "particlesPerSecond"},
        {"hud", "disableFlashing"},
        {"general", "traceWatchOffset"},
        {"general", "traceWatchMode"},
        {"general", "traceWatchHits"},
        {"general", "traceWatchSamples"},
        {"general", "traceWatchArmDelay"},
        {"general", "traceWatchReports"},
    };
    for (const auto& item : keys) {
        RegisterConfigKey(item.section, item.key);
    }
}

void ValidateUnknownConfigKeys() {
    g_iniSectionBuffer.fill('\0');
    GetPrivateProfileSectionNamesA(g_iniSectionBuffer.data(),
                                   static_cast<DWORD>(g_iniSectionBuffer.size()),
                                   g_iniPath.c_str());
    for (const char* section = g_iniSectionBuffer.data(); *section;
         section += std::strlen(section) + 1) {
        g_iniEntryBuffer.fill('\0');
        GetPrivateProfileSectionA(section, g_iniEntryBuffer.data(),
                                  static_cast<DWORD>(g_iniEntryBuffer.size()),
                                  g_iniPath.c_str());
        for (const char* entry = g_iniEntryBuffer.data(); *entry;
             entry += std::strlen(entry) + 1) {
            const char* equals = std::strchr(entry, '=');
            if (!equals) {
                continue;
            }
            std::string key(entry, static_cast<size_t>(equals - entry));
            if (!IsKnownConfigKey(section, key.c_str())) {
                AddConfigWarning(section, key.c_str(), "is not recognized.");
            }
        }
    }
}

void ReportConfigWarnings() {
    if (g_configWarningCount == 0) {
        return;
    }
    const bool loggingWasEnabled = g_loggingEnabled;
    g_loggingEnabled = true;
    if (!loggingWasEnabled) {
        Log("Logging enabled because the INI contains configuration warnings.");
    }
    for (size_t i = 0; i < g_configWarningCount; ++i) {
        Log(g_configWarnings[i].c_str());
    }
}

bool WriteBytes(uintptr_t address, const uint8_t* bytes, size_t size) {
    DWORD oldProtect{};
    void* destination = reinterpret_cast<void*>(address);
    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    std::memcpy(destination, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), destination, size);

    DWORD ignored{};
    VirtualProtect(destination, size, oldProtect, &ignored);
    return true;
}

bool MemoryMatchesRaw(uintptr_t address, const uint8_t* expected, size_t size) {
    __try {
        return std::memcmp(reinterpret_cast<const void*>(address), expected,
                           size) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool CopyMemoryForDiagnostics(uintptr_t address, uint8_t* destination,
                              size_t size) {
    __try {
        std::memcpy(destination, reinterpret_cast<const void*>(address), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void ReportPatchMismatch(uintptr_t address, const uint8_t* expected,
                         size_t size) {
    std::array<uint8_t, 48> actual{};
    if (size > actual.size()) {
        size = actual.size();
    }
    const bool readable =
        CopyMemoryForDiagnostics(address, actual.data(), size);

    std::string message("Patch mismatch at 0x");
    char number[24];
    std::snprintf(number, sizeof(number), "%08X",
                  static_cast<unsigned>(address));
    message += number;
    message += ": expected";
    for (size_t i = 0; i < size; ++i) {
        char byte[5];
        std::snprintf(byte, sizeof(byte), " %02X", expected[i]);
        message += byte;
    }
    message += readable ? ", found" : ", memory is unreadable";
    if (readable) {
        for (size_t i = 0; i < size; ++i) {
            char byte[5];
            std::snprintf(byte, sizeof(byte), " %02X", actual[i]);
            message += byte;
        }
    }
    message += ".";

    // A failed patch is actionable even when routine logging is disabled.
    // Enable the log from this point onward so the cause and final summary are
    // available without asking the player to reproduce the failure first.
    g_loggingEnabled = true;
    Log(message.c_str());
}

template <size_t Size>
bool MemoryMatches(uintptr_t address,
                   const std::array<uint8_t, Size>& expected) {
    return MemoryMatchesRaw(address, expected.data(), expected.size());
}

bool SwimmingMovementCodeIsUnmodified() {
    return MemoryMatches(kSwimDiveScale, kExpectedSwimDiveScale)
        && MemoryMatches(kSwimAscentBias, kExpectedSwimAscentBias)
        && MemoryMatches(kSwimVectorSetup, kExpectedSwimVectorSetup)
        && MemoryMatches(kSwimVectorTransform,
                         kExpectedSwimVectorTransform);
}


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

std::array<ClaimedRange, 384> g_claimedRanges{};
size_t g_claimedRangeCount{};

bool ClaimPatchRange(uintptr_t address, size_t size) {
    const uintptr_t begin = address;
    const uintptr_t end = address + size;
    for (size_t i = 0; i < g_claimedRangeCount; ++i) {
        if (begin < g_claimedRanges[i].end
            && g_claimedRanges[i].begin < end) {
            char line[160];
            std::snprintf(line, sizeof(line),
                          "Patch site refused: 0x%08X..0x%08X overlaps "
                          "0x%08X..0x%08X, already patched by another fix.",
                          static_cast<unsigned>(begin),
                          static_cast<unsigned>(end),
                          static_cast<unsigned>(g_claimedRanges[i].begin),
                          static_cast<unsigned>(g_claimedRanges[i].end));
            Log(line);
            return false;
        }
    }
    if (g_claimedRangeCount >= g_claimedRanges.size()) {
        Log("Patch site refused: the claimed range table is full.");
        return false;
    }
    g_claimedRanges[g_claimedRangeCount++] = {begin, end};
    return true;
}

void ReleasePatchRange(uintptr_t address) {
    for (size_t i = 0; i < g_claimedRangeCount; ++i) {
        if (g_claimedRanges[i].begin == address) {
            g_claimedRanges[i] = g_claimedRanges[--g_claimedRangeCount];
            return;
        }
    }
}
// Replaces `size` original bytes with a relative branch to `target` and pads
// the remainder with NOPs. `opcode` is 0xE8 for a call or 0xE9 for a jump.
bool InstallBranch(SitePatch& patch, uintptr_t address, const void* target,
                   const uint8_t* expected, size_t size, uint8_t opcode) {
    if (size < 5 || size > patch.original.size()) {
        Log("Patch site rejected: the site is larger than a patch record.");
        return false;
    }
    if (!MemoryMatchesRaw(address, expected, size)) {
        ReportPatchMismatch(address, expected, size);
        return false;
    }

    const intptr_t displacement = reinterpret_cast<intptr_t>(target)
                                - static_cast<intptr_t>(address + 5);
    if (displacement < std::numeric_limits<int32_t>::min()
        || displacement > std::numeric_limits<int32_t>::max()) {
        return false;
    }

    if (!ClaimPatchRange(address, size)) {
        return false;
    }

    patch.address = address;
    patch.size = size;
    std::memcpy(patch.original.data(), expected, size);

    std::array<uint8_t, 48> replacement{};
    replacement.fill(0x90);
    replacement[0] = opcode;
    const int32_t relative = static_cast<int32_t>(displacement);
    std::memcpy(replacement.data() + 1, &relative, sizeof(relative));
    patch.installed = WriteBytes(address, replacement.data(), size);
    if (!patch.installed) {
        ReleasePatchRange(address);
    } else if (!RegisterInstalledPatch(&patch, RegisteredPatchKind::site)) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
        ReleasePatchRange(address);
        patch.installed = false;
    }
    return patch.installed;
}

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

void RestoreSite(SitePatch& patch) {
    if (patch.installed) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
        ReleasePatchRange(patch.address);
        patch.installed = false;
        UnregisterInstalledPatch(&patch);
    }
}

bool InstallByte(BytePatch& patch, uintptr_t address, uint8_t value) {
    __try {
        patch.original = *reinterpret_cast<const uint8_t*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (!ClaimPatchRange(address, 1)) {
        return false;
    }
    patch.address = address;
    patch.installed = WriteBytes(address, &value, 1);
    if (!patch.installed) {
        ReleasePatchRange(address);
    } else if (!RegisterInstalledPatch(&patch, RegisteredPatchKind::byte)) {
        WriteBytes(patch.address, &patch.original, 1);
        ReleasePatchRange(address);
        patch.installed = false;
    }
    return patch.installed;
}

void RestoreByte(BytePatch& patch) {
    if (patch.installed) {
        WriteBytes(patch.address, &patch.original, 1);
        ReleasePatchRange(patch.address);
        patch.installed = false;
        UnregisterInstalledPatch(&patch);
    }
}

bool InstallRawPatch(RawPatch& patch, uintptr_t address,
                     const uint8_t* replacement, size_t size) {
    if (size == 0 || size > patch.original.size()) {
        return false;
    }
    __try {
        std::memcpy(patch.original.data(),
                    reinterpret_cast<const void*>(address), size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (!ClaimPatchRange(address, size)) {
        return false;
    }
    patch.address = address;
    patch.size = size;
    patch.installed = WriteBytes(address, replacement, size);
    if (!patch.installed) {
        ReleasePatchRange(address);
    } else if (!RegisterInstalledPatch(&patch, RegisteredPatchKind::raw)) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
        ReleasePatchRange(address);
        patch.installed = false;
    }
    return patch.installed;
}

void RestoreRawPatch(RawPatch& patch) {
    if (patch.installed) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
        ReleasePatchRange(patch.address);
        patch.installed = false;
        UnregisterInstalledPatch(&patch);
    }
}

void RestoreAbsoluteOperand(AbsoluteOperandPatch& patch) {
    if (patch.installed) {
        WriteBytes(patch.instruction + 2, patch.expected.data() + 2, 4);
        ReleasePatchRange(patch.instruction + 2);
        patch.installed = false;
        UnregisterInstalledPatch(&patch);
    }
}

void RestoreAbsoluteOperandPatches(
    std::array<AbsoluteOperandPatch, 13>& patches) {
    for (auto& patch : patches) {
        RestoreAbsoluteOperand(patch);
    }
}

bool InstallAimTimeStepOperands() {
    for (const auto& patch : g_aimTimeStepPatches) {
        if (!MemoryMatches(patch.instruction, patch.expected)) {
            return false;
        }
    }

    const uintptr_t replacement = reinterpret_cast<uintptr_t>(&g_aimTimeStep);
    for (auto& patch : g_aimTimeStepPatches) {
        const uintptr_t operand = patch.instruction + 2;
        if (!ClaimPatchRange(operand, sizeof(replacement))) {
            RestoreAbsoluteOperandPatches(g_aimTimeStepPatches);
            return false;
        }
        if (!WriteBytes(operand,
                        reinterpret_cast<const uint8_t*>(&replacement),
                        sizeof(replacement))) {
            ReleasePatchRange(operand);
            RestoreAbsoluteOperandPatches(g_aimTimeStepPatches);
            return false;
        }
        patch.installed = true;
        if (!RegisterInstalledPatch(&patch,
                                    RegisteredPatchKind::absoluteOperand)) {
            RestoreAbsoluteOperandPatches(g_aimTimeStepPatches);
            return false;
        }
    }
    return true;
}

bool InstallDetour(DetourPatch& patch, uintptr_t address, const void* target,
                   const uint8_t* expected, size_t size) {
    if (size < 5 || size > patch.original.size()) {
        return false;
    }
    if (!MemoryMatchesRaw(address, expected, size)) {
        ReportPatchMismatch(address, expected, size);
        return false;
    }
    if (!ClaimPatchRange(address, size)) {
        return false;
    }

    auto* gateway = static_cast<uint8_t*>(VirtualAlloc(
        nullptr, size + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!gateway) {
        ReleasePatchRange(address);
        return false;
    }
    std::memcpy(gateway, reinterpret_cast<const void*>(address), size);
    gateway[size] = 0xE9;
    const int32_t gatewayBack = static_cast<int32_t>(
        address + size - reinterpret_cast<uintptr_t>(gateway + size + 5));
    std::memcpy(gateway + size + 1, &gatewayBack, sizeof(gatewayBack));

    const intptr_t displacement = reinterpret_cast<intptr_t>(target)
                                - static_cast<intptr_t>(address + 5);
    if (displacement < std::numeric_limits<int32_t>::min()
        || displacement > std::numeric_limits<int32_t>::max()) {
        ReleasePatchRange(address);
        VirtualFree(gateway, 0, MEM_RELEASE);
        return false;
    }

    patch.address = address;
    patch.size = size;
    patch.gateway = gateway;
    std::memcpy(patch.original.data(), expected, size);
    std::array<uint8_t, 16> replacement{};
    replacement.fill(0x90);
    replacement[0] = 0xE9;
    const int32_t relative = static_cast<int32_t>(displacement);
    std::memcpy(replacement.data() + 1, &relative, sizeof(relative));
    patch.installed = WriteBytes(address, replacement.data(), size);
    if (!patch.installed) {
        ReleasePatchRange(address);
        VirtualFree(gateway, 0, MEM_RELEASE);
        patch.gateway = nullptr;
    } else if (!RegisterInstalledPatch(&patch, RegisteredPatchKind::detour)) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
        ReleasePatchRange(address);
        VirtualFree(gateway, 0, MEM_RELEASE);
        patch.gateway = nullptr;
        patch.installed = false;
    }
    return patch.installed;
}

void RestoreDetour(DetourPatch& patch) {
    if (patch.installed) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
        ReleasePatchRange(patch.address);
        patch.installed = false;
        UnregisterInstalledPatch(&patch);
    }
    if (patch.gateway) {
        VirtualFree(patch.gateway, 0, MEM_RELEASE);
        patch.gateway = nullptr;
    }
}

void RestoreAllPatches() {
    while (g_installedPatchCount != 0) {
        const RegisteredPatch entry =
            g_installedPatches[g_installedPatchCount - 1];
        switch (entry.kind) {
        case RegisteredPatchKind::site:
            RestoreSite(*static_cast<SitePatch*>(entry.patch));
            break;
        case RegisteredPatchKind::byte:
            RestoreByte(*static_cast<BytePatch*>(entry.patch));
            break;
        case RegisteredPatchKind::raw:
            RestoreRawPatch(*static_cast<RawPatch*>(entry.patch));
            break;
        case RegisteredPatchKind::detour:
            RestoreDetour(*static_cast<DetourPatch*>(entry.patch));
            break;
        case RegisteredPatchKind::absoluteOperand:
            RestoreAbsoluteOperand(
                *static_cast<AbsoluteOperandPatch*>(entry.patch));
            break;
        }
    }
}

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

// ---------------------------------------------------------------------------
// Shared timestep helpers
// ---------------------------------------------------------------------------

// Framerate Vigilante calls this ratio the "normalizer": it is 1.0 at the
// original 30 FPS timestep and shrinks proportionally as the frame rate rises.
float TimeStepRatio() {
    __try {
        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep <= 0.0f) {
            return 1.0f;
        }
        return timeStep / kOriginalTimeStep;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1.0f;
    }
}

float VectorLength(const float v[3]);

float ReadGameFloat(uintptr_t address, float fallback) {
    __try {
        const float value = *reinterpret_cast<const float*>(address);
        return std::isfinite(value) ? value : fallback;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return fallback;
    }
}

// ---------------------------------------------------------------------------
// Stunt jump camera timers
// ---------------------------------------------------------------------------

int AccumulateMilliseconds(float milliseconds, float& fraction) {
    if (!std::isfinite(milliseconds) || milliseconds <= 0.0f) {
        fraction = 0.0f;
        return 0;
    }

    const float total = milliseconds + fraction;
    const int whole = static_cast<int>(total);
    fraction = total - static_cast<float>(whole);
    return whole;
}

int __cdecl AccumulateFlightTimer(float milliseconds) {
    if (!g_flightTimerActive) {
        g_flightTimerFraction = 0.0f;
        g_flightTimerActive = true;
    }
    g_endTimerActive = false;
    return AccumulateMilliseconds(milliseconds, g_flightTimerFraction);
}

int __cdecl AccumulateEndTimer(float milliseconds) {
    if (!g_endTimerActive) {
        g_endTimerFraction = 0.0f;
        g_endTimerActive = true;
    }
    g_flightTimerActive = false;
    return AccumulateMilliseconds(milliseconds, g_endTimerFraction);
}
