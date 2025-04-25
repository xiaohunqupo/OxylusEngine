#include "OS.hpp"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace ox {
auto os::mem_page_size() -> uint64 {
    OX_SCOPED_ZONE;

    SYSTEM_INFO sys_info = {};
    GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
}

auto os::mem_reserve(uint64 size) -> void * {
    OX_SCOPED_ZONE;

    return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
}

auto os::mem_release(void *data, [[maybe_unused]] uint64 size) -> void {
    OX_SCOPED_ZONE;
    TracyFree(data);
    VirtualFree(data, 0, MEM_RELEASE);
}

auto os::mem_commit(void *data, uint64 size) -> bool {
    OX_SCOPED_ZONE;
    TracyAllocN(data, size, "Virtual Alloc");
    return VirtualAlloc(data, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

auto os::mem_decommit(void *data, [[maybe_unused]] uint64 size) -> void {
    OX_SCOPED_ZONE;

    VirtualFree(data, 0, MEM_DECOMMIT | MEM_RELEASE);
}
}
