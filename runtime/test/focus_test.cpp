#define DIRECTINPUT_VERSION 0x0800
#include <Windows.h>
#include <dinput.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>

int failures=0, deactivations=0, manual_minimizes=0;
#define CHECK(x) do{if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);++failures;}}while(false)
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
    wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
    const auto directory=std::filesystem::path(executable).parent_path();
    // Always test the current built DLL, even when this test executable did not
    // need relinking after a runtime-only edit.
    std::filesystem::copy_file(argv[1],directory/L"dinput8.dll",std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(argv[2],directory/L"libIGDisplay.dll",std::filesystem::copy_options::overwrite_existing);
    std::ofstream(directory/L"dgVoodoo.conf") << "[General]\nFullScreenMode=false\n[DirectX]\nAppControlledScreenMode=false\n";
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
    if(input)input->Release();
    std::printf("focus background=%d: %d failure(s)\n",background,failures);
    return failures?1:0;
}
