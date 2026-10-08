#include "scenario_tools.h"

#include <cmath>
#include <cstring>
#include <iterator>

namespace scenarios {
namespace {

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

// A car at 100 km/h braking hard to a stop on the airport apron: how far it
// travels and how long it takes.
namespace braking {
constexpr int32_t kModel = 400;
uint8_t* g_vehicle = nullptr;
Vector g_start{};

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    game::PutPlayerIn(g_vehicle);
    Launch(g_vehicle, 100.0f);
    g_start = game::Position(g_vehicle);
    return true;
}

void Input(const Frame&, uint8_t* pad) {
    Press(pad, game::kPadButtonSquare);
}

bool Step(const Frame& frame) {
    const float t = Elapsed(frame);
    if (t < 0.2f || (Speed(g_vehicle) > 0.002f && t < 10.0f)) {
        return false;
    }
    Vector end = game::Position(g_vehicle);
    end.z = g_start.z;
    Record("braking", "stopping_distance_m", Distance(end, g_start));
    Record("braking", "seconds_to_stop", t);
    return true;
}
}  // namespace braking

// A car at 60 km/h with the throttle held, steered to full right lock for a
// second and a half: how far it turns.
namespace cornering {
constexpr int32_t kModel = 400;
uint8_t* g_vehicle = nullptr;
float g_heading = 0.0f;

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    game::PutPlayerIn(g_vehicle);
    Launch(g_vehicle, 60.0f);
    return true;
}

void Input(const Frame& frame, uint8_t* pad) {
    Press(pad, game::kPadButtonCross);
    if (Elapsed(frame) > 0.5f) {
        Field<int16_t>(pad, game::kPadLeftStickX) = 128;
    }
}

bool Step(const Frame& frame) {
    if (At(frame, 0.5f)) {
        g_heading = Heading(g_vehicle);
    } else if (At(frame, 2.0f)) {
        Record("cornering", "heading_change_deg", AngleDelta(Heading(g_vehicle), g_heading));
        Record("cornering", "speed_kmh", Speed(g_vehicle) * 50.0f * 3.6f);
        return true;
    }
    return false;
}
}  // namespace cornering

// A car dropped into the sea: how fast it loses buoyancy, as a share of its
// mass in ten thousandths a second, over the first second it is under water.
namespace sinking {
constexpr int32_t kModel = 400;
uint8_t* g_vehicle = nullptr;
float g_drowned = -1.0f;
float g_buoyancy = 0.0f;

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle || game::GroundZ(kSeaX, kSeaY) > -8.0f) {
        return false;
    }
    game::Teleport(g_vehicle, {kSeaX, kSeaY, 1.0f});
    g_drowned = -1.0f;
    return true;
}

bool Step(const Frame& frame) {
    const float t = Elapsed(frame);
    const float buoyancy = Field<float>(g_vehicle, game::kPhysicalBuoyancy);
    if (g_drowned < 0.0f) {
        if (Field<uint8_t>(g_vehicle, game::kVehicleDrowningByte) & game::kVehicleDrowning) {
            g_drowned = t;
            g_buoyancy = buoyancy;
        } else if (t > 8.0f) {
            Record("sinking", "never_under", 1.0);
            return true;
        }
        return false;
    }
    if (t < g_drowned + 1.0f) {
        return false;
    }
    const float mass = Field<float>(g_vehicle, game::kPhysicalMass);
    Record("sinking", "buoyancy_loss_per_second", (g_buoyancy - buoyancy) / mass * 10000.0f
                                                      / (t - g_drowned));
    return true;
}
}  // namespace sinking

// A lowrider with hydraulics standing still with its stance raised to the
// top: how long it takes to settle to the idle stance.
namespace hydraulics {
constexpr int32_t kModel = 412;
constexpr uint16_t kTopStance = 60;
uint8_t* g_vehicle = nullptr;

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    game::PutPlayerIn(g_vehicle);
    Field<uint32_t>(g_vehicle, game::kVehicleHandlingFlags) |= game::kHydraulicsInstalled;
    Field<uint16_t>(g_vehicle, game::kAutomobileMiscAngle) = kTopStance;
    return true;
}

