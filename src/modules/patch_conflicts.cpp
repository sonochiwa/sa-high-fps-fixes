#include "modules/modules.h"

namespace hff {

// Other frame-rate plugins patch some of the same instructions this one does.
// FramerateVigilante, for instance, writes its own branch over the wheel
// friction, burnout, rotor and siren sites, and it does so from the RenderWare
// init event, after this plugin has already patched them. Whichever wrote last
// wins and the other's thunk is simply never reached, so two plugins side by
// side would apply one or the other fix at random per site and, where their
// spans differ, both halves at once.
//
// This plugin takes precedence at every site it patches. At install time a
// site already holding another module's branch is patched over it, and the
// guard below runs once a frame on the game thread and puts the patch back if
// it was written over later. A site is only claimed when the foreign bytes are
// recognisably a hook of the same shape: a five byte relative branch into some
// other module, padded with NOPs to at most this plugin's span. Anything else
// is left alone and reported, since overwriting an unknown modification could
// split an instruction.

bool g_overrideConflictingHooks{true};

ImageRange ModuleImageRange(HMODULE module) {
    ImageRange range{};
    __try {
        const auto base = reinterpret_cast<uintptr_t>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) {
            return range;
        }
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
            base + static_cast<uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            return range;
        }
        range.begin = base;
        range.end = base + nt->OptionalHeader.SizeOfImage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        range = {};
    }
    return range;
}

ImageRange g_gameImage{};
ImageRange g_pluginImage{};

bool AddressInRange(uintptr_t address, const ImageRange& range) {
    return range.end != 0 && address >= range.begin && address < range.end;
}

// Only the plugin's own correction sites inside the game image are contested.
// The two hooks it uses to get a call each frame are shared with other
// plugins by design: `CTheScripts::Process` is a function entry other mods
// hook for their own script processing, and the menu background call is
// where plugin-sdk's `drawMenuBackgroundEvent` chains through. Taking either
// over would silently break the other plugin rather than pick a fix.
bool SiteMayOverrideConflicts(uintptr_t address) {
    return AddressInRange(address, g_gameImage)
        && address != kScriptsProcess && address != kMenuBackground;
}

// The file name of whichever module owns `address`, or a placeholder when the
// branch lands in memory no module maps, such as a hook library's trampoline.
std::string ModuleNameForAddress(uintptr_t address) {
    HMODULE owner{};
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(address), &owner)
        || !owner) {
        return "an unknown module";
    }
    std::array<char, MAX_PATH> path{};
    const DWORD length = GetModuleFileNameA(owner, path.data(),
                                            static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return "an unknown module";
    }
    std::string name(path.data(), length);
    const size_t slash = name.find_last_of("\\/");
    return slash == std::string::npos ? name : name.substr(slash + 1);
}

// Decides whether the `size` bytes at `address`, which no longer equal
// `reference`, differ from it only by a foreign hook this plugin may safely
// write over. `reference` is the stock code at install time and this plugin's
// own branch afterwards.
ForeignBranch AnalyzeForeignBranch(uintptr_t address, const uint8_t* reference,
                                   size_t size) {
    ForeignBranch result{};
    std::array<uint8_t, 48> actual{};
    uint8_t following = 0;
    if (size > actual.size()
        || !CopyMemoryForDiagnostics(address, actual.data(), size)
        || !CopyMemoryForDiagnostics(address + size, &following, 1)) {
        return result;
    }

    size_t mismatch = 0;
    while (mismatch < size && actual[mismatch] == reference[mismatch]) {
        ++mismatch;
    }
    if (mismatch == size) {
        return result;
    }
    // A branch written at the start of the site can share its opcode with the
    // reference and differ only in the displacement.
    const size_t start = (mismatch < 5
                          && (actual[0] == 0xE8 || actual[0] == 0xE9))
                             ? 0 : mismatch;
    if (start + 5 > size || (actual[start] != 0xE8 && actual[start] != 0xE9)) {
        return result;
    }
    int32_t relative{};
    std::memcpy(&relative, actual.data() + start + 1, sizeof(relative));
    const uintptr_t target = address + start + 5
                           + static_cast<uintptr_t>(relative);
    if (AddressInRange(target, g_gameImage)
        || AddressInRange(target, g_pluginImage)) {
        return result;
    }

    size_t cursor = start + 5;
    while (cursor < size && actual[cursor] == 0x90) {
        ++cursor;
    }
    for (size_t i = cursor; i < size; ++i) {
        if (actual[i] != reference[i]) {
            return result;
        }
    }
    result.found = true;
    result.mayOverrun = cursor == size && following == 0x90;
    result.target = target;
    return result;
}

