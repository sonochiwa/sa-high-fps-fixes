#include "weapons/weapon_ammo.h"

#include "core/log.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/weapons.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace hff::weapons {

namespace {

struct AmmoConsumptionSlot {
    void* weapon{};
    int32_t weaponType{};
    uint32_t lastUpdate{};
    float credit{};
};

SitePatch g_continuousAmmoPatch{};
std::array<AmmoConsumptionSlot, 16> g_ammoConsumptionSlots{};

bool IsContinuousWeapon(int32_t weaponType) {
    constexpr int32_t kFlamethrower = 37;
    constexpr int32_t kSpraycan = 41;
    constexpr int32_t kExtinguisher = 42;
    return weaponType == kFlamethrower || weaponType == kSpraycan
        || weaponType == kExtinguisher;
}

AmmoConsumptionSlot& FindAmmoConsumptionSlot(void* weapon) {
    AmmoConsumptionSlot* oldest = &g_ammoConsumptionSlots.front();
    for (auto& slot : g_ammoConsumptionSlots) {
        if (slot.weapon == weapon) {
            return slot;
        }
        if (!slot.weapon) {
            return slot;
        }
        if (static_cast<int32_t>(slot.lastUpdate - oldest->lastUpdate) < 0) {
            oldest = &slot;
        }
    }
    return *oldest;
}

// A continuous weapon spends rounds thirty times a second of game time, the
// rate it fires at 30 FPS, with the fraction carried per weapon, instead of on
// every frame it fires.
int32_t __cdecl ShouldConsumeContinuousWeaponAmmo(uintptr_t weapon) {
    if (!weapon) {
        return true;
    }

    __try {
        const int32_t weaponType = *reinterpret_cast<int32_t*>(weapon);
        if (!IsContinuousWeapon(weaponType)) {
            return true;
        }

        const float timeStep = *reinterpret_cast<float*>(kTimerTimeStep);
        if (!std::isfinite(timeStep) || timeStep >= kOriginalTimeStep) {
            return true;
        }

        const uint32_t now = *reinterpret_cast<uint32_t*>(
            kTimerTimeInMilliseconds);
        auto& slot = FindAmmoConsumptionSlot(reinterpret_cast<void*>(weapon));
        if (slot.weapon != reinterpret_cast<void*>(weapon)
            || slot.weaponType != weaponType) {
            slot = {reinterpret_cast<void*>(weapon), weaponType, now, 0.0f};
            return true;
        }

        const uint32_t elapsed = now - slot.lastUpdate;
        slot.lastUpdate = now;
        constexpr uint32_t kNewBurstThresholdMs = 200;
        if (elapsed > kNewBurstThresholdMs) {
            slot.credit = 0.0f;
            return true;
        }

        slot.credit += static_cast<float>(elapsed)
                     * (kOriginalWeaponConsumptionRate / 1000.0f);
        if (slot.credit < 1.0f) {
            return false;
        }

        slot.credit -= std::floor(slot.credit);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return true;
    }
}

__declspec(naked) void ContinuousWeaponAmmoThunk() {
    __asm {
        pushfd
        pushad
        push esi
        call ShouldConsumeContinuousWeaponAmmo
        add esp, 4
        mov dword ptr [esp + 28], eax
        popad
        popfd
        test eax, eax
        jz skipConsumption
        mov eax, dword ptr [esi + 8]
        test eax, eax
        jmp kContinuousAmmoConsume
    skipConsumption:
        jmp kContinuousAmmoSkip
    }
}

} // namespace

bool InstallContinuousWeaponAmmoFix() {
    if (!InstallJump(g_continuousAmmoPatch, kContinuousAmmoPatch,
                     &ContinuousWeaponAmmoThunk, kExpectedContinuousAmmo)) {
        Log("Continuous weapon ammo fix skipped: CWeapon::Fire bytes do not match GTA SA 1.0 US.");
        return false;
    }
    Log("Installed time-based ammo consumption for continuous area-effect weapons.");
    return true;
}

} // namespace hff::weapons
