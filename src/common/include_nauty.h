//
// Created by steven on 12/3/16.
//

#ifndef MOTIVO_INCLUDE_NAUTY_H
#define MOTIVO_INCLUDE_NAUTY_H

#if defined(HAVE_TLS) || defined(MAXN)
    #error defines clash
#endif

#include <nauty/nauty.h>

#if(!defined(HAVE_TLS) || HAVE_TLS==0)
    #error Nauty does not have multithreading support
#endif

#if(MAXN != 0)
    #error Nauty is compiled with static allocation
#endif

static constexpr int MOTIVO_NAUTY_WORDSIZE = WORDSIZE;
static constexpr int MOTIVO_NAUTY_TRUE = TRUE;

typedef graph nauty_graph;
typedef set nauty_set;

#endif //MOTIVO_INCLUDE_NAUTY_H
