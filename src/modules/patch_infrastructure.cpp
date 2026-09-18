#include "modules/modules.h"

namespace hff {

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

bool SwimmingMovementCodeIsUnmodified() {
    return MemoryMatches(kSwimDiveScale, kExpectedSwimDiveScale)
        && MemoryMatches(kSwimAscentBias, kExpectedSwimAscentBias)
        && MemoryMatches(kSwimVectorSetup, kExpectedSwimVectorSetup)
        && MemoryMatches(kSwimVectorTransform,
                         kExpectedSwimVectorTransform);
}

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

} // namespace hff
