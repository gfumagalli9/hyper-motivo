//
// Created by steven on 12/3/16.
//

#ifndef MOTIVO_INCLUDE_NAUTY_H
#define MOTIVO_INCLUDE_NAUTY_H

#if defined(HAVE_TLS) || defined(MAXN)
    #error defines clash
#endif

#define MAXN 16 //FIXME: Can we set it here?
#include <nauty/nauty.h>

#ifndef HAVE_TLS
    #error Nauty does not have multithreading support
#endif

static constexpr int MOTIVO_NAUTY_MAXN = MAXN;
static constexpr int MOTIVO_NAUTY_MAXM = MAXM;
static constexpr int MOTIVO_NAUTY_WORDSIZE = WORDSIZE;
static constexpr int MOTIVO_NAUTY_TRUE = TRUE;

typedef graph nauty_graph;
typedef set nauty_set;

#endif //MOTIVO_INCLUDE_NAUTY_H
