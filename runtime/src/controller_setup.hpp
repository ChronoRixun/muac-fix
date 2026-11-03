#pragma once
#include <Windows.h>
#include <string>
namespace controller_setup {
    enum class result { created, existing, failed };
    result create_profile(HKEY parent,const wchar_t* name,const std::wstring& instance,const std::wstring& product);
    result copy_profile(HKEY parent,const wchar_t* source,const wchar_t* destination);
    bool prepare_name(const std::wstring& old_name,const std::wstring& new_name);
    void select_if_unconfigured(REFGUID instance,REFGUID product);
}
