#pragma once

#include <cstdint>
#include <string>

namespace nodash {

struct ResolverResult {
    std::uintptr_t targetAddress = 0;
    std::string detail;

    explicit operator bool() const noexcept { return targetAddress != 0; }
};

ResolverResult ResolveTargetFunction();

} // namespace nodash
