#include <signal.h>

#include <atomic>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "AnalyticsEngine.hpp"
#include "Config.hpp"
#include "DevicePulseSource.hpp"
#include "HttpServer.hpp"
#include "Logger.hpp"
#include "PulseEvent.hpp"
#include "SharedState.hpp"
#include "SimulatedPulseSource.hpp"
#include "Storage.hpp"
#include "ThreadSafeQueue.hpp"

static std::atomic<bool> g_running{true};

// Runs when you press Ctrl+C or send SIGTERM. It only flips a flag.
static void onSignal(int) { g_running = false; }

int main(int argc, char* argv[]) {
    std::string configPath = (argc > 1) ? argv[1] : "agent.conf";

    Config config;
    if (!config.load(configPath)) {
        logError("Could not open config file: " + configPath);
        return 1;
    }
    Logger::instance().openFile(config.getString("log_file", "energy-agent.log"));

    int          ppk        = config.getInt("pulses_per_kwh", 3600);
    const double windowSec  = config.getDouble("window_seconds", 10.0);
    const double alertWatts = config.getDouble("alert_watts", 3000.0);
    double       loadWatts  = config.getDouble("sim_watts", 1500.0);

    Storage storage;
    if (!storage.open(config.getString("db_path", "energy.db"))) return 1;

    struct sigaction sa {};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // Choose where the pulses come from
    const std::string sourceType = config.getString("source", "simulated");
    std::unique_ptr<PulseSource> source;

    if (sourceType == "device") {
        const std::string path = config.getString("device_path", "/dev/pulsecnt");
        auto dev = std::make_unique<DevicePulseSource>(path, g_running);
        if (!dev->isOpen()) {
            logError("Is the driver loaded? Try: scripts/driver_load.sh");
            return 1;
        }
        // The driver knows its own meter constant and load: trust it over the config
        unsigned devWatts = 0, devPpk = 0;
        if (dev->readDriverSettings(devWatts, devPpk)) {
            if (static_cast<int>(devPpk) != ppk) {
                logWarn("pulses_per_kwh in the config (" + std::to_string(ppk) +
                        ") differs from the driver's (" + std::to_string(devPpk) +
                        "); using the driver's value");
            }
            ppk = static_cast<int>(devPpk);
            loadWatts = devWatts;
        } else {
            logWarn("Could not read the driver settings; using values from the config");
        }
        source = std::move(dev);
        logInfo("energy-agent starting (pulses from " + path + ")");
    } else if (sourceType == "simulated") {
        source = std::make_unique<SimulatedPulseSource>(loadWatts, ppk, g_running);
        logInfo("energy-agent starting (simulated load " + std::to_string(loadWatts) + " W)");
    } else {
        logError("Unknown source '" + sourceType + "' (use 'simulated' or 'device')");
        return 1;
    }

    SharedState shared;
    ThreadSafeQueue<PulseEvent> eventQueue;
    ThreadSafeQueue<Reading> readingQueue;

    ServerSettings web;
    web.bindAddress      = config.getString("bind_address", "127.0.0.1");
    web.port             = config.getInt("port", 8080);
    web.webRoot          = config.getString("web_root", "web");
    web.tariffPerKwh     = config.getDouble("tariff_per_kwh", 8.0);
    web.alertWatts       = alertWatts;
    web.initialLoadWatts = loadWatts;

    PulseSource* src = source.get();
    HttpServer server(web, storage, shared, [src](double w) { return src->setLoad(w); });
    if (!server.start()) return 1;     // start before the threads: easy to exit if it fails

    // Thread 1: get pulses from the source
    std::thread reader([&] {
        PulseEvent ev{};
        while (source->next(ev)) eventQueue.push(ev);
        logInfo("reader thread stopped");
        g_running = false;   // if the source ended on its own, stop the whole agent
    });

    // Thread 2: turn pulses into readings and publish the newest one
    std::thread analytics([&] {
        AnalyticsEngine engine(ppk, windowSec, alertWatts);
        while (auto ev = eventQueue.waitPop()) {
            Reading r = engine.update(*ev);
            shared.set(LiveState{true, r.timestamp_ns, r.watts, r.kwhTotal,
                                 engine.peakWatts(), r.alert});
            readingQueue.push(r);
        }
        logInfo("analytics thread stopped");
    });

    // Thread 3: save readings and alerts
    std::thread storer([&] {
        bool wasAlert = false;
        while (auto r = readingQueue.waitPop()) {
            storage.insertReading(r->timestamp_ns, r->watts, r->kwhTotal);

            // Record an alert only when it starts, not on every reading
            if (r->alert && !wasAlert) {
                std::ostringstream msg;
                msg << "High load: " << std::fixed << std::setprecision(0) << r->watts << " W";
                storage.insertAlert(r->timestamp_ns, msg.str());
                logWarn(msg.str());
            }
            wasAlert = r->alert;

            std::ostringstream line;
            line << std::fixed << std::setprecision(1) << "Power: " << r->watts
                 << " W | Total: " << std::setprecision(4) << r->kwhTotal << " kWh";
            logInfo(line.str());
        }
        logInfo("storage thread stopped");
    });

    // Main thread just waits for Ctrl+C
    while (g_running) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logInfo("shutdown requested");

    // Stop in order: no new web requests, then the pipeline from front to back
    server.stop();
    reader.join();
    eventQueue.stop();
    analytics.join();
    readingQueue.stop();
    storer.join();

    logInfo("energy-agent finished");
    return 0;
}
