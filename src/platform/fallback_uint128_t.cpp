//This is a modified version of uint128_t.cpp by Jason Lee. See header file for copyright notice.

#include "fallback_uint128_t.h"

const fallback_uint128_t fallback_uint128_0(0u);
const fallback_uint128_t fallback_uint128_1(1u);

fallback_uint128_t::fallback_uint128_t()
        : LOWER(0u), UPPER(0u)
{}

fallback_uint128_t::fallback_uint128_t(const fallback_uint128_t & rhs)
        : LOWER(rhs.LOWER), UPPER(rhs.UPPER)
{}

fallback_uint128_t::fallback_uint128_t(const fallback_uint128_t && rhs)
        : LOWER(std::move(rhs.LOWER)), UPPER(std::move(rhs.UPPER))
{}

fallback_uint128_t fallback_uint128_t::operator=(const fallback_uint128_t & rhs){
    UPPER = rhs.UPPER;
    LOWER = rhs.LOWER;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator=(const fallback_uint128_t && rhs){
    UPPER = std::move(rhs.UPPER);
    LOWER = std::move(rhs.LOWER);
    return *this;
}

fallback_uint128_t::operator bool() const{
    return static_cast<bool>(UPPER | LOWER);
}

fallback_uint128_t::operator char() const{
    return static_cast<char>(LOWER);
}
fallback_uint128_t::operator int() const{
    return static_cast<int>(LOWER);
}

fallback_uint128_t::operator uint8_t() const{
    return static_cast<uint8_t>(LOWER);
}

fallback_uint128_t::operator uint16_t() const{
    return static_cast<uint16_t>(LOWER);
}

fallback_uint128_t::operator uint32_t() const{
    return static_cast<uint32_t>(LOWER);
}

fallback_uint128_t::operator uint64_t() const{
    return static_cast<uint64_t>(LOWER);
}

fallback_uint128_t fallback_uint128_t::operator&(const fallback_uint128_t & rhs) const{
    return fallback_uint128_t(UPPER & rhs.UPPER, LOWER & rhs.LOWER);
}

fallback_uint128_t fallback_uint128_t::operator|(const fallback_uint128_t & rhs) const{
    return fallback_uint128_t(UPPER | rhs.UPPER, LOWER | rhs.LOWER);
}

fallback_uint128_t fallback_uint128_t::operator^(const fallback_uint128_t & rhs) const{
    return fallback_uint128_t(UPPER ^ rhs.UPPER, LOWER ^ rhs.LOWER);
}

fallback_uint128_t fallback_uint128_t::operator&=(const fallback_uint128_t & rhs){
    UPPER &= rhs.UPPER;
    LOWER &= rhs.LOWER;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator|=(const fallback_uint128_t & rhs){
    UPPER |= rhs.UPPER;
    LOWER |= rhs.LOWER;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator^=(const fallback_uint128_t & rhs){
    UPPER ^= rhs.UPPER;
    LOWER ^= rhs.LOWER;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator~() const{
    return fallback_uint128_t(~UPPER, ~LOWER);
}

fallback_uint128_t fallback_uint128_t::operator<<(const fallback_uint128_t & rhs) const{
    uint64_t shift = rhs.LOWER;
    if (static_cast<bool>(rhs.UPPER) || (shift >= 128)){
        return fallback_uint128_0;
    }
    else if (shift == 64){
        return fallback_uint128_t(LOWER, 0u);
    }
    else if (shift == 0){
        return *this;
    }
    else if (shift < 64){
        return fallback_uint128_t((UPPER << shift) + (LOWER >> (64 - shift)), LOWER << shift);
    }
    else if ((128 > shift) && (shift > 64)){
        return fallback_uint128_t(LOWER << (shift - 64), 0u);
    }
    else{
        return fallback_uint128_0;
    }
}

fallback_uint128_t fallback_uint128_t::operator>>(const fallback_uint128_t & rhs) const{
    uint64_t shift = rhs.LOWER;
    if (static_cast<bool>(rhs.UPPER) || (shift >= 128)){
        return fallback_uint128_0;
    }
    else if (shift == 64){
        return fallback_uint128_t(0u, UPPER);
    }
    else if (shift == 0){
        return *this;
    }
    else if (shift < 64){
        return fallback_uint128_t(UPPER >> shift, (UPPER << (64 - shift)) + (LOWER >> shift));
    }
    else if ((128 > shift) && (shift > 64)){
        return fallback_uint128_t(0u, (UPPER >> (shift - 64)));
    }
    else{
        return fallback_uint128_0;
    }
}

fallback_uint128_t fallback_uint128_t::operator<<=(const fallback_uint128_t & rhs){
    *this = *this << rhs;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator>>=(const fallback_uint128_t & rhs){
    *this = *this >> rhs;
    return *this;
}

bool fallback_uint128_t::operator!() const{
    return !static_cast<bool>(UPPER | LOWER);
}

bool fallback_uint128_t::operator&&(const fallback_uint128_t & rhs) const{
    return *this && rhs;
}

bool fallback_uint128_t::operator||(const fallback_uint128_t & rhs) const{
    return *this || rhs;
}

bool fallback_uint128_t::operator==(const fallback_uint128_t & rhs) const{
    return ((UPPER == rhs.UPPER) && (LOWER == rhs.LOWER));
}

bool fallback_uint128_t::operator!=(const fallback_uint128_t & rhs) const{
    return ((UPPER != rhs.UPPER) | (LOWER != rhs.LOWER));
}

bool fallback_uint128_t::operator>(const fallback_uint128_t & rhs) const{
    if (UPPER == rhs.UPPER){
        return (LOWER > rhs.LOWER);
    }
    return (UPPER > rhs.UPPER);
}

bool fallback_uint128_t::operator<(const fallback_uint128_t & rhs) const{
    if (UPPER == rhs.UPPER){
        return (LOWER < rhs.LOWER);
    }
    return (UPPER < rhs.UPPER);
}

bool fallback_uint128_t::operator>=(const fallback_uint128_t & rhs) const{
    return ((*this > rhs) | (*this == rhs));
}

bool fallback_uint128_t::operator<=(const fallback_uint128_t & rhs) const{
    return ((*this < rhs) | (*this == rhs));
}

fallback_uint128_t fallback_uint128_t::operator+(const fallback_uint128_t & rhs) const{
    return fallback_uint128_t(UPPER + rhs.UPPER + ((LOWER + rhs.LOWER) < LOWER), LOWER + rhs.LOWER);
}

fallback_uint128_t fallback_uint128_t::operator+=(const fallback_uint128_t & rhs){
    UPPER = rhs.UPPER + UPPER + ((LOWER + rhs.LOWER) < LOWER);
    LOWER += rhs.LOWER;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator-(const fallback_uint128_t & rhs) const{
    return fallback_uint128_t(UPPER - rhs.UPPER - ((LOWER - rhs.LOWER) > LOWER), LOWER - rhs.LOWER);
}

fallback_uint128_t fallback_uint128_t::operator-=(const fallback_uint128_t & rhs){
    *this = *this - rhs;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator*(const fallback_uint128_t & rhs) const{
    // split values into 4 32-bit parts
    uint64_t top[4] = {UPPER >> 32, UPPER & 0xffffffff, LOWER >> 32, LOWER & 0xffffffff};
    uint64_t bottom[4] = {rhs.UPPER >> 32, rhs.UPPER & 0xffffffff, rhs.LOWER >> 32, rhs.LOWER & 0xffffffff};
    uint64_t products[4][4];

    // multiply each component of the values
    for(int y = 3; y > -1; y--){
        for(int x = 3; x > -1; x--){
            products[3 - x][y] = top[x] * bottom[y];
        }
    }

    // first row
    uint64_t fourth32 = (products[0][3] & 0xffffffff);
    uint64_t third32  = (products[0][2] & 0xffffffff) + (products[0][3] >> 32);
    uint64_t second32 = (products[0][1] & 0xffffffff) + (products[0][2] >> 32);
    uint64_t first32  = (products[0][0] & 0xffffffff) + (products[0][1] >> 32);

    // second row
    third32  += (products[1][3] & 0xffffffff);
    second32 += (products[1][2] & 0xffffffff) + (products[1][3] >> 32);
    first32  += (products[1][1] & 0xffffffff) + (products[1][2] >> 32);

    // third row
    second32 += (products[2][3] & 0xffffffff);
    first32  += (products[2][2] & 0xffffffff) + (products[2][3] >> 32);

    // fourth row
    first32  += (products[3][3] & 0xffffffff);

    // combines the values, taking care of carry over
    return fallback_uint128_t(first32 << 32, 0u) + fallback_uint128_t(third32 >> 32, third32 << 32) + fallback_uint128_t(second32, 0u) + fallback_uint128_t(fourth32);
}

fallback_uint128_t fallback_uint128_t::operator*=(const fallback_uint128_t & rhs){
    *this = *this * rhs;
    return *this;
}

std::pair <fallback_uint128_t, fallback_uint128_t> fallback_uint128_t::divmod(const fallback_uint128_t & lhs, const fallback_uint128_t & rhs) const{
    // Save some calculations /////////////////////
    if (rhs == fallback_uint128_0){
        throw std::runtime_error("Error: division or modulus by 0");
    }
    else if (rhs == fallback_uint128_1){
        return std::pair <fallback_uint128_t, fallback_uint128_t> (lhs, fallback_uint128_0);
    }
    else if (lhs == rhs){
        return std::pair <fallback_uint128_t, fallback_uint128_t> (fallback_uint128_1, fallback_uint128_0);
    }
    else if ((lhs == fallback_uint128_0) || (lhs < rhs)){
        return std::pair <fallback_uint128_t, fallback_uint128_t> (fallback_uint128_0, lhs);
    }

    std::pair <fallback_uint128_t, fallback_uint128_t> qr(fallback_uint128_0, lhs);
    fallback_uint128_t copyd = rhs << (lhs.bits() - rhs.bits());
    fallback_uint128_t adder = fallback_uint128_1 << (lhs.bits() - rhs.bits());
    if (copyd > qr.second){
        copyd >>= fallback_uint128_1;
        adder >>= fallback_uint128_1;
    }
    while (qr.second >= rhs){
        if (qr.second >= copyd){
            qr.second -= copyd;
            qr.first |= adder;
        }
        copyd >>= fallback_uint128_1;
        adder >>= fallback_uint128_1;
    }
    return qr;
}

fallback_uint128_t fallback_uint128_t::operator/(const fallback_uint128_t & rhs) const{
    return divmod(*this, rhs).first;
}

fallback_uint128_t fallback_uint128_t::operator/=(const fallback_uint128_t & rhs){
    *this = *this / rhs;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator%(const fallback_uint128_t & rhs) const{
    return *this - (rhs * (*this / rhs));
}

fallback_uint128_t fallback_uint128_t::operator%=(const fallback_uint128_t & rhs){
    *this = *this % rhs;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator++(){
    *this += fallback_uint128_1;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator++(int){
    fallback_uint128_t temp(*this);
    ++*this;
    return temp;
}

fallback_uint128_t fallback_uint128_t::operator--(){
    *this -= fallback_uint128_1;
    return *this;
}

fallback_uint128_t fallback_uint128_t::operator--(int){
    fallback_uint128_t temp(*this);
    --*this;
    return temp;
}

const uint64_t & fallback_uint128_t::upper() const{
    return UPPER;
}

const uint64_t & fallback_uint128_t::lower() const{
    return LOWER;
}

uint8_t fallback_uint128_t::bits() const{
    uint8_t out = 0;
    if (UPPER){
        out = 64;
        uint64_t up = UPPER;
        while (up){
            up >>= 1;
            out++;
        }
    }
    else{
        uint64_t low = LOWER;
        while (low){
            low >>= 1;
            out++;
        }
    }
    return out;
}

std::string fallback_uint128_t::str(uint8_t base, const unsigned int & len) const{
    if ((base < 2) || (base > 16)){
        throw std::invalid_argument("Base must be in th range 2-16");
    }
    std::string out = "";
    if (!(*this)){
        out = "0";
    }
    else{
        std::pair <fallback_uint128_t, fallback_uint128_t> qr(*this, fallback_uint128_0);
        do{
            qr = divmod(qr.first, base);
            out = "0123456789abcdef"[static_cast<uint8_t>(qr.second)] + out;
        } while (qr.first);
    }
    if (out.size() < len){
        out = std::string(len - out.size(), '0') + out;
    }
    return out;
}

std::ostream & operator<<(std::ostream & stream, const fallback_uint128_t & rhs){
    if (stream.flags() & stream.oct){
        stream << rhs.str(8);
    }
    else if (stream.flags() & stream.dec){
        stream << rhs.str(10);
    }
    else if (stream.flags() & stream.hex){
        stream << rhs.str(16);
    }
    return stream;
}