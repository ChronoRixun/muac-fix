#pragma once
#include <Windows.h>
#include <string_view>
namespace window_focus_rules {
    // dgVoodoo.conf [GeneralExt] WindowedAttributes, e.g. "borderless,fullscreensize" (any case/order).
    inline bool borderless_attribute(std::string_view attributes) {
        std::size_t start=0;
        while(start<=attributes.size()) {
            auto end=attributes.find(',',start); if(end==std::string_view::npos) end=attributes.size();
            auto item=attributes.substr(start,end-start);
            while(!item.empty() && (item.front()==' '||item.front()=='\t')) item.remove_prefix(1);
            while(!item.empty() && (item.back()==' '||item.back()=='\t')) item.remove_suffix(1);
            if(item.size()==10) {
                bool same=true;
                for(std::size_t i=0;i<10;++i){char c=item[i];if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');if(c!="borderless"[i]){same=false;break;}}
                if(same) return true;
            }
            start=end+1;
        }
        return false;
    }
    constexpr DWORD borderless_style(DWORD style) {
        return style & ~static_cast<DWORD>(WS_CAPTION|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
    }
    // The engine's window also has a 2-pixel client edge.
    constexpr DWORD borderless_ex_style(DWORD style) {
        return style & ~static_cast<DWORD>(WS_EX_CLIENTEDGE|WS_EX_WINDOWEDGE|WS_EX_STATICEDGE|WS_EX_DLGMODALFRAME);
    }
    // Windowed at (or above) the monitor's own size: the frame makes the window bigger than the primary
    // monitor and dgVoodoo2 then places it at the monitor's corner, so its right edge reached the next
    // monitor. Centre it horizontally on the primary and keep the title bar on screen; a window that fits
    // keeps its position (dgVoodoo2 centres it, and the player may move it).
    constexpr POINT oversize_position(RECT monitor, int x, int y, int width, int height) {
        const int monitor_width=monitor.right-monitor.left, monitor_height=monitor.bottom-monitor.top;
        if(width>monitor_width) x=monitor.left-(width-monitor_width)/2;
        if(height>monitor_height) y=monitor.top;
        return POINT{x,y};
    }
    // The game and dgVoodoo2 render on adapter 0, the primary monitor (physical pixels once DPI aware).
    inline RECT primary_monitor() {
        MONITORINFO info{};info.cbSize=sizeof(info);
        if(GetMonitorInfoW(MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY),&info)) return info.rcMonitor;
        return RECT{0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
    }
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
