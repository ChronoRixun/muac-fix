#define DIRECTINPUT_VERSION 0x0800
#include <Windows.h>
#include <dinput.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>

int failures=0, deactivations=0, manual_minimizes=0;
#define CHECK(x) do{if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);++failures;}}while(false)
WNDPROC engine_previous{};
// Like the engine's later window procedure: swallows the "changing" messages, forwards the rest.
LRESULT CALLBACK engine_procedure(HWND window,UINT message,WPARAM value,LPARAM detail) {
    if(message==WM_WINDOWPOSCHANGING || message==WM_STYLECHANGING) return DefWindowProcA(window,message,value,detail);
    return CallWindowProcA(engine_previous,window,message,value,detail);
}
LRESULT CALLBACK recording(HWND window,UINT message,WPARAM value,LPARAM detail) {
    if(message==WM_ACTIVATEAPP && !value) ++deactivations;
    if(message==WM_APP+7) return 12345;
    if(message==WM_SYSCOMMAND && (value&0xfff0)==SC_MINIMIZE) {
        ++manual_minimizes;
        ShowWindow(window,SW_SHOWMINNOACTIVE); // Never activates this synthetic window.
        return 0;
    }
    return DefWindowProcA(window,message,value,detail);
}
int main(int argc,char** argv) {
    if(argc<3){std::puts("Expected runtime DLL and synthetic engine DLL paths");return 2;}
    const bool background=argc<4 || std::strcmp(argv[3],"off")!=0;
    const bool borderless=argc>=4 && std::strcmp(argv[3],"borderless")==0;
    wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
    const auto directory=std::filesystem::path(executable).parent_path();
    // Always test the current built DLL, even when this test executable did not
    // need relinking after a runtime-only edit.
    std::filesystem::copy_file(argv[1],directory/L"dinput8.dll",std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(argv[2],directory/L"libIGDisplay.dll",std::filesystem::copy_options::overwrite_existing);
    std::ofstream(directory/L"dgVoodoo.conf") << "[General]\nFullScreenMode=false\n[GeneralExt]\nWindowedAttributes="
        << (borderless?"Borderless, fullscreensize":"") << "\n[DirectX]\nAppControlledScreenMode=false\n";
    std::ofstream(directory/L"muac-fix.ini") << "[Display]\nRunInBackground=" << (background?1:0) << "\n[Online]\nDomain=off\nLocalIP=first\n";
    WNDCLASSA klass{};klass.lpfnWndProc=recording;klass.hInstance=GetModuleHandleW(nullptr);klass.lpszClassName="igWin32WindowClass";
    CHECK(RegisterClassA(&klass)!=0);
    HWND window=CreateWindowExA(WS_EX_TOOLWINDOW,klass.lpszClassName,"MUAC hidden focus test",WS_OVERLAPPEDWINDOW,
                                0,0,320,200,nullptr,nullptr,klass.hInstance,nullptr);
    CHECK(window!=nullptr);
    const auto engine=LoadLibraryW(L"libIGDisplay.dll");CHECK(engine!=nullptr);
    const auto fix=LoadLibraryW(L"dinput8.dll");CHECK(fix!=nullptr);
    const auto create=reinterpret_cast<decltype(&DirectInput8Create)>(GetProcAddress(fix,"DirectInput8Create"));
    IDirectInput8A* input{};
    CHECK(create && SUCCEEDED(create(klass.hInstance,DIRECTINPUT_VERSION,IID_IDirectInput8A,reinterpret_cast<void**>(&input),nullptr)));
    CHECK(reinterpret_cast<WNDPROC>(GetWindowLongPtrA(window,GWLP_WNDPROC))!=recording);
    CHECK(SendMessageA(window,WM_APP+7,0,0)==12345); // Original procedure remains chained.
    deactivations=0;
    SendMessageA(window,WM_ACTIVATEAPP,FALSE,0);
    CHECK(deactivations==(background?0:1));
    ShowWindow(window,SW_SHOWMINNOACTIVE);
    CHECK(!IsIconic(window)); // Game-initiated minimization was suppressed.
    SendMessageA(window,WM_SYSCOMMAND,SC_MINIMIZE,0);
    CHECK(manual_minimizes==1 && IsIconic(window)); // Explicit minimize still works.
    ShowWindow(window,SW_HIDE);
    CHECK(DestroyWindow(window));
    {
        // Borderless: a game window created now (DPI aware, as in the game) loses its frame and covers
        // exactly the primary monitor, and keeps doing so when the engine restores its style or moves it.
        // Never shown: the suppressed minimize attaches the policy to the hidden window.
        HWND game=CreateWindowExA(WS_EX_TOOLWINDOW|WS_EX_CLIENTEDGE,klass.lpszClassName,"MUAC hidden borderless test",WS_OVERLAPPEDWINDOW,
                                  40,40,640,480,nullptr,nullptr,klass.hInstance,nullptr);
        CHECK(game!=nullptr);
        ShowWindow(game,SW_SHOWMINNOACTIVE);
        MONITORINFO info{};info.cbSize=sizeof(info);
        GetMonitorInfoW(MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY),&info);
        const auto check=[&](const char* step){
            RECT rect{};GetWindowRect(game,&rect);
            const auto style=static_cast<DWORD>(GetWindowLongA(game,GWL_STYLE));
            const bool framed=(style&WS_CAPTION)==WS_CAPTION || (style&WS_THICKFRAME);
            RECT client{};GetClientRect(game,&client);
            const bool primary=EqualRect(&rect,&info.rcMonitor)!=FALSE &&
                client.right==info.rcMonitor.right-info.rcMonitor.left && client.bottom==info.rcMonitor.bottom-info.rcMonitor.top;
            if(borderless!=(!framed && primary)) std::printf("FAIL %s: style %08lx rect %ld,%ld,%ld,%ld\n",step,style,rect.left,rect.top,rect.right,rect.bottom),++failures;
        };
        check("attach");
        SetWindowLongA(game,GWL_STYLE,WS_OVERLAPPEDWINDOW);
        SetWindowLongA(game,GWL_EXSTYLE,WS_EX_TOOLWINDOW|WS_EX_CLIENTEDGE|WS_EX_WINDOWEDGE);
        SetWindowPos(game,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        check("style reset");
        MoveWindow(game,25,25,800,600,FALSE);
        check("move");
        if(!borderless) {
            // Windowed bigger than the primary monitor: centred on it horizontally, title bar on screen.
            const int width=info.rcMonitor.right-info.rcMonitor.left, height=info.rcMonitor.bottom-info.rcMonitor.top;
            MoveWindow(game,50,50,width+26,height+60,FALSE);
            RECT rect{};GetWindowRect(game,&rect);
            CHECK(rect.left==info.rcMonitor.left-13 && rect.top==info.rcMonitor.top && rect.right-rect.left==width+26);
            MoveWindow(game,60,70,320,200,FALSE); // a window that fits keeps its place
            GetWindowRect(game,&rect);
            CHECK(rect.left==60 && rect.top==70);
        }
        // The engine replaces the procedure and moves the window again: corrected after the move.
        engine_previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrA(game,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&engine_procedure)));
        if(borderless) {
            SetWindowLongA(game,GWL_STYLE,WS_OVERLAPPEDWINDOW);
            MoveWindow(game,0,0,900,700,FALSE);
            check("engine procedure");
        }
        else {
            const int width=info.rcMonitor.right-info.rcMonitor.left, height=info.rcMonitor.bottom-info.rcMonitor.top;
            MoveWindow(game,info.rcMonitor.left,info.rcMonitor.top,width+26,height+60,FALSE);
            RECT rect{};GetWindowRect(game,&rect);
            CHECK(rect.left==info.rcMonitor.left-13 && rect.top==info.rcMonitor.top);
        }
        CHECK(DestroyWindow(game));
    }
    if(input)input->Release();
    std::printf("focus background=%d: %d failure(s)\n",background,failures);
    return failures?1:0;
}
