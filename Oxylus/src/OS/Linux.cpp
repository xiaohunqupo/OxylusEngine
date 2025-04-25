#include "OS.hpp"

#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace ox {
auto os::mem_page_size() -> uint64 {
    return sysconf(_SC_PAGESIZE);
}

auto os::mem_reserve(uint64 size) -> void * {
    OX_SCOPED_ZONE;

    return mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

auto os::mem_release(void *data, uint64 size) -> void {
    OX_SCOPED_ZONE;

    munmap(data, size);
}

auto os::mem_commit(void *data, uint64 size) -> bool {
    OX_SCOPED_ZONE;

    return mprotect(data, size, PROT_READ | PROT_WRITE);
}

auto os::mem_decommit(void *data, uint64 size) -> void {
    OX_SCOPED_ZONE;

    madvise(data, size, MADV_DONTNEED);
    mprotect(data, size, PROT_NONE);
}
}
