#pragma once

#include <filesystem>
#include <string>

namespace nodash {

enum class FilterMode : int {
    Native = -1,
    Disabled = 0,
};

struct Config {
    FilterMode mode = FilterMode::Native;
};

struct ConfigResult {
    Config config;
    std::string error;

    explicit operator bool() const noexcept { return error.empty(); }
};

void WriteDefaultConfigIfMissing(const std::filesystem::path& path);
ConfigResult LoadConfig(const std::filesystem::path& path);

} // namespace nodash
