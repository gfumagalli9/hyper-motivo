// src/common/io/IEReader.h
// MIT License
// (c) 2025 — Adapter for Inclusion–Exclusion (IE) tables
// NOTE:
//  - The implementation lives in src/common/io/IEReader.cpp and includes:
//      #include "CompressedRecordFile.h"
//      #include "../treelets/TreeletTable.h"
//      #include "../treelets/TreeletTableCollection.h"
//  - This header stays lightweight thanks to forward declarations.

#ifndef MOTIVO_COMMON_IO_IEREADER_H
#define MOTIVO_COMMON_IO_IEREADER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <utility>   // std::pair
#include <random>    // std::uniform_real_distribution

// Forward declarations (defined in src/common/treelets/)
class Treelet;
class TreeletTable;
class TreeletTableCollection;

/**
 * IEReader — a thin adapter that centralizes naming/opening/prefault
 * for Inclusion–Exclusion (IE) tables and provides semantically clear APIs.
 *
 * V1 (minimal):
 *  - "Owned" ctor: opens IE tables for size = 1..max_size from "<ie_base>.<i>.ie.dtz"
 *                  and prefaults the vertex range [from_vertex..to_vertex].
 *  - "Wrapper" ctor: wraps an existing TreeletTableCollection (non-owning).
 *  - table(size2):    returns the IE table for that size (1-based).
 *  - collection():    access to the underlying collection (for legacy code paths).
 *  - union_weight():  alias to get_count(u, T2_colored) in the IE table of |T2|.
 *
 * Extended APIs for sampling:
 *  - get_count / try_get_count / sum_all
 *  - mass_root / mass_step (readable aliases of get_count)
 *  - for_each(u): iterate all (Treelet, count) pairs for a vertex u
 *  - gather_counts(vs): batch counts for a fixed Treelet across many vertices
 *  - sample_by_counts(begin,end): weighted sampling over a candidate set (header-only)
 */
class IEReader {
public:
  // Ctor that opens and owns IE tables (sizes 1..max_size)
  IEReader(const std::string& ie_base,
           unsigned           max_size,
           std::uint32_t      from_vertex = 0,
           std::uint32_t      to_vertex   = 0xFFFFFFFFu);

  // "Wrapper" ctor over an already-loaded collection (non-owning)
  explicit IEReader(const TreeletTableCollection* existing);

  // Non-copyable; movable
  IEReader(const IEReader&)            = delete;
  IEReader& operator=(const IEReader&) = delete;
  IEReader(IEReader&&)                 noexcept = default;
  IEReader& operator=(IEReader&&)      noexcept = default;

  ~IEReader();

  /** IE table for a given size (1..max_size). size2 is 1-based. */
  TreeletTable& table(unsigned size2) const;

  /** Access to the underlying collection (useful for builder/legacy). */
  TreeletTableCollection& collection() const;

  /**
   * Union weight of "HIGH" neighbors for (u, T2_colored) where size2 = |T2|.
   * Alias of table(size2).get_count(u, T2_colored).
   */
  std::uint64_t union_weight(unsigned size2,
                             const Treelet& T2_colored,
                             std::uint32_t u) const;

  // -------------------- Extended APIs (sampling-friendly) --------------------

  /** Return C(T_colored, u) from IE table of given size. */
  std::uint64_t get_count(unsigned size,
                          const Treelet& T_colored,
                          std::uint32_t u) const;

  /**
   * Try to read C(T_colored, u). Returns false if not present (typical IE tables
   * omit zero-count entries, so 0 usually means "absent"). On success writes 'out'.
   */
  bool try_get_count(unsigned size,
                     const Treelet& T_colored,
                     std::uint32_t u,
                     std::uint64_t& out) const;

  /** Sum of all counts for vertex u in the IE table of given size. */
  std::uint64_t sum_all(unsigned size, std::uint32_t u) const;

  /**
   * Iterate all (Treelet, count) entries for vertex u in IE table of given size.
   * The callback is invoked as: fn(const Treelet& tre, std::uint64_t count).
   */
  void for_each(unsigned size, std::uint32_t u,
                const std::function<void(const Treelet&, std::uint64_t)>& fn) const;

  /**
   * Batch: out_counts[i] = get_count(size, T_colored, vs[i]).
   * 'out_counts' is cleared and resized appropriately.
   */
  void gather_counts(unsigned size,
                     const Treelet& T_colored,
                     const std::vector<std::uint32_t>& vs,
                     std::vector<std::uint64_t>& out_counts) const;

  // Readable aliases (paper notation)
  inline std::uint64_t mass_root(unsigned size1,
                                 const Treelet& T1_colored,
                                 std::uint32_t u) const {
    return get_count(size1, T1_colored, u);
  }
  inline std::uint64_t mass_step(unsigned size2,
                                 const Treelet& T2_colored,
                                 std::uint32_t u) const {
    return get_count(size2, T2_colored, u);
  }

  /**
   * Header-only helper: sample a vertex from a candidate range [begin,end),
   * with weight w(v) = get_count(size, T_colored, v). Returns
   *   <selected_vertex, selection_probability>
   * If all weights are zero, returns {0xFFFFFFFFu, 0.0}.
   *
   * Iter must dereference to std::uint32_t (vertex id). URNG is a uniform RNG.
   */
  template <class Iter, class URNG>
  std::pair<std::uint32_t, double>
  sample_by_counts(unsigned size, const Treelet& T_colored,
                   Iter begin, Iter end, URNG& rng) const {
    long double total = 0.0L;
    for (auto it = begin; it != end; ++it) {
      total += static_cast<long double>(get_count(size, T_colored, *it));
    }
    if (total <= 0.0L) {
      return {0xFFFFFFFFu, 0.0};
    }
    // non-allocating linear scan
    std::uniform_real_distribution<long double> U(0.0L, total);
    long double r = U(rng), acc = 0.0L;
    std::uint32_t chosen = 0xFFFFFFFFu;
    long double w_chosen = 0.0L;
    for (auto it = begin; it != end; ++it) {
      const long double w = static_cast<long double>(get_count(size, T_colored, *it));
      if (w <= 0.0L) continue;
      acc += w;
      if (r <= acc) { chosen = *it; w_chosen = w; break; }
    }
    if (chosen == 0xFFFFFFFFu) { // numeric corner: pick last positive
      for (auto it = end; it != begin;) {
        --it;
        const long double w = static_cast<long double>(get_count(size, T_colored, *it));
        if (w > 0.0L) { chosen = *it; w_chosen = w; break; }
      }
    }
    const double p = static_cast<double>(w_chosen / total);
    return {chosen, p};
  }

private:
  // State for "owned" opening
  std::string   base_;
  unsigned      max_size_ = 0;
  std::uint32_t from_ = 0, to_ = 0;

  // PIMPL to hide concrete Reader/Table/Collection deps from the header
  class Impl;
  std::unique_ptr<Impl> impl_;

  // State for "wrapper" mode (non-owning)
  const TreeletTableCollection* ttc_ = nullptr;

private:
  void ensure_initialized_() const; // throws if neither owned nor wrapper
};

#endif // MOTIVO_COMMON_IO_IEREADER_H