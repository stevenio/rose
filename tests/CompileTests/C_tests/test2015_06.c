
// #include <bits/types.h>
#include <stdint.h>

// DQ (3/15/2015): Note these types were added to the rose_edg_required_macros_and_functions.h.in file
// but they should have been defined in EDG directly.
// typedef long long int __int128_t;
// typedef unsigned long long int __uint128_t;

// Example code from Xen
static inline void mulu64(uint64_t *plow, uint64_t *phigh,
                          uint64_t a, uint64_t b)
{
    __uint128_t r = (__uint128_t)a * b;
    *plow = r;
    *phigh = r >> 64;
}
