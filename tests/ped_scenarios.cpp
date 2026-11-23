#include "scenario_tools.h"

#include <cmath>
#include <cstring>
#include <iterator>

namespace scenarios {
namespace {

// The sniper scope zoomed in for half a second.
namespace sniper {
constexpr int32_t kWeapon = 34;
constexpr int32_t kWeaponModel = 358;
float g_fov = 0.0f;

bool Start() {
    game::GiveWeapon(kWeapon, kWeaponModel, 100);
    return true;
}

void Input(const Frame& frame, uint8_t* pad) {
    Press(pad, game::kPadRightShoulder1);
    const float t = Elapsed(frame);
    if (t > 1.0f && t <= 1.5f) {
        Press(pad, game::kPadButtonSquare);
    }
}

bool Step(const Frame& frame) {
    uint8_t* cam = game::ActiveCam();
    if (At(frame, 1.0f)) {
        g_fov = Field<float>(cam, game::kCamFov);
        Record("sniper", "camera_mode", Field<int16_t>(cam, game::kCamMode));
    } else if (At(frame, 1.5f)) {
        Record("sniper", "zoom_fov_per_second", (g_fov - Field<float>(cam, game::kCamFov)) / 0.5);
        return true;
    }
    return false;
}
}  // namespace sniper

// The player swimming in the sea, pushed 0.6 m under the water line once
// settled: how long the surface hold takes to bring them back.
namespace swim {
uint8_t* g_matrix = nullptr;
float g_settled = 0.0f;
float g_pushed = -1.0f;

bool Start() {
    if (game::GroundZ(kSeaX, kSeaY) > -8.0f) {
        return false;
    }
    uint8_t* player = game::Player();
    game::Teleport(player, {kSeaX, kSeaY, 0.5f});
    g_matrix = Field<uint8_t*>(player, game::kEntityMatrix);
    g_pushed = -1.0f;
    return g_matrix != nullptr;
}

bool Step(const Frame& frame) {
    const float t = Elapsed(frame);
    float& z = Field<Vector>(g_matrix, game::kMatrixPosition).z;
    if (At(frame, 3.0f)) {
        g_settled = z;
        z -= 0.6f;
        g_pushed = t;
        return false;
    }
    if (g_pushed < 0.0f) {
        return false;
    }
    if (z >= g_settled - 0.1f || t > g_pushed + 4.0f) {
        Record("swim", "seconds_to_resurface", t - g_pushed);
        return true;
    }
    return false;
}
}  // namespace swim

// The player swimming forward on the surface of the sea for three seconds.
namespace swim_speed {
uint8_t* g_player = nullptr;
Vector g_start{};

bool Start() {
    if (game::GroundZ(kSeaX, kSeaY) > -8.0f) {
        return false;
    }
    g_player = game::Player();
    game::Teleport(g_player, {kSeaX, kSeaY, 0.5f});
    return true;
}

void Input(const Frame& frame, uint8_t* pad) {
    if (Elapsed(frame) > 1.0f) {
        Field<int16_t>(pad, game::kPadLeftStickY) = -128;
    }
}

bool Step(const Frame& frame) {
    if (At(frame, 1.5f)) {
        g_start = game::Position(g_player);
    } else if (At(frame, 4.5f)) {
        Vector end = game::Position(g_player);
        end.z = g_start.z;
        Record("swim_speed", "metres_per_second", Distance(end, g_start) / 3.0f);
        return true;
    }
    return false;
}
}  // namespace swim_speed

// The player sprinting into the back of a parked Manana on the airport
// tarmac and leaning on it: how far the car has rolled after two seconds and
// in the two after. In SA-MP this is how a stalled car is shoved, and above
// 30 FPS it moves much more easily than at 30.
namespace push {
constexpr int32_t kModel = 410;
// CPed::m_fCurrentRotation and m_fAimingRotation; zero faces north.
constexpr size_t kPedCurrentRotation = 0x558;
constexpr size_t kPedAimingRotation = 0x55C;
constexpr float kCarAhead = 4.0f;
constexpr float kPlayerBehindCar = 4.5f;
uint8_t* g_vehicle = nullptr;
Vector g_mark{};

bool Start() {
    uint8_t* player = game::Player();
    const Vector site = game::Position(player);
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    const float z = game::GroundZ(site.x, site.y + kCarAhead);
    game::Teleport(g_vehicle, {site.x, site.y + kCarAhead, z + 1.0f});
    FaceLevel(g_vehicle, 0.0f, 1.0f);
    Field<Vector>(g_vehicle, game::kPhysicalMoveSpeed) = {};
    Field<Vector>(g_vehicle, game::kPhysicalTurnSpeed) = {};
    game::Teleport(player, {site.x, site.y + kCarAhead - kPlayerBehindCar, z + 1.0f});
    Field<float>(player, kPedCurrentRotation) = 0.0f;
    Field<float>(player, kPedAimingRotation) = 0.0f;
    FaceLevel(player, 0.0f, 1.0f);
    game::ShowPlayer();
    g_mark = game::Position(g_vehicle);
    return true;
}

void Input(const Frame&, uint8_t* pad) {
    Field<int16_t>(pad, game::kPadLeftStickY) = -128;
    Press(pad, game::kPadButtonCross);
}

// Along the ground only: the car settles onto its wheels after the teleport.
float Rolled() {
    Vector here = game::Position(g_vehicle);
    here.z = g_mark.z;
    const float distance = Distance(here, g_mark);
    g_mark = game::Position(g_vehicle);
    return distance;
}

bool Step(const Frame& frame) {
    if (At(frame, 2.0f)) {
        Record("push", "car_moved_m_0s_to_2s", Rolled());
    } else if (At(frame, 4.0f)) {
        Record("push", "car_moved_m_2s_to_4s", Rolled());
        return true;
    }
    return false;
}
}  // namespace push

// The player holding a Desert Eagle whose muzzle flash is lit half a second
// in, as a shot lights it: how many milliseconds it takes to fade out.
namespace gun_flash {
constexpr int32_t kWeapon = 24;
constexpr int32_t kModel = 348;
// CPed::DoGunFlash(duration in ms, left hand) and the right hand's flash
// alpha, which it sets and the ped's ProcessControl fades.
constexpr uintptr_t kDoGunFlash = 0x5DF340;
constexpr size_t kGunFlashAlpha = 0x504;
constexpr int32_t kFlashMs = 200;
constexpr float kGiveUpSeconds = 3.0f;
using DoGunFlashFn = bool(__thiscall*)(void*, int32_t, bool);
float g_lit = -1.0f;

bool Start() {
    game::GiveWeapon(kWeapon, kModel, 100);
    g_lit = -1.0f;
    return true;
}

bool Step(const Frame& frame) {
    uint8_t* player = game::Player();
    const float t = Elapsed(frame);
    if (g_lit < 0.0f) {
        if (t >= 0.5f && reinterpret_cast<DoGunFlashFn>(kDoGunFlash)(player, kFlashMs, false)) {
            g_lit = t;
        } else if (t > kGiveUpSeconds) {
            Record("gun_flash", "never_lit", 1.0);
            return true;
        }
        return false;
    }
    if (Field<int16_t>(player, kGunFlashAlpha) <= 0) {
        Record("gun_flash", "fade_ms", (t - g_lit) * 1000.0f);
        return true;
    }
    if (t > g_lit + kGiveUpSeconds) {
        Record("gun_flash", "fade_ms", kGiveUpSeconds * 1000.0f);
        return true;
    }
    return false;
}
}  // namespace gun_flash

const harness::Scenario kList[] = {
    {"sniper", sniper::Start, sniper::Step, sniper::Input},
    {"swim", swim::Start, swim::Step, nullptr},
    {"swim_speed", swim_speed::Start, swim_speed::Step, swim_speed::Input},
    {"push", push::Start, push::Step, push::Input},
    {"gun_flash", gun_flash::Start, gun_flash::Step, nullptr},
};

}  // namespace

ScenarioList PedScenarios() {
    return {kList, std::size(kList)};
}

}  // namespace scenarios
