// TreeletList.cpp
#include "TreeletList.h"
#include <stdexcept>

TreeletList::TreeletList(BaseRecordSource<const Treelet>* source) noexcept
    : source_(source), num_records_(0)
{
    if (source_) num_records_ = source_->number_of_records();
}

TreeletList::~TreeletList() = default;

uint64_t TreeletList::size() const noexcept {
    return num_records_;
}

TreeletList::iterator TreeletList::begin() const noexcept {
    return iterator(this, 0);
}

TreeletList::iterator TreeletList::end() const noexcept {
    return iterator(this, num_records_);
}

// iterator implementation

TreeletList::iterator::iterator() noexcept
    : list_(nullptr), index_(0), current_(invalid_treelet)
{}

TreeletList::iterator::iterator(const TreeletList* list, uint64_t index) noexcept
    : list_(list), index_(index), current_(invalid_treelet)
{
    if (list_ && index_ < list_->num_records_) {
        auto rec = list_->source_->get_record(index_);
        // each record is a single Treelet
        if (rec.length() > 0) {
            current_ = rec.begin()[0];
        }
        rec.free();
    }
}

const TreeletList::iterator::reference TreeletList::iterator::operator*() const noexcept {
    return current_;
}

TreeletList::iterator& TreeletList::iterator::operator++() noexcept {
    if (!list_) return *this;
    ++index_;
    if (index_ < list_->num_records_) {
        auto rec = list_->source_->get_record(index_);
        if (rec.length() > 0) current_ = rec.begin()[0];
        else current_ = invalid_treelet;
        rec.free();
    } else {
        current_ = invalid_treelet;
    }
    return *this;
}

TreeletList::iterator TreeletList::iterator::operator++(int) noexcept {
    iterator tmp = *this;
    ++(*this);
    return tmp;
}

bool TreeletList::iterator::operator==(const iterator& other) const noexcept {
    return list_ == other.list_ && index_ == other.index_;
}

bool TreeletList::iterator::operator!=(const iterator& other) const noexcept {
    return !(*this == other);
}
