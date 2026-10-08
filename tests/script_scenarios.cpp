#include "scenario_tools.h"

#include <iterator>

namespace scenarios {
namespace {

// Missions loaded from the stock main.scm and left asleep: each frame the
// scenario reads the steps and thresholds the plugin wrote into the mission
// and adds up what the mission's own instructions would do with them, from
// one second on. The offsets are from the start of the mission.
float ElapsedSince(const Frame& frame, float start) {
    return Elapsed(frame) - start;
}

int8_t Int8Literal(uint32_t offset) {
    return At<int8_t>(game::kMissionBlock + offset + 6);
}

float FloatLiteral(uint32_t offset) {
    return At<float>(game::kMissionBlock + offset + 6);
}

// Catalyst, RYDER3: the throw meter rises by local 124@ a frame while fire
// is held, and a crate breaks after more than four frames of contact,
// counted by `126@ += 1`.
namespace catalyst {
constexpr uint32_t kStart = 0xC0A67;
constexpr uint32_t kEnd = 0xC5958;
constexpr uint32_t kCrateContact = 12859;
constexpr size_t kMeterStepLocal = 124;
constexpr int32_t kMeterStep = 5;
int32_t g_meter = 0;
int32_t g_contact = 0;

int32_t& MeterStep() {
    return At<int32_t>(game::kMissionLocals + kMeterStepLocal * sizeof(int32_t));
}

bool Start() {
    g_meter = 0;
    g_contact = 0;
    if (!game::LoadSleepingMission("RYDER3", kStart, kEnd)) {
        return false;
    }
    // As the mission sets it when it starts.
    MeterStep() = kMeterStep;
    return true;
}

bool Step(const Frame& frame) {
    const float t = ElapsedSince(frame, 1.0f);
    if (t > 0.0f && t <= 1.0f) {
        g_meter += MeterStep();
        g_contact += Int8Literal(kCrateContact);
    }
    if (!At(frame, 2.0f)) {
        return false;
    }
    Record("catalyst", "meter_rise_per_second", g_meter);
    Record("catalyst", "crate_contact_count_per_second", g_contact);
    return true;
}
}  // namespace catalyst

// Interdiction, DES3: an attacking helicopter far from Mike's is pushed by a
// literal a frame, the landing one is braked by a share of its speed a
// frame, and Mike's helicopter takes damage whenever a millisecond timer
// passes a threshold, the timer emptied each time.
namespace interdiction {
constexpr uint32_t kStart = 0x1DA540;
constexpr uint32_t kEnd = 0x1E0FF4;
constexpr uint32_t kDamageThreshold = 11364;
constexpr uint32_t kPush = 19743;
constexpr uint32_t kBrake = 20309;
// GET_CAR_SPEED_VECTOR gives the speed in m/s, fifty times the move speed
// the brake takes it from.
constexpr float kSpeedVectorScale = 50.0f;
float g_push = 0.0f;
float g_kept = 1.0f;
int32_t g_timer = 0;
int32_t g_damage = 0;
uint32_t g_time = 0;

bool Start() {
    g_push = 0.0f;
    g_kept = 1.0f;
    g_timer = 0;
    g_damage = 0;
    g_time = At<uint32_t>(game::kTimerTime);
    return game::LoadSleepingMission("DES3", kStart, kEnd);
}

bool Step(const Frame& frame) {
    const uint32_t time = At<uint32_t>(game::kTimerTime);
    const float t = ElapsedSince(frame, 1.0f);
    if (t > 0.0f && t <= 1.0f) {
        g_push += FloatLiteral(kPush);
        g_kept *= 1.0f + FloatLiteral(kBrake) * kSpeedVectorScale;
    }
    if (t > 0.0f && t <= 2.0f) {
        g_timer += static_cast<int32_t>(time - g_time);
        if (g_timer > Int8Literal(kDamageThreshold)) {
            ++g_damage;
            g_timer = 0;
        }
    }
    g_time = time;
    if (!At(frame, 3.0f)) {
        return false;
    }
    Record("interdiction", "push_per_second", g_push);
    Record("interdiction", "speed_kept_braking_a_second", g_kept);
    Record("interdiction", "damage_ticks_per_second", g_damage / 2.0);
    return true;
}
}  // namespace interdiction

const harness::Scenario kList[] = {
    {"catalyst", catalyst::Start, catalyst::Step, nullptr},
    {"interdiction", interdiction::Start, interdiction::Step, nullptr},
};

}  // namespace

ScenarioList ScriptScenarios() {
    return {kList, std::size(kList)};
}

}  // namespace scenarios
