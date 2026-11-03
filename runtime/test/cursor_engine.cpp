// Synthetic engine fixture: resolves cursor APIs dynamically, as a protected
// executable can. No game bytes and no deliberate cursor movement.
#include <Windows.h>
extern "C" __declspec(dllexport) BOOL MuacTest_DynamicWarp() {
    const auto user=GetModuleHandleW(L"user32.dll");
    const auto get=reinterpret_cast<decltype(&GetCursorPos)>(GetProcAddress(user,"GetCursorPos"));
    const auto set=reinterpret_cast<decltype(&SetCursorPos)>(GetProcAddress(user,"SetCursorPos"));
    POINT point{};
    return get && set && get(&point) && set(point.x,point.y);
}
