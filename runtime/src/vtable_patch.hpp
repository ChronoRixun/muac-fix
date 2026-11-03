#pragma once
#include <Windows.h>

namespace vtable_patch {
    enum class result { installed, write_denied, restore_failed };

    // Caller serializes access and publishes the saved pointer before the hook.
    // The injectable API permits failure tests without modifying system pages.
    inline result install(void** slot, void* replacement, void*& saved,
                          decltype(&VirtualProtect) protect = &VirtualProtect) {
        DWORD previous{};
        if (!protect(slot, sizeof(void*), PAGE_READWRITE, &previous)) return result::write_denied;
        saved = *slot;
        InterlockedExchangePointer(slot, replacement);
        DWORD ignored{};
        if (protect(slot, sizeof(void*), previous, &ignored)) return result::installed;
        // The failed protection change leaves the slot writable. Withdraw the hook
        // and retry restoring protection; the caller reports the failure.
        InterlockedExchangePointer(slot, saved);
        protect(slot, sizeof(void*), previous, &ignored);
        return result::restore_failed;
    }
}
