#include "game/scripts.h"

#include "game/addresses.h"

#include <array>
#include <cstring>

namespace hff {

bool ScriptNameMatches(const char* name, const char* expected) {
    std::array<char, kRunningScriptNameSize + 1> buffer{};
    std::memcpy(buffer.data(), name, kRunningScriptNameSize);
    return _stricmp(buffer.data(), expected) == 0;
}

uintptr_t FirstRunningScript() {
    const auto queueHead = *reinterpret_cast<const uintptr_t*>(kScriptQueueOperand);
    return queueHead ? *reinterpret_cast<const uintptr_t*>(queueHead) : 0;
}

uintptr_t FindRunningScript(const char* name) {
    for (auto script = FirstRunningScript(); script;
         script = *reinterpret_cast<const uintptr_t*>(script)) {
        if (ScriptNameMatches(reinterpret_cast<const char*>(
                                  script + kRunningScriptNameOffset),
                              name)) {
            return script;
        }
    }
    return 0;
}

} // namespace hff
