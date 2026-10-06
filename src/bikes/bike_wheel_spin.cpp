#include "bikes/bike_wheel_spin.h"

#include "core/log.h"
#include "core/patch.h"
#include "core/timestep.h"
#include "game/addresses.h"
#include "game/frame_steps.h"
#include "game/sites/bikes.h"

#include <array>
#include <cstdint>

namespace hff::bikes {

namespace {

std::array<SitePatch, 5> g_bikeWheelSpinPatches{};

// Scales the free front wheel's angular velocity into the current frame before
// the pitch angle integrates it. `esi` is the bike and the field offset is the
// one the replaced `fadd` used; the original `fstp` at the return address
// stores the result.
__declspec(naked) void BikeWheelPitchIntegrateThunk() {
    __asm {
        fld dword ptr ds:[0x00B7CB5C]
        fdiv g_originalTimeStepValue
        fmulp st(1), st
        fadd dword ptr [esi + 0x750]
        ret
    }
}

} // namespace

bool InstallBikeWheelSpinFix() {
    PatchSet patches("Bike wheel spin fix");
    struct Site {
        uintptr_t address;
        const void* thunk;
        const uint8_t* expected;
    };
    const Site sites[] = {
        {kBikeWheelSpinDampA, &FrameStepDecayThunk,
         kExpectedFrameStepDecay.data()},
        {kBikeWheelSpinDampB, &FrameStepDecayThunk,
         kExpectedFrameStepDecay.data()},
        {kBikeWheelPitchIntegrate, &BikeWheelPitchIntegrateThunk,
         kExpectedBikeWheelPitchIntegrate.data()},
        {kBikeRearWheelSpeedDecel, &FrameStepDecrementThunk,
         kExpectedFrameStepDecrement.data()},
        {kBikeRearWheelSpeedAccel, &FrameStepIncrementThunk,
         kExpectedFrameStepIncrement.data()},
    };
    for (size_t i = 0; i < g_bikeWheelSpinPatches.size(); ++i) {
        if (!patches.Track(
                InstallBranch(g_bikeWheelSpinPatches[i], sites[i].address,
                              sites[i].thunk, sites[i].expected, 6, 0xE8),
                g_bikeWheelSpinPatches[i])) {
            Log("Bike wheel spin fix skipped: CBike::ProcessControl bytes do "
                "not match GTA SA 1.0 US.");
            return false;
        }
    }
    patches.Commit();
    Log("Installed a timestep-scaled bike front wheel spin.");
    return true;
}

} // namespace hff::bikes
