#include "scenario_tools.h"

#include <cmath>

namespace scenarios {
namespace {

constexpr float kRadToDeg = 57.2957795f;

}  // namespace

float Elapsed(const Frame& frame) {
    return static_cast<float>(frame.index + 1) / static_cast<float>(frame.fps);
}

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

float Speed(const void* entity) {
    const Vector v = Field<Vector>(entity, game::kPhysicalMoveSpeed);
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

void Launch(uint8_t* vehicle, float kmh) {
    // A move speed is in metres per fiftieth of a second.
    const float speed = kmh / 3.6f / 50.0f;
    const Vector forward = game::MatrixRow(vehicle, game::kMatrixForward);
    Field<Vector>(vehicle, game::kPhysicalMoveSpeed) = {forward.x * speed, forward.y * speed,
                                                        0.0f};
}

}  // namespace scenarios
