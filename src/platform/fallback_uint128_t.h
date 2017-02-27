/*
This is a modified version of fallback_uint128_t.h by Jason Lee. Original Copyright notice follows.


An unsigned 128 bit integer type for C++
Copyright (c) 2013, 2014, 2016 Jason Lee @ calccrypto at gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

With much help from Auston Sterling

Thanks to Stefan Deigmüller for finding
a bug in operator*.

Thanks to François Dessenne for convincing me
to do a general rewrite of this class.
*/

#ifndef MOTIVO_FALLBACK_UINT128_T_H
#define MOTIVO_FALLBACK_UINT128_T_H


#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>

class fallback_uint128_t{
private:
    uint64_t UPPER, LOWER;

public:
    // Constructors
    fallback_uint128_t();
    fallback_uint128_t(const fallback_uint128_t & rhs);
    fallback_uint128_t(const fallback_uint128_t && rhs);

    template <typename T> fallback_uint128_t(const T & rhs) : UPPER(0u), LOWER(static_cast<typename std::make_unsigned<T>::type>(rhs))
    {}

    template <typename S, typename T> fallback_uint128_t(const S & upper_rhs, const T & lower_rhs)
            : UPPER(upper_rhs), LOWER(lower_rhs)
    {}

    //  RHS input args only

    // Assignment Operator
    fallback_uint128_t operator=(const fallback_uint128_t & rhs);
    fallback_uint128_t operator=(const fallback_uint128_t && rhs);

    template <typename T> fallback_uint128_t operator=(const T & rhs){
        UPPER = 0;
        LOWER = rhs;
        return *this;
    }

    // Typecast Operators
    operator bool() const;
    operator char() const;
    operator int() const;
    operator uint8_t() const;
    operator uint16_t() const;
    operator uint32_t() const;
    operator uint64_t() const;

