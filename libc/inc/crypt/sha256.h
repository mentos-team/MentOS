/// @file sha256.c
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief Implementation of the SHA-256 hashing algorithm.
/// @details The original code was written by Brad Conte, and is available at:
///     https://github.com/B-Con/crypto-algorithms
/// 
/// SHA-256 is one of the three algorithms in the SHA2
/// specification. The others, SHA-384 and SHA-512, are not
/// offered in this implementation.
/// Algorithm specification can be found here:
///     http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf
/// This implementation uses little endian byte order.

#pragma once

#include <stdint.h>
#include <stddef.h>

/// @brief SHA256 outputs a 32 byte digest.
#define SHA256_BLOCK_SIZE 32

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    unsigned long long bitlen;
    uint32_t state[8];
} SHA256_ctx_t;

void sha256_init(SHA256_ctx_t *ctx);
void sha256_update(SHA256_ctx_t *ctx, const uint8_t data[], size_t len);
void sha256_final(SHA256_ctx_t *ctx, uint8_t hash[]);
void sha256_bytes_to_hex(uint8_t *src, size_t src_length, char *out, size_t out_length);
