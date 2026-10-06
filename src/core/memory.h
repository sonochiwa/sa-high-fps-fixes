#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

bool WriteBytes(uintptr_t address, const uint8_t* bytes, size_t size);
bool MemoryMatchesRaw(uintptr_t address, const uint8_t* expected, size_t size);

template <size_t Size>
bool MemoryMatches(uintptr_t address,
                   const std::array<uint8_t, Size>& expected) {
    return MemoryMatchesRaw(address, expected.data(), expected.size());
}

// Copies `size` bytes, or returns false when any of them is unreadable.
bool CopyMemoryForDiagnostics(uintptr_t address, uint8_t* destination, size_t size);
// The float at `address`, or `fallback` when it is unreadable or not finite.
float ReadGameFloat(uintptr_t address, float fallback);
// Writes a float into a write-protected game constant.
bool WriteProtectedGameFloat(uintptr_t address, float value);
// Whether a game constant still holds its stock value.
bool NearlyEqual(float a, float b);

} // namespace hff
