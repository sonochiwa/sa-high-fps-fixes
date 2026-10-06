#include "scenarios.h"

#include "game.h"

#include <cmath>
#include <cstring>

namespace scenarios {
namespace {

using game::At;
using game::Field;
using game::Vector;
using harness::Frame;
using harness::Record;

constexpr float kRadToDeg = 57.2957795f;

// Seconds of virtual time once this frame has run.
float Elapsed(const Frame& frame) {
    return static_cast<float>(frame.index + 1) / static_cast<float>(frame.fps);
}

// True on the one frame at which `seconds` of virtual time have passed.
bool At(const Frame& frame, float seconds) {
    const auto target = static_cast<uint32_t>(std::lround(seconds * frame.fps));
    return frame.index + 1 == target;
}

void Press(uint8_t* pad, size_t button) {
    Field<int16_t>(pad, button) = game::kButtonDown;
}

Vector Add(const Vector& a, const Vector& b, float scale) {
    return {a.x + b.x * scale, a.y + b.y * scale, a.z + b.z * scale};
}

float Distance(const Vector& a, const Vector& b) {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return std::sqrt(x * x + y * y + z * z);
}

// The active fire nearest to a point, or null when none is within reach.
uint8_t* FireNear(const Vector& point, float reach) {
    uint8_t* nearest = nullptr;
    float best = reach;
    for (size_t i = 0; i < game::kFireCount; ++i) {
        auto* fire = reinterpret_cast<uint8_t*>(game::kFireManager + i * game::kFireSize);
        if ((Field<uint8_t>(fire, game::kFireFlags) & 1) == 0) {
            continue;
        }
        const float distance = Distance(Field<Vector>(fire, game::kFirePosition), point);
        if (distance < best) {
            best = distance;
            nearest = fire;
        }
    }
    return nearest;
}

float Heading(const void* entity) {
    const Vector forward = game::MatrixRow(entity, game::kMatrixForward);
    return std::atan2(-forward.x, forward.y) * kRadToDeg;
}

// Bank angle over the whole circle, from how far the right and up axes tilt.
float Roll(const void* entity) {
    const Vector right = game::MatrixRow(entity, game::kMatrixRight);
    const Vector up = game::MatrixRow(entity, game::kMatrixUp);
    return std::atan2(right.z, up.z) * kRadToDeg;
}

float AngleDelta(float to, float from) {
    float delta = to - from;
    while (delta > 180.0f) {
        delta -= 360.0f;
    }
    while (delta < -180.0f) {
        delta += 360.0f;
    }
    return delta;
}

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

// Forklift forks raised for a second, then lowered for half a second.
namespace forklift {
constexpr int32_t kModel = 530;
uint8_t* g_vehicle = nullptr;
uint16_t g_mark = 0;

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    game::PutPlayerIn(g_vehicle);
    return true;
}

void Input(const Frame& frame, uint8_t* pad) {
    const float t = Elapsed(frame);
    int16_t stick = 0;
    if (t > 0.5f && t <= 1.5f) {
        stick = -128;
    } else if (t > 1.5f && t <= 2.0f) {
        stick = 128;
    }
    Field<int16_t>(pad, game::kPadRightStickY) = stick;
}

bool Step(const Frame& frame) {
    const uint16_t angle = Field<uint16_t>(g_vehicle, game::kAutomobileMiscAngle);
    if (At(frame, 0.5f)) {
        g_mark = angle;
    } else if (At(frame, 1.5f)) {
        Record("forklift", "raise_units_per_second", angle - g_mark);
        g_mark = angle;
    } else if (At(frame, 2.0f)) {
        Record("forklift", "lower_units_per_second", (g_mark - angle) / 0.5);
        return true;
    }
    return false;
}
}  // namespace forklift

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

// A stunt plane at height, rolled right for a second, then let go. The roll is
// summed frame by frame, so a turn past upside down is counted in full.
namespace plane {
constexpr int32_t kModel = 513;
constexpr size_t kEngineByte = 0x428;
constexpr uint8_t kEngineOn = 0x10;
uint8_t* g_vehicle = nullptr;
float g_lastRoll = 0.0f;
float g_rolled = 0.0f;
float g_heading = 0.0f;

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    game::PutPlayerIn(g_vehicle);
    Vector position = game::Position(g_vehicle);
    position.z += 250.0f;
    game::Teleport(g_vehicle, position);
    Field<uint8_t>(g_vehicle, kEngineByte) |= kEngineOn;
    const Vector forward = game::MatrixRow(g_vehicle, game::kMatrixForward);
    Field<Vector>(g_vehicle, game::kPhysicalMoveSpeed) = {forward.x * 0.8f, forward.y * 0.8f,
                                                          0.0f};
    g_lastRoll = Roll(g_vehicle);
    g_rolled = 0.0f;
    return true;
}

void Input(const Frame& frame, uint8_t* pad) {
    Press(pad, game::kPadButtonCross);
    const float t = Elapsed(frame);
    if (t > 1.0f && t <= 2.0f) {
        Field<int16_t>(pad, game::kPadLeftStickX) = 128;
    }
}

bool Step(const Frame& frame) {
    const float roll = Roll(g_vehicle);
    g_rolled += AngleDelta(roll, g_lastRoll);
    g_lastRoll = roll;
    if (At(frame, 1.0f)) {
        g_rolled = 0.0f;
        g_heading = Heading(g_vehicle);
    } else if (At(frame, 2.0f)) {
        Record("plane", "roll_deg_during_input", g_rolled);
        g_rolled = 0.0f;
    } else if (At(frame, 3.0f)) {
        Record("plane", "roll_deg_after_release", g_rolled);
        Record("plane", "heading_change_deg", AngleDelta(Heading(g_vehicle), g_heading));
        return true;
    }
    return false;
}
}  // namespace plane

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

