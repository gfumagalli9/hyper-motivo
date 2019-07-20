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
