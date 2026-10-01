#include <string>

#include "Config.hpp"
#include "Logger.hpp"
#include "PulseEvent.hpp"
#include "ThreadSafeQueue.hpp"

int main(int argc, char* argv[]) {
    std::string configPath = (argc > 1) ? argv[1] : "agent.conf";

    Config config;
    if (!config.load(configPath)) {
        logError("Could not open config file: " + configPath);
        return 1;
    }

    Logger::instance().openFile(config.getString("log_file", "energy-agent.log"));
    logInfo("energy-agent starting");
    logInfo("port = " + std::to_string(config.getInt("port", 8080)));
    logInfo("pulses_per_kwh = " + std::to_string(config.getInt("pulses_per_kwh", 1000)));
    logInfo("tariff_per_kwh = " + std::to_string(config.getDouble("tariff_per_kwh", 0.0)));

    ThreadSafeQueue<PulseEvent> queue;
    queue.push(PulseEvent{1000000000ULL, 1});
    auto ev = queue.waitPop();
    if (ev) logInfo("queue test OK, count = " + std::to_string(ev->count));

    logWarn("this is a warning example");
    logInfo("energy-agent finished");
    return 0;
}
