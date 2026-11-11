// Makes MUA 2006 per-monitor DPI aware (UL: window ~1.5x too big under display scaling).
//
// Game.exe has no DPI manifest, so Windows runs it DPI-unaware and scales every window coordinate by
// the monitor's scale factor. dgVoodoo2 sizes the game window (Borderless "fullscreensize", the window
// for the saved Resolution, CenterAppWindow) from the output's real pixel size, but the unaware process
// applies it in scaled units: on a 3840x2160 monitor at 150% the window became 3840 *logical* pixels
// wide = 5760 physical and ran onto the next monitor; a Windowed 2560x1440 came out 3840x2160 plus frame.
// Per-monitor-v2 awareness makes window, monitor and swap-chain sizes the same physical pixels.
//
// This runs at process attach: dinput8.dll is a static import of libIGDisplay.dll, so it is loaded
// before the game's WinMain and before any window exists (awareness cannot change afterwards). Wine and
// Proton are left alone, and [Display] DpiAware=0 in muac-fix.ini restores the old behavior.
#include "dpi_awareness.hpp"
#include "ini_rules.hpp"
#include "log.hpp"
#include <Windows.h>

namespace dpi_awareness {
namespace {
    enum class outcome { not_game, wine, disabled, set, already_set, unavailable, failed };
    outcome result = outcome::not_game;
    DWORD error = 0;
}
void apply() {
    // Only inside the game: the engine DLL that imports this proxy is already mapped.
    if (!GetModuleHandleW(L"libIGDisplay.dll")) { result = outcome::not_game; return; }
    if (GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version")) { result = outcome::wine; return; }
    if (!ini_rules::read_flag(logger::module_dir() / L"muac-fix.ini", L"Display", L"DpiAware", true)) {
        result = outcome::disabled; return;
    }
    using set_context_t = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    const auto user32 = GetModuleHandleW(L"user32.dll");
    const auto set_context = user32 ? reinterpret_cast<set_context_t>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")) : nullptr;
    if (set_context) {
        if (set_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) { result = outcome::set; return; }
        error = GetLastError();
        result = error == ERROR_ACCESS_DENIED ? outcome::already_set : outcome::failed;
        return;
    }
    // Windows 8.1 / early 10: system awareness still matches the primary monitor's pixels.
    result = SetProcessDPIAware() ? outcome::set : outcome::unavailable;
}
void report() {
    switch (result) {
    case outcome::not_game: break;
    case outcome::wine: logger::write("display: DPI awareness left to Wine"); break;
    case outcome::disabled: logger::write("display: DPI awareness off (DpiAware=0); Windows scales the window"); break;
    case outcome::set: logger::write("display: per-monitor DPI aware (window sizes are physical pixels)"); break;
    case outcome::already_set: logger::write("display: DPI awareness already set for this process"); break;
    case outcome::unavailable: logger::write("display: DPI awareness unavailable on this Windows"); break;
    case outcome::failed: logger::write("ERROR: could not make the game DPI aware (%lu)", error); break;
    }
}
}
