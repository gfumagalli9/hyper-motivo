// TreeletList.h
#ifndef MOTIVO_TREELETLIST_H
#define MOTIVO_TREELETLIST_H

#include <cstdint>
#include "Treelet.h"
#include "../io/BaseRecordSource.h"

/// \class TreeletList
/// \brief Iteratore su una sequenza di Treelet letti da BaseRecordSource<Treelet>
class TreeletList {
public:
    class iterator;

    /// Costruisce la lista da un source (ownership non trasferita)
    explicit TreeletList(BaseRecordSource<const Treelet>* source) noexcept;
    ~TreeletList();

    /// Ritorna iterator begin/end
    iterator begin() const noexcept;
    iterator end() const noexcept;

    Treelet at(uint64_t idx) const {
        if (idx >= num_records_) throw std::out_of_range("TreeletList::at: index out of range");
        auto rec = source_->get_record(idx);
        Treelet t = rec.length() > 0 ? rec.begin()[0] : invalid_treelet;
        rec.free();
        return t;
    }

    /// Numero di Treelet (num_records)
    uint64_t size() const noexcept;

private:
    BaseRecordSource<const Treelet>* source_;
    uint64_t num_records_;
};

// Nested iterator
class TreeletList::iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = Treelet;
    using reference         = const Treelet&;
    using pointer           = const Treelet*;
    using difference_type   = std::ptrdiff_t;

    iterator() noexcept;
    iterator(const TreeletList* list, uint64_t index) noexcept;

    reference operator*() const noexcept;
    iterator& operator++() noexcept;
    iterator  operator++(int) noexcept;

    bool operator==(const iterator& other) const noexcept;
    bool operator!=(const iterator& other) const noexcept;

private:
    const TreeletList* list_;
    uint64_t index_;
    Treelet current_;
};

#endif // MOTIVO_TREELETLIST_H