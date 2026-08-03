#include "hooks.hpp"

#include "logging.hpp"

namespace nodash {
namespace {

// Last reviewed CN-client contract (binary build identifier was not recorded):
// RCX=textBuffer, EDX=level; no R8/R9 or stack arguments are consumed, callers
// ignore RAX, and level 0 takes the native early-return path.
using FilterChat = void(__fastcall*)(void*, int);

FilterChat gOriginalFilterChat = nullptr;

void __fastcall FilterChatDetour(void* textBuffer, int) {
    if (gOriginalFilterChat) {
        gOriginalFilterChat(textBuffer, 0);
    }
}

} // namespace

bool InstallHook(nexus::AddonAPI& api, std::uintptr_t targetAddress) {
    if (!api.MinHook.Create || !api.MinHook.Enable || !api.MinHook.Remove) {
        return false;
    }

    const auto target = reinterpret_cast<LPVOID>(targetAddress);
    FilterChat original = nullptr;
    const auto created = api.MinHook.Create(
        target,
        reinterpret_cast<LPVOID>(&FilterChatDetour),
        reinterpret_cast<LPVOID*>(&original));
    if (created != nexus::MinHookStatus::Ok) {
        return false;
    }
    if (!original) {
        const auto removed = api.MinHook.Remove(target);
        if (removed != nexus::MinHookStatus::Ok &&
            removed != nexus::MinHookStatus::ErrorNotCreated) {
            Error(
                "hook creation returned no trampoline, and the hook "
                "record could not be removed");
        } else {
            Error("hook creation returned no trampoline");
        }
        return false;
    }

    gOriginalFilterChat = original;
    if (api.MinHook.Enable(target) == nexus::MinHookStatus::Ok) {
        return true;
    }

    const auto removed = api.MinHook.Remove(target);
    if (removed != nexus::MinHookStatus::Ok &&
        removed != nexus::MinHookStatus::ErrorNotCreated) {
        Error("hook creation succeeded, enabling failed, and the hook record could not be removed");
        return false;
    }

    gOriginalFilterChat = nullptr;
    return false;
}

} // namespace nodash
