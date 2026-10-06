#pragma once

#include "core/patch.h"

namespace hff::bikes {

bool InstallAbandonedBikePhysicsStepFix();
// Runs CBike::ProcessControl for an abandoned bike at the original rate and
// returns true. Any other bike is left to the caller, which runs it as usual.
bool ProcessAbandonedBikeControl(void* bike, const DetourPatch& processControl);
// Stops stepping abandoned bikes at the original rate.
void DisableAbandonedBikePhysicsStep();

} // namespace hff::bikes
