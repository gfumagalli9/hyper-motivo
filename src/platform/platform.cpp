#include <sys/mman.h>

void* motivo_mmap(size_t length, int prot, int fd)
{
    return mmap(nullptr, length, prot, MAP_PRIVATE | MAP_POPULATE, fd, 0);
}

int motivo_munmap(void *addr, size_t length)
{
    return munmap(addr, length);
}
