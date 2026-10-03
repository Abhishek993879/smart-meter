#pragma once
#include "PulseEvent.hpp"

// Anything that can deliver pulses: a simulator, or the /dev/pulsecnt driver.
class PulseSource {
public:
    virtual ~PulseSource() = default;

    // Blocks until the next pulse. Returns false when the source has ended
    // or the program is shutting down.
    virtual bool next(PulseEvent& event) = 0;

    // Change the (simulated) load. Returns false if this source cannot do that.
    virtual bool setLoad(double watts) {
        (void)watts;
        return false;
    }
};
