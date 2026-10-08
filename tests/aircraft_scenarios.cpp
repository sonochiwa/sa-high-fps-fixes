#include "scenario_tools.h"

#include <cmath>
#include <cstring>
#include <iterator>

namespace scenarios {
namespace {

// A stunt plane at height, rolled right for a second, then let go. The roll is
// summed frame by frame, so a turn past upside down is counted in full. The
// gentle variant moves the stick a sixth of the way. The damaged variant has
// every moving part damaged, which bleeds the controls away each frame, jolts
// them by a random amount and makes the engine sputter at a random pace; the
// random draws are held at the middle of their range so the run repeats.
namespace plane {
constexpr int32_t kModel = 513;
constexpr size_t kEngineByte = 0x428;
constexpr uint8_t kEngineOn = 0x10;
constexpr int32_t kFirstPart = 12;
constexpr int32_t kLastPart = 20;
constexpr int32_t kDamaged = 2;
// CPlane::m_fSteeringLeftRight, the eased roll input.
constexpr size_t kRollInput = 0x990;
// The `call rand` sites in CPlane::ProcessFlyingCarStuff: the sputter pace,
// then the jolts in the damage switch.
constexpr uintptr_t kDamageJolts[] = {0x6CB800, 0x6CBA28, 0x6CBAF4, 0x6CBB62,
                                      0x6CBC0D, 0x6CBC7B, 0x6CBD32, 0x6CBDA0};
uint8_t* g_vehicle = nullptr;
const char* g_name = "plane";
int16_t g_stick = 128;
float g_lastRoll = 0.0f;
float g_rolled = 0.0f;
float g_heading = 0.0f;
float g_inputSum = 0.0f;
uint32_t g_inputFrames = 0;

bool Start() {
    g_name = "plane";
    g_stick = 128;
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

bool StartGentle() {
    if (!Start()) {
        return false;
    }
    g_stick = 20;
    g_name = "plane_gentle";
    return true;
}

bool StartDamaged() {
    if (!Start()) {
        return false;
    }
    for (int32_t part = kFirstPart; part <= kLastPart; ++part) {
        game::DamagePlanePart(g_vehicle, part, kDamaged);
    }
    for (const uintptr_t site : kDamageJolts) {
        game::MidRangeRandom(site, true);
    }
    g_name = "plane_damaged";
    return true;
}

void Input(const Frame& frame, uint8_t* pad) {
    Press(pad, game::kPadButtonCross);
    const float t = Elapsed(frame);
    if (t > 1.0f && t <= 2.0f) {
        Field<int16_t>(pad, game::kPadLeftStickX) = g_stick;
    }
}

bool Step(const Frame& frame) {
    const float roll = Roll(g_vehicle);
    g_rolled += AngleDelta(roll, g_lastRoll);
    g_lastRoll = roll;
    const float t = Elapsed(frame);
    if (t > 1.0f && t <= 2.0f) {
        g_inputSum += Field<float>(g_vehicle, kRollInput);
        ++g_inputFrames;
    }
    if (At(frame, 1.0f)) {
        g_rolled = 0.0f;
        g_heading = Heading(g_vehicle);
        g_inputSum = 0.0f;
        g_inputFrames = 0;
    } else if (At(frame, 2.0f)) {
        Record(g_name, "roll_deg_during_input", g_rolled);
        Record(g_name, "roll_input_average", g_inputSum / static_cast<float>(g_inputFrames));
        g_rolled = 0.0f;
    } else if (At(frame, 3.0f)) {
        Record(g_name, "roll_deg_after_release", g_rolled);
        // A damaged plane turns only a few degrees here, and that small turn
        // differs by about a degree above 30 FPS for reasons outside its
        // control inputs, so only its roll is compared.
        if (std::strcmp(g_name, "plane_damaged") != 0) {
            Record(g_name, "heading_change_deg", AngleDelta(Heading(g_vehicle), g_heading));
        } else {
            for (const uintptr_t site : kDamageJolts) {
                game::MidRangeRandom(site, false);
            }
        }
        return true;
    }
    return false;
}
}  // namespace plane

// A helicopter on the apron with its engine started: how fast its rotor turns
// one and three seconds later.
namespace heli {
constexpr int32_t kModel = 487;
uint8_t* g_vehicle = nullptr;

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    game::PutPlayerIn(g_vehicle);
    Field<uint8_t>(g_vehicle, game::kVehicleEngineByte) |= game::kVehicleEngineOn;
    return true;
}

bool Step(const Frame& frame) {
    if (At(frame, 1.0f)) {
        Record("heli", "rotor_speed_at_1s", Field<float>(g_vehicle, game::kHeliRotorSpeed));
    } else if (At(frame, 3.0f)) {
        Record("heli", "rotor_speed_at_3s", Field<float>(g_vehicle, game::kHeliRotorSpeed));
        return true;
    }
    return false;
}
}  // namespace heli

// A helicopter of a mission, rotor at full speed, 150 m from the player and
// 40 m up, told to follow the player within 20 m as Interdiction's attackers
// do: how far from the player it is at three seconds, how far it flies from
// one second to six and how fast it goes at six. The autopilot jolts the
// throttle each frame by a random step from -7 to 8, held at 0 so the run
// repeats.
namespace ai_heli_follow {
constexpr int32_t kModel = 487;
constexpr uintptr_t kThrottleJolt = 0x42A0F6;
constexpr int32_t kNoJolt = 7;
constexpr float kFollowRadius = 20.0f;
uint8_t* g_vehicle = nullptr;
Vector g_mark{};

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    uint8_t* player = game::Player();
    const Vector target = game::Position(player);
    game::Teleport(g_vehicle, {target.x + 150.0f, target.y, target.z + 40.0f});
    Field<uint8_t>(g_vehicle, game::kVehicleEngineByte) |= game::kVehicleEngineOn;
    Field<float>(g_vehicle, game::kHeliRotorSpeed) = game::kHeliRotorFullSpeed;
    game::FlyHeliTo(g_vehicle, target, 0.0f, target.z + 40.0f);
    Field<uint8_t>(g_vehicle, game::kAutoPilotMission) = game::kMissionHeliFollowEntity;
    Field<uint8_t*>(g_vehicle, game::kAutoPilotTarget) = player;
    Field<float>(g_vehicle, game::kHeliFollowRadius) = kFollowRadius;
    game::HoldRandom(kThrottleJolt, kNoJolt, true);
    return true;
}

float GroundDistance(Vector a, const Vector& b) {
    a.z = b.z;
    return Distance(a, b);
}

bool Step(const Frame& frame) {
    if (At(frame, 1.0f)) {
        g_mark = game::Position(g_vehicle);
    } else if (At(frame, 3.0f)) {
        Record("ai_heli_follow", "player_distance_m_at_3s",
               GroundDistance(game::Position(g_vehicle), game::Position(game::Player())));
    } else if (At(frame, 6.0f)) {
        Record("ai_heli_follow", "distance_m_1s_to_6s",
               GroundDistance(game::Position(g_vehicle), g_mark));
        Vector velocity = Field<Vector>(g_vehicle, game::kPhysicalMoveSpeed);
        velocity.z = 0.0f;
        Record("ai_heli_follow", "speed_kmh_at_6s", Distance(velocity, {}) * 50.0f * 3.6f);
        game::HoldRandom(kThrottleJolt, kNoJolt, false);
        return true;
    }
    return false;
}
}  // namespace ai_heli_follow

const harness::Scenario kList[] = {
    {"plane", plane::Start, plane::Step, plane::Input},
    {"plane_gentle", plane::StartGentle, plane::Step, plane::Input},
    {"plane_damaged", plane::StartDamaged, plane::Step, plane::Input},
    {"heli", heli::Start, heli::Step, nullptr},
    {"ai_heli_follow", ai_heli_follow::Start, ai_heli_follow::Step, nullptr},
};

}  // namespace

ScenarioList AircraftScenarios() {
    return {kList, std::size(kList)};
}

}  // namespace scenarios
