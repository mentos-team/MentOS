/// @file port_io.h
/// @brief Byte I/O on ports prototypes.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Reads a 8-bit value from the given port.
/// @param port the port we want to read from.
/// @return the value we read.
static inline unsigned char inportb(unsigned short port)
{
    unsigned char result;
    __asm__ __volatile__("inb %%dx, %%al"
                         : "=a"(result)
                         : "dN"(port)
                         : "memory");
    return result;
}

/// @brief Reads a 16-bit value from the given port.
/// @param port the port we want to read from.
/// @return the value we read.
static inline unsigned short inports(unsigned short port)
{
    unsigned short result;
    __asm__ __volatile__("inw %1, %0"
                         : "=a"(result)
                         : "dN"(port)
                         : "memory");
    return result;
}

/// @brief Reads a 32-bit value from the given port.
/// @param port the port we want to read from.
/// @return the value we read.
static inline unsigned int inportl(unsigned short port)
{
    unsigned int result;
    __asm__ __volatile__("inl %%dx, %%eax"
                         : "=a"(result)
                         : "dN"(port)
                         : "memory");
    return result;
}

/// @brief Writes a 8-bit value at the given port.
/// @param port the port we want to write to.
/// @param value the value we want to write.
static inline void outportb(unsigned short port, unsigned char value)
{
    __asm__ __volatile__("outb %%al, %%dx"
                         :
                         : "a"(value), "dN"(port)
                         : "memory");
}

/// @brief Writes a 16-bit value at the given port.
/// @param port the port we want to write to.
/// @param value the value we want to write.
static inline void outports(unsigned short port, unsigned short value)
{
    __asm__ __volatile__("outw %1, %0"
                         :
                         : "dN"(port), "a"(value)
                         : "memory");
}

/// @brief Writes a 32-bit value at the given port.
/// @param port the port we want to write to.
/// @param value the value we want to write.
static inline void outportl(unsigned short port, unsigned int value)
{
    __asm__ __volatile__("outl %%eax, %%dx"
                         :
                         : "dN"(port), "a"(value)
                         : "memory");
}

/// @brief Reads multiple 8-bit values from the given port.
/// @param port the port we want to read from.
/// @param addr the location where we store the values we read.
/// @param count the number of values we want to read.
static inline void inportsb(unsigned short port, void *addr, unsigned long count)
{
    __asm__ __volatile__(
        "cld ; rep ; insb "
        : "=D"(addr), "=c"(count)
        : "d"(port), "0"(addr), "1"(count));
}

/// @brief Reads multiple 16-bit values from the given port.
/// @param port the port we want to read from.
/// @param addr the location where we store the values we read.
/// @param count the number of values we want to read.
static inline void inportsw(unsigned short port, void *addr, unsigned long count)
{
    __asm__ __volatile__(
        "cld ; rep ; insw "
        : "=D"(addr), "=c"(count)
        : "d"(port), "0"(addr), "1"(count));
}

/// @brief Reads multiple 32-bit values from the given port.
/// @param port the port we want to read from.
/// @param addr the location where we store the values we read.
/// @param count the number of values we want to read.
static inline void inportsl(unsigned short port, void *addr, unsigned long count)
{
    __asm__ __volatile__(
        "cld ; rep ; insl "
        : "=D"(addr), "=c"(count)
        : "d"(port), "0"(addr), "1"(count));
}

/// @brief Writes multiple 8-bit values to the given port.
/// @param port the port we want to write to.
/// @param addr the location where we get the values we need to write.
/// @param count the number of values we want to write.
static inline void outportsb(unsigned short port, void *addr, unsigned long count)
{
    __asm__ __volatile__(
        "cld ; rep ; outsb "
        : "=S"(addr), "=c"(count)
        : "d"(port), "0"(addr), "1"(count));
}

/// @brief Writes multiple 16-bit values to the given port.
/// @param port the port we want to write to.
/// @param addr the location where we get the values we need to write.
/// @param count the number of values we want to write.
static inline void outportsw(unsigned short port, void *addr, unsigned long count)
{
    __asm__ __volatile__(
        "cld ; rep ; outsw "
        : "=S"(addr), "=c"(count)
        : "d"(port), "0"(addr), "1"(count));
}

/// @brief Writes multiple 32-bit values to the given port.
/// @param port the port we want to write to.
/// @param addr the location where we get the values we need to write.
/// @param count the number of values we want to write.
static inline void outportsl(unsigned short port, void *addr, unsigned long count)
{
    __asm__ __volatile__(
        "cld ; rep ; outsl "
        : "=S"(addr), "=c"(count)
        : "d"(port), "0"(addr), "1"(count));
}
