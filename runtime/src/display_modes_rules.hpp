#pragma once
// Pure rules for the retail display-mode list (see display_modes.cpp for the patch sites).
//
// The retail game builds its mode list from the primary adapter's modes, keeps only a
// hard-coded set of 14 sizes (640x480 ... 1920x1200, 2048x1536), and later looks the saved
// "Settings\Display\Resolution" string up in that list with no end check. Any other saved
// size (2560x1440, 3840x2160, even 1600x900) walks off the list and crashes on startup.
#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

namespace display_modes_rules {
    struct size { std::uint32_t width{}, height{}; };

    // The retail whitelist (Game.exe .data 0x833ea8 widths / 0x833ee0 heights).
    inline constexpr std::array<size, 14> retail_whitelist{{
        {640,480},{720,480},{800,600},{1024,768},{1280,720},{1280,768},{1280,960},
        {1280,1024},{1440,900},{1600,1200},{1680,1050},{1920,1080},{1920,1200},{2048,1536}}};

    // Accepts exactly what the game's own "%dx%d" writer/reader round-trips: digits, 'x',
    // digits, nothing else. Bounds match what the launcher can write.
    inline std::optional<size> parse(std::string_view text) {
        size out{};
        std::size_t i = 0;
        const auto number = [&](std::uint32_t& value) {
            const auto start = i;
            value = 0;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9' && i - start < 5) {
                value = value * 10 + static_cast<std::uint32_t>(text[i] - '0');
                ++i;
            }
            return i > start;
        };
        if (!number(out.width) || i >= text.size() || text[i] != 'x') return std::nullopt;
        ++i;
        if (!number(out.height) || i != text.size()) return std::nullopt;
        if (out.width < 640 || out.height < 480 || out.width > 16384 || out.height > 16384) return std::nullopt;
        return out;
    }

    constexpr bool whitelisted(std::uint32_t width, std::uint32_t height) {
        for (const auto& item : retail_whitelist)
            if (item.width == width && item.height == height) return true;
        return false;
    }

    // The mode-list filter: the retail sizes plus the size the player saved.
    constexpr bool accept(std::uint32_t width, std::uint32_t height, std::optional<size> saved) {
        return whitelisted(width, height) || (saved && saved->width == width && saved->height == height);
    }

    // One mode-list entry as the game stores it: width, height, refresh, format.
    struct entry { std::uint32_t width, height, refresh, format; };

    // True when the game's own unbounded lookup would find `saved_text` in the list
    // (it compares sprintf("%dx%d") of each entry with the saved string).
    inline bool listed(const entry* modes, std::uint32_t count, std::string_view saved_text) {
        if (!modes) return false;
        for (std::uint32_t i = 0; i < count; ++i) {
            char text[32]{};
            const int length = std::snprintf(text, sizeof(text), "%d" "x" "%d",
                static_cast<int>(modes[i].width), static_cast<int>(modes[i].height));
            if (length > 0 && saved_text == std::string_view(text, static_cast<std::size_t>(length))) return true;
        }
        return false;
    }

    // The mode the game renders with when the saved size is not in the list. The renderer
    // reads the same string with sscanf and falls back to 800x600, so mirror that.
    inline entry unlisted_mode(std::string_view saved_text) {
        const auto saved = parse(saved_text);
        return saved ? entry{saved->width, saved->height, 60, 22} : entry{800, 600, 60, 22};
    }
}
