#include "bikes/process_control.h"

#include "bikes/abandoned_bike.h"
#include "bikes/bmx_jump.h"
#include "game/sites/bikes.h"

namespace hff::bikes {

namespace {

using ThisCallVoidFn = void(__thiscall*)(void*);

DetourPatch g_bikeProcessPatch{};

void __fastcall HookedBikeProcessControl(void* bike, void*) {
    if (ProcessAbandonedBikeControl(bike, g_bikeProcessPatch)) {
        return;
    }
    reinterpret_cast<ThisCallVoidFn>(g_bikeProcessPatch.gateway)(bike);
    CorrectBmxLaunchPitch(bike);
    UpdateBmxLandingProtection(bike);
}

} // namespace

bool EnsureBikeProcessControlHook() {
    return g_bikeProcessPatch.installed
        || InstallDetour(g_bikeProcessPatch, kBikeProcessControl,
                         &HookedBikeProcessControl,
                         kExpectedBikeProcessControl.data(),
                         kExpectedBikeProcessControl.size());
}

DetourPatch& BikeProcessControlPatch() {
    return g_bikeProcessPatch;
}

} // namespace hff::bikes
