#pragma once

namespace hff {

// MinHook is set up once for every fix that hooks through it and released at
// shutdown, after those hooks are removed.
bool InitializeMinHook();
void UninitializeMinHook();

// Held around every MinHook call that suspends the other threads of the
// process (enable, disable, remove, uninitialize). Two plugins that suspend
// all threads at once from different threads suspend each other and the game
// hangs, before its window appears when it happens at start-up. The mutex is
// named per process and shared by every plugin that takes it around its
// thread freezes; a holder suspended by a plugin that does not take it is
// waited for only so long, then the freeze goes ahead.
class ThreadFreezeLock {
public:
    ThreadFreezeLock();
    ~ThreadFreezeLock();
    ThreadFreezeLock(const ThreadFreezeLock&) = delete;
    ThreadFreezeLock& operator=(const ThreadFreezeLock&) = delete;

private:
    void* mutex;
    bool owned;
};

} // namespace hff
