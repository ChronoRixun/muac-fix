// Process DPI awareness set by the runtime at load (UL: oversized window under display scaling).
// One process per case, because awareness is per process and cannot be reset:
//   on      engine loaded first, default ini      -> per-monitor aware, logged
//   off     engine loaded first, DpiAware=0       -> still unaware
//   nogame  no engine DLL (unrelated utility)     -> still unaware
#define DIRECTINPUT_VERSION 0x0800
#include <Windows.h>
#include <dinput.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <string>

int failures=0;
#define CHECK(x) do{if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);++failures;}}while(false)
static DPI_AWARENESS awareness() {
    return GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext());
}
int main(int argc,char** argv) {
    if(argc<4){std::puts("Expected runtime DLL, synthetic engine DLL and on|off|nogame");return 2;}
    const std::string mode=argv[3];
    wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
    const auto directory=std::filesystem::path(executable).parent_path()/(L"dpi-"+std::wstring(mode.begin(),mode.end()));
    std::filesystem::create_directories(directory);
    std::filesystem::copy_file(argv[1],directory/L"dinput8.dll",std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(argv[2],directory/L"libIGDisplay.dll",std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(directory/L"muac-fix.log");
    std::ofstream(directory/L"muac-fix.ini") << "[Display]\n" << (mode=="off"?"DpiAware=0\n":"") << "[Online]\nDomain=off\nLocalIP=first\n";
    CHECK(awareness()==DPI_AWARENESS_UNAWARE); // No manifest: the test starts like Game.exe.
    SetDllDirectoryW(directory.c_str());
    if(mode!="nogame") CHECK(LoadLibraryW((directory/L"libIGDisplay.dll").c_str())!=nullptr);
    const auto fix=LoadLibraryW((directory/L"dinput8.dll").c_str());CHECK(fix!=nullptr);
    const auto expected=mode=="on"?DPI_AWARENESS_PER_MONITOR_AWARE:DPI_AWARENESS_UNAWARE;
    CHECK(awareness()==expected);
    if(mode=="on") {
        CHECK(GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext())==DPI_AWARENESS_PER_MONITOR_AWARE);
        // A window created now is per-monitor aware too (what the game's window inherits).
        WNDCLASSA klass{};klass.lpfnWndProc=DefWindowProcA;klass.hInstance=GetModuleHandleW(nullptr);klass.lpszClassName="muac_dpi_test";
        RegisterClassA(&klass);
        const auto window=CreateWindowA("muac_dpi_test","",WS_OVERLAPPEDWINDOW,0,0,100,100,nullptr,nullptr,klass.hInstance,nullptr);
        CHECK(window && GetAwarenessFromDpiAwarenessContext(GetWindowDpiAwarenessContext(window))==DPI_AWARENESS_PER_MONITOR_AWARE);
        if(window) DestroyWindow(window);
        using create_t=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
        const auto create=reinterpret_cast<create_t>(GetProcAddress(fix,"DirectInput8Create"));
        void* input{};
        // The log line is written once the game starts DirectInput (runtime::install).
        if(create && SUCCEEDED(create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8A,&input,nullptr)) && input)
            static_cast<IUnknown*>(input)->Release();
        std::ifstream log(directory/L"muac-fix.log"); std::stringstream text; text<<log.rdbuf();
        CHECK(text.str().find("per-monitor DPI aware")!=std::string::npos);
    }
    if(mode=="off") {
        using create_t=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
        const auto create=reinterpret_cast<create_t>(GetProcAddress(fix,"DirectInput8Create"));
        void* input{};
        if(create && SUCCEEDED(create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8A,&input,nullptr)) && input)
            static_cast<IUnknown*>(input)->Release();
        std::ifstream log(directory/L"muac-fix.log"); std::stringstream text; text<<log.rdbuf();
        CHECK(text.str().find("DpiAware=0")!=std::string::npos);
    }
    std::printf("dpi %s: %d failure(s)\n",mode.c_str(),failures);
    return failures?1:0;
}
