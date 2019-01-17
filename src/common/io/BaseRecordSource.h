//
// Created by steven on 8/12/17.
//

#ifndef MOTIVO_BASERECORDSOURCE_H
#define MOTIVO_BASERECORDSOURCE_H

#include <cstdint>

template<typename T> class Record
{
    static_assert(std::is_trivially_destructible<T>::value, "Template argument is not trivially destructable");

private:
    const T* ptr;
    const uint64_t len;
    const char* free_ptr;

public:
    Record(const T* ptr, uint64_t len, const char* free_ptr) noexcept : ptr(ptr), len(len), free_ptr(free_ptr) {}

    uint64_t length() const noexcept { return len; }
    const T* begin() const noexcept { return ptr; }
    const T* end() const noexcept { return ptr+len; }
    void free() { delete[] free_ptr; free_ptr=nullptr; }
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
    virtual Record<T> get_record(uint64_t record_no) const = 0;
    virtual uint64_t number_of_records() const = 0;
    virtual ~BaseRecordSource() = default;
};

#endif //MOTIVO_BASERECORDSOURCE_H
