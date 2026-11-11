#define DIRECTINPUT_VERSION 0x0800
#include <Windows.h>
#include <dinput.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "runtime_rules.hpp"
#include "controller_identity_rules.hpp"
#include "window_focus_rules.hpp"
#include "online_rules.hpp"
#include "vtable_patch.hpp"
#include "display_modes_rules.hpp"

int failures = 0;
#define CHECK(x) do { if(!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while(false)
BOOL CALLBACK enumerate(const DIDEVICEINSTANCEA* item, void* context) {
    ++*static_cast<int*>(context);
    std::printf("Controller: %s, product %08lX\n", item->tszProductName, item->guidProduct.Data1);
    return DIENUM_CONTINUE;
}
int protection_calls = 0, fail_protection_call = 0;
BOOL WINAPI fixture_protect(LPVOID, SIZE_T, DWORD, PDWORD old) {
    *old = PAGE_READONLY;
    return ++protection_calls != fail_protection_call;
}
int main() {
    {
        namespace dm = display_modes_rules;
        CHECK(dm::parse("2560x1440") && dm::parse("2560x1440")->width == 2560 && dm::parse("2560x1440")->height == 1440);
        CHECK(dm::parse("3840x2160") && dm::parse("3840x2160")->height == 2160);
        CHECK(!dm::parse("") && !dm::parse("2560") && !dm::parse("2560x") && !dm::parse("x1440"));
        CHECK(!dm::parse("2560x1440 ") && !dm::parse("2560X1440") && !dm::parse("-1x480") && !dm::parse("320x240"));
        CHECK(!dm::parse("1234567x480"));
        CHECK(dm::whitelisted(1920, 1080) && dm::whitelisted(1680, 1050) && dm::whitelisted(2048, 1536));
        CHECK(!dm::whitelisted(1600, 900) && !dm::whitelisted(2560, 1440) && !dm::whitelisted(2048, 1152));
        CHECK(dm::accept(2560, 1440, dm::parse("2560x1440")) && !dm::accept(2560, 1080, dm::parse("2560x1440")));
        CHECK(dm::accept(1280, 720, std::nullopt) && !dm::accept(3840, 2160, std::nullopt));
        const dm::entry modes[] = {{640,480,60,22},{1920,1080,60,22},{2560,1440,60,22}};
        CHECK(dm::listed(modes, 3, "2560x1440") && dm::listed(modes, 3, "640x480"));
        CHECK(!dm::listed(modes, 2, "2560x1440") && !dm::listed(modes, 3, "3840x2160") && !dm::listed(nullptr, 3, "640x480"));
        CHECK(!dm::listed(modes, 3, "2560x14400") && !dm::listed(modes, 3, ""));
        const auto unlisted = dm::unlisted_mode("3840x2160");
        CHECK(unlisted.width == 3840 && unlisted.height == 2160 && unlisted.refresh == 60 && unlisted.format == 22);
        const auto fallback = dm::unlisted_mode("garbage");
        CHECK(fallback.width == 800 && fallback.height == 600);
    }    int first{}, second{};
    void* slot = &first;
    void* saved = nullptr;
    fail_protection_call = 1;
    CHECK(vtable_patch::install(&slot, &second, saved, fixture_protect) == vtable_patch::result::write_denied);
    CHECK(slot == &first && saved == nullptr && protection_calls == 1);
    protection_calls = 0; fail_protection_call = 2;
    CHECK(vtable_patch::install(&slot, &second, saved, fixture_protect) == vtable_patch::result::restore_failed);
    CHECK(slot == &first && saved == &first && protection_calls == 3);
    protection_calls = 0; fail_protection_call = 0;
    CHECK(vtable_patch::install(&slot, &second, saved, fixture_protect) == vtable_patch::result::installed);
    CHECK(slot == &second && saved == &first && protection_calls == 2);
    DIDEVICEINSTANCEA ansi{};ansi.dwSize=sizeof(ansi);ansi.guidInstance.Data1=0x12345678;
    strcpy_s(ansi.tszInstanceName,"Bluetooth LE XINPUT compatible input device");
    controller_identity_rules::apply(ansi);
    CHECK(ansi.guidProduct.Data1==0x028e045e && ansi.guidInstance.Data1==0x12345678);
    CHECK(std::strcmp(ansi.tszInstanceName,"Xbox 360 - Bluetooth LE XINPUT compatible input device")==0);
    CHECK(std::strcmp(ansi.tszProductName,ansi.tszInstanceName)==0);
    DIDEVICEINSTANCEW unicode{};unicode.dwSize=sizeof(unicode);unicode.guidInstance.Data1=0x87654321;
    wcscpy_s(unicode.tszInstanceName,L"Wireless Pad");
    controller_identity_rules::apply(unicode);
    CHECK(unicode.guidInstance.Data1==0x87654321 && unicode.guidProduct.Data1==0x028e045e);
    CHECK(std::wcscmp(unicode.tszInstanceName,L"Xbox 360 - Wireless Pad")==0);
    CHECK(std::wcscmp(unicode.tszProductName,unicode.tszInstanceName)==0);
    CHECK(controller_identity_rules::name("Pad One")!=controller_identity_rules::name("Pad Two"));
    CHECK(!controller_identity_rules::name(std::string(MAX_PATH,'x')));
    CHECK(controller_identity_rules::name(std::string(244,'x')).has_value());
    CHECK(!controller_identity_rules::name(std::string(245,'x')));
    DIDEVICEINSTANCEA unterminated{};
    std::fill(std::begin(unterminated.tszInstanceName),std::end(unterminated.tszInstanceName),'x');
    CHECK(!controller_identity_rules::name(unterminated));
    CHECK(window_focus_rules::suppress_minimize(true,false,SW_MINIMIZE));
    CHECK(!window_focus_rules::suppress_minimize(true,true,SW_MINIMIZE));
    CHECK(!window_focus_rules::suppress_minimize(false,false,SW_MINIMIZE));
    CHECK(!window_focus_rules::suppress_minimize(true,false,SW_HIDE));
    CHECK(window_focus_rules::suppress_deactivation(true,true,WM_ACTIVATEAPP,FALSE));
    CHECK(window_focus_rules::suppress_deactivation(true,true,WM_ACTIVATE,WA_INACTIVE));
    CHECK(!window_focus_rules::suppress_deactivation(true,false,WM_ACTIVATEAPP,FALSE));
    CHECK(!window_focus_rules::suppress_deactivation(false,true,WM_ACTIVATEAPP,FALSE));
    CHECK(!window_focus_rules::suppress_deactivation(true,true,WM_ACTIVATEAPP,TRUE));
    const auto engine=LoadLibraryW(L"libIGDisplay.dll");
    CHECK(engine!=nullptr);
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
    CHECK(version && std::strcmp(version(), "MUA Classic Fix 0.1.3") == 0);
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
    const auto warp=engine?reinterpret_cast<BOOL(*)()>(GetProcAddress(engine,"MuacTest_DynamicWarp")):nullptr;
    CHECK(warp && warp());
    wchar_t filename[MAX_PATH]{};GetModuleFileNameW(nullptr,filename,MAX_PATH);
    std::ifstream log(std::filesystem::path(filename).parent_path()/L"muac-fix.log");
    const std::string contents{std::istreambuf_iterator<char>(log),{}};
    CHECK(contents.find("process cursor recentering suppressed")!=std::string::npos);
    std::printf("%d failure(s)\n",failures);
    return failures ? 1 : 0;
}
