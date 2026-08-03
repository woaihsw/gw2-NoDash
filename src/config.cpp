#include "config.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <system_error>

namespace nodash {
namespace {

std::string Trim(std::string value) {
    const auto notSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace

void WriteDefaultConfigIfMissing(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) return;

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot create " + path.string());
    }
    output << "[NoDash]\nmode=-1\n";
    if (!output) {
        throw std::runtime_error("cannot write " + path.string());
    }
}

ConfigResult LoadConfig(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return {{}, "cannot open " + path.string()};

    Config config;
    std::string line;
    std::size_t lineNumber = 0;
    bool foundSection = false;
    bool foundMode = false;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line[0] == '[') {
            if (line != "[NoDash]") {
                return {{}, "unexpected section on line " + std::to_string(lineNumber)};
            }
            if (foundSection) {
                return {{}, "duplicate [NoDash] section on line " +
                    std::to_string(lineNumber)};
            }
            foundSection = true;
            continue;
        }
        if (!foundSection) {
            return {{}, "key outside [NoDash] on line " + std::to_string(lineNumber)};
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            return {{}, "invalid line " + std::to_string(lineNumber)};
        }

        const auto key = Lower(Trim(line.substr(0, separator)));
        if (key != "mode") {
            return {{}, "unknown key on line " + std::to_string(lineNumber)};
        }
        if (foundMode) {
            return {{}, "duplicate mode on line " + std::to_string(lineNumber)};
        }
        foundMode = true;

        const auto value = Trim(line.substr(separator + 1));
        int parsed = 0;
        const auto result = std::from_chars(
            value.data(), value.data() + value.size(), parsed, 10);
        if (value.empty() || result.ec != std::errc{} ||
            result.ptr != value.data() + value.size() ||
            (parsed != static_cast<int>(FilterMode::Native) &&
             parsed != static_cast<int>(FilterMode::Disabled))) {
            return {{}, "invalid mode on line " + std::to_string(lineNumber)};
        }
        config.mode = static_cast<FilterMode>(parsed);
    }

    if (input.bad()) return {{}, "failed while reading " + path.string()};
    if (!foundSection) return {{}, "missing [NoDash] section"};
    if (!foundMode) return {{}, "missing mode"};
    return {config, {}};
}

} // namespace nodash
