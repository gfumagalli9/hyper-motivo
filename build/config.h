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

#ifndef MOTIVO_CONFIG_H
#define MOTIVO_CONFIG_H

#include "git_id.h"

#define MOTIVO_VERSION "0.1.1"
#define MOTIVO_BUILD_TYPE "RelWithDebInfo"
#define MOTIVO_FLAGS "ea om MA dh sf OS al"
#define MOTIVO_VERSION_STRING ( MOTIVO_VERSION " (" MOTIVO_GIT_ID " " MOTIVO_BUILD_TYPE " " MOTIVO_FLAGS ")" )

#define MOTIVO_COPYRIGHT_NOTICE "Copyright (c) 2017-2019 Stefano Leucci and Marco Bressan\n" \
"Distributed under the MIT Software License \n" \
"\n" \
"If you publish results based on Motivo, please acknowledge us by citing:\n" \
"M. Bressan, S. Leucci, A. Panconesi.\n" \
"Motivo: fast motif counting via succinct color coding and adaptive sampling.\n" \
"PVLDB, 12(11):1651-1663, 2019.\n" \
"DOI: https://doi.org/10.14778/3342263.3342640\n"

#define MOTIVO_HAS_BUILTIN_POPCOUNT 1
#define MOTIVO_HAS_BUILTIN_POPCOUNTL 1

#define MOTIVO_SHORT_SIZE 4
#define MOTIVO_INT_SIZE 4
#define MOTIVO_LONG_SIZE 8
#define MOTIVO_LONG_LONG_SIZE 8

#define MOTIVO_HAS_BUILTIN_ADD_OVERFLOW 1
#define MOTIVO_HAS_BUILTIN_MUL_OVERFLOW 1
#define MOTIVO_OVERFLOW_SAFE 1

#define MOTIVO_HAS_UINT64_T 1
/* #undef MOTIVO_HAS_UINT128_T */
#define MOTIVO_HAS___UINT128_T 1

/* #undef MOTIVO_DENSE_HASHMAP */

#define MOTIVO_MAY_ALIAS

/* #undef MOTIVO_STAR_SAMPLER_FLOATS */

#define MOTIVO_ARG_MAX 4096

#endif //MOTIVO_CONFIG_H
