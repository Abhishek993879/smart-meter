#pragma once
#include "PulseEvent.hpp"

// Anything that can deliver pulses: a simulator now, /dev/pulsecnt later.
class PulseSource {
public:
    virtual ~PulseSource() = default;

    // Blocks until the next pulse. Returns false when the program is shutting down.
    virtual bool next(PulseEvent& event) = 0;
};
