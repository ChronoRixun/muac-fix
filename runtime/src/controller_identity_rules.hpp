#pragma once
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <Windows.h>
#include <dinput.h>
#include <cstring>
#include <cwchar>
#include <optional>
#include <string>
#include <string_view>
#include <algorithm>
#include <iterator>

namespace controller_identity_rules {
    constexpr DWORD wired_xbox360=0x028e045e;
    // MUA's UI selects its existing Xbox atlas when an input-device name contains
    // Xbox/XBox/XBOX. A PID-only substitution leaves Bluetooth pads on numbered art.
    inline std::optional<std::string> name(std::string_view original) {
        if(original.empty() || original.size()+11>255) return std::nullopt;
        return "Xbox 360 - "+std::string(original);
    }
    inline std::optional<std::wstring> name(std::wstring_view original) {
        if(original.empty() || original.size()+11>255) return std::nullopt;
        return L"Xbox 360 - "+std::wstring(original);
    }
    inline std::optional<std::string> name(const DIDEVICEINSTANCEA& info) {
        const auto end=std::find(std::begin(info.tszInstanceName),std::end(info.tszInstanceName),'\0');
        if(end==std::end(info.tszInstanceName)) return std::nullopt;
        return name(std::string_view(info.tszInstanceName,end-info.tszInstanceName));
    }
    inline std::optional<std::wstring> name(const DIDEVICEINSTANCEW& info) {
        const auto end=std::find(std::begin(info.tszInstanceName),std::end(info.tszInstanceName),L'\0');
        if(end==std::end(info.tszInstanceName)) return std::nullopt;
        return name(std::wstring_view(info.tszInstanceName,end-info.tszInstanceName));
    }
    inline bool apply(DIDEVICEINSTANCEA& info) {
        const auto alias=name(info);
        if(!alias) return false;
        info.guidProduct.Data1=wired_xbox360;
        strcpy_s(info.tszInstanceName,alias->c_str());
        strcpy_s(info.tszProductName,alias->c_str());
        return true;
    }
    inline bool apply(DIDEVICEINSTANCEW& info) {
        const auto alias=name(info);
        if(!alias) return false;
        info.guidProduct.Data1=wired_xbox360;
        wcscpy_s(info.tszInstanceName,alias->c_str());
        wcscpy_s(info.tszProductName,alias->c_str());
        return true;
    }
}
