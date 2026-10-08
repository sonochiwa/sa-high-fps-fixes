#pragma once

#include "game.h"
#include "harness.h"

#include <cstddef>
#include <cstdint>

// What every scenario uses, and the scenarios each group file defines.
namespace scenarios {

using game::At;
using game::Field;
using game::Vector;
using harness::Frame;
using harness::Record;

struct ScenarioList {
    const harness::Scenario* first;
    size_t count;
};

ScenarioList WorldScenarios();
ScenarioList VehicleScenarios();
ScenarioList AircraftScenarios();
ScenarioList PedScenarios();
ScenarioList ScriptScenarios();

// Open sea past the end of the Santa Maria pier.
constexpr float kSeaX = 836.0f;
constexpr float kSeaY = -2250.0f;

// Seconds of virtual time once this frame has run.
float Elapsed(const Frame& frame);
// True on the one frame at which `seconds` of virtual time have passed.
bool At(const Frame& frame, float seconds);
void Press(uint8_t* pad, size_t button);
Vector Add(const Vector& a, const Vector& b, float scale);
float Distance(const Vector& a, const Vector& b);
// The active fire nearest to a point, or null when none is within reach.
uint8_t* FireNear(const Vector& point, float reach);
float Heading(const void* entity);
// Bank angle over the whole circle, from how far the right and up axes tilt.
float Roll(const void* entity);
float AngleDelta(float to, float from);
float Speed(const void* entity);
// Gives a vehicle the player is in a speed along its nose, in km/h.
void Launch(uint8_t* vehicle, float kmh);

}  // namespace scenarios
