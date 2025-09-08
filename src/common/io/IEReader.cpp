// src/common/io/IEReader.cpp
// MIT License
// (c) 2025 — Implementation of IEReader (Inclusion–Exclusion tables adapter)

#include "IEReader.h"

#include <stdexcept>
#include <vector>
#include <memory>
#include <sstream>

// Concrete deps live here (keep header light with forward decls)
#include "CompressedRecordFile.h"                 // src/common/io/
#include "../treelets/TreeletTable.h"             // src/common/treelets/
#include "../treelets/TreeletTableCollection.h"   // src/common/treelets/

// ---------------------- IEReader::Impl (owned mode) ----------------------
class IEReader::Impl {
public:
  using ReaderT = CompressedRecordFileReader<
      const TreeletTable::treelet_count_pair_maybe_alias,
      TreeletTable::may_alias>;

  explicit Impl(unsigned max_size)
    : readers_(max_size),
      tables_(max_size),
      collection_(new TreeletTableCollection()) {}

  void open_all(const std::string& base, unsigned max_size,
                std::uint32_t from, std::uint32_t to) {
    // Open IE tables for sizes 1..max_size from "<base>.<i>.ie.dtz"
    for (unsigned i = 0; i < max_size; ++i) {
      const std::string prefix = base + "." + std::to_string(i + 1) + ".ie";

      readers_[i] = std::make_unique<ReaderT>();
      readers_[i]->open(prefix + ".dtz");
      readers_[i]->prefault(from, to);

      tables_[i] = std::make_unique<TreeletTable>(readers_[i].get());
      collection_->add(tables_[i].get());  // non-owning inside collection
    }
  }

  TreeletTable& table(unsigned size2) const {
    // size2 is 1-based (1..max_size)
    if (size2 == 0 || size2 > readers_.size()) {
      std::ostringstream oss;
      oss << "IEReader: requested size2=" << size2
          << " out of range [1.." << readers_.size() << "]";
      throw std::out_of_range(oss.str());
    }
    return *tables_[size2 - 1];
  }

  TreeletTableCollection& collection() const {
    return *collection_;
  }

private:
  // Own: readers + tables + collection
  std::vector<std::unique_ptr<ReaderT>>       readers_;
  std::vector<std::unique_ptr<TreeletTable>>  tables_;
  std::unique_ptr<TreeletTableCollection>     collection_;
};

// ---------------------- IEReader (public) ----------------------

IEReader::IEReader(const std::string& ie_base,
                   unsigned           max_size,
                   std::uint32_t      from_vertex,
                   std::uint32_t      to_vertex)
  : base_(ie_base),
    max_size_(max_size),
    from_(from_vertex),
    to_(to_vertex),
    impl_(std::make_unique<Impl>(max_size)),
    ttc_(nullptr)
{
  impl_->open_all(base_, max_size_, from_, to_);
}

IEReader::IEReader(const TreeletTableCollection* existing)
  : base_(),
    max_size_(0),
    from_(0),
    to_(0),
    impl_(nullptr),
    ttc_(existing)
{}

IEReader::~IEReader() = default;

void IEReader::ensure_initialized_() const {
  if (!impl_ && !ttc_) {
    throw std::runtime_error("IEReader: not initialized (neither owned nor wrapper mode)");
  }
}

TreeletTable& IEReader::table(unsigned size2) const {
  ensure_initialized_();
  if (impl_) {
    return impl_->table(size2);
  }
  // Wrapper path (non-owning)
  if (size2 == 0) {
    throw std::out_of_range("IEReader: requested size2=0");
  }
  return *const_cast<TreeletTableCollection*>(ttc_)->get_table(size2);
}

TreeletTableCollection& IEReader::collection() const {
  ensure_initialized_();
  if (impl_) {
    return impl_->collection();
  }
  return *const_cast<TreeletTableCollection*>(ttc_);
}

std::uint64_t IEReader::union_weight(unsigned size2,
                                     const Treelet& T2_colored,
                                     std::uint32_t u) const {
  // V1: direct alias to the IE table count
  return table(size2).get_count(u, T2_colored);
}