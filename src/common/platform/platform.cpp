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
#include <unistd.h>
#include <cerrno>
#include <string>
#include <stdexcept>
#include <cassert>

// Su macOS MAP_POPULATE non è definito: definiamolo a 0 per rendere il codice portabile.
#ifndef MAP_POPULATE
#  define MAP_POPULATE 0
#endif

/**
 * Esegue mmap con flag di "populate" se disponibili (Linux) altrimenti si risolve in MAP_PRIVATE.
 */
void* motivo_mmap_populate(size_t length, int prot, int fd)
{
    int flags = MAP_PRIVATE | MAP_POPULATE;
    return mmap(nullptr, length, prot, flags, fd, 0); // NOLINT
}

/**
 * Esegue mmap senza alcun flag di popolamento (semplice MAP_PRIVATE).
 */
void* motivo_mmap(size_t length, int prot, int fd)
{
    return mmap(nullptr, length, prot, MAP_PRIVATE, fd, 0);
}

/**
 * Prefault: mappa una regione in sola lettura e la immediatamente unmappa per riscaldare le pagine in memoria.
 * Su macOS MAP_POPULATE vale 0, quindi si comporta come un normale MAP_PRIVATE.
 */
void motivo_prefault(off_t off, size_t length, int fd)
{
    const long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size <= 0) {
        throw std::runtime_error("Impossibile ottenere la dimensione della pagina di memoria");
    }

    // Allineiamo l'offset al multiplo della dimensione di pagina
    off_t aligned_off = (off / page_size) * page_size;
    // La lunghezza aumentata per coprire l'eventuale frammento iniziale
    size_t aligned_len = length + static_cast<size_t>(off - aligned_off);

    int flags = MAP_PRIVATE | MAP_POPULATE;
    void* m = mmap(nullptr, aligned_len, PROT_READ, flags, fd, aligned_off); // NOLINT

    if (m == MAP_FAILED) {
        int err = errno; // salviamo errno prima di qualsiasi altra chiamata
        throw std::runtime_error(std::string("Map failed with error ") + std::to_string(err));
    }

    // Dopo aver toccato le pagine in memoria, smappiamo
    munmap(m, aligned_len);
}

/**
 * Smappa una regione di memoria precedentemente mappata con motivo_mmap*.
 */
int motivo_munmap(void* addr, size_t length)
{
    return munmap(addr, length);
}