#include "config.hpp"
#include "hooks.hpp"
#include "logging.hpp"
#include "nexus_minimal.hpp"
#include "resolver.hpp"

#include <exception>
#include <filesystem>
#include <string>
#include <windows.h>

namespace nodash {
namespace {

std::filesystem::path AddonDirectory(nexus::AddonAPI& api) {
    if (!api.Paths.GetAddonDirectory) return {};
    const char* directory = api.Paths.GetAddonDirectory("gw2-NoDash");
    if (!directory || directory[0] == '\0') return {};

    std::filesystem::path path(directory);
    if (!path.is_absolute()) return {};
    return path;
}

void LoadImpl(nexus::AddonAPI* api) {
    gApi = api;
    if (!api) return;

    const auto addonDirectory = AddonDirectory(*api);
    if (addonDirectory.empty()) {
        Warn("Nexus returned an invalid addon directory");
        return;
    }

    const auto configPath = addonDirectory / "gw2-NoDash.ini";
    WriteDefaultConfigIfMissing(configPath);
    const auto loaded = LoadConfig(configPath);
    if (!loaded) {
        Warn("failed to load config: " + loaded.error);
        return;
    }
    if (loaded.config.mode == FilterMode::Native) {
        Info("loaded; mode=-1 keeps native chat filtering and installs no hook");
        return;
    }

    const auto resolved = ResolveTargetFunction();
    if (!resolved) {
        Warn("chat filter target was not resolved: " + resolved.detail);
        return;
    }
    Info(resolved.detail);

    if (!InstallHook(*api, resolved.targetAddress)) {
        Warn("failed to install chat filter hook");
        return;
    }
    Info("chat filter hook installed; native filter level forced to 0");
}

void Load(nexus::AddonAPI* api) noexcept {
    try {
        LoadImpl(api);
    } catch (const std::exception& exception) {
        Error("load failed: " + std::string(exception.what()));
    } catch (...) {
        Error("load failed: unknown exception");
    }
}

} // namespace
} // namespace nodash

extern "C" __declspec(dllexport) nexus::AddonDefinition* GetAddonDef() {
    static nexus::AddonDefinition definition{
        -0x90DA5,
        nexus::kApiVersion,
        "gw2-NoDash",
        {1, 0, 1, 0},
        "woaihsw",
        "Disables local GW2 CN chat filtering through the native level-0 path.",
        &nodash::Load,
        nullptr,
        nexus::AddonFlags::DisableHotloading,
        nexus::UpdateProvider::GitHub,
        "https://github.com/woaihsw/gw2-NoDash"
    };
    return &definition;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
