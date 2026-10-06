#include "player/drowning.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/player.h"

#include <windows.h>

#include <cmath>
#include <cstdint>

namespace hff::player {

namespace {

SitePatch g_drowningDamagePatch{};

// The fraction the truncation would have thrown away, carried into the next
// frame. At 30 FPS the damage is exactly five per frame and the carry stays at
// zero, so a capped run is bit for bit what it was. `HandlePlayerBreath` also
// runs for SA-MP's remote players, so every ped under water in one frame gets
// the whole number the first one got and the carry advances once a frame.
float g_drowningDamageCarry{};
uint32_t g_drowningDamageFrame{0xFFFFFFFFu};
float g_drowningDamageValue{};
int32_t g_drowningDamageResult{};

int32_t __cdecl AccumulateDrowningDamage(float damage) {
    __try {
        if (!std::isfinite(damage) || damage <= 0.0f) {
            g_drowningDamageCarry = 0.0f;
            return 0;
        }
        const uint32_t frame = *reinterpret_cast<const uint32_t*>(kFrameCounter);
        if (frame == g_drowningDamageFrame && damage == g_drowningDamageValue) {
            return g_drowningDamageResult;
        }
        g_drowningDamageCarry += damage;
        const float whole = std::floor(g_drowningDamageCarry);
        g_drowningDamageCarry -= whole;
        g_drowningDamageFrame = frame;
        g_drowningDamageValue = damage;
        g_drowningDamageResult = static_cast<int32_t>(whole);
        return g_drowningDamageResult;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return static_cast<int32_t>(damage);
    }
}

// Replaces the multiply, the truncation and the two argument pushes the
// optimizer moved in front of it. The two pushes are reproduced afterwards, so
// the six arguments `CWeapon::GenerateDamageEvent` is about to receive sit in
// the original order. `eax` carries the damage into the `push eax` at the
// return address, exactly as `_ftol` left it.
__declspec(naked) void DrowningDamageThunk() {
    __asm {
        fmul dword ptr ds:[0x00858B3C]
        sub esp, 4
        fstp dword ptr [esp]
        call AccumulateDrowningDamage
        add esp, 4
        push 0
        push 3
        jmp kDrowningDamageReturn
    }
}

} // namespace

bool InstallDrowningDamageFix() {
    if (!InstallJump(g_drowningDamagePatch, kDrowningDamage,
                     &DrowningDamageThunk, kExpectedDrowningDamage)) {
        Log("Drowning damage fix skipped: CPlayerPed::HandlePlayerBreath bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed fraction-preserving drowning damage.");
    return true;
}

} // namespace hff::player
