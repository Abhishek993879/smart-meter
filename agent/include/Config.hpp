#pragma once
#include <string>
#include <unordered_map>

// Reads a simple key=value file. Lines starting with # are comments.
class Config {
public:
    bool load(const std::string& path);

    std::string getString(const std::string& key, const std::string& def) const;
    int         getInt(const std::string& key, int def) const;
    double      getDouble(const std::string& key, double def) const;

private:
    std::unordered_map<std::string, std::string> values_;
};