    // Bitwise Operators
    fallback_uint128_t operator&(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator|(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator^(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator&=(const fallback_uint128_t & rhs);
    fallback_uint128_t operator|=(const fallback_uint128_t & rhs);
    fallback_uint128_t operator^=(const fallback_uint128_t & rhs);
    fallback_uint128_t operator~() const;

    template <typename T> fallback_uint128_t operator&(const T & rhs) const{
        return fallback_uint128_t(0, LOWER & static_cast<uint64_t>(rhs));
    }

    template <typename T> fallback_uint128_t operator|(const T & rhs) const{
        return fallback_uint128_t(UPPER, LOWER | static_cast<uint64_t>(rhs));
    }

    template <typename T> fallback_uint128_t operator^(const T & rhs) const{
        return fallback_uint128_t(UPPER, LOWER ^ static_cast<uint64_t>(rhs));
    }

    template <typename T> fallback_uint128_t operator&=(const T & rhs){
        UPPER = 0;
        LOWER &= rhs;
        return *this;
    }

    template <typename T> fallback_uint128_t operator|=(const T & rhs){
        LOWER |= static_cast<uint64_t>(rhs);
        return *this;
    }

    template <typename T> fallback_uint128_t operator^=(const T & rhs){
        LOWER ^= static_cast<uint64_t>(rhs);
        return *this;
    }

    // Bit Shift Operators
    fallback_uint128_t operator<<(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator>>(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator<<=(const fallback_uint128_t & rhs);
    fallback_uint128_t operator>>=(const fallback_uint128_t & rhs);

    template <typename T>fallback_uint128_t operator<<(const T & rhs) const{
        return *this << fallback_uint128_t(rhs);
    }

    template <typename T>fallback_uint128_t operator>>(const T & rhs) const{
        return *this >> fallback_uint128_t(rhs);
    }

    template <typename T>fallback_uint128_t operator<<=(const T & rhs){
        *this = *this << fallback_uint128_t(rhs);
        return *this;
    }

    template <typename T>fallback_uint128_t operator>>=(const T & rhs){
        *this = *this >> fallback_uint128_t(rhs);
        return *this;
    }

    // Logical Operators
    bool operator!() const;
    bool operator&&(const fallback_uint128_t & rhs) const;
    bool operator||(const fallback_uint128_t & rhs) const;

    template <typename T> bool operator&&(const T & rhs){
        return *this && rhs;
    }

    template <typename T> bool operator||(const T & rhs){
        return *this || rhs;
    }

    // Comparison Operators
    bool operator==(const fallback_uint128_t & rhs) const;
    bool operator!=(const fallback_uint128_t & rhs) const;
    bool operator>(const fallback_uint128_t & rhs) const;
    bool operator<(const fallback_uint128_t & rhs) const;
    bool operator>=(const fallback_uint128_t & rhs) const;
    bool operator<=(const fallback_uint128_t & rhs) const;

    template <typename T> bool operator==(const T & rhs) const{
        return (!UPPER && (LOWER == static_cast<uint64_t>(rhs)));
    }

    template <typename T> bool operator!=(const T & rhs) const{
        return static_cast<bool>(UPPER | (LOWER != static_cast<uint64_t>(rhs)));
    }

    template <typename T> bool operator>(const T & rhs) const{
        return (UPPER || (LOWER > static_cast<uint64_t>(rhs)));
    }

    template <typename T> bool operator<(const T & rhs) const{
        return (!UPPER)?(LOWER < static_cast<uint64_t>(rhs)):false;
    }

    template <typename T> bool operator>=(const T & rhs) const{
        return ((*this > rhs) | (*this == rhs));
    }

    template <typename T> bool operator<=(const T & rhs) const{
        return ((*this < rhs) | (*this == rhs));
    }

    // Arithmetic Operators
    fallback_uint128_t operator+(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator+=(const fallback_uint128_t & rhs);
    fallback_uint128_t operator-(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator-=(const fallback_uint128_t & rhs);
    fallback_uint128_t operator*(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator*=(const fallback_uint128_t & rhs);

private:
    std::pair <fallback_uint128_t, fallback_uint128_t> divmod(const fallback_uint128_t & lhs, const fallback_uint128_t & rhs) const;

public:
    fallback_uint128_t operator/(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator/=(const fallback_uint128_t & rhs);
    fallback_uint128_t operator%(const fallback_uint128_t & rhs) const;
    fallback_uint128_t operator%=(const fallback_uint128_t & rhs);

    template <typename T> fallback_uint128_t operator+(const T & rhs) const{
        return fallback_uint128_t(UPPER + ((LOWER + static_cast<uint64_t>(rhs)) < LOWER), LOWER + static_cast<uint64_t>(rhs));
    }

    template <typename T> fallback_uint128_t operator+=(const T & rhs){
        UPPER = UPPER + ((LOWER + rhs) < LOWER);
        LOWER = LOWER + rhs;
        return *this;
    }

    template <typename T> fallback_uint128_t operator-(const T & rhs) const{
        return fallback_uint128_t(static_cast<uint64_t>(UPPER - ((LOWER - rhs) > LOWER)), static_cast<uint64_t>(LOWER - rhs));
    }

    template <typename T> fallback_uint128_t operator-=(const T & rhs){
        *this = *this - rhs;
        return *this;
    }

    template <typename T> fallback_uint128_t operator*(const T & rhs) const{
        return (*this) * (fallback_uint128_t(rhs));
    }

    template <typename T> fallback_uint128_t operator*=(const T & rhs){
        *this = *this * fallback_uint128_t(rhs);
        return *this;
    }

    template <typename T> fallback_uint128_t operator/(const T & rhs) const{
        return *this / fallback_uint128_t(rhs);
    }

    template <typename T> fallback_uint128_t operator/=(const T & rhs){
        *this = *this / fallback_uint128_t(rhs);
        return *this;
    }

    template <typename T> fallback_uint128_t operator%(const T & rhs) const{
        return *this - (rhs * (*this / rhs));
    }

    template <typename T> fallback_uint128_t operator%=(const T & rhs){
        *this = *this % fallback_uint128_t(rhs);
        return *this;
    }

    // Increment Operator
    fallback_uint128_t operator++();
    fallback_uint128_t operator++(int);

    // Decrement Operator
    fallback_uint128_t operator--();
    fallback_uint128_t operator--(int);

    // Get private values
    const uint64_t & upper() const;
    const uint64_t & lower() const;

    // Get bitsize of value
    uint8_t bits() const;

    // Get string representation of value
    std::string str(uint8_t base = 10, const unsigned int & len = 0) const;
};

// lhs type T as first arguemnt
// If the output is not a bool, casts to type T

// Bitwise Operators
template <typename T> T operator&(const T & lhs, const fallback_uint128_t & rhs){
    return static_cast<T>(lhs & static_cast<T>(rhs.lower()));
}

template <typename T> T operator|(const T & lhs, const fallback_uint128_t & rhs){
    return static_cast<T>(lhs | static_cast<T>(rhs.lower()));
}

template <typename T> T operator^(const T & lhs, const fallback_uint128_t & rhs){
    return static_cast<T>(lhs ^ static_cast<T>(rhs.lower()));
}

template <typename T> T operator&=(T & lhs, const fallback_uint128_t & rhs){
    lhs &= static_cast<T>(rhs.lower()); return lhs;
}

template <typename T> T operator|=(T & lhs, const fallback_uint128_t & rhs){
    lhs |= static_cast<T>(rhs.lower()); return lhs;
}

template <typename T> T operator^=(T & lhs, const fallback_uint128_t & rhs){
    lhs ^= static_cast<T>(rhs.lower()); return lhs;
}

// Comparison Operators
template <typename T> bool operator==(const T & lhs, const fallback_uint128_t & rhs){
    return !rhs.upper() && (static_cast<uint64_t>(lhs) == rhs.lower());
}

template <typename T> bool operator!=(const T & lhs, const fallback_uint128_t & rhs){
    return rhs.upper() || (static_cast<uint64_t>(lhs) != rhs.lower());
}

template <typename T> bool operator>(const T & lhs, const fallback_uint128_t & rhs){
    return (!rhs.upper()) && (static_cast<uint64_t>(lhs) > rhs.lower());
}

template <typename T> bool operator<(const T & lhs, const fallback_uint128_t & rhs){
    if (rhs.upper()){
        return true;
    }
    return static_cast<uint64_t>(lhs) < rhs.lower();
}

template <typename T> bool operator>=(const T & lhs, const fallback_uint128_t & rhs){
    if (rhs.upper()){
        return false;
    }
    return static_cast<uint64_t>(lhs) >= rhs.lower();
}

template <typename T> bool operator<=(const T & lhs, const fallback_uint128_t & rhs){
    if (rhs.upper()){
        return true;
    }
    return static_cast<uint64_t>(lhs) <= rhs.lower();
}

// Arithmetic Operators
template <typename T> T operator+(const T & lhs, const fallback_uint128_t & rhs){
    return static_cast<T>(rhs + lhs);
}

template <typename T> T & operator+=(T & lhs, const fallback_uint128_t & rhs){
    lhs = static_cast<T>(rhs + lhs);
    return lhs;
}

template <typename T> T operator-(const T & lhs, const fallback_uint128_t & rhs){
    return static_cast<T>(fallback_uint128_t(lhs) - rhs);
}

template <typename T> T & operator-=(T & lhs, const fallback_uint128_t & rhs){
    lhs = static_cast<T>(fallback_uint128_t(lhs) - rhs);
    return lhs;
}

template <typename T> T operator*(const T & lhs, const fallback_uint128_t & rhs){
    return lhs * static_cast<T>(rhs.lower());
}

template <typename T> T & operator*=(T & lhs, const fallback_uint128_t & rhs){
    lhs *= static_cast<T>(rhs.lower());
    return lhs;
}

template <typename T> T operator/(const T & lhs, const fallback_uint128_t & rhs){
    return static_cast<T>(fallback_uint128_t(lhs) / rhs);
}

template <typename T> T & operator/=(T & lhs, const fallback_uint128_t & rhs){
    lhs = static_cast<T>(fallback_uint128_t(lhs) / rhs);
    return lhs;
}

template <typename T> T operator%(const T & lhs, const fallback_uint128_t & rhs){
    return static_cast<T>(fallback_uint128_t(lhs) % rhs);
}

template <typename T> T & operator%=(T & lhs, const fallback_uint128_t & rhs){
    lhs = static_cast<T>(fallback_uint128_t(lhs) % rhs);
    return lhs;
}

// IO Operator
std::ostream & operator<<(std::ostream & stream, const fallback_uint128_t & rhs);

#endif //MOTIVO_FALLBACK_UINT128_T_H
