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
   * V1: alias to table(size2).get_count(u, T2_colored).
   */
  std::uint64_t union_weight(unsigned size2,
                             const Treelet& T2_colored,
                             std::uint32_t u) const;

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