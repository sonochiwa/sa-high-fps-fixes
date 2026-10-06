#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hff {

// Direct particle adds. `FxSystem_c::AddParticle` at 0x4AA440 is the one
// function every hand-written particle spawn in the game funnels through: the
// 43 call sites in the reversed source cover exhaust smoke, tyre spray, boat
// wake, water cannon, sandstorm, plane damage trails, ped splashes, gun shells
// and breaking debris. Almost all of them sit in a per-frame `ProcessControl`
// and add a fixed number of particles with no timestep anywhere, so emission
// scales with the frame rate: `CVehicle::AddExhaustParticles` alone adds two to
// eight per frame for every car in the world, and `CVehicle::DoBoatSplashes`
// adds two. At 2000 FPS that is about 66 times the density the game was drawn
// for, and the particle budget goes with it.
//
// This is deliberately not the same mechanism as `continuousWeaponParticles`,
// which patches `FxEmitter_c::CreateParticles` at 0x4A41E0, the emitter path,
// where a system converts elapsed time into a whole number of particles and
// drops the fraction. The two paths never meet: `CreateParticles` calls
// `FxEmitter_c::AddParticle`, not this function.
//
// The gate is per call site, keyed on the return address, which is finer than
// keying on the effect system: `m_SmokeII3expand` is shared by exhaust, fire,
// the water cannon and breaking objects, and those want different treatment.
// A site that was idle for at least one original 30 Hz frame is always let
// through, so an intermittent spawn (a shell casing, a hit spark, debris)
// never loses a particle. Shorter gaps remain part of the same stream. This is
// important for exhaust smoke: its random test often skips rendered frames at
// high FPS, but those sub-33-ms gaps do not make each following particle a new
// event. A stream is opened thirty times a second, which is what it emitted
// when the game was built. At 30 FPS or below every frame is open and nothing
// changes at all.
constexpr uintptr_t kFxAddParticle = 0x004AA440;
// sub esp,8 / push esi / push edi
constexpr std::array<uint8_t, 5> kExpectedFxAddParticle{
    0x83, 0xEC, 0x08, 0x56, 0x57
};

// Fire spread. `CFire::ProcessFire` at 0x53A570 runs once per frame per active
// fire and decides three of its four events by drawing a random number every
// frame and testing it against a fixed modulus, with no timestep anywhere:
//
//     if (rand() % 32  == 0) { set nearby vehicles alight }        // 0x53A8CA
//     if (rand() % 4   == 0) { damage nearby objects }             // 0x53AA0A
//     if (rand() % 128 == 0) { start a new fire nearby }           // 0x53AABB
//     if (rand() % 16  == 0) { merge with a nearby weak fire }     // 0x53AC46
//
// A per-frame probability is a rate per frame, so the events happen as many
// times more often as there are frames. At 2000 FPS that is roughly 66 times
// the 30 FPS rate: a car parked next to a fire catches almost at once instead
// of after about a second, and fires propagate explosively rather than
// creeping.
//
// The 0.02 modulus is deliberately not patched. Its body is
// `obj.ObjectFireDamage(CTimer::GetTimeStep() * 8.0f, ...)`, so the damage per
// hit already shrinks with the frame while the number of hits grows with it.
// The two cancel and the damage per second is the same at any frame rate.
//
// Each patched site is gated on a carry accumulator rather than a scaled
// probability, so the original draw is evaluated at an effective 30 FPS and the
// per-evaluation odds are untouched. At or below 30 FPS every frame is
// evaluated, which is stock behaviour exactly.
constexpr uintptr_t kFireVehicleGate = 0x0053A8CA;
constexpr uintptr_t kFireVehicleResume = 0x0053A8D2;
constexpr uintptr_t kFireVehicleSkip = 0x0053AA05;
constexpr uintptr_t kFireSpreadGate = 0x0053AABB;
constexpr uintptr_t kFireSpreadResume = 0x0053AAC3;
constexpr uintptr_t kFireSpreadSkip = 0x0053AC22;
constexpr uintptr_t kFireMergeGate = 0x0053AC46;
constexpr uintptr_t kFireMergeResume = 0x0053AC4E;
constexpr uintptr_t kFireMergeSkip = 0x0053AD72;
// test al,1Fh / jne 0053AA05
constexpr std::array<uint8_t, 8> kExpectedFireVehicleGate{
    0xA8, 0x1F, 0x0F, 0x85, 0x33, 0x01, 0x00, 0x00
};
// test al,7Fh / jne 0053AC22
constexpr std::array<uint8_t, 8> kExpectedFireSpreadGate{
    0xA8, 0x7F, 0x0F, 0x85, 0x5F, 0x01, 0x00, 0x00
};
// test al,0Fh / jne 0053AD72
constexpr std::array<uint8_t, 8> kExpectedFireMergeGate{
    0xA8, 0x0F, 0x0F, 0x85, 0x24, 0x01, 0x00, 0x00
};

