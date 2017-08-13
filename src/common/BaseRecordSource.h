//
// Created by steven on 8/12/17.
//

#ifndef MOTIVO_BASERECORDSOURCE_H
#define MOTIVO_BASERECORDSOURCE_H

#include <cstdint>

template<typename T> class Record
{
private:
    const T* ptr;
    const uint64_t len;
    const char* free_ptr;

public:
    Record(const T* ptr, uint64_t len, const char* free_ptr) noexcept : ptr(ptr), len(len), free_ptr(free_ptr) {}

    uint64_t length() const noexcept { return len; }
    const T* begin() const noexcept { return ptr; }
    const T* end() const noexcept { return ptr+len; }
    void free() { if(free_ptr) delete[] free_ptr; free_ptr=nullptr; }
};

struct [[gnu::packed]] record_offset_t
{
    static constexpr uint64_t file_offset_mask = 0x0000FFFFFFFFFFFF;
    uint64_t file_offset : 48; //Max 256 TB
};

static_assert( sizeof(record_offset_t) == 6, "Structure record_offset_t is not packed." );

template<typename T> class BaseRecordSource
{
public:
    virtual Record<T> get_record(const uint64_t record_no) const = 0;
    virtual void prefault(const uint64_t from, const uint64_t to) = 0;
    virtual uint64_t number_of_records() = 0;
};

#endif //MOTIVO_BASERECORDSOURCE_H