// A fire truck spraying straight ahead: how far from the truck the jet reaches.
namespace firetruck {
constexpr int32_t kModel = 407;
uint8_t* g_vehicle = nullptr;
float g_reach = 0.0f;

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    game::PutPlayerIn(g_vehicle);
    g_reach = 0.0f;
    return true;
}

void Input(const Frame& frame, uint8_t* pad) {
    if (Elapsed(frame) > 0.5f) {
        Press(pad, game::kPadButtonCircle);
    }
}

// The farthest live section of the truck's jet, along the ground.
float JetReach() {
    const Vector truck = game::Position(g_vehicle);
    float reach = 0.0f;
    for (size_t i = 0; i < game::kWaterCannonCount; ++i) {
        const uintptr_t cannon = game::kWaterCannons + i * game::kWaterCannonSize;
        if (At<uint32_t>(cannon + game::kWaterCannonId)
            != reinterpret_cast<uintptr_t>(g_vehicle)) {
            continue;
        }
        for (size_t section = 0; section < game::kWaterCannonSections; ++section) {
            if (!At<uint8_t>(cannon + game::kWaterCannonUsed + section)) {
                continue;
            }
            Vector point = At<Vector>(cannon + game::kWaterCannonPoints
                                      + section * sizeof(Vector));
            point.z = truck.z;
            reach = std::fmax(reach, Distance(point, truck));
        }
    }
    return reach;
}

bool Step(const Frame& frame) {
    if (Elapsed(frame) > 2.0f) {
        g_reach = std::fmax(g_reach, JetReach());
    }
    if (!At(frame, 3.0f)) {
        return false;
    }
    Record("firetruck", "jet_reach_m", g_reach);
    return true;
}
}  // namespace firetruck

// A car left standing: on how many frames it is awake once it has come to rest.
namespace parking {
constexpr int32_t kModel = 400;
uint8_t* g_vehicle = nullptr;
uint32_t g_frames = 0;
uint32_t g_awake = 0;

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    g_frames = 0;
    g_awake = 0;
    return g_vehicle != nullptr;
}

bool Moving(const void* vehicle) {
    const Vector move = Field<Vector>(vehicle, game::kPhysicalMoveSpeed);
    const Vector turn = Field<Vector>(vehicle, game::kPhysicalTurnSpeed);
    return move.x != 0.0f || move.y != 0.0f || move.z != 0.0f || turn.x != 0.0f
        || turn.y != 0.0f || turn.z != 0.0f;
}

bool Step(const Frame& frame) {
    if (Elapsed(frame) > 3.0f) {
        ++g_frames;
        g_awake += Moving(g_vehicle) ? 1 : 0;
    }
    if (!At(frame, 5.0f)) {
        return false;
    }
    Record("parking", "awake_fraction", static_cast<double>(g_awake) / g_frames);
    return true;
}
}  // namespace parking

const harness::Scenario kScenarios[] = {
    {"timer", timer::Start, timer::Step, nullptr},
    {"forklift", forklift::Start, forklift::Step, forklift::Input},
    {"burning", burning::Start, burning::Step, nullptr},
    {"plane", plane::Start, plane::Step, plane::Input},
    {"sniper", sniper::Start, sniper::Step, sniper::Input},
    {"firetruck", firetruck::Start, firetruck::Step, firetruck::Input},
    {"parking", parking::Start, parking::Step, nullptr},
};

}  // namespace

const harness::Scenario* Find(const char* name) {
    for (const harness::Scenario& scenario : kScenarios) {
        if (std::strcmp(scenario.name, name) == 0) {
            return &scenario;
        }
    }
    return nullptr;
}

}  // namespace scenarios
