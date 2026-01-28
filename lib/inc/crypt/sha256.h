/// @file sha256.h
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

#include <stddef.h>
#include <stdint.h>

/// @brief SHA256 outputs a 32 byte digest.
#define SHA256_BLOCK_SIZE 32

/// @brief Structure that holds context information for SHA-256 operations.
typedef struct {
    uint8_t data[64];          ///< Input data block being processed (512 bits / 64 bytes).
    uint32_t datalen;          ///< Length of the current data in the buffer (in bytes).
    unsigned long long bitlen; ///< Total length of the input in bits (for padding).
    uint32_t state[8];         ///< Current hash state (256 bits / 8 * 32-bit words).
} SHA256_ctx_t;

/// @brief Initializes the SHA-256 context.
/// @param ctx Pointer to the SHA-256 context to initialize.
void sha256_init(SHA256_ctx_t *ctx);

/// @brief Adds data to the SHA-256 context for hashing.
/// @param ctx Pointer to the SHA-256 context.
/// @param data Pointer to the data to be hashed.
/// @param len Length of the data to hash, in bytes.
void sha256_update(SHA256_ctx_t *ctx, const uint8_t data[], size_t len);

/// @brief Finalizes the hashing and produces the final SHA-256 digest.
/// @param ctx Pointer to the SHA-256 context.
/// @param hash Pointer to a buffer where the final hash will be stored (must be at least 32 bytes long).
void sha256_final(SHA256_ctx_t *ctx, uint8_t hash[]);

/// @brief Converts a byte array to its hexadecimal string representation.
/// @param src Pointer to the source byte array.
/// @param src_length Length of the source byte array.
/// @param out Pointer to the output buffer for the hexadecimal string.
/// @param out_length Length of the output buffer (must be at least 2 * src_length + 1).
/// @details The output string will be null-terminated if the buffer is large enough.
void sha256_bytes_to_hex(uint8_t *src, size_t src_length, char *out, size_t out_length);
