#include "scenario_tools.h"

#include <cmath>
#include <cstring>
#include <iterator>

namespace scenarios {
namespace {

// How fast the game's own clock runs against the frames it is given.
namespace timer {
uint32_t g_start = 0;

bool Start() {
    g_start = At<uint32_t>(game::kTimerTime);
    return true;
}

bool Step(const Frame& frame) {
    if (!At(frame, 4.0f)) {
        return false;
    }
    const uint32_t elapsed = At<uint32_t>(game::kTimerTime) - g_start;
    Record("timer", "game_ms_per_second", elapsed / Elapsed(frame));
    return true;
}
}  // namespace timer

// The on-screen mission clock, started at ten minutes and read four seconds
// later. Every script sleeps through the test, so a global only Zeroing In
// uses is free to count.
namespace countdown {
constexpr uint32_t kVariable = 7405 * 4;
constexpr int32_t kStart = 600000;

bool Start() {
    game::StartCountdown(kVariable, kStart);
    return true;
}

bool Step(const Frame& frame) {
    if (!At(frame, 4.0f)) {
        return false;
    }
    const int32_t left = At<int32_t>(game::kScriptSpace + kVariable);
    Record("countdown", "game_ms_per_second", (kStart - left) / Elapsed(frame));
    game::StopCountdown(kVariable);
    return true;
}
}  // namespace countdown

// The player standing in a fire. How soon the game starts to hurt a burning
// player varies from run to run, by up to about a second at any frame rate, so
// the damage rate is taken over the second after the first damage, inside the
// 2.333 s a player burns at a time.
namespace burning {
constexpr float kWatch = 3.0f;
constexpr float kWindow = 1.0f;
float g_ignited = -1.0f;
float g_hurt = -1.0f;
float g_hurtHealth = 0.0f;

bool Start() {
    uint8_t* player = game::Player();
    Field<float>(player, game::kPedHealth) = 100.0f;
    Vector feet = game::Position(player);
    feet.z -= 0.9f;
    game::StartFire(feet, 1.0f);
    g_ignited = -1.0f;
    g_hurt = -1.0f;
    return FireNear(feet, 1.5f) != nullptr;
}

bool Step(const Frame& frame) {
    uint8_t* player = game::Player();
    const float t = Elapsed(frame);
    const float health = Field<float>(player, game::kPedHealth);
    if (g_ignited < 0.0f && Field<void*>(player, game::kPedFire)) {
        g_ignited = t;
    }
    if (g_hurt < 0.0f && health < 100.0f) {
        g_hurt = t;
        g_hurtHealth = health;
    }
    const bool measured = g_hurt >= 0.0f && t >= g_hurt + kWindow;
    if (!measured && !At(frame, kWatch)) {
        return false;
    }
    Record("burning", "seconds_to_catch_fire", g_ignited);
    if (measured) {
        Record("burning", "health_lost_per_second", (g_hurtHealth - health) / (t - g_hurt));
    }
    game::ExtinguishFiresAround(game::Position(player), 10.0f);
    return true;
}
}  // namespace burning

// A car explosion set off 30 m in front of the player: how long it stays
// active. Car explosions last 4.25 s of game time.
namespace explosion {
constexpr int32_t kCarExplosion = 4;
float g_lastActive = 0.0f;

bool Active() {
    for (size_t i = 0; i < game::kExplosionCount; ++i) {
        const uintptr_t slot = game::kExplosions + i * game::kExplosionSize;
        if (At<int32_t>(slot + game::kExplosionType) == kCarExplosion
            && At<uint8_t>(slot + game::kExplosionActiveCounter) != 0) {
            return true;
        }
    }
    return false;
}

bool Start() {
    uint8_t* player = game::Player();
    game::AddExplosion(kCarExplosion,
                       Add(game::Position(player),
                           game::MatrixRow(player, game::kMatrixForward), 30.0f));
    g_lastActive = 0.0f;
    return Active();
}

bool Step(const Frame& frame) {
    const float t = Elapsed(frame);
    if (Active()) {
        g_lastActive = t;
        if (t < 8.0f) {
            return false;
        }
    }
    Record("explosion", "seconds_active", g_lastActive);
    return true;
}
}  // namespace explosion

// A tear gas grenade dropped at the player's feet: how much health the gas
// takes a second once it chokes. Gas spares peds a mission created, which the
// player is, so the player counts as a random ped meanwhile. Every percentage
// roll walks the range evenly, so the share of rolls that choke is exact.
namespace teargas {
constexpr int32_t kTearGas = 17;
constexpr int32_t kTearGasModel = 343;
constexpr float kWindow = 4.0f;
float g_hurt = -1.0f;
float g_hurtHealth = 0.0f;
uint8_t g_createdBy = 0;

bool Start() {
    uint8_t* player = game::Player();
    Field<float>(player, game::kPedHealth) = 100.0f;
    g_createdBy = Field<uint8_t>(player, game::kPedCreatedBy);
    Field<uint8_t>(player, game::kPedCreatedBy) = game::kPedCreatedByGame;
    game::EvenRandomPercent(true);
    game::DropProjectile(kTearGas, kTearGasModel, game::Position(player));
    g_hurt = -1.0f;
    return true;
}

bool Finish() {
    Field<uint8_t>(game::Player(), game::kPedCreatedBy) = g_createdBy;
    game::EvenRandomPercent(false);
    return true;
}

bool Step(const Frame& frame) {
    const float t = Elapsed(frame);
    const float health = Field<float>(game::Player(), game::kPedHealth);
    if (g_hurt < 0.0f) {
        if (health < 100.0f) {
            g_hurt = t;
            g_hurtHealth = health;
        } else if (t > 10.0f) {
            Record("teargas", "never_choked", 1.0);
            return Finish();
        }
        return false;
    }
    if (t < g_hurt + kWindow) {
        return false;
    }
    Record("teargas", "health_lost_per_second", (g_hurtHealth - health) / (t - g_hurt));
    return Finish();
}
}  // namespace teargas

const harness::Scenario kList[] = {
    {"timer", timer::Start, timer::Step, nullptr},
    {"countdown", countdown::Start, countdown::Step, nullptr},
    {"burning", burning::Start, burning::Step, nullptr},
    {"explosion", explosion::Start, explosion::Step, nullptr},
    {"teargas", teargas::Start, teargas::Step, nullptr},
};

}  // namespace

ScenarioList WorldScenarios() {
    return {kList, std::size(kList)};
}

}  // namespace scenarios
