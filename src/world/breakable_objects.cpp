#include "world/breakable_objects.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/sites/world.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hff::world {

namespace {

SitePatch g_breakObjectLifetimePatch{};
uint32_t g_breakLifetimeLastFrame{0xFFFFFFFFu};
float g_breakLifetimeCarry{};
int g_breakLifetimeTicks{};

// The ticks of the original 30 FPS frames that elapsed this frame, decided
// once per frame and taken off every piece, never below zero.
int __cdecl ConsumeBreakObjectLifetimeTicks(int32_t* lifetime) {
    if (!lifetime || *lifetime <= 0) {
        return 0;
    }
    const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
    if (frame != g_breakLifetimeLastFrame) {
        g_breakLifetimeLastFrame = frame;
        const float timeStep = *reinterpret_cast<const float*>(kTimerTimeStep);
        // A broken timestep must not turn the carry into NaN for good.
        g_breakLifetimeCarry += std::isfinite(timeStep) && timeStep > 0.0f
                              ? timeStep / g_originalTimeStepValue
                              : 1.0f;
        g_breakLifetimeTicks = static_cast<int>(g_breakLifetimeCarry);
        g_breakLifetimeCarry -= static_cast<float>(g_breakLifetimeTicks);
    }
    return std::min(g_breakLifetimeTicks, *lifetime);
}

__declspec(naked) void BreakObjectLifetimeThunk() {
    __asm {
        pushfd
        pushad
        lea eax, [edi + eax + 0x70]
        push eax
        call ConsumeBreakObjectLifetimeTicks
        add esp, 4
        mov dword ptr [esp + 24], eax
        popad
        popfd
        mov edx, dword ptr [edi + eax + 0x70]
        lea eax, [edi + eax + 0x70]
        sub edx, ecx
        mov dword ptr [eax], edx
        jmp kBreakObjectLifetimeReturn
    }
}

} // namespace

bool InstallBreakableObjectLifetimeFix() {
    if (!InstallJump(g_breakObjectLifetimePatch, kBreakObjectLifetime,
                     &BreakObjectLifetimeThunk,
                     kExpectedBreakObjectLifetime)) {
        Log("Breakable object lifetime fix skipped: BreakObject_c::Update "
            "bytes do not match GTA SA 1.0 US.");
        return false;
    }
    g_breakLifetimeLastFrame = 0xFFFFFFFFu;
    g_breakLifetimeCarry = 0.0f;
    g_breakLifetimeTicks = 0;
    Log("Installed real-time breakable object lifetime counters.");
    return true;
}

} // namespace hff::world
