#pragma once
#include <Windows.h>
namespace window_focus_rules {
    constexpr bool suppress_minimize(bool windowed, bool user_request, int command) {
        return windowed && !user_request && (command==SW_MINIMIZE || command==SW_SHOWMINIMIZED ||
            command==SW_SHOWMINNOACTIVE || command==SW_FORCEMINIMIZE);
    }
    constexpr bool suppress_deactivation(bool windowed, bool run_in_background, UINT message, WPARAM value) {
        return windowed && run_in_background &&
            ((message==WM_ACTIVATEAPP && value==FALSE) ||
             (message==WM_ACTIVATE && LOWORD(value)==WA_INACTIVE) || message==WM_KILLFOCUS);
    }
}
