#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include "DevicePulseSource.hpp"
#include "pulsecnt_ioctl.h"

// A FIFO stands in for /dev/pulsecnt: it gives the same poll() and read()
// behaviour, so the real reading code runs without needing the driver.
class FifoTest : public ::testing::Test {
protected:
    void SetUp() override {
        char tmpl[] = "/tmp/pulsetestXXXXXX";
        ASSERT_NE(mkdtemp(tmpl), nullptr);
        dir_ = tmpl;
        path_ = dir_ + "/fifo";
        ASSERT_EQ(mkfifo(path_.c_str(), 0600), 0);
    }

    void TearDown() override {
        if (writeFd_ >= 0) close(writeFd_);
        unlink(path_.c_str());
        rmdir(dir_.c_str());
    }

    // Must be called after the DevicePulseSource has opened the read end
    void openWriter() {
        writeFd_ = open(path_.c_str(), O_WRONLY | O_NONBLOCK);
        ASSERT_GE(writeFd_, 0);
    }

    std::string dir_, path_;
    int writeFd_ = -1;
    std::atomic<bool> running_{true};
};

TEST_F(FifoTest, MissingPathFailsToOpen) {
    DevicePulseSource src("/tmp/this-pulse-device-does-not-exist", running_);
    EXPECT_FALSE(src.isOpen());

    PulseEvent e{};
    EXPECT_FALSE(src.next(e));
}

TEST_F(FifoTest, ReadsSeveralSamples) {
    DevicePulseSource src(path_, running_);
    ASSERT_TRUE(src.isOpen());
    openWriter();

    pulsecnt_sample samples[2] = {{111, 1}, {222, 2}};
    ASSERT_EQ(write(writeFd_, samples, sizeof(samples)),
              static_cast<ssize_t>(sizeof(samples)));

    PulseEvent a{}, b{};
    ASSERT_TRUE(src.next(a));
    ASSERT_TRUE(src.next(b));
    EXPECT_EQ(a.timestamp_ns, 111u);
    EXPECT_EQ(a.count, 1u);
    EXPECT_EQ(b.timestamp_ns, 222u);
    EXPECT_EQ(b.count, 2u);
}

TEST_F(FifoTest, ReassemblesARecordSplitAcrossReads) {
    DevicePulseSource src(path_, running_);
    ASSERT_TRUE(src.isOpen());
    openWriter();

    pulsecnt_sample s{123, 7};
    const unsigned char* raw = reinterpret_cast<const unsigned char*>(&s);
    ASSERT_EQ(write(writeFd_, raw, 10), 10);          // only part of the record

    std::thread rest([this, raw] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ssize_t w = write(writeFd_, raw + 10, 6);     // the remaining 6 bytes
        (void)w;
    });

    PulseEvent e{};
    bool got = src.next(e);
    rest.join();

    ASSERT_TRUE(got);
    EXPECT_EQ(e.timestamp_ns, 123u);
    EXPECT_EQ(e.count, 7u);
}

TEST_F(FifoTest, StopsWhenNotRunning) {
    DevicePulseSource src(path_, running_);
    ASSERT_TRUE(src.isOpen());
    running_ = false;

    PulseEvent e{};
    EXPECT_FALSE(src.next(e));
}

TEST_F(FifoTest, EndsWhenTheWriterCloses) {
    DevicePulseSource src(path_, running_);
    ASSERT_TRUE(src.isOpen());
    openWriter();
    close(writeFd_);
    writeFd_ = -1;

    PulseEvent e{};
    EXPECT_FALSE(src.next(e));                        // end of file, not a hang
}

TEST_F(FifoTest, DriverOnlyCallsFailOnAPlainFile) {
    DevicePulseSource src(path_, running_);
    ASSERT_TRUE(src.isOpen());

    unsigned w = 0, ppk = 0;
    EXPECT_FALSE(src.setLoad(2000.0));                // a FIFO does not know the ioctl
    EXPECT_FALSE(src.readDriverSettings(w, ppk));
}

TEST_F(FifoTest, RejectsOutOfRangeLoadWithoutCallingTheDriver) {
    DevicePulseSource src(path_, running_);
    ASSERT_TRUE(src.isOpen());
    EXPECT_FALSE(src.setLoad(0.0));
    EXPECT_FALSE(src.setLoad(1e9));
}