// Falling glass: `FallingGlassPane::Update` stores a displacement and two
// angular displacements per frame and adds them to the pane unscaled.
constexpr uintptr_t kFallingGlassMove = 0x0071AABF;
constexpr uintptr_t kFallingGlassMoveReturn = 0x0071AAC5;
constexpr uintptr_t kFallingGlassTurnA = 0x0071AAEA;
constexpr uintptr_t kFallingGlassTurnAReturn = 0x0071AAF0;
constexpr uintptr_t kFallingGlassTurnB = 0x0071AB29;
constexpr uintptr_t kFallingGlassTurnBReturn = 0x0071AB2F;
constexpr std::array<uint8_t, 6> kExpectedFallingGlassMove{
    0xD9, 0x44, 0x24, 0x20, 0xD8, 0x06
};
constexpr std::array<uint8_t, 6> kExpectedFallingGlassTurnA{
    0x8B, 0x08, 0x89, 0x4C, 0x24, 0x2C
};
constexpr std::array<uint8_t, 6> kExpectedFallingGlassTurnB{
    0x8B, 0x10, 0x89, 0x54, 0x24, 0x38
};

// Breakable objects: `BreakObject_c::Update` counts each piece's lifetime down
// by one a frame.
constexpr uintptr_t kBreakObjectLifetime = 0x0059E420;
constexpr uintptr_t kBreakObjectLifetimeReturn = 0x0059E42B;
constexpr std::array<uint8_t, 11> kExpectedBreakObjectLifetime{
    0x8B, 0x54, 0x07, 0x70,
    0x8D, 0x44, 0x07, 0x70,
    0x4A,
    0x89, 0x10
};

// SA-MP CObject::Process. A moving object's rotation is a slerp whose parameter
// is `1 - GetDistance(target) / m_fTotalDistance`, so it advances only as far
// as the object's real position has advanced. SA-MP drives that position by
// handing CPhysical a move speed rather than writing coordinates, and a server
// that animates with a millimetre-scale move (casino reels) produces a
// per-frame delta that float world coordinates cannot represent at all at high
// FPS. The fraction then stays pinned at zero, and SA-MP's own catch-up term
// cannot recover it: its gain is per world unit, so over a millimetre it is a
// fourth-decimal correction.
//
// The stall also strands the move itself. Arrival is only declared when this
// frame's step would overshoot what is left of the distance, which never
// happens while the remaining distance is frozen, so CObject::Stop is never
// reached and the object is left mid-move at its original position. The next
// MoveObject then measures its distance from that stale position: for a server
// that animates by shuttling an object between two points, the second move is a
// zero-length one that arrives on its first frame and snaps instead of
// animating.
//
// Both are rebuilt from the elapsedSeconds * m_fSpeed the function already
// computes for its catch-up term, which is the schedule the server assumes.
//
// The two sites are found by scanning samp.dll's code section for the arrival
// test rather than by hardcoded addresses: samp.dll may be relocated, and the
// sites sit at different offsets in each SA-MP build. The pattern spans the
// whole test and both of its stack operands, so it also pins down the frame
// layout the thunks rely on. It must match exactly once:
// fld [esp+24h] (this frame's step) / fcomp [esp+20h] (remaining distance) /
// fnstsw ax / test ah,1 / jne <still moving>
constexpr std::array<uint8_t, 15> kSampObjectMoveArrivalPattern{
    0xD9, 0x44, 0x24, 0x24,
    0xD8, 0x5C, 0x24, 0x20,
    0xDF, 0xE0,
    0xF6, 0xC4, 0x01,
    0x0F, 0x85
};
// The 13 bytes of the test are replaced; the jne that follows stays in place
// but is jumped over, and supplies the still-moving destination through its
// rel32. The instruction after it is the arrival branch.
constexpr size_t kSampObjectArrivalPatchSize = 13;
constexpr size_t kSampObjectMoveContinueDisplacement = 15;
constexpr size_t kSampObjectMoveArrivedOffset = 19;
// Distance from the arrival test to the rotation fraction, within the same
// function. Verified against the bytes at the destination before patching.
constexpr size_t kSampObjectRotationProgressOffset = 0x12C;
constexpr size_t kSampObjectRotationProgressPatchSize = 10;
// fld [esp+28h] (remaining distance) / fdiv [ebx+15Bh] (total distance)
constexpr std::array<uint8_t, 10> kExpectedSampObjectRotation{
    0xD9, 0x44, 0x24, 0x28,
    0xD8, 0xB3, 0x5B, 0x01, 0x00, 0x00
};

