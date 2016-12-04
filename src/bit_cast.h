//
// Created by steven on 11/27/16.
//

#ifndef MOTIVO_BIT_CAST_H
#define MOTIVO_BIT_CAST_H

#include <cstring> // memcpy
#include <type_traits> // is_trivially_copyable

// From: https://chromium.googlesource.com/chromium/src.git/+/lkcr/base/bit_cast.h

// bit_cast<Dest,Source> is a template function that implements the
// equivalent of "*reinterpret_cast<Dest*>(&source)".  We need this in
// very low-level functions like the protobuf library and fast math
// support.
//
//   float f = 3.14159265358979;
//   int i = bit_cast<int32>(f);
//   // i = 0x40490fdb
//
// The classical address-casting method is:
//
//   // WRONG
//   float f = 3.14159265358979;            // WRONG
//   int i = * reinterpret_cast<int*>(&f);  // WRONG
//
// The address-casting method actually produces undefined behavior
// according to ISO C++ specification section 3.10 -15 -.  Roughly, this
// section says: if an object in memory has one type, and a program
// accesses it with a different type, then the result is undefined
// behavior for most values of "different type".
//
// This is true for any cast syntax, either *(int*)&f or
// *reinterpret_cast<int*>(&f).  And it is particularly true for
// conversions betweeen integral lvalues and floating-point lvalues.
//
// The purpose of 3.10 -15- is to allow optimizing compilers to assume
// that expressions with different types refer to different memory.  gcc
// 4.0.1 has an optimizer that takes advantage of this.  So a
// non-conforming program quietly produces wildly incorrect output.
//
// The problem is not the use of reinterpret_cast.  The problem is type
// punning: holding an object in memory of one type and reading its bits
// back using a different type.
//
// The C++ standard is more subtle and complex than this, but that
// is the basic idea.
//
// Anyways ...
//
// bit_cast<> calls memcpy() which is blessed by the standard,
// especially by the example in section 3.9 .  Also, of course,
// bit_cast<> wraps up the nasty logic in one place.
//
// Fortunately memcpy() is very fast.  In optimized mode, with a
// constant size, gcc 2.95.3, gcc 4.0.1, and msvc 7.1 produce inline
// code with the minimal amount of data movement.  On a 32-bit system,
// memcpy(d,s,4) compiles to one load and one store, and memcpy(d,s,8)
// compiles to two loads and two stores.
//
// I tested this code with gcc 2.95.3, gcc 4.0.1, icc 8.1, and msvc 7.1.


template <class Dest, class Source> inline Dest bit_cast(Source const &source)
{
    static_assert(sizeof(Dest)==sizeof(Source), "size of destination and source objects must be equal");

    //FIXME: Uncomment
    //static_assert(std::is_trivially_copyable<Dest>::value, "destination type must be trivially copyable.");
    //static_assert(std::is_trivially_copyable<Source>::value, "source type must be trivially copyable");

    Dest dest;
    std::memcpy(&dest, &source, sizeof(dest));
    return dest;
}

/*template <class Dest, class Source> inline Dest bit_cast(Source const &source)
{
    return *reinterpret_cast<Dest*>(const_cast<Source*>(&source));
}*/

#endif //MOTIVO_BIT_CAST_H
