// ---------------------------------------------------------------------------
// INI file: creation, upgrade and settings
// ---------------------------------------------------------------------------

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

// The version comment on the first line is the one piece of the template that
// is kept current: an INI created by an older release would otherwise carry
// that release's number forever and be mistaken for a stale plugin. Only a
// first line that is itself a version header is touched, and only when it
// differs, so a file the user has edited is not rewritten on every launch.
// Returns true when the header was brought up to date.
bool RefreshIniVersionHeader() {
    if (g_iniPath.empty()) {
        return false;
    }
    const char* templateEnd = std::strchr(kDefaultIni, '\n');
    if (!templateEnd) {
        return false;
    }
    const std::string header(kDefaultIni,
                             static_cast<size_t>(templateEnd - kDefaultIni));
    constexpr char prefix[] = "# High FPS Fixes v";

    HANDLE file = CreateFileA(g_iniPath.c_str(), GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    std::string contents;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > (1 << 20)) {
        CloseHandle(file);
        return false;
    }
    contents.resize(static_cast<size_t>(size.QuadPart));
    DWORD read{};
    if (!contents.empty()
        && (!ReadFile(file, contents.data(),
                      static_cast<DWORD>(contents.size()), &read, nullptr)
            || read != contents.size())) {
        CloseHandle(file);
        return false;
    }

    size_t lineEnd = contents.find('\n');
    if (lineEnd == std::string::npos) {
        lineEnd = contents.size();
    }
    size_t lineLength = lineEnd;
    if (lineLength != 0 && contents[lineLength - 1] == '\r') {
        --lineLength;
    }
    const bool isHeader =
        contents.compare(0, sizeof(prefix) - 1, prefix) == 0;
    if (!isHeader || contents.compare(0, lineLength, header) == 0) {
        CloseHandle(file);
        return false;
    }

    contents.replace(0, lineLength, header);
    // Write first and truncate after, so a failed write leaves the old file
    // rather than an empty one.
    bool written = false;
    if (SetFilePointer(file, 0, nullptr, FILE_BEGIN)
        != INVALID_SET_FILE_POINTER) {
        DWORD count{};
        written = WriteFile(file, contents.data(),
                            static_cast<DWORD>(contents.size()), &count,
                            nullptr) != FALSE
               && count == contents.size()
               && SetEndOfFile(file) != FALSE;
    }
    CloseHandle(file);
    return written;
}

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

// Switches that existed in an earlier release and were removed. An INI written by
// that release still carries them; that is not a mistake on the user's part, so
// they are reported once as information rather than as a warning that forces
// logging on.
bool IsRetiredConfigKey(const char* section, const char* key) {
    constexpr ConfigKey retired[] = {
        {"vehicles", "groundFriction"},
        {"vehicles", "turnAirResistance"},
        {"vehicles", "wheelSlipScale"},
        {"vehicles", "moveSpeedSnap"},
        {"vehicles", "rollOntoWheels"},
        {"vehicles", "suspensionDampingLimit"},
        {"vehicles", "collisionPushOut"},
    };
    for (const auto& item : retired) {
        if (_stricmp(item.section, section) == 0
            && _stricmp(item.key, key) == 0) {
            return true;
        }
    }
    return false;
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
            if (IsRetiredConfigKey(section, key.c_str())) {
                std::string message("Configuration note: [");
                message += section;
                message += "] ";
                message += key;
                message += " was removed and is ignored; the driving physics "
                           "now match FramerateVigilante.";
                Log(message.c_str());
            } else if (!IsKnownConfigKey(section, key.c_str())) {
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
