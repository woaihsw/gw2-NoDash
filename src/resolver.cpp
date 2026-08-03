#include "resolver.hpp"

#include <array>
#include <cstring>
#include <span>
#include <sstream>
#include <vector>
#include <windows.h>
#include <psapi.h>

namespace nodash {
namespace {

constexpr std::array<std::uint8_t, 19> kCoreLoop{
    0x66, 0x83, 0x39, 0x20, 0x74, 0x05, 0x66, 0xC7, 0x02, 0x2D,
    0x00, 0x48, 0x83, 0xC1, 0x02, 0x48, 0x83, 0xC2, 0x02,
};
constexpr char kAssertion[] = "filterLevelIndex < marrsize(FilterRec, level)";

struct SectionRange {
    std::uintptr_t address = 0;
    std::size_t size = 0;
};

std::vector<std::uintptr_t> FindAllMatches(
    const SectionRange& range,
    std::span<const std::uint8_t> pattern) {
    std::vector<std::uintptr_t> matches;
    if (pattern.empty() || pattern.size() > range.size) return matches;

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(range.address);
    for (std::size_t offset = 0; offset <= range.size - pattern.size(); ++offset) {
        if (std::memcmp(bytes + offset, pattern.data(), pattern.size()) == 0) {
            matches.push_back(range.address + offset);
        }
    }
    return matches;
}

bool VerifyStringXref(
    std::uintptr_t functionStart,
    std::uintptr_t functionEnd,
    std::uintptr_t stringAddress) {
    const auto* code = reinterpret_cast<const std::uint8_t*>(functionStart);
    const auto size = functionEnd - functionStart;

    // The reviewed function references the assertion with REX.W LEA reg,[RIP+disp32].
    for (std::size_t offset = 0; offset + 7 <= size; ++offset) {
        if ((code[offset] & 0xF8) != 0x48 || code[offset + 1] != 0x8D ||
            (code[offset + 2] & 0xC7) != 0x05) {
            continue;
        }

        std::int32_t displacement = 0;
        std::memcpy(&displacement, code + offset + 3, sizeof(displacement));
        const auto instructionEnd = static_cast<std::intptr_t>(functionStart + offset + 7);
        const auto resolved = static_cast<std::uintptr_t>(
            instructionEnd + static_cast<std::intptr_t>(displacement));
        if (resolved == stringAddress) return true;
    }
    return false;
}

const RUNTIME_FUNCTION* FindRuntimeFunction(
    std::uintptr_t moduleBase,
    std::size_t imageSize,
    const IMAGE_NT_HEADERS* ntHeaders,
    std::uint32_t rva) {
    const auto& directory =
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (directory.VirtualAddress == 0 || directory.Size == 0 ||
        directory.VirtualAddress > imageSize ||
        directory.Size > imageSize - directory.VirtualAddress ||
        directory.Size % sizeof(RUNTIME_FUNCTION) != 0) {
        return nullptr;
    }

    const auto* functions = reinterpret_cast<const RUNTIME_FUNCTION*>(
        moduleBase + directory.VirtualAddress);
    const auto count = directory.Size / sizeof(RUNTIME_FUNCTION);
    for (std::size_t index = 0; index < count; ++index) {
        const auto& function = functions[index];
        if (function.BeginAddress >= function.EndAddress ||
            function.EndAddress > imageSize) {
            continue;
        }
        if (rva >= function.BeginAddress && rva < function.EndAddress) {
            return &function;
        }
    }
    return nullptr;
}

bool IsInsideExecutableSection(
    std::uintptr_t begin,
    std::uintptr_t end,
    const std::vector<SectionRange>& sections) {
    for (const auto& section : sections) {
        if (begin >= section.address && end <= section.address + section.size) {
            return true;
        }
    }
    return false;
}

} // namespace

ResolverResult ResolveTargetFunction() {
    ResolverResult result;

    auto module = GetModuleHandleA("Gw2-64.exe");
    if (!module) module = GetModuleHandleA(nullptr);
    if (!module) {
        result.detail = "could not obtain the main module";
        return result;
    }

    MODULEINFO moduleInfo{};
    if (!GetModuleInformation(
            GetCurrentProcess(), module, &moduleInfo, sizeof(moduleInfo))) {
        result.detail = "could not query the main module image";
        return result;
    }

    const auto moduleBase = reinterpret_cast<std::uintptr_t>(module);
    const auto imageSize = static_cast<std::size_t>(moduleInfo.SizeOfImage);
    if (imageSize < sizeof(IMAGE_DOS_HEADER)) {
        result.detail = "PE image is smaller than its DOS header";
        return result;
    }

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE || dosHeader->e_lfanew <= 0) {
        result.detail = "invalid PE DOS header";
        return result;
    }

