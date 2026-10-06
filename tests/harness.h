#pragma once

#include <cstdint>

// The frame-by-frame driver of the autotest and what a scenario sees of it.
namespace harness {

// The frame a scenario is on: frames since it started, the virtual frame
// rate and the seconds of virtual time those frames make.
struct Frame {
    uint32_t index;
    int fps;
    float seconds;
};

// A scenario is set up once, then stepped every game frame until it says it
// is done; it writes its pad input after the game has read the real pad.
struct Scenario {
    const char* name;
    // Returns false when the scenario cannot run; the reason is recorded.
    bool (*start)();
    // Called before the game's frame; returns true when the scenario is done.
    bool (*step)(const Frame& frame);
    // The pad state of this frame, CPad::Pads[0].NewState; may be null.
    void (*input)(const Frame& frame, uint8_t* pad);
};

// Writes "scenario metric value" to the results file.
void Record(const char* scenario, const char* metric, double value);
void Note(const char* text);

bool Install();

}  // namespace harness
