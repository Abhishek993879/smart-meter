#include "HttpServer.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "EnergyCalculator.hpp"
#include "Logger.hpp"
#include "SharedState.hpp"
#include "Storage.hpp"
#include "httplib.h"

using json = nlohmann::json;

struct HttpServer::Impl {
    httplib::Server server;
};

static void sendJson(httplib::Response& res, const json& body, int status = 200) {
    res.status = status;
    res.set_header("Cache-Control", "no-store");
    res.set_content(body.dump(), "application/json");
}

static int intParam(const httplib::Request& req, const char* name, int def, int lo, int hi) {
    if (!req.has_param(name)) return def;
    try {
        return std::clamp(std::stoi(req.get_param_value(name)), lo, hi);
    } catch (...) {
        return def;
    }
}

HttpServer::HttpServer(const ServerSettings& settings, Storage& storage,
                       SharedState& state, SetLoadFn setLoad)
    : impl_(std::make_unique<Impl>()),
      settings_(settings),
      storage_(storage),
      state_(state),
      setLoad_(std::move(setLoad)),
      loadWatts_(settings.initialLoadWatts) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start() {
    auto& svr = impl_->server;

    // Serves index.html, chart.umd.js, ... from the web folder
    if (!svr.set_mount_point("/", settings_.webRoot)) {
        logError("Web folder not found: " + settings_.webRoot +
                 " (run the agent from the agent/ directory)");
        return false;
    }

    svr.Get("/api/live", [this](const httplib::Request&, httplib::Response& res) {
        LiveState s = state_.get();
        sendJson(res, json{
            {"has_data", s.hasData},
            {"timestamp_ms", s.timestamp_ns / 1000000ULL},
            {"watts", s.watts},
            {"kwh_total", s.kwhTotal},
            {"cost", EnergyCalculator::cost(s.kwhTotal, settings_.tariffPerKwh)},
            {"peak_watts", s.peakWatts},
            {"alert", s.alert},
            {"load_watts", loadWatts_.load()},
            {"alert_watts", settings_.alertWatts}});
    });

    svr.Get("/api/history", [this](const httplib::Request& req, httplib::Response& res) {
        int limit = intParam(req, "limit", 300, 1, 5000);
        json arr = json::array();
        for (const auto& r : storage_.latestReadings(limit)) {
            arr.push_back(json{{"t", r.timestamp_ns / 1000000ULL},
                               {"watts", r.watts},
                               {"kwh", r.kwhTotal}});
        }
        sendJson(res, arr);
    });

    svr.Get("/api/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        int limit = intParam(req, "limit", 20, 1, 500);
        json arr = json::array();
        for (const auto& a : storage_.latestAlerts(limit)) {
            arr.push_back(json{{"t", a.timestamp_ns / 1000000ULL}, {"message", a.message}});
        }
        sendJson(res, arr);
    });

    svr.Post("/api/load", [this](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body, nullptr, false);   // false = don't throw
        if (body.is_discarded() || !body.is_object() || !body.contains("watts") ||
            !body["watts"].is_number()) {
            sendJson(res, json{{"ok", false},
                               {"error", "expected JSON like {\"watts\": 2500}"}}, 400);
            return;
        }
        double w = body["watts"].get<double>();
        if (w < 100.0 || w > 10000.0) {
            sendJson(res, json{{"ok", false},
                               {"error", "watts must be between 100 and 10000"}}, 400);
            return;
        }
        if (!setLoad_(w)) {
            sendJson(res, json{{"ok", false},
                               {"error", "the pulse source could not change its load"}}, 409);
            return;
        }
        loadWatts_ = w;
        logInfo("Load changed from the dashboard: " + std::to_string(static_cast<int>(w)) + " W");
        sendJson(res, json{{"ok", true}, {"watts", w}});
    });

    // Bind first so we know the port is free, then serve on a background thread
    if (!svr.bind_to_port(settings_.bindAddress, settings_.port)) {
        logError("Cannot listen on " + settings_.bindAddress + ":" +
                 std::to_string(settings_.port) + " (port already in use?)");
        return false;
    }
    thread_ = std::thread([this] { impl_->server.listen_after_bind(); });
    logInfo("Dashboard: http://localhost:" + std::to_string(settings_.port));
    return true;
}

void HttpServer::stop() {
    if (thread_.joinable()) {
        impl_->server.stop();
        thread_.join();
    }
}
