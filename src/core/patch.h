#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

struct SitePatch {
    uintptr_t address{};
    // The bytes the site held when it was patched: the stock code, or the
    // foreign branch this plugin's patch was written over.
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
    // `GetTickCount` of the last time the guard put the patch back.
    uint32_t reassertedAt{};
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

// Replaces `size` original bytes with a relative branch to `target` and pads
// the remainder with NOPs. `opcode` is 0xE8 for a call or 0xE9 for a jump.
bool InstallBranch(SitePatch& patch, uintptr_t address, const void* target,
                   const uint8_t* expected, size_t size, uint8_t opcode);

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

// Redirects a five byte call that must land on `expectedCallee`.
bool RepointCall(SitePatch& patch, uintptr_t address, uintptr_t expectedCallee,
                 const void* replacement);
void RestoreSite(SitePatch& patch);
bool InstallByte(BytePatch& patch, uintptr_t address, uint8_t value);
void RestoreByte(BytePatch& patch);
bool InstallRawPatch(RawPatch& patch, uintptr_t address,
                     const uint8_t* replacement, size_t size);
void RestoreRawPatch(RawPatch& patch);
// Jumps from a function entry to `target`; `patch.gateway` then runs the
// original function.
bool InstallDetour(DetourPatch& patch, uintptr_t address, const void* target,
                   const uint8_t* expected, size_t size);
void RestoreDetour(DetourPatch& patch);
void RestoreAllPatches();
void ForEachInstalledSite(void (*visit)(SitePatch& patch));

// Collects every patch installed by one fix and restores them in reverse order
// unless Commit is reached. Installers therefore describe only their forward
// path; an early return cannot leave half of a multi-site fix active.
class PatchSet {
public:
    explicit PatchSet(const char* name);
    ~PatchSet();
    PatchSet(const PatchSet&) = delete;
    PatchSet& operator=(const PatchSet&) = delete;

    bool Track(bool installed, SitePatch& patch);
    bool Track(bool installed, DetourPatch& patch);
    bool Track(bool installed, BytePatch& patch);
    bool Track(bool installed, RawPatch& patch);
    bool Commit();

private:
    struct Entry {
        void* patch;
        void (*restore)(void*);
    };

    bool Add(void* patch, void (*restore)(void*));

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

} // namespace hff
