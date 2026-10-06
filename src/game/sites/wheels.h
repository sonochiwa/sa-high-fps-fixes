#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// Wheel friction. `CVehicle::ProcessWheel` loads the friction constant at
// 0xC2B9CC once per frame and applies it as a per-frame loss on the car drive
// and brake paths and the three bike paths. Each load is replaced by the
// constant scaled into the current frame.
constexpr uintptr_t kWheelFriction = 0x00C2B9CC;
constexpr std::array<uintptr_t, 5> kWheelFrictionSites{
    0x006D6E69, 0x006D6EA8, 0x006D767F, 0x006D76AB, 0x006D76CD
};
constexpr uintptr_t kWheelFrictionCarDriveReturn = 0x006D6E6F;
constexpr uintptr_t kWheelFrictionCarBrakeReturn = 0x006D6EAE;
constexpr uintptr_t kWheelFrictionBikeBaseReturn = 0x006D7685;
constexpr uintptr_t kWheelFrictionBikeDriveReturn = 0x006D76B1;
constexpr uintptr_t kWheelFrictionBikeBrakeReturn = 0x006D76D3;
constexpr std::array<uint8_t, 6> kExpectedWheelFriction{
    0xD9, 0x05, 0xCC, 0xB9, 0xC2, 0x00
};

// Wheels of a simple (on-rails) AI car turn once per frame with no timestep.
constexpr std::array<uintptr_t, 4> kRailWheelSpinSites{
    0x006B523F, 0x006B524F, 0x006B525D, 0x006B5269
};
constexpr uintptr_t kRailWheelSpinReturn0 = 0x006B5245;
constexpr uintptr_t kRailWheelSpinReturn1 = 0x006B5255;
constexpr uintptr_t kRailWheelSpinReturn2 = 0x006B5263;
constexpr uintptr_t kRailWheelSpinReturn3 = 0x006B526F;
constexpr std::array<std::array<uint8_t, 6>, 4> kExpectedRailWheelSpin{{
    {0xD8, 0x86, 0x28, 0x08, 0x00, 0x00},
    {0xD8, 0x86, 0x2C, 0x08, 0x00, 0x00},
    {0xD8, 0x86, 0x30, 0x08, 0x00, 0x00},
    {0xD8, 0x86, 0x34, 0x08, 0x00, 0x00},
}};

// Burnout: the 3000.0 wheel speed `CAutomobile::ProcessCarWheelPair` adds per
// frame while the car burns out.
constexpr uintptr_t kBurnoutPatch = 0x006A4FE6;
constexpr uintptr_t kBurnoutReturn = 0x006A4FEC;
constexpr uintptr_t kBurnoutConstant = 0x00859A94;
constexpr std::array<uint8_t, 6> kExpectedBurnout{
    0xD9, 0x05, 0x94, 0x9A, 0x85, 0x00
};

// Free wheel spin. In `CAutomobile::ProcessCarWheelPair`, a wheel that is not
// touching the ground has its speed changed once per frame with no timestep,
// and then, two instructions later, its rotation is integrated with one:
//
//     if (driveWheels && acceleration != 0.0f) {
//         if (acceleration > 0.0f) { if (speed <  1.0f) speed -= 0.1f;  }
//         else                     { if (speed > -1.0f) speed += 0.05f; }
//     } else {
//         speed *= 0.95f;
//     }
//     m_wheelRotation[i] += CTimer::GetTimeStep() * m_wheelSpeed[i];
//
// The integration is right and the three lines above it are wrong. At a high
// frame rate an airborne drive wheel spins up to its limit almost instantly
// and a free wheel stops almost instantly. Six sites, the three shared frame
// steps mirrored across the left and right wheel.
//
// `burnout` also patches this function, at 0x6A4FE6, where it scales the 3000.0
// burnout speed constant. That is a different site and the two do not overlap.
constexpr uintptr_t kWheelSpinDecelLeft = 0x006A5DB2;
constexpr uintptr_t kWheelSpinDecelRight = 0x006A5F1D;
constexpr uintptr_t kWheelSpinAccelLeft = 0x006A5DF7;
constexpr uintptr_t kWheelSpinAccelRight = 0x006A5F62;
constexpr uintptr_t kWheelSpinDampLeft = 0x006A5E54;
constexpr uintptr_t kWheelSpinDampRight = 0x006A5FBF;

// Rendered wheel settle. Vehicle `PreRender` functions keep a visual wheel
// offset separate from the physical suspension and ease a wheel returning down
// with `position += (target - position) * 0.75` once per rendered frame. The
// timestep-scaled weight is applied to bikes and aircraft, where it smooths
// the animation. Automobiles deliberately keep the stock weight: stretching
// their downward travel to the 30 FPS duration lets long-travel rear wheels
// visibly hang below the body after the physical suspension has already moved.
// Cosmetic: none of these sites changes the suspension that moves the vehicle.
constexpr uintptr_t kWheelSettleBikeA = 0x006BD150;
constexpr uintptr_t kWheelSettleBikeB = 0x006BD1D0;
constexpr uintptr_t kWheelSettleBmxA = 0x006C08D0;
constexpr uintptr_t kWheelSettleBmxB = 0x006C0950;
constexpr uintptr_t kWheelSettleHeli = 0x006C559E;
constexpr uintptr_t kWheelSettlePlane = 0x006C95AE;
constexpr uintptr_t kWheelSettleConstant = 0x00858F34;
// fmul dword ptr ds:[00858F34h]   (0.75)
constexpr std::array<uint8_t, 6> kExpectedWheelSettle{
    0xD8, 0x0D, 0x34, 0x8F, 0x85, 0x00
};

} // namespace hff
