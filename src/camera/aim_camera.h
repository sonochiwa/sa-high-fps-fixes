#pragma once

namespace hff::camera {

bool InstallAimCameraShakeFix();
void RemoveAimCameraHooks();
// The aim guard pins both camera timesteps to the 50 FPS minimum for the whole
// of CCamera::Process. Corrections patched into code that runs inside that
// call read the real frame duration through here instead. Outside the guard
// this is just CTimer::ms_fTimeStep.
float __cdecl UnguardedTimeStep();

} // namespace hff::camera
