#include "core/patch.h"

#include "core/conflicts.h"
#include "core/log.h"
#include "core/memory.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

namespace hff {

namespace {

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

std::array<RegisteredPatch, 512> g_installedPatches{};
size_t g_installedPatchCount{};

// Every byte range this plugin has taken, so two fixes cannot claim the same
// instruction. The byte check catches an overlap only when the first patch
// happens to change the bytes the second expects; refusing the second claim
// catches every overlap, and the log line names both ranges.
struct ClaimedRange {
    uintptr_t begin;
    uintptr_t end;
};

std::array<ClaimedRange, 384> g_claimedRanges{};
size_t g_claimedRangeCount{};

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

    Log(message.c_str());
}

void RecordSiteTail(SitePatch& patch) {
    patch.tail.fill(0);
    CopyMemoryForDiagnostics(patch.address + patch.size, patch.tail.data(),
                             patch.tail.size());
}

} // namespace

bool InstallBranch(SitePatch& patch, uintptr_t address, const void* target,
                   const uint8_t* expected, size_t size, uint8_t opcode) {
    if (size < 5 || size > patch.original.size()) {
        Log("Patch site rejected: the site is larger than a patch record.");
        return false;
    }
    if (!MemoryMatchesRaw(address, expected, size)) {
        const ForeignBranch foreign =
            ConflictGuardEnabled() && SiteMayOverrideConflicts(address)
                ? AnalyzeForeignBranch(address, expected, size)
                : ForeignBranch{};
        // Without the stock bytes past the span there is nothing to put back
        // under a longer foreign patch, so that case stays refused.
        if (!foreign.found || foreign.mayOverrun) {
            ReportPatchMismatch(address, expected, size);
            return false;
        }
        LogSiteTakeover(address, foreign.target,
                        "this plugin's patch is installed over it.");
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
    if (!CopyMemoryForDiagnostics(address, patch.original.data(), size)) {
        std::memcpy(patch.original.data(), expected, size);
    }

    std::array<uint8_t, 48> replacement{};
    replacement.fill(0x90);
    replacement[0] = opcode;
    const int32_t relative = static_cast<int32_t>(displacement);
    std::memcpy(replacement.data() + 1, &relative, sizeof(relative));
    patch.written = replacement;
    patch.reasserted = 0;
    RecordSiteTail(patch);
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

// The five bytes differ between sites because each holds its own relative
// displacement, so instead of comparing bytes this verifies the opcode and
// resolves the displacement to check the call really does land on the expected
// callee. That is a stronger check than a byte match, not a weaker one.
bool RepointCall(SitePatch& patch, uintptr_t address, uintptr_t expectedCallee,
                 const void* replacement) {
    std::array<uint8_t, 5> expected{};
    __try {
        if (*reinterpret_cast<const uint8_t*>(address) != 0xE8) {
            return false;
        }
        const auto original = *reinterpret_cast<const int32_t*>(address + 1);
        if (address + 5 + static_cast<uintptr_t>(original) != expectedCallee) {
            return false;
        }
        std::memcpy(expected.data(), reinterpret_cast<const void*>(address),
                    expected.size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return InstallBranch(patch, address, replacement, expected.data(),
                         expected.size(), 0xE8);
}

// A site another module has rewritten since is left as that module has it:
// putting the old bytes back would cut its hook out as well.
void RestoreSite(SitePatch& patch) {
    if (!patch.installed) {
        return;
    }
    if (MemoryMatchesRaw(patch.address, patch.written.data(), patch.size)) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
    } else {
        char line[128];
        std::snprintf(line, sizeof(line),
                      "Patch at 0x%08X was rewritten by another module; left "
                      "as it is.",
                      static_cast<unsigned>(patch.address));
        Log(line);
    }
    ReleasePatchRange(patch.address);
    patch.installed = false;
    UnregisterInstalledPatch(&patch);
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
        }
    }
}

void ForEachInstalledSite(void (*visit)(SitePatch& patch)) {
    for (size_t i = 0; i < g_installedPatchCount; ++i) {
        if (g_installedPatches[i].kind == RegisteredPatchKind::site) {
            visit(*static_cast<SitePatch*>(g_installedPatches[i].patch));
        }
    }
}

PatchSet::PatchSet(const char* name) : m_name(name) {}

PatchSet::~PatchSet() {
    if (m_committed) {
        return;
    }
    while (m_count != 0) {
        Entry& entry = m_entries[--m_count];
        entry.restore(entry.patch);
    }
}

bool PatchSet::Track(bool installed, SitePatch& patch) {
    return installed && Add(&patch, [](void* site) {
        RestoreSite(*static_cast<SitePatch*>(site));
    });
}

bool PatchSet::Track(bool installed, DetourPatch& patch) {
    return installed && Add(&patch, [](void* detour) {
        RestoreDetour(*static_cast<DetourPatch*>(detour));
    });
}

bool PatchSet::Track(bool installed, BytePatch& patch) {
    return installed && Add(&patch, [](void* byte) {
        RestoreByte(*static_cast<BytePatch*>(byte));
    });
}

bool PatchSet::Track(bool installed, RawPatch& patch) {
    return installed && Add(&patch, [](void* raw) {
        RestoreRawPatch(*static_cast<RawPatch*>(raw));
    });
}

bool PatchSet::Commit() {
    m_committed = true;
    return true;
}

bool PatchSet::Add(void* patch, void (*restore)(void*)) {
    if (m_count == m_entries.size()) {
        restore(patch);
        char line[160];
        std::snprintf(line, sizeof(line),
                      "%s refused: patch transaction is too large.", m_name);
        Log(line);
        return false;
    }
    m_entries[m_count++] = {patch, restore};
    return true;
}

} // namespace hff
