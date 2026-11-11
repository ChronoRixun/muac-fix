#include "window_focus.hpp"
#include "window_focus_rules.hpp"
#include "ini_rules.hpp"
#include "log.hpp"
#include <Windows.h>
#include <MinHook.h>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace window_focus {
namespace {
    bool windowed=false, background=true, borderless=false;
    decltype(&::ShowWindow) original_show{};
    struct window_state {WNDPROC previous{}; bool user_minimize{};};
    std::mutex windows_mutex;
    std::unordered_map<HWND,window_state> windows;
    LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM value,LPARAM detail) {
        window_state state;
        const bool minimize=message==WM_SYSCOMMAND && (value&0xfff0)==SC_MINIMIZE;
        {
            std::lock_guard lock(windows_mutex);
            const auto it=windows.find(window);
            if(it!=windows.end()) {
                if(minimize) it->second.user_minimize=true;
                state=it->second;
            }
        }
        if(!state.previous) return DefWindowProcA(window,message,value,detail);
        if(borderless) {
            // dgVoodoo2's "borderless" attribute does not survive the engine's own SetWindowLongA /
            // MoveWindow calls, so the title bar stayed and the window overhung the monitor. Keep the
            // style borderless and the window exactly on the primary monitor.
            if(message==WM_STYLECHANGING && detail) {
                auto* change=reinterpret_cast<STYLESTRUCT*>(detail);
                if(value==static_cast<WPARAM>(GWL_STYLE)) change->styleNew=window_focus_rules::borderless_style(change->styleNew);
                if(value==static_cast<WPARAM>(GWL_EXSTYLE)) change->styleNew=window_focus_rules::borderless_ex_style(change->styleNew);
            }
            else if(message==WM_WINDOWPOSCHANGING && detail && !IsIconic(window) &&
                    (reinterpret_cast<WINDOWPOS*>(detail)->flags&(SWP_NOMOVE|SWP_NOSIZE))!=(SWP_NOMOVE|SWP_NOSIZE)) {
                auto* position=reinterpret_cast<WINDOWPOS*>(detail);
                const auto area=window_focus_rules::primary_monitor();
                position->x=area.left;position->y=area.top;position->cx=area.right-area.left;position->cy=area.bottom-area.top;
                position->flags&=~static_cast<UINT>(SWP_NOMOVE|SWP_NOSIZE);
            }
        }
        if(!state.user_minimize && !IsIconic(window) &&
           window_focus_rules::suppress_deactivation(windowed,background,message,value)) {
            logger::write_once("background-focus", "display: keeping the window active in the background without reclaiming focus");
            return DefWindowProcA(window,message,value,detail);
        }
        const auto result=CallWindowProcA(state.previous,window,message,value,detail);
        if(minimize || message==WM_NCDESTROY) {
            std::lock_guard lock(windows_mutex);
            if(message==WM_NCDESTROY) windows.erase(window);
            else if(const auto it=windows.find(window);it!=windows.end()) it->second.user_minimize=false;
        }
        return result;
    }
    bool game_window(HWND window) {
        DWORD pid{};GetWindowThreadProcessId(window,&pid);
        if(pid!=GetCurrentProcessId()) return false;
        char name[96]{};
        return GetClassNameA(window,name,sizeof(name)) && std::strcmp(name,"igWin32WindowClass")==0;
    }
    void attach(HWND window) {
        if(!game_window(window) || GetWindowThreadProcessId(window,nullptr)!=GetCurrentThreadId()) return;
        const auto previous=reinterpret_cast<WNDPROC>(GetWindowLongPtrA(window,GWLP_WNDPROC));
        if(!previous || previous==&procedure) return;
        {
            std::lock_guard lock(windows_mutex);
            if(windows.contains(window)) return;
            windows.emplace(window,window_state{previous,false});
        }
        SetLastError(0);
        if(!SetWindowLongPtrA(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&procedure)) && GetLastError()) {
            std::lock_guard lock(windows_mutex);
            windows.erase(window);logger::write("ERROR: could not attach window focus policy");return;
        }
        logger::write("display: window focus policy installed (RunInBackground=%d)",background);
        if(borderless) {
            const auto style=static_cast<DWORD>(GetWindowLongA(window,GWL_STYLE));
            SetWindowLongA(window,GWL_STYLE,static_cast<LONG>(window_focus_rules::borderless_style(style)));
            const auto ex_style=static_cast<DWORD>(GetWindowLongA(window,GWL_EXSTYLE));
            SetWindowLongA(window,GWL_EXSTYLE,static_cast<LONG>(window_focus_rules::borderless_ex_style(ex_style)));
            const auto area=window_focus_rules::primary_monitor();
            SetWindowPos(window,nullptr,area.left,area.top,area.right-area.left,area.bottom-area.top,
                         SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
            RECT placed{};GetWindowRect(window,&placed);
            logger::write("display: borderless window on the primary monitor (%ld,%ld %ldx%ld)",
                          placed.left,placed.top,placed.right-placed.left,placed.bottom-placed.top);
        }
    }
    BOOL WINAPI show(HWND window,int command) {
        if(windowed && game_window(window)) {
            attach(window);
            bool manual=false;
            {std::lock_guard lock(windows_mutex);const auto it=windows.find(window);manual=it!=windows.end()&&it->second.user_minimize;}
            if(window_focus_rules::suppress_minimize(true,manual,command)) {
                logger::write_once("automatic-minimize", "display: automatic minimization suppressed in windowed mode");
                return IsWindowVisible(window);
            }
        }
        return original_show(window,command);
    }
    BOOL CALLBACK attach_existing(HWND window,LPARAM) {attach(window);return TRUE;}
}
void install() {
    const auto folder=logger::module_dir();
    windowed=!ini_rules::read_flag(folder/L"dgVoodoo.conf",L"General",L"FullScreenMode",true) &&
             !ini_rules::read_flag(folder/L"dgVoodoo.conf",L"DirectX",L"AppControlledScreenMode",true);
    background=ini_rules::read_flag(folder/L"muac-fix.ini",L"Display",L"RunInBackground",true);
    borderless=windowed && window_focus_rules::borderless_attribute(
        ini_rules::read(folder/L"dgVoodoo.conf",L"GeneralExt",L"WindowedAttributes").value_or(""));
    if(!windowed) return;
    const auto initialized=MH_Initialize();
    if(initialized!=MH_OK && initialized!=MH_ERROR_ALREADY_INITIALIZED) return;
    auto* target=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"ShowWindow"));
    if(!target || MH_CreateHook(target,reinterpret_cast<void*>(&show),reinterpret_cast<void**>(&original_show))!=MH_OK ||
       MH_EnableHook(target)!=MH_OK) {logger::write("ERROR: window focus hook unavailable");return;}
    EnumThreadWindows(GetCurrentThreadId(),attach_existing,0);
    logger::write("display: windowed focus policy armed%s",borderless?" (borderless)":"");
}
}
