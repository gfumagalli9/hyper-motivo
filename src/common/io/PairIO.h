#pragma once
#include <string>
#include "../types/PairSet.h"

// Carica il file .pairs nel formato: [uint64_t M][(u,v)*M] (binario).
// Ritorna un PairSet con coppie CANONICALIZZATE (u < v) se canonicalize = true.
PairSet load_pairs_set(const std::string& filename, bool canonicalize = true);

// Come sopra ma inserisce in 'out' (append). Se clear_out = true, svuota prima 'out'.
void load_pairs_append(const std::string& filename, PairSet& out,
                       bool canonicalize = true, bool clear_out = true);

// (Opzionale) Salva un PairSet in formato .pairs (assume coppie già canoniche).
void save_pairs_set(const std::string& filename, const PairSet& pairs);