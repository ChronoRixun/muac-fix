// Cursor focus behavior follows XML2 Fix's display.cpp; see licenses/.
#include "runtime.hpp"
#include "runtime_rules.hpp"
#include "log.hpp"
#include "openspy_redirect.hpp"
#include "window_focus.hpp"
#include "display_modes.hpp"
#include <Windows.h>
#include <mutex>
#include <array>
#include <MinHook.h>

namespace runtime {
namespace {
    bool controllers = true, capture = false;
    decltype(&::ClipCursor) original_clip{};
    decltype(&::SetCursorPos) original_warp{};
    decltype(&::SetCapture) original_capture{};
    decltype(&::GetCursorPos) original_position{};

    bool foreground() {
        DWORD pid{};
        GetWindowThreadProcessId(GetForegroundWindow(), &pid);
        return pid == GetCurrentProcessId();
    }
    BOOL WINAPI clip(const RECT* rect) {
        if (rect && !runtime_rules::may_capture(capture, foreground())) {
            logger::write_once("clip-suppressed", "mouse: process cursor confinement suppressed");
            return TRUE;
        }
        return original_clip(rect);
    }
    BOOL WINAPI warp(int x, int y) {
        if (!runtime_rules::may_capture(capture, foreground())) {
            logger::write_once("warp-suppressed", "mouse: process cursor recentering suppressed");
            return TRUE;
        }
        return original_warp(x,y);
    }
    HWND WINAPI capture_window(HWND window) {
        if (!runtime_rules::may_capture(capture, foreground())) return nullptr;
        return original_capture(window);
    }
    BOOL WINAPI position(POINT* point) {
        const BOOL result=original_position(point);
        if(result && point && !foreground()) {
            point->x=-32000; point->y=-32000;
        }
        return result;
    }
}
void install() {
    static std::once_flag once;
    std::call_once(once, [] {
        const auto ini=(logger::module_dir()/L"muac-fix.ini").wstring();
        controllers=ini_rules::read_flag(ini,L"Input",L"ControllerSupport",true);
        capture=ini_rules::read_flag(ini,L"Input",L"CaptureMouse",false);
        logger::write("MUA Classic Fix 0.1.2: controllers=%d capture_mouse=%d",controllers,capture);
        const auto user=GetModuleHandleW(L"user32.dll");
        original_clip=reinterpret_cast<decltype(original_clip)>(GetProcAddress(user,"ClipCursor"));
        original_warp=reinterpret_cast<decltype(original_warp)>(GetProcAddress(user,"SetCursorPos"));
        original_capture=reinterpret_cast<decltype(original_capture)>(GetProcAddress(user,"SetCapture"));
        original_position=reinterpret_cast<decltype(original_position)>(GetProcAddress(user,"GetCursorPos"));
        if(!original_clip||!original_warp||!original_capture||!original_position) {
            logger::write("ERROR: cursor APIs unavailable"); return;
        }
        // The engine import is the gate: loading this proxy in an unrelated
        // utility must not change that utility's cursor behavior.
        const auto engine=GetModuleHandleW(L"libIGDisplay.dll");
        if(!engine) return;
        const auto read_online = [&](const wchar_t* key, const wchar_t* fallback) {
            wchar_t text[256]{};
            GetPrivateProfileStringW(L"Online", key, fallback, text, 256, ini.c_str());
            return ini_rules::narrow(text);
        };
        openspy_redirect::install(GetModuleHandleW(nullptr),
            online_rules::choose(read_online(L"Domain",L"openspy.net"), read_online(L"Server",L"")),
            local_ip_rules::choose(read_online(L"LocalIP",L"auto")));
        window_focus::install();
        display_modes::install();
        // The protected EXE restores API addresses without restoring a usable
        // import-name table. IAT-only hooks covered the engine DLL but missed
        // the game's dynamically resolved calls. These hooks remain process-local.
        struct hook {void* target;void* replacement;void** original;};
        const std::array<hook,4> hooks{{
            {reinterpret_cast<void*>(original_clip),reinterpret_cast<void*>(&clip),reinterpret_cast<void**>(&original_clip)},
            {reinterpret_cast<void*>(original_warp),reinterpret_cast<void*>(&warp),reinterpret_cast<void**>(&original_warp)},
            {reinterpret_cast<void*>(original_capture),reinterpret_cast<void*>(&capture_window),reinterpret_cast<void**>(&original_capture)},
            {reinterpret_cast<void*>(original_position),reinterpret_cast<void*>(&position),reinterpret_cast<void**>(&original_position)}
        }};
        const auto initialized=MH_Initialize();
        if(initialized!=MH_OK && initialized!=MH_ERROR_ALREADY_INITIALIZED) {
            logger::write("ERROR: cursor hook initialization failed (%d)",initialized);return;
        }
        std::size_t created=0;
        bool enabled=true;
        for(const auto& item:hooks) {
            if(MH_CreateHook(item.target,item.replacement,item.original)!=MH_OK) {enabled=false;break;}
            ++created;
        }
        if(enabled) for(const auto& item:hooks) {
            if(MH_EnableHook(item.target)!=MH_OK) {enabled=false;break;}
        }
        if(!enabled) {
            for(std::size_t i=0;i<created;++i) {MH_DisableHook(hooks[i].target);MH_RemoveHook(hooks[i].target);*hooks[i].original=hooks[i].target;}
            logger::write("ERROR: cursor hooks could not be installed; capture policy is unavailable");return;
        }
        if(!capture && foreground()) original_clip(nullptr);
        logger::write("mouse: process cursor APIs hooked (including dynamic calls)");
    });
}
bool controller_support(){install();return controllers;}
bool capture_mouse(){install();return capture;}
}
