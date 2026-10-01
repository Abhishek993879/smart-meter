#include "Config.hpp"

#include <fstream>

static std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    std::size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

bool Config::load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;   // ignore malformed lines

        values_[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return true;
}

std::string Config::getString(const std::string& key, const std::string& def) const {
    auto it = values_.find(key);
    return it == values_.end() ? def : it->second;
}

int Config::getInt(const std::string& key, int def) const {
    auto it = values_.find(key);
    if (it == values_.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}

double Config::getDouble(const std::string& key, double def) const {
    auto it = values_.find(key);
    if (it == values_.end()) return def;
    try { return std::stod(it->second); } catch (...) { return def; }
}