void LogSiteTakeover(uintptr_t address, uintptr_t target, const char* how) {
    std::string message("Conflicting hook: 0x");
    char number[24];
    std::snprintf(number, sizeof(number), "%08X",
                  static_cast<unsigned>(address));
    message += number;
    message += " was patched by ";
    message += ModuleNameForAddress(target);
    message += "; ";
    message += how;
    Log(message.c_str());
}

void RecordSiteTail(SitePatch& patch) {
    patch.tail.fill(0);
    CopyMemoryForDiagnostics(patch.address + patch.size, patch.tail.data(),
                             patch.tail.size());
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
        const ForeignBranch foreign =
            g_overrideConflictingHooks && SiteMayOverrideConflicts(address)
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
    std::memcpy(patch.original.data(), expected, size);

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

void RestoreSite(SitePatch& patch) {
    if (patch.installed) {
        WriteBytes(patch.address, patch.original.data(), patch.size);
        ReleasePatchRange(patch.address);
        patch.installed = false;
        UnregisterInstalledPatch(&patch);
    }
}

// Puts this plugin's branch back over a site another module has written over.
// Runs on the game thread, so no code can be executing at the site while it
// is rewritten.
void ReassertSite(SitePatch& patch) {
    if (patch.reasserted > kSiteReassertLimit) {
        return;
    }
    if (patch.reasserted == kSiteReassertLimit) {
        ++patch.reasserted;
        char line[128];
        std::snprintf(line, sizeof(line),
                      "Conflicting hook: 0x%08X keeps being rewritten by "
                      "another module; giving it up.",
                      static_cast<unsigned>(patch.address));
        Log(line);
        return;
    }

    // With the plugin's own branch intact only the padding has changed, and
    // nothing in it executes, so restoring it needs no analysis.
    const bool branchIntact = MemoryMatchesRaw(patch.address,
                                               patch.written.data(), 5);
    ForeignBranch foreign{};
    if (!branchIntact) {
        foreign = AnalyzeForeignBranch(patch.address, patch.written.data(),
                                       patch.size);
        if (!foreign.found) {
            ++patch.reasserted;
            char line[160];
            std::snprintf(line, sizeof(line),
                          "Conflicting hook: 0x%08X was modified in a way this "
                          "plugin does not recognise; leaving it alone.",
                          static_cast<unsigned>(patch.address));
            Log(line);
            patch.reasserted = kSiteReassertLimit + 1;
            return;
        }
    }

    if (!WriteBytes(patch.address, patch.written.data(), patch.size)) {
        return;
    }
    if (foreign.mayOverrun) {
        // The foreign padding ran past this span. Put back the bytes it
        // replaced, stopping at the first one that was not padded over.
        std::array<uint8_t, 16> after{};
        if (CopyMemoryForDiagnostics(patch.address + patch.size, after.data(),
                                     after.size())) {
            size_t count = 0;
            while (count < after.size() && after[count] == 0x90
                   && patch.tail[count] != 0x90) {
                ++count;
            }
            if (count != 0) {
                WriteBytes(patch.address + patch.size, patch.tail.data(),
                           count);
            }
        }
    }
    ++patch.reasserted;
    if (!branchIntact) {
        LogSiteTakeover(patch.address, foreign.target,
                        "this plugin's patch has been put back.");
    }
}

// Called once a frame from the game thread once every fix is installed.
void __cdecl GuardInstalledSites() {
    if (!g_overrideConflictingHooks) {
        return;
    }
    for (size_t i = 0; i < g_installedPatchCount; ++i) {
        if (g_installedPatches[i].kind != RegisteredPatchKind::site) {
            continue;
        }
        auto& patch = *static_cast<SitePatch*>(g_installedPatches[i].patch);
        if (!patch.installed || !SiteMayOverrideConflicts(patch.address)
            || MemoryMatchesRaw(patch.address, patch.written.data(),
                                patch.size)) {
            continue;
        }
        ReassertSite(patch);
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

} // namespace hff
