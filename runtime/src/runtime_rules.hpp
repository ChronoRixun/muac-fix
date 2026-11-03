#pragma once
#include <Windows.h>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <optional>
#include <span>

namespace runtime_rules {
    // DirectInput enumeration order is not an XInput user-index identity.
    // Only a single connected XInput slot can be associated without guessing.
    inline std::optional<int> single_xinput_slot(std::span<const int> connected) {
        return connected.size()==1 ? std::optional<int>{connected.front()} : std::nullopt;
    }
    constexpr bool may_capture(bool capture, bool foreground) { return capture && foreground; }
    constexpr DWORD mouse_flags(bool mouse, bool capture, DWORD flags) {
        return mouse && !capture ? (flags & ~DISCL_EXCLUSIVE) | DISCL_NONEXCLUSIVE : flags;
    }
}
