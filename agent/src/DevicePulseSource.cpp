#include "DevicePulseSource.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>

#include "Logger.hpp"
#include "pulsecnt_ioctl.h"

static_assert(sizeof(pulsecnt_sample) == 16, "driver record must be 16 bytes");

DevicePulseSource::DevicePulseSource(const std::string& path,
                                     const std::atomic<bool>& running)
    : running_(running) {
    // O_NONBLOCK: we wait with poll(), so a read never gets stuck
    fd_ = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        logError("Cannot open " + path + ": " + std::strerror(errno));
    }
}

DevicePulseSource::~DevicePulseSource() {
    if (fd_ >= 0) ::close(fd_);
}

bool DevicePulseSource::next(PulseEvent& event) {
    constexpr std::size_t kRecord = sizeof(pulsecnt_sample);

    while (running_) {
        if (!ready_.empty()) {
            event = ready_.front();
            ready_.pop_front();
            return true;
        }
        if (fd_ < 0) return false;

        // Wake up every 200 ms so a shutdown request is noticed quickly
        pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLIN;
        int r = ::poll(&pfd, 1, 200);
        if (r < 0) {
            if (errno == EINTR) continue;           // for example Ctrl+C
            logError(std::string("poll failed: ") + std::strerror(errno));
            return false;
        }
        if (r == 0) continue;                       // timeout, check running_ again
        if (pfd.revents & (POLLERR | POLLNVAL)) {
            logError("poll reported an error on the pulse device");
            return false;
        }

        unsigned char tmp[kRecord * 32];
        ssize_t n = ::read(fd_, tmp, sizeof(tmp));
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            logError(std::string("read failed: ") + std::strerror(errno));
            return false;
        }
        if (n == 0) {
            logWarn("Pulse device reached end of file");
            return false;
        }

        // A read may end in the middle of a record, so collect bytes first
        partial_.insert(partial_.end(), tmp, tmp + n);
        std::size_t off = 0;
        while (partial_.size() - off >= kRecord) {
            pulsecnt_sample s;
            std::memcpy(&s, partial_.data() + off, kRecord);
            ready_.push_back(PulseEvent{s.timestamp_ns, s.count});
            off += kRecord;
        }
        partial_.erase(partial_.begin(), partial_.begin() + static_cast<std::ptrdiff_t>(off));
    }
    return false;
}

bool DevicePulseSource::setLoad(double watts) {
    if (fd_ < 0) return false;
    if (watts < PULSECNT_MIN_WATTS || watts > PULSECNT_MAX_WATTS) return false;

    __u32 w = static_cast<__u32>(watts + 0.5);
    if (::ioctl(fd_, PULSECNT_IOC_SET_WATTS, &w) < 0) {
        logWarn(std::string("Could not set load on the device: ") + std::strerror(errno));
        return false;
    }
    return true;
}

bool DevicePulseSource::readDriverSettings(unsigned& watts, unsigned& pulsesPerKwh) {
    if (fd_ < 0) return false;

    pulsecnt_stats st{};
    if (::ioctl(fd_, PULSECNT_IOC_GET_STATS, &st) < 0) return false;

    watts = st.watts;
    pulsesPerKwh = st.pulses_per_kwh;
    return true;
}
