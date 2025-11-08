#pragma once
#include <string>
#include "../types/PairSet.h"

// Carica il file .pairs nel formato: [uint64_t M][(u,v)*M] (binario, u<v).
// Se il file non esiste, ritorna un PairSet vuoto.
PairSet load_pairs_set(const std::string& filename);

// (compat) append non più necessario: rimpiazza 'out' se esiste il file
inline void load_pairs_append(const std::string& filename, PairSet& out,
                              bool /*canonicalize*/ = true, bool /*clear_out*/ = true) {
    out = load_pairs_set(filename);
}

// (facoltativo) salva in formato .pairs canonico
void save_pairs_set(const std::string& filename, const PairSet& pairs);