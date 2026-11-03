#define DIRECTINPUT_VERSION 0x0800
#include <Windows.h>
#include <dinput.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <array>
#include "runtime_rules.hpp"
#include "online_rules.hpp"

int failures = 0;
#define CHECK(x) do { if(!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while(false)
BOOL CALLBACK enumerate(const DIDEVICEINSTANCEA* item, void* context) {
    ++*static_cast<int*>(context);
    std::printf("Controller: %s, product %08lX\n", item->tszProductName, item->guidProduct.Data1);
    return DIENUM_CONTINUE;
}
int main() {
    const std::array<int,1> one{2};
    const std::array<int,2> mixed{0,2};
    CHECK(runtime_rules::single_xinput_slot(one)==2);
    CHECK(!runtime_rules::single_xinput_slot(mixed));
    CHECK(!runtime_rules::single_xinput_slot(std::span<const int>{}));
    const auto online=online_rules::choose("openspy.net", "");
    CHECK(online_rules::redirect("fixture.available.gamespy.com", online)=="fixture.available.openspy.net");
    CHECK(online_rules::redirect("natneg1.gamespy.com", online)=="natneg1.openspy.net");
    CHECK(online_rules::redirect("peerchat.gamespy.com", online)=="peerchat.openspy.net");
    CHECK(!online_rules::redirect("notgamespy.com", online));
    CHECK(!online_rules::redirect("gamespy.com.example.org", online));
    CHECK(!online_rules::redirect("fixture.gamespy.com", online_rules::choose("off", "")));
    CHECK(online_rules::redirect("fixture.gamespy.com", online_rules::choose("openspy.net", "127.0.0.1"))=="127.0.0.1");
    CHECK(runtime_rules::mouse_flags(true, false, DISCL_EXCLUSIVE | DISCL_FOREGROUND) ==
          (DISCL_NONEXCLUSIVE | DISCL_FOREGROUND));
    CHECK(runtime_rules::mouse_flags(false, false, DISCL_EXCLUSIVE | DISCL_FOREGROUND) ==
          (DISCL_EXCLUSIVE | DISCL_FOREGROUND));
    CHECK(runtime_rules::mouse_flags(true, true, DISCL_EXCLUSIVE | DISCL_FOREGROUND) ==
          (DISCL_EXCLUSIVE | DISCL_FOREGROUND));
    CHECK(!runtime_rules::may_capture(true, false));
    CHECK(!runtime_rules::may_capture(false, true));
    CHECK(runtime_rules::may_capture(true, true));
    const auto module = LoadLibraryW(L"dinput8.dll");
    CHECK(module != nullptr);
    const auto version = reinterpret_cast<const char*(*)()>(GetProcAddress(module, "MuacFix_Version"));
    CHECK(version && std::strcmp(version(), "MUA Classic Fix 0.1.0") == 0);
    for(const char* name : {"DirectInput8Create", "DllCanUnloadNow", "DllGetClassObject",
                           "DllRegisterServer", "DllUnregisterServer", "GetdfDIJoystick"})
        CHECK(GetProcAddress(module, name) != nullptr);
    IDirectInput8A* input = nullptr;
    CHECK(SUCCEEDED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
                    IID_IDirectInput8A, reinterpret_cast<void**>(&input), nullptr)));
    if(input) {
        int count=0;
        CHECK(SUCCEEDED(input->EnumDevices(DI8DEVCLASS_GAMECTRL, enumerate, &count, DIEDFL_ATTACHEDONLY)));
        std::printf("Connected DirectInput controllers: %d\n", count);
        IDirectInputDevice8A* mouse=nullptr;
        CHECK(SUCCEEDED(input->CreateDevice(GUID_SysMouse, &mouse, nullptr)));
        if(mouse) {
            CHECK(SUCCEEDED(mouse->SetDataFormat(&c_dfDIMouse2)));
            DIDEVICEINSTANCEA info{}; info.dwSize=sizeof(info);
            CHECK(SUCCEEDED(mouse->GetDeviceInfo(&info)));
            CHECK(GET_DIDEVICE_TYPE(info.dwDevType)==DI8DEVTYPE_MOUSE);
            mouse->Release();
        }
        IDirectInput8W* wide=nullptr;
        CHECK(SUCCEEDED(input->QueryInterface(IID_IDirectInput8W,reinterpret_cast<void**>(&wide))));
        if(wide) wide->Release();
        input->Release();
    }
    std::printf("%d failure(s)\n",failures);
    return failures ? 1 : 0;
}
