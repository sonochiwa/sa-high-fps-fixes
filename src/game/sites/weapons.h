#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// `FxEmitter_c::CreateParticles`, where an effect system converts elapsed time
// into a whole number of particles and drops the fraction.
constexpr uintptr_t kFxCreateParticles = 0x004A41E0;
constexpr std::array<uint8_t, 6> kExpectedFxCreateParticles{
    0x81, 0xEC, 0x8C, 0x00, 0x00, 0x00
};
// FxEmitter_c: its blueprint, its effect system and the emission intensity it
// carries between calls; FxSystem_c flag 0x20 at +0x62 marks the systems of
// the continuous weapons.
constexpr size_t kFxEmitterBlueprint = 0x04;
constexpr size_t kFxEmitterSystem = 0x08;
constexpr size_t kFxEmitterIntensity = 0x10;
constexpr size_t kFxSystemFlags = 0x62;
constexpr uint8_t kFxSystemWeaponFlag = 0x20;

// `CWeapon::Fire` consuming ammunition for an area-effect weapon.
constexpr uintptr_t kContinuousAmmoPatch = 0x007428A8;
constexpr uintptr_t kContinuousAmmoConsume = 0x007428AD;
constexpr uintptr_t kContinuousAmmoSkip = 0x007428E9;
constexpr float kOriginalWeaponConsumptionRate = 30.0f;
constexpr std::array<uint8_t, 5> kExpectedContinuousAmmo{
    0x8B, 0x46, 0x08, 0x85, 0xC0
};

// Continuous weapon shots. `CWeapon::FireAreaEffect` (0x73E800) calls
// `CShotInfo::AddShot` for the shot and, for the flamethrower,
// `CCreepingFire::TryToStartFireAtCoors`; both call sites are redirected.
constexpr uintptr_t kAreaEffectAddShotCall = 0x0073EB20;
constexpr uintptr_t kShotInfoAddShot = 0x00739C30;
constexpr uintptr_t kAreaEffectCreepingFireCall = 0x0073EBFE;
constexpr uintptr_t kCreepingFireTryToStart = 0x0053A450;
// `CShotInfo::Update` puts fires out with one call per extinguisher shot to
// `CFireManager::ExtinguishPointWithWater(point, range, 2.0f)`, whose
// `CFire::ExtinguishWithWater` already multiplies the strength by the frame's
// time. A shot gated to the original rate stands for a whole original frame,
// so this call is redirected to scale the strength up by the frames it covers.
constexpr uintptr_t kShotExtinguishCall = 0x0073A1E1;
constexpr uintptr_t kFireManagerExtinguishPointWithWater = 0x005394C0;
// call CShotInfo::AddShot, from CWeapon::FireAreaEffect
constexpr std::array<uint8_t, 5> kExpectedAreaEffectAddShotCall{
    0xE8, 0x0B, 0xB1, 0xFF, 0xFF
};
// call CCreepingFire::TryToStartFireAtCoors, from CWeapon::FireAreaEffect
constexpr std::array<uint8_t, 5> kExpectedAreaEffectCreepingFireCall{
    0xE8, 0x4D, 0xB8, 0xDF, 0xFF
};
// call CFireManager::ExtinguishPointWithWater, from CShotInfo::Update
constexpr std::array<uint8_t, 5> kExpectedShotExtinguishCall{
    0xE8, 0xDA, 0xF2, 0xDF, 0xFF
};

// Chainsaw strike rate. `CTaskSimpleFight::ProcessPed` keeps the player's held
// chainsaw cutting by rewinding the moving-attack animation to `hit - 0.01`
// every time it passes `chain`, and the strike itself fires on the frame the
// animation crosses `hit`. `melee.dat` gives the chainsaw's AMOVING entry
// `hit 1.0` and `chain 1.1`, which the loader scales by 1/30 into 0.0333 s and
// 0.0367 s, so the whole loop is shorter than a single frame at 30 FPS and its
// length is decided by the frame quantisation rather than by the animation:
// the rewind and the strike cannot happen on the same frame, so the loop costs
// a near constant two to four frames whatever the frame rate. That is fifteen
// strikes a second at 30 FPS and three times as many at 144 FPS, against peds
// and vehicles alike. The patched instruction is the `fsub` that subtracts the
// 0.01: `0x858C58` holds a shared `0.01` that a few hundred other sites read,
// so the constant itself must not be touched.
constexpr uintptr_t kChainsawStrikeRewind = 0x00629F83;
// fsub dword ptr ds:[00858C58h]
constexpr std::array<uint8_t, 6> kExpectedChainsawStrikeRewind{
    0xD8, 0x25, 0x58, 0x8C, 0x85, 0x00
};
// The rewind the chainsaw loop performs in the stock game, and the strike
// period that rewind produces at 30 FPS: the animation crosses `hit` on the
// frame after the rewind and `chain` on the one after that, so a strike lands
// on every second frame.
constexpr float kChainsawStockRewind = 0.01f;
constexpr float kChainsawStrikePeriodMs = 2000.0f / 30.0f;
// A gap this long means the player let go and started sawing again, so the
// schedule restarts rather than paying back the whole idle time at once.
constexpr uint32_t kChainsawBurstGapMs = 300;
constexpr float kChainsawParkMargin = 0.001f;

// CProjectileInfo::Update chokes the peds around a tear gas grenade when
// `GetRandomNumberInRange(0, 100) < 10`, rolled every frame for every grenade,
// and every choke costs health, so above 30 FPS the gas chokes many times
// harder.
constexpr uintptr_t kTearGasChokeRoll = 0x00738C19;

} // namespace hff
