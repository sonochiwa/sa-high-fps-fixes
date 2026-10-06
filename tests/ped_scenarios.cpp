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

const harness::Scenario kList[] = {
    {"sniper", sniper::Start, sniper::Step, sniper::Input},
    {"swim", swim::Start, swim::Step, nullptr},
    {"swim_speed", swim_speed::Start, swim_speed::Step, swim_speed::Input},
};

}  // namespace

ScenarioList PedScenarios() {
    return {kList, std::size(kList)};
}

}  // namespace scenarios
