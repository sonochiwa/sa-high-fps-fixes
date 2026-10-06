#include "framerate/frame_limit.h"

#include "core/config.h"
#include "core/log.h"
#include "core/memory.h"
#include "core/patch.h"
#include "game/addresses.h"
#include "game/sites/framerate.h"

#include <windows.h>

#include <algorithm>

namespace hff::framerate {

namespace {

BytePatch g_frameLimiterGatePatch{};
BytePatch g_frameLimitStorePatch{};

} // namespace

void WriteFrameLimit(uint8_t value) {
    WriteBytes(kFrameLimit, &value, 1);
}

uint8_t ReadFrameLimit() {
    __try {
        return *reinterpret_cast<const uint8_t*>(kFrameLimit);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool FrameLimitHoldsGate() {
    return g_frameLimiterGatePatch.installed;
}

void InstallFrameLimit() {
    const int limit = std::clamp(ReadNumber("framerate", "fpsLimit", 0), 0, 255);
    if (limit == 0) {
        return;
    }
    PatchSet patches("Frame limit");
    if (!MemoryMatches(kFrameLimiterGate, kExpectedFrameLimiterGate)
        || !MemoryMatches(kFrameLimitStore, kExpectedFrameLimitStore)) {
        Log("Frame limit skipped: frame limiter bytes do not match GTA SA 1.0 US.");
        return;
    }
    // 0x75 is `jne`, 0xEB the unconditional `jmp` with the same displacement.
    if (!patches.Track(
            InstallByte(g_frameLimiterGatePatch, kFrameLimiterGate, 0xEB),
            g_frameLimiterGatePatch)
        || !patches.Track(
            InstallByte(g_frameLimitStorePatch, kFrameLimitStoreOperand,
                        static_cast<uint8_t>(limit)),
            g_frameLimitStorePatch)) {
        Log("Frame limit failed while patching the frame limiter.");
        return;
    }
    WriteFrameLimit(static_cast<uint8_t>(limit));
    patches.Commit();
    Log("Installed the configured frame limit.");
}

} // namespace hff::framerate