// CTaskComplexCopInCar::ControlSubTask lets a cop leave the police car and
// arrest on foot once the suspect's car is slow, tested in two places as
// |moveSpeed|^2 * timestep * 50 <= 1. With the timestep in the test the speed
// that counts as slow rises with the frame rate, from about 20 km/h at
// 30 FPS to about 60 km/h at 300.
// fmul dword ptr ds:[00B7CB5Ch]
constexpr std::array<uintptr_t, 2> kCopSuspectSlowTests{0x0068FF6F, 0x0069017E};
constexpr std::array<uint8_t, 6> kExpectedCopSuspectSlowTest{
    0xD8, 0x0D, 0x5C, 0xCB, 0xB7, 0x00
};

// CExplosion::Update. Each explosion counts its frames in a byte that the
// update increments and that ends the explosion when it wraps to zero, after
// 255 frames: 8.5 s at 30 FPS, longer than any explosion lasts, but 0.85 s at
// 300 FPS, which cuts car, boat and aircraft explosions and the fires they
// light short.
// inc byte ptr [esi-8] / jmp 007379F7h
constexpr uintptr_t kExplosionFrameCount = 0x007379EE;
constexpr uintptr_t kExplosionFrameCountResume = 0x007379F7;
constexpr std::array<uint8_t, 5> kExpectedExplosionFrameCount{
    0xFE, 0x46, 0xF8, 0xEB, 0x04
};
// Rolled once a frame for every explosion: an aircraft explosion sets off a
// smaller one with `GetRandomNumberInRange(0, 100) < 5`, and an explosion
// with a victim sets it alight with `rand() & 0x1F == 0`. The burning fuel of
// a car, boat or aircraft explosion is drawn with one `CreateFxSystem` per
// fuel stream a frame for its first 200 ms.
constexpr uintptr_t kExplosionAircraftBlastRoll = 0x007378D7;
constexpr uintptr_t kExplosionVictimFireRoll = 0x00737717;
constexpr uintptr_t kExplosionFuelEffect = 0x00737AE0;

// CWeather::Update starts a lightning burst in a storm when
// `(rand() & 0xFFFF) < 200` and ends it when `(rand() & 0xFF) < 24`, both
// rolled every frame, and measures the burst in CTimer::m_FrameCounter frames,
// up to 20; that length sets how late the thunder follows and how hard the pad
// shakes. Above 30 FPS bursts start many times more often and count short
// frames.
constexpr uintptr_t kLightningEndRoll = 0x0072B9DB;
constexpr uintptr_t kLightningStartRoll = 0x0072BA88;
// mov eax,dword ptr ds:[00B7CB4Ch]
constexpr std::array<uintptr_t, 2> kLightningFrameReads{0x0072B9EA, 0x0072BAA6};
constexpr std::array<uint8_t, 5> kExpectedLightningFrameRead{
    0xA1, 0x4C, 0xCB, 0xB7, 0x00
};

} // namespace hff
