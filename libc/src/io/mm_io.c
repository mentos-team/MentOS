/// @file mm_io.c
/// @brief Memory Mapped IO functions implementation.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "io/mm_io.h"

inline uint8_t in_memb(uint32_t addr)
{
    return *((uint8_t *) (addr));
}

inline uint16_t in_mems(uint32_t addr)
{
    return *((uint16_t *) (addr));
}

inline uint32_t in_meml(uint32_t addr)
{
    return *((uint32_t *) (addr));
}

inline void out_memb(uint32_t addr, uint8_t value)
{
    (*((uint8_t *) (addr))) = (value);
}

inline void out_mems(uint32_t addr, uint16_t value)
{
    (*((uint16_t *) (addr))) = (value);
}

inline void out_meml(uint32_t addr, uint32_t value)
{
    (*((uint32_t *) (addr))) = (value);
}
