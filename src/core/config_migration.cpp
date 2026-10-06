#include "core/config_migration.h"

#include <windows.h>

#include <array>
#include <cstring>

namespace hff {

// 1.2.0 folded [autoLimitFps] into [framerate] and dropped refreshRate.
// Values move over once and the old section is removed, so an upgraded
// file has no unknown keys to warn about.
void MigrateIniLayout(const std::string& iniPath) {
    if (iniPath.empty()) {
        return;
    }
    constexpr const char* keys[] = {
        "forMissions", "forMinigames", "forSchools",
        "forCutscenes", "forScriptedCutscenes", "forPauseMenu",
    };
    constexpr char missingValue[] = "\x1Dhigh-fps-fixes-missing\x1D";
    bool changed = false;
    for (const char* key : keys) {
        std::array<char, 128> oldValue{};
        GetPrivateProfileStringA("autoLimitFps", key, missingValue,
                                 oldValue.data(),
                                 static_cast<DWORD>(oldValue.size()),
                                 iniPath.c_str());
        if (std::strcmp(oldValue.data(), missingValue) == 0) {
            continue;
        }
        std::array<char, 128> newValue{};
        GetPrivateProfileStringA("framerate", key, missingValue,
                                 newValue.data(),
                                 static_cast<DWORD>(newValue.size()),
                                 iniPath.c_str());
        if (std::strcmp(newValue.data(), missingValue) == 0) {
            WritePrivateProfileStringA("framerate", key, oldValue.data(),
                                       iniPath.c_str());
        }
        changed = true;
    }
    // Up to 1.4.1 these keys were on/off switches with a built-in limit per
    // case. `1` now reads as one frame a second, so it becomes the limit that
    // case applied before.
    constexpr struct {
        const char* key;
        const char* limit;
    } legacyCaps[] = {
        {"forMissions", "50"},   {"forMinigames", "30"},
        {"forSchools", "80"},    {"forCutscenes", "60"},
        {"forScriptedCutscenes", "80"}, {"forPauseMenu", "60"},
    };
    for (const auto& item : legacyCaps) {
        std::array<char, 128> value{};
        GetPrivateProfileStringA("framerate", item.key, missingValue,
                                 value.data(),
                                 static_cast<DWORD>(value.size()),
                                 iniPath.c_str());
        if (std::strcmp(value.data(), "1") == 0) {
            WritePrivateProfileStringA("framerate", item.key, item.limit,
                                       iniPath.c_str());
            changed = true;
        }
    }
    std::array<char, 128> refreshRate{};
    GetPrivateProfileStringA("framerate", "refreshRate", missingValue,
                             refreshRate.data(),
                             static_cast<DWORD>(refreshRate.size()),
                             iniPath.c_str());
    if (std::strcmp(refreshRate.data(), missingValue) != 0) {
        WritePrivateProfileStringA("framerate", "refreshRate", nullptr,
                                   iniPath.c_str());
        changed = true;
    }
    if (changed) {
        WritePrivateProfileStringA("autoLimitFps", nullptr, nullptr,
                                   iniPath.c_str());
        WritePrivateProfileStringA(nullptr, nullptr, nullptr,
                                   iniPath.c_str());
    }
}

} // namespace hff
