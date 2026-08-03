#pragma once

#include "nexus_minimal.hpp"

#include <string>

namespace nodash {

inline nexus::AddonAPI* gApi = nullptr;
constexpr const char* kLogChannel = "gw2-NoDash";

inline void Log(nexus::LogLevel level, const std::string& message) {
    if (gApi && gApi->Log) gApi->Log(level, kLogChannel, message.c_str());
}

inline void Info(const std::string& message) { Log(nexus::LogLevel::Info, message); }
inline void Warn(const std::string& message) { Log(nexus::LogLevel::Warning, message); }
inline void Error(const std::string& message) { Log(nexus::LogLevel::Critical, message); }

} // namespace nodash
