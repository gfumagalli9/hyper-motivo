// MIT License
//
// Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <sys/mman.h>
#include <cassert>
#include <stdexcept>
#include "unistd.h"

void* motivo_mmap_populate(size_t length, int prot, int fd)
{
    return mmap(nullptr, length, prot, MAP_PRIVATE | MAP_POPULATE, fd, 0); // NOLINT
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
    void* m=mmap(nullptr, aligned_len, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, aligned_off); // NOLINT

    if(m==MAP_FAILED)
        throw std::runtime_error("Map failed with error " + std::to_string(errno));

    munmap(m, aligned_len);
}

int motivo_munmap(void *addr, size_t length)
{
    return munmap(addr, length);
}
