// Resolution guard for the retail 2006 Game.exe (timestamp 0x45141833).
//
// The display-mode manager (constructor 0x74d1b0) enumerates the primary adapter's
// X8R8G8B8 modes through the system d3d9.dll and keeps only modes that pass the size
// whitelist at 0x74ca00. During startup the HUD (0x74dbf0) calls 0x74cb80, which searches
// that list for the saved Resolution string (0xd402a4) with no end check. A saved size that
// is not in the list reads past the heap block and faults at 0x74cb9f/0x74cba4.
//
// Two hooks fix it:
//   * 0x74ca00 also accepts the saved size, so the primary monitor's real modes such as
//     2560x1440 appear in the list (and the in-game Options menu) like the retail sizes.
//   * 0x74cb80 returns the saved size when it is still not listed (for example a 3840x2160
//     window on a smaller primary monitor) instead of reading past the list.
// Both sites are checked byte for byte; any other executable is left untouched.
#include "display_modes.hpp"
#include "display_modes_rules.hpp"
#include "log.hpp"

#include <Windows.h>
#include <MinHook.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string_view>

namespace display_modes {
namespace {
    constexpr std::uintptr_t image_base = 0x400000;
    constexpr std::uintptr_t accept_site = 0x74ca00;
    constexpr std::uintptr_t lookup_site = 0x74cb80;
    constexpr std::uintptr_t saved_text_site = 0xd402a4;
    constexpr std::size_t saved_text_capacity = 32;
    constexpr std::size_t list_offset = 0x57c, count_offset = 0x580, text_offset = 0x594;

    constexpr std::uint8_t accept_bytes[] = {0x8B,0x4C,0x24,0x08,0x8B,0x54,0x24,0x04,0x33,0xC0,0x8D,0x9B,0x00,0x00,0x00,0x00,
                                             0x39,0x90,0xA8,0x3E,0x83,0x00};
    constexpr std::uint8_t lookup_bytes[] = {0x83,0xEC,0x14,0x53,0x55,0x56,0x57,0x8B,0xF9,0x33,0xDB,0x89,0x5C,0x24,0x10,
                                             0x8D,0xAF,0x94,0x05,0x00,0x00,0x8B,0x87,0x7C};

    using accept_fn = bool(__stdcall*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
    using lookup_fn = void(__fastcall*)(std::uint8_t*, void*, std::uint32_t*);
    accept_fn original_accept{};
    lookup_fn original_lookup{};

    bool matches(std::uintptr_t address, const std::uint8_t* bytes, std::size_t size) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT) return false;
        return std::memcmp(reinterpret_cast<const void*>(address), bytes, size) == 0;
    }

    std::string_view saved_text() {
        const auto* text = reinterpret_cast<const char*>(saved_text_site);
        return std::string_view(text, strnlen(text, saved_text_capacity));
    }

    bool __stdcall accept(std::uint32_t width, std::uint32_t height, std::uint32_t refresh, std::uint32_t format) {
        if (original_accept(width, height, refresh, format)) return true;
        const bool extra = display_modes_rules::accept(width, height, display_modes_rules::parse(saved_text()));
        if (extra) logger::write_once("display-list", "display: added %ux%u to the game's mode list", width, height);
        return extra;
    }

    void __fastcall lookup(std::uint8_t* self, void* edx, std::uint32_t* out) {
        const auto text = saved_text();
        const auto* modes = *reinterpret_cast<const display_modes_rules::entry* const*>(self + list_offset);
        const auto count = *reinterpret_cast<const std::uint32_t*>(self + count_offset);
        if (display_modes_rules::listed(modes, count, text)) { original_lookup(self, edx, out); return; }
        const auto mode = display_modes_rules::unlisted_mode(text);
        std::snprintf(reinterpret_cast<char*>(self + text_offset), 0x74, "%d" "x" "%d",
            static_cast<int>(mode.width), static_cast<int>(mode.height));
        out[0] = mode.width; out[1] = mode.height; out[2] = mode.refresh; out[3] = mode.format;
        logger::write_once("display-unlisted", "display: %ux%u is not a primary-monitor mode; using it without the list",
            mode.width, mode.height);
    }
}

void install() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) != image_base ||
            !matches(accept_site, accept_bytes, sizeof(accept_bytes)) ||
            !matches(lookup_site, lookup_bytes, sizeof(lookup_bytes))) {
            logger::write("display: resolution guard not installed (unrecognized Game.exe)");
            return;
        }
        const auto initialized = MH_Initialize();
        if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) {
            logger::write("ERROR: resolution guard initialization failed (%d)", initialized);
            return;
        }
        auto* accept_target = reinterpret_cast<void*>(accept_site);
        auto* lookup_target = reinterpret_cast<void*>(lookup_site);
        if (MH_CreateHook(accept_target, reinterpret_cast<void*>(&accept), reinterpret_cast<void**>(&original_accept)) != MH_OK) {
            logger::write("ERROR: resolution guard could not hook the mode filter");
            return;
        }
        if (MH_CreateHook(lookup_target, reinterpret_cast<void*>(&lookup), reinterpret_cast<void**>(&original_lookup)) != MH_OK) {
            MH_RemoveHook(accept_target);
            logger::write("ERROR: resolution guard could not hook the mode lookup");
            return;
        }
        if (MH_EnableHook(accept_target) != MH_OK || MH_EnableHook(lookup_target) != MH_OK) {
            MH_DisableHook(accept_target); MH_DisableHook(lookup_target);
            MH_RemoveHook(accept_target); MH_RemoveHook(lookup_target);
            logger::write("ERROR: resolution guard hooks could not be enabled");
            return;
        }
        logger::write("display: resolution guard installed (saved Resolution=%.*s)",
            static_cast<int>(saved_text().size()), saved_text().data());
    });
}
}
