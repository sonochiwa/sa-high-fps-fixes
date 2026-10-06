#pragma once

#include "core/patch.h"

namespace hff::bikes {

// Adds the bunny hop, knock-off and rider-fall hooks to `patches`, so they roll
// back with the rest of the bike pitch fix. Logs and returns false when a site
// does not match.
bool InstallBmxJumpHooks(PatchSet& patches);
// Called after every CBike::ProcessControl. Both do nothing unless the BMX
// the player just launched with a bunny hop is the bike being processed.
void CorrectBmxLaunchPitch(void* vehicle);
void UpdateBmxLandingProtection(void* vehicle);

} // namespace hff::bikes
