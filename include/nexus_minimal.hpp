#pragma once

#include <cstddef>
#include <cstdint>
#include <windows.h>

static_assert(sizeof(void*) == 8, "gw2-NoDash requires a 64-bit build");

namespace nexus {

constexpr int kApiVersion = 6;

// Reviewed against RaidcoreGG/Nexus f99003c207c2aac8702e59db3c3e00274537a52c:
// Engine/Loader/AddonDefinition.h and Engine/Loader/API/AddonAPI.h.
// Recheck these layouts before changing kApiVersion.

enum class LogLevel : std::uint32_t {
    Critical = 1,
    Warning = 2,
    Info = 3,
};

enum class UpdateProvider {
    GitHub = 2,
};

enum class AddonFlags {
    DisableHotloading = 1 << 1,
};

struct AddonVersion {
    signed short Major;
    signed short Minor;
    signed short Build;
    signed short Revision;
};

struct AddonAPI;
using AddonLoad = void (*)(AddonAPI*);
using AddonUnload = void (*)();

struct AddonDefinition {
    signed int Signature;
    signed int APIVersion;
    const char* Name;
    AddonVersion Version;
    const char* Author;
    const char* Description;
    AddonLoad Load;
    AddonUnload Unload;
    AddonFlags Flags;
    UpdateProvider Provider;
    const char* UpdateLink;
};

static_assert(sizeof(AddonVersion) == 0x08);
static_assert(sizeof(AddonDefinition) == 0x48);
static_assert(offsetof(AddonDefinition, Name) == 0x08);
static_assert(offsetof(AddonDefinition, Version) == 0x10);
static_assert(offsetof(AddonDefinition, Load) == 0x28);
static_assert(offsetof(AddonDefinition, Unload) == 0x30);
static_assert(offsetof(AddonDefinition, Flags) == 0x38);
static_assert(offsetof(AddonDefinition, Provider) == 0x3C);
static_assert(offsetof(AddonDefinition, UpdateLink) == 0x40);

enum class MinHookStatus : int {
    Unknown = -1,
    Ok = 0,
    ErrorAlreadyInitialized = 1,
    ErrorNotInitialized = 2,
    ErrorAlreadyCreated = 3,
    ErrorNotCreated = 4,
    ErrorEnabled = 5,
    ErrorDisabled = 6,
    ErrorNotExecutable = 7,
    ErrorUnsupportedFunction = 8,
    ErrorMemoryAlloc = 9,
    ErrorMemoryProtect = 10,
    ErrorModuleNotFound = 11,
    ErrorFunctionNotFound = 12,
};

using LoggerLog = void (*)(LogLevel, const char*, const char*);
using GetAddonDirectoryFn = const char* (*)(const char*);
using MinHookCreate = MinHookStatus(__stdcall*)(LPVOID, LPVOID, LPVOID*);
using MinHookRemove = MinHookStatus(__stdcall*)(LPVOID);
using MinHookEnable = MinHookStatus(__stdcall*)(LPVOID);
using MinHookDisable = MinHookStatus(__stdcall*)(LPVOID);

// Nexus API v6 prefix through MinHook. Disable is unused but occupies an ABI slot.
struct AddonAPI {
    void* SwapChain;
    void* ImguiContext;
    void* ImguiMalloc;
    void* ImguiFree;
    struct { void* Register; void* Deregister; } Renderer;
    void* RequestUpdate;
    LoggerLog Log;
    struct { void* SendAlert; void* RegisterCloseOnEscape; void* DeregisterCloseOnEscape; } UI;
    struct {
        void* GetGameDirectory;
        GetAddonDirectoryFn GetAddonDirectory;
        void* GetCommonDirectory;
    } Paths;
    struct {
        MinHookCreate Create;
        MinHookRemove Remove;
        MinHookEnable Enable;
        MinHookDisable Disable;
    } MinHook;
};

static_assert(offsetof(AddonAPI, Renderer) == 0x20);
static_assert(offsetof(AddonAPI, RequestUpdate) == 0x30);
static_assert(offsetof(AddonAPI, Log) == 0x38);
static_assert(offsetof(AddonAPI, UI) == 0x40);
static_assert(offsetof(AddonAPI, Paths) == 0x58);
static_assert(offsetof(AddonAPI, MinHook) == 0x70);
static_assert(sizeof(AddonAPI) == 0x90);

} // namespace nexus
