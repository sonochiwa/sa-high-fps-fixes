#include "core/memory.h"

#include <windows.h>

#include <cmath>
#include <cstring>

namespace hff {

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

float ReadGameFloat(uintptr_t address, float fallback) {
    __try {
        const float value = *reinterpret_cast<const float*>(address);
        return std::isfinite(value) ? value : fallback;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return fallback;
    }
}

bool WriteProtectedGameFloat(uintptr_t address, float value) {
    DWORD oldProtect{};
    if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(value),
                        PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    *reinterpret_cast<float*>(address) = value;
    DWORD ignored{};
    VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), oldProtect,
                   &ignored);
    return true;
}

bool NearlyEqual(float a, float b) {
    return std::fabs(a - b) < 0.002f;
}

} // namespace hff
