#include "hud/map_zoom.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/sites/hud.h"

#include <cstdint>

namespace hff::hud {

namespace {

SitePatch g_mapWheelSamplePatch{};
SitePatch g_mapZoomInGatePatch{};
SitePatch g_mapZoomOutGatePatch{};
uint8_t g_mapWheelUpEdge = 0;
uint8_t g_mapWheelDownEdge = 0;
uint8_t g_mapWheelUpPrev = 0;
uint8_t g_mapWheelDownPrev = 0;

void __cdecl SampleMapWheelEdges() {
    const uint8_t up = *reinterpret_cast<volatile uint8_t*>(kMouseWheelUpFlag);
    const uint8_t down =
        *reinterpret_cast<volatile uint8_t*>(kMouseWheelDownFlag);
    g_mapWheelUpEdge = (up && !g_mapWheelUpPrev) ? 1 : 0;
    g_mapWheelDownEdge = (down && !g_mapWheelDownPrev) ? 1 : 0;
    g_mapWheelUpPrev = up;
    g_mapWheelDownPrev = down;
}

// The x87 stack is empty here and the replaced load is reproduced before the
// return. The flags matter: a `test al,al` above and a `jge` below straddle
// this point.
__declspec(naked) void MapWheelSampleThunk() {
    __asm {
        pushfd
        pushad
        call SampleMapWheelEdges
        popad
        popfd
        fld dword ptr ds:[0x008653F4]
        ret
    }
}

// Entered with the elapsed pause-mode milliseconds in eax, replacing
// `cmp eax,14h / jbe skip`. Proceeds when the tick has passed, as before, or on
// a wheel notch.
__declspec(naked) void MapZoomInGateThunk() {
    __asm {
        cmp eax, 0x14
        ja proceed
        cmp byte ptr [g_mapWheelUpEdge], 0
        jne proceed
        jmp kMapZoomInSkip
    proceed:
        jmp kMapZoomInProceed
    }
}

__declspec(naked) void MapZoomOutGateThunk() {
    __asm {
        cmp eax, 0x14
        ja proceed
        cmp byte ptr [g_mapWheelDownEdge], 0
        jne proceed
        jmp kMapZoomOutSkip
    proceed:
        jmp kMapZoomOutProceed
    }
}

} // namespace

bool InstallMapZoomWheelFix() {
    PatchSet patches("Map zoom wheel fix");
    if (!patches.Track(
            InstallBranch(g_mapWheelSamplePatch, kMapWheelSample,
                          &MapWheelSampleThunk, kExpectedMapWheelSample.data(),
                          6, 0xE8),
            g_mapWheelSamplePatch)) {
        Log("Map zoom wheel fix skipped: the map bounds block does not match "
            "GTA SA 1.0 US.");
        return false;
    }
    if (!patches.Track(
            InstallBranch(g_mapZoomInGatePatch, kMapZoomInGate,
                          &MapZoomInGateThunk, kExpectedMapZoomInGate.data(), 9,
                          0xE9),
            g_mapZoomInGatePatch)
        || !patches.Track(
            InstallBranch(g_mapZoomOutGatePatch, kMapZoomOutGate,
                          &MapZoomOutGateThunk, kExpectedMapZoomOutGate.data(),
                          5, 0xE9),
            g_mapZoomOutGatePatch)) {
        Log("Map zoom wheel fix skipped: the map zoom gates do not match "
            "GTA SA 1.0 US.");
        return false;
    }
    patches.Commit();
    Log("Installed a frame-rate independent pause menu map zoom.");
    return true;
}

} // namespace hff::hud
