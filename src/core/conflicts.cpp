#include "core/conflicts.h"

#include "core/log.h"
#include "core/memory.h"
#include "core/module.h"
#include "core/patch.h"

#include <windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

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

namespace hff {

namespace {

struct ImageRange {
    uintptr_t begin{};
    uintptr_t end{};
};

bool g_overrideConflictingHooks{true};
ImageRange g_gameImage{};
ImageRange g_pluginImage{};
std::array<uintptr_t, 4> g_exemptSites{};
size_t g_exemptSiteCount{};

// A plugin that keeps rewriting a site every frame would otherwise be fought
// forever; after this many rounds the site is conceded and reported once.
constexpr uint8_t kSiteReassertLimit = 3;
// A conceded site is taken back once nobody has rewritten it for this long,
// so a one-off rewrite, a reload of another plugin or a code check that puts
// the stock bytes back does not cost the fix for the rest of the session.
constexpr uint32_t kSiteReassertRearmMs = 60000;

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

bool AddressInRange(uintptr_t address, const ImageRange& range) {
    return range.end != 0 && address >= range.begin && address < range.end;
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

// Puts this plugin's branch back over a site another module has written over.
// Runs on the game thread, so no code can be executing at the site while it
// is rewritten.
void ReassertSite(SitePatch& patch) {
    const uint32_t now = GetTickCount();
    if (patch.reasserted > kSiteReassertLimit) {
        if (now - patch.reassertedAt < kSiteReassertRearmMs) {
            return;
        }
        patch.reasserted = 0;
    }
    if (patch.reasserted == kSiteReassertLimit) {
        ++patch.reasserted;
        patch.reassertedAt = now;
        char line[128];
        std::snprintf(line, sizeof(line),
                      "Conflicting hook: 0x%08X keeps being rewritten by "
                      "another module; giving it up for a minute.",
                      static_cast<unsigned>(patch.address));
        Log(line);
        return;
    }

    // With the plugin's own branch intact only the padding has changed, and
    // nothing in it executes, so restoring it needs no analysis. Bytes put back
    // to what the site held before the patch keep the instruction boundaries
    // the patch was written for, so they need none either.
    const bool branchIntact = MemoryMatchesRaw(patch.address,
                                               patch.written.data(), 5);
    const bool restored = MemoryMatchesRaw(patch.address,
                                           patch.original.data(), patch.size);
    ForeignBranch foreign{};
    if (!branchIntact && !restored) {
        foreign = AnalyzeForeignBranch(patch.address, patch.written.data(),
                                       patch.size);
        if (!foreign.found) {
            char line[160];
            std::snprintf(line, sizeof(line),
                          "Conflicting hook: 0x%08X was modified in a way this "
                          "plugin does not recognise; leaving it alone.",
                          static_cast<unsigned>(patch.address));
            Log(line);
            patch.reasserted = kSiteReassertLimit + 1;
            patch.reassertedAt = now;
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
    patch.reassertedAt = now;
    if (restored) {
        char line[128];
        std::snprintf(line, sizeof(line),
                      "Conflicting hook: 0x%08X had its earlier bytes put back; "
                      "this plugin's patch has been put back.",
                      static_cast<unsigned>(patch.address));
        Log(line);
    } else if (!branchIntact) {
        LogSiteTakeover(patch.address, foreign.target,
                        "this plugin's patch has been put back.");
    }
}

void GuardSite(SitePatch& patch) {
    if (!patch.installed || !SiteMayOverrideConflicts(patch.address)
        || MemoryMatchesRaw(patch.address, patch.written.data(), patch.size)) {
        return;
    }
    ReassertSite(patch);
}

} // namespace

void InitializeConflictGuard(bool overrideEnabled) {
    g_gameImage = ModuleImageRange(GetModuleHandleA(nullptr));
    g_pluginImage = ModuleImageRange(g_module);
    g_overrideConflictingHooks = overrideEnabled;
}

bool ConflictGuardEnabled() {
    return g_overrideConflictingHooks;
}

void ExemptFromConflictGuard(uintptr_t address) {
    for (size_t i = 0; i < g_exemptSiteCount; ++i) {
        if (g_exemptSites[i] == address) {
            return;
        }
    }
    if (g_exemptSiteCount < g_exemptSites.size()) {
        g_exemptSites[g_exemptSiteCount++] = address;
    }
}

// Only the plugin's own correction sites inside the game image are contested.
// The hooks it uses to get a call each frame are shared with other plugins by
// design, and taking one over would silently break the other plugin rather
// than pick a fix.
bool SiteMayOverrideConflicts(uintptr_t address) {
    if (!AddressInRange(address, g_gameImage)) {
        return false;
    }
    for (size_t i = 0; i < g_exemptSiteCount; ++i) {
        if (g_exemptSites[i] == address) {
            return false;
        }
    }
    return true;
}

// `reference` is the stock code at install time and this plugin's own branch
// afterwards.
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

void __cdecl GuardInstalledSites() {
    if (g_overrideConflictingHooks) {
        ForEachInstalledSite(&GuardSite);
    }
}

} // namespace hff
