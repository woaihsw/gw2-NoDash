#pragma once

#include "nexus_minimal.hpp"

#include <cstdint>

namespace nodash {

bool InstallHook(nexus::AddonAPI& api, std::uintptr_t targetAddress);

} // namespace nodash
