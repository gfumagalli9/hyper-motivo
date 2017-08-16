#include <sys/mman.h>
#include "unistd.h"

void* motivo_mmap_populate(size_t length, int prot, int fd)
{
    return mmap(nullptr, length, prot, MAP_PRIVATE | MAP_POPULATE, fd, 0);
}

void* motivo_mmap(size_t length, int prot, int fd)
{
    return mmap(nullptr, length, prot, MAP_PRIVATE, fd, 0);
}

void motivo_prefault(off_t off, size_t length, int fd)
{
    const long page_size = sysconf(_SC_PAGE_SIZE);

    off_t aligned_off = (off/page_size)*page_size;
    size_t aligned_len = length + static_cast<size_t>(off-aligned_off);
    void* m=mmap(nullptr, aligned_len, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, aligned_off);
    munmap(m, aligned_len);
}

int motivo_munmap(void *addr, size_t length)
{
    return munmap(addr, length);
}