bool Step(const Frame& frame) {
    const float t = Elapsed(frame);
    if (Field<uint16_t>(g_vehicle, game::kAutomobileMiscAngle) != 0 && t < 8.0f) {
        return false;
    }
    Record("hydraulics", "seconds_to_idle_stance", t);
    return true;
}
}  // namespace hydraulics

// A car standing with the throttle pressed from half a second: how fast the
// engine note rises towards the pedal, over a tenth of a second while it is
// still rising.
namespace engine_revs {
constexpr int32_t kModel = 400;
uint8_t* g_vehicle = nullptr;
float g_note = 0.0f;

bool Start() {
    g_vehicle = game::SpawnVehicle(kModel);
    if (!g_vehicle) {
        return false;
    }
    game::PutPlayerIn(g_vehicle);
    return true;
}

void Input(const Frame& frame, uint8_t* pad) {
    if (Elapsed(frame) > 0.5f) {
        Press(pad, game::kPadButtonCross);
    }
}

bool Step(const Frame& frame) {
    const float note = Field<float>(g_vehicle, game::kAutomobileGasPedalAudio);
    if (At(frame, 0.6f)) {
        g_note = note;
    } else if (At(frame, 0.7f)) {
        Record("engine_revs", "note_rise_per_second", (note - g_note) / 0.1f);
        return true;
    }
    return false;
}
}  // namespace engine_revs

// A police car whose horn is tapped for a tenth of a second at one second,
// held from two to three seconds and tapped again at four: a tap switches the
// siren, a hold sounds the fast siren and leaves it switched. The horn goes
// into the pad's horn history too, which the stock code reads.
namespace siren {
constexpr int32_t kModel = 596;
uint8_t* g_vehicle = nullptr;

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
    const bool horn = (t > 1.0f && t <= 1.1f) || (t > 2.0f && t <= 3.0f)
                   || (t > 4.0f && t <= 4.1f);
    if (horn) {
        Press(pad, game::kPadShockButtonL);
    }
    const uint8_t newest = Field<uint8_t>(pad, game::kPadHornHistoryIndex);
    if (newest < game::kPadHornHistorySize) {
        Field<uint8_t>(pad, game::kPadHornHistory + newest) = horn ? 1 : 0;
    }
}

double SirenOn() {
    return (Field<uint8_t>(g_vehicle, game::kVehicleSirenByte) & game::kVehicleSiren) ? 1.0
                                                                                       : 0.0;
}

bool Step(const Frame& frame) {
    if (At(frame, 1.5f)) {
        Record("siren", "on_after_first_tap", SirenOn());
    } else if (At(frame, 2.9f)) {
        Record("siren", "fast_siren_while_held",
               Field<uint32_t>(g_vehicle, game::kVehicleHornCounter) != 0 ? 1.0 : 0.0);
    } else if (At(frame, 3.5f)) {
        Record("siren", "on_after_hold", SirenOn());
    } else if (At(frame, 4.5f)) {
        Record("siren", "on_after_second_tap", SirenOn());
        return true;
    }
    return false;
}
}  // namespace siren

const harness::Scenario kList[] = {
    {"forklift", forklift::Start, forklift::Step, forklift::Input},
    {"firetruck", firetruck::Start, firetruck::Step, firetruck::Input},
    {"parking", parking::Start, parking::Step, nullptr},
    {"braking", braking::Start, braking::Step, braking::Input},
    {"cornering", cornering::Start, cornering::Step, cornering::Input},
    {"sinking", sinking::Start, sinking::Step, nullptr},
    {"hydraulics", hydraulics::Start, hydraulics::Step, nullptr},
    {"engine_revs", engine_revs::Start, engine_revs::Step, engine_revs::Input},
    {"siren", siren::Start, siren::Step, siren::Input},
};

}  // namespace

ScenarioList VehicleScenarios() {
    return {kList, std::size(kList)};
}

}  // namespace scenarios
