#include "controller_setup.hpp"
#include "log.hpp"
#include <objbase.h>
#include <string>
#include <mutex>
#include <map>

namespace controller_setup {
namespace {
    bool retail_host() {
        const auto base=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) return false;
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
        if(pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
           pe->FileHeader.TimeDateStamp!=0x45141833 || pe->OptionalHeader.SizeOfImage!=0x98b000) return false;
        wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);
        const auto name=wcsrchr(path,L'\\');
        return lstrcmpiW(name?name+1:path,L"Game.exe")==0;
    }
    std::wstring guid(REFGUID value) {
        wchar_t text[40]{};
        if(!StringFromGUID2(value,text,40)) return {};
        std::wstring result(text);
        return result.substr(1,result.size()-2);
    }
    result associated_profile(HKEY parent,const wchar_t* destination,const wchar_t* source) {
        HKEY key{};
        if(RegOpenKeyExW(parent,destination,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS) return result::failed;
        wchar_t owner[MAX_PATH+1]{};DWORD bytes=MAX_PATH*sizeof(wchar_t),type{};
        const auto status=RegQueryValueExW(key,L"MuacFixSourceName",nullptr,&type,reinterpret_cast<BYTE*>(owner),&bytes);
        RegCloseKey(key);
        return status==ERROR_SUCCESS && type==REG_SZ && _wcsicmp(owner,source)==0 ? result::existing : result::failed;
    }
}
result create_profile(HKEY parent,const wchar_t* name,const std::wstring& id,const std::wstring& type) {
    HKEY present{};
    const auto found=RegOpenKeyExW(parent,name,0,KEY_QUERY_VALUE,&present);
    if(found==ERROR_SUCCESS){RegCloseKey(present);return result::existing;}
    if(found!=ERROR_FILE_NOT_FOUND || id.empty() || type.empty()) return result::failed;
    const auto pending=L"MuacFixPending-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HKEY key{};DWORD disposition{};
    if(RegCreateKeyExW(parent,pending.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&key,&disposition)!=ERROR_SUCCESS) return result::failed;
    if(disposition!=REG_CREATED_NEW_KEY){RegCloseKey(key);return result::failed;}
    const DWORD config=0;
    const bool complete=
        RegSetValueExW(key,L"DeviceUniqueID",0,REG_SZ,reinterpret_cast<const BYTE*>(id.c_str()),static_cast<DWORD>((id.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS &&
        RegSetValueExW(key,L"DeviceType",0,REG_SZ,reinterpret_cast<const BYTE*>(type.c_str()),static_cast<DWORD>((type.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS &&
        RegSetValueExW(key,L"Config",0,REG_DWORD,reinterpret_cast<const BYTE*>(&config),sizeof(config))==ERROR_SUCCESS;
    RegCloseKey(key);
    // Publish only a complete profile. RegRenameKey refuses an existing target;
    // a concurrent game or settings editor's Player0 is never overwritten.
    if(complete && RegRenameKey(parent,pending.c_str(),name)==ERROR_SUCCESS) return result::created;
    RegDeleteTreeW(parent,pending.c_str()); // Only the temporary key we just created.
    if(RegOpenKeyExW(parent,name,0,KEY_QUERY_VALUE,&present)==ERROR_SUCCESS){RegCloseKey(present);return result::existing;}
    return result::failed;
}
result copy_profile(HKEY parent,const wchar_t* source,const wchar_t* destination) {
    HKEY existing{};
    if(RegOpenKeyExW(parent,destination,0,KEY_READ,&existing)==ERROR_SUCCESS){RegCloseKey(existing);return associated_profile(parent,destination,source);}
    HKEY original{};
    const auto opened=RegOpenKeyExW(parent,source,0,KEY_READ,&original);
    if(opened!=ERROR_SUCCESS && opened!=ERROR_FILE_NOT_FOUND) return result::failed;
    const auto pending=L"MuacFixCopy-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HKEY temporary{};DWORD disposition{};
    const auto created=RegCreateKeyExW(parent,pending.c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&temporary,&disposition);
    if(created!=ERROR_SUCCESS || disposition!=REG_CREATED_NEW_KEY){if(temporary)RegCloseKey(temporary);if(original)RegCloseKey(original);return result::failed;}
    // Even a first-time device needs an association marker before the game adds
    // its defaults, so the next launch can recognize this as the same alias.
    auto copied=original?RegCopyTreeW(original,nullptr,temporary):ERROR_SUCCESS;
    if(copied==ERROR_SUCCESS) copied=RegSetValueExW(temporary,L"MuacFixSourceName",0,REG_SZ,
        reinterpret_cast<const BYTE*>(source),static_cast<DWORD>((wcslen(source)+1)*sizeof(wchar_t)));
    RegCloseKey(temporary);if(original)RegCloseKey(original);
    if(copied==ERROR_SUCCESS && RegRenameKey(parent,pending.c_str(),destination)==ERROR_SUCCESS) return result::created;
    RegDeleteTreeW(parent,pending.c_str());
    if(RegOpenKeyExW(parent,destination,0,KEY_READ,&existing)==ERROR_SUCCESS){RegCloseKey(existing);return associated_profile(parent,destination,source);}
    return result::failed;
}
bool prepare_name(const std::wstring& old_name,const std::wstring& new_name) {
    static const bool host=retail_host();
    if(!host || old_name==new_name) return true;
    // The game stores device profiles by friendly name. Copy an existing profile
    // to the new name only if that destination is absent; retain the old profile.
    if(old_name.empty() || old_name.find_first_of(L"\\/")!=std::wstring::npos) return false;
    static std::mutex mutex;
    static std::map<std::wstring,bool> prepared;
    std::lock_guard lock(mutex);
    if(const auto found=prepared.find(old_name);found!=prepared.end()) return found->second;
    HKEY parent{};
    const auto opened=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Activision\\Marvel Ultimate Alliance\\Controls\\DeviceConfig",0,nullptr,0,KEY_ALL_ACCESS,nullptr,&parent,nullptr);
    bool ok=false;
    if(opened==ERROR_SUCCESS){ok=copy_profile(parent,old_name.c_str(),new_name.c_str())!=result::failed;RegCloseKey(parent);}
    prepared.emplace(old_name,ok);
    if(!ok) logger::write("input: retaining the original device name because its saved profile could not be preserved");
    return ok;
}
void select_if_unconfigured(REFGUID instance,REFGUID product) {
    // The stock first-run path prefers the keyboard and gives the pad to player
    // two. Seed only an absent player-one entry; never overwrite a saved choice.
    // Restrict this registry behavior to the checked retail MUA executable.
    if(!retail_host()) return;
    static std::once_flag once;
    std::call_once(once,[&]{
        constexpr auto path=L"Software\\Activision\\Marvel Ultimate Alliance\\Controls\\PlayerConfig";
        HKEY parent{};
        if(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_ALL_ACCESS,nullptr,&parent,nullptr)!=ERROR_SUCCESS) return;
        const auto outcome=create_profile(parent,L"Player0",guid(instance),guid(product));
        RegCloseKey(parent);
        if(outcome!=result::existing) logger::write("input: first-run player-one controller selection %s",
            outcome==result::created?"created (no existing choice replaced)":"could not be completed");
    });
}
}
