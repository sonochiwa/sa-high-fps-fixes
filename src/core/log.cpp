#include "core/log.h"

#include "version.h"

#include <windows.h>

#include <cstdio>

namespace hff {

namespace {

std::string g_logPath;

} // namespace

void OpenLog(const std::string& path) {
    FILE* file{};
    if (path.empty() || fopen_s(&file, path.c_str(), "w") != 0 || !file) {
        return;
    }
    std::fprintf(file, "%s v%s\n", PLUGIN_NAME, PLUGIN_VERSION);
    std::fclose(file);
    g_logPath = path;
}

void Log(const char* message) {
    if (g_logPath.empty()) {
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

} // namespace hff