    const auto ntOffset = static_cast<std::size_t>(dosHeader->e_lfanew);
    if (ntOffset > imageSize || sizeof(IMAGE_NT_HEADERS) > imageSize - ntOffset) {
        result.detail = "PE NT headers are outside the image";
        return result;
    }

    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        moduleBase + ntOffset);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
        ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        result.detail = "main module is not a valid 64-bit PE image";
        return result;
    }

    const auto sectionTable = reinterpret_cast<std::uintptr_t>(
        IMAGE_FIRST_SECTION(ntHeaders));
    const auto sectionBytes = static_cast<std::size_t>(
        ntHeaders->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
    if (sectionTable < moduleBase || sectionTable > moduleBase + imageSize ||
        sectionBytes > moduleBase + imageSize - sectionTable) {
        result.detail = "PE section table is outside the image";
        return result;
    }

    std::vector<SectionRange> executableSections;
    std::vector<SectionRange> readOnlySections;
    const auto* section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD index = 0; index < ntHeaders->FileHeader.NumberOfSections;
         ++index, ++section) {
        const auto rva = static_cast<std::size_t>(section->VirtualAddress);
        const auto size = static_cast<std::size_t>(section->Misc.VirtualSize);
        if (rva > imageSize || size > imageSize - rva) {
            result.detail = "PE section is outside the image";
            return result;
        }

        const SectionRange range{moduleBase + rva, size};
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) {
            executableSections.push_back(range);
        }
        if ((section->Characteristics & IMAGE_SCN_MEM_READ) != 0 &&
            (section->Characteristics & IMAGE_SCN_MEM_WRITE) == 0 &&
            (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) {
            readOnlySections.push_back(range);
        }
    }

    if (executableSections.empty() || readOnlySections.empty()) {
        result.detail = "required executable or read-only PE sections are missing";
        return result;
    }

    std::vector<std::uintptr_t> coreMatches;
    for (const auto& sectionRange : executableSections) {
        auto matches = FindAllMatches(sectionRange, kCoreLoop);
        coreMatches.insert(coreMatches.end(), matches.begin(), matches.end());
    }
    if (coreMatches.size() != 1) {
        result.detail = coreMatches.empty()
            ? "core replacement-loop bytes were not found"
            : "core replacement-loop bytes are not unique";
        return result;
    }

    const auto coreLoopRva = static_cast<std::uint32_t>(
        coreMatches.front() - moduleBase);
    const auto* runtimeFunction = FindRuntimeFunction(
        moduleBase, imageSize, ntHeaders, coreLoopRva);
    if (!runtimeFunction) {
        result.detail = "core replacement loop has no valid .pdata function boundary";
        return result;
    }

    const auto functionStart = moduleBase + runtimeFunction->BeginAddress;
    const auto functionEnd = moduleBase + runtimeFunction->EndAddress;
    if (!IsInsideExecutableSection(functionStart, functionEnd, executableSections)) {
        result.detail = ".pdata function boundary is outside executable sections";
        return result;
    }

    const auto assertionBytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(kAssertion), sizeof(kAssertion));
    std::vector<std::uintptr_t> assertionMatches;
    for (const auto& sectionRange : readOnlySections) {
        auto matches = FindAllMatches(sectionRange, assertionBytes);
        assertionMatches.insert(assertionMatches.end(), matches.begin(), matches.end());
    }
    if (assertionMatches.size() != 1) {
        result.detail = assertionMatches.empty()
            ? "filter-level assertion string was not found"
            : "filter-level assertion string is not unique";
        return result;
    }

    if (!VerifyStringXref(functionStart, functionEnd, assertionMatches.front())) {
        result.detail = "candidate function does not reference the filter-level assertion";
        return result;
    }

    // Reviewed call sites enter at this BeginAddress; the matched loop is not a
    // detached cold-code fragment or funclet.
    const auto targetRva = runtimeFunction->BeginAddress;
    result.targetAddress = moduleBase + targetRva;
    const auto assertionRva = assertionMatches.front() - moduleBase;
    std::ostringstream message;
    message << "resolved chat filter target_rva=0x" << std::hex << std::uppercase
            << targetRva << " core_loop_rva=0x" << coreLoopRva
            << " assertion_rva=0x" << assertionRva
            << " function_end_rva=0x" << runtimeFunction->EndAddress;
    result.detail = message.str();
    return result;
}

} // namespace nodash
