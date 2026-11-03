#include "controller_setup.hpp"
#include <Windows.h>
#include <cstdio>
#include <string>
int main(){
    const auto path=L"Software\\MuacFixSyntheticTests\\"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HKEY parent{};DWORD disposition{};
    if(RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&parent,&disposition)!=ERROR_SUCCESS || disposition!=REG_CREATED_NEW_KEY)return 2;
    int failures=0;
    const auto check=[&](bool value,const char* name){if(!value){++failures;std::printf("FAIL %s\n",name);}};
    using controller_setup::result;
    check(controller_setup::create_profile(parent,L"Player0",L"fixture-instance",L"fixture-type")==result::created,"creates an absent profile");
    HKEY player{};RegOpenKeyExW(parent,L"Player0",0,KEY_READ|KEY_SET_VALUE,&player);
    DWORD custom=1;RegSetValueExW(player,L"Config",0,REG_DWORD,reinterpret_cast<const BYTE*>(&custom),sizeof(custom));
    check(controller_setup::create_profile(parent,L"Player0",L"replacement",L"replacement")==result::existing,"keeps an existing profile");
    wchar_t text[80]{};DWORD bytes=sizeof(text);
    check(RegQueryValueExW(player,L"DeviceUniqueID",nullptr,nullptr,reinterpret_cast<BYTE*>(text),&bytes)==ERROR_SUCCESS && std::wstring(text)==L"fixture-instance","keeps instance choice");
    bytes=sizeof(custom);custom=0;RegQueryValueExW(player,L"Config",nullptr,nullptr,reinterpret_cast<BYTE*>(&custom),&bytes);
    check(custom==1,"keeps custom bindings selection");RegCloseKey(player);
    check(controller_setup::create_profile(parent,L"Player1",L"",L"fixture-type")==result::failed,"rejects incomplete profile");
    DWORD children{};RegQueryInfoKeyW(parent,nullptr,nullptr,nullptr,&children,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);
    check(children==1,"leaves no temporary or incomplete keys");
    HKEY source{};RegCreateKeyExW(parent,L"Original\\Custom",0,nullptr,0,KEY_ALL_ACCESS,nullptr,&source,nullptr);
    DWORD binding=42;RegSetValueExW(source,L"Binding",0,REG_DWORD,reinterpret_cast<const BYTE*>(&binding),sizeof(binding));
    check(controller_setup::copy_profile(parent,L"Original",L"XboxAlias")==result::created,"copies original named profile");
    binding=77;RegSetValueExW(source,L"Binding",0,REG_DWORD,reinterpret_cast<const BYTE*>(&binding),sizeof(binding));RegCloseKey(source);
    check(controller_setup::copy_profile(parent,L"Original",L"XboxAlias")==result::existing,"keeps existing Xbox profile");
    HKEY alias{};RegOpenKeyExW(parent,L"XboxAlias\\Custom",0,KEY_READ,&alias);bytes=sizeof(binding);binding=0;
    RegQueryValueExW(alias,L"Binding",nullptr,nullptr,reinterpret_cast<BYTE*>(&binding),&bytes);RegCloseKey(alias);
    check(binding==42,"preserves custom binding in destination");
    RegOpenKeyExW(parent,L"Original\\Custom",0,KEY_READ,&source);bytes=sizeof(binding);binding=0;
    RegQueryValueExW(source,L"Binding",nullptr,nullptr,reinterpret_cast<BYTE*>(&binding),&bytes);RegCloseKey(source);
    check(binding==77,"leaves original profile untouched");
    HKEY unrelated{};RegCreateKeyExW(parent,L"UnrelatedXbox",0,nullptr,0,KEY_ALL_ACCESS,nullptr,&unrelated,nullptr);RegCloseKey(unrelated);
    check(controller_setup::copy_profile(parent,L"Original",L"UnrelatedXbox")==result::failed,"refuses an unassociated existing alias");
    check(controller_setup::copy_profile(parent,L"AnotherOriginal",L"XboxAlias")==result::failed,"refuses an alias belonging to another source name");
    check(controller_setup::copy_profile(parent,L"FreshSource",L"FreshAlias")==result::created,"associates a fresh device before defaults exist");
    check(controller_setup::copy_profile(parent,L"FreshSource",L"FreshAlias")==result::existing,"recognizes the fresh alias on another launch");
    RegCloseKey(parent);RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());
    std::printf("controller setup: %d failure(s)\n",failures);return failures?1:0;
}
