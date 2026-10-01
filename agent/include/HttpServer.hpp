#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

class Storage;
class SharedState;

struct ServerSettings {
    std::string bindAddress = "127.0.0.1";
    int port = 8080;
    std::string webRoot = "web";
    double tariffPerKwh = 8.0;
    double alertWatts = 3000.0;
    double initialLoadWatts = 1500.0;
};

// Serves the dashboard files and a small JSON API on its own thread.
class HttpServer {
public:
    using SetLoadFn = std::function<void(double)>;

    HttpServer(const ServerSettings& settings, Storage& storage,
               SharedState& state, SetLoadFn setLoad);
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    bool start();
    void stop();

private:
    struct Impl;                       // hides httplib so other files compile fast
    std::unique_ptr<Impl> impl_;

    ServerSettings settings_;
    Storage& storage_;
    SharedState& state_;
    SetLoadFn setLoad_;
    std::atomic<double> loadWatts_;
    std::thread thread_;
};
