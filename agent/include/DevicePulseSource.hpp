#pragma once
#include <atomic>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "PulseSource.hpp"

// Reads pulses from the kernel driver (/dev/pulsecnt) using poll() and read().
// Any file that delivers 16-byte pulsecnt_sample records works, which is how
// the unit tests run it without a driver (they use a FIFO).
class DevicePulseSource : public PulseSource {
public:
    DevicePulseSource(const std::string& path, const std::atomic<bool>& running);
    ~DevicePulseSource() override;
    DevicePulseSource(const DevicePulseSource&) = delete;
    DevicePulseSource& operator=(const DevicePulseSource&) = delete;

    bool isOpen() const { return fd_ >= 0; }

    bool next(PulseEvent& event) override;

    // ioctl PULSECNT_IOC_SET_WATTS. Safe to call from another thread.
    bool setLoad(double watts) override;

    // ioctl PULSECNT_IOC_GET_STATS: the driver's current load and meter constant.
    bool readDriverSettings(unsigned& watts, unsigned& pulsesPerKwh);

private:
    int fd_ = -1;
    const std::atomic<bool>& running_;
    std::vector<unsigned char> partial_;   // bytes of a record not yet complete
    std::deque<PulseEvent> ready_;         // parsed pulses waiting to be returned
};
