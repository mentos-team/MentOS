/// @file sha256.c
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

#include "crypt/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// @brief Rotate left operation on a 32-bit unsigned integer.
/// @param a The value to rotate.
/// @param b The number of positions to rotate.
/// @return The rotated value.
#define ROTLEFT(a, b) (((a) << (b)) | ((a) >> (32 - (b))))

/// @brief Rotate right operation on a 32-bit unsigned integer.
/// @param a The value to rotate.
/// @param b The number of positions to rotate.
/// @return The rotated value.
#define ROTRIGHT(a, b) (((a) >> (b)) | ((a) << (32 - (b))))

/// @brief Chooses bits from y if x is set, otherwise from z.
/// @param x, y, z Input values.
/// @return Result of CH function.
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))

/// @brief Majority function used in SHA-256.
/// @param x, y, z Input values.
/// @return Result of the majority function.
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/// @brief First expansion function for the working variables.
/// @param x Input value.
/// @return Result of EP0.
#define EP0(x) (ROTRIGHT(x, 2) ^ ROTRIGHT(x, 13) ^ ROTRIGHT(x, 22))

/// @brief Second expansion function for the working variables.
/// @param x Input value.
/// @return Result of EP1.
#define EP1(x) (ROTRIGHT(x, 6) ^ ROTRIGHT(x, 11) ^ ROTRIGHT(x, 25))

/// @brief First Sigma function for message scheduling.
/// @param x Input value.
/// @return Result of SIG0.
#define SIG0(x) (ROTRIGHT(x, 7) ^ ROTRIGHT(x, 18) ^ ((x) >> 3))

/// @brief Second Sigma function for message scheduling.
/// @param x Input value.
/// @return Result of SIG1.
#define SIG1(x) (ROTRIGHT(x, 17) ^ ROTRIGHT(x, 19) ^ ((x) >> 10))

/// Max data length for message scheduling expansion.
#define SHA256_MAX_DATA_LENGTH (SHA256_BLOCK_SIZE * 2)

/// @brief The constants used in the SHA-256 algorithm, as defined by the
/// specification. These are the first 32 bits of the fractional parts of the
/// cube roots of the first 64 primes (2..311).
static const uint32_t k[SHA256_MAX_DATA_LENGTH] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

/// @brief Transforms the state of the SHA-256 context based on a block of input data.
/// @param ctx Pointer to the SHA-256 context. Must not be NULL.
/// @param data Input data block to process (64 bytes). Must not be NULL.
static inline void __sha256_transform(SHA256_ctx_t *ctx, const uint8_t data[])
{
    // Error checks: Ensure the input parameters are not NULL.
    if (!ctx) {
        perror("SHA256 context is NULL.\n");
        return;
    }
    if (!data) {
        perror("Input data is NULL.\n");
        return;
    }

    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    uint32_t i;
    uint32_t j;
    uint32_t t1;
    uint32_t t2;
    uint32_t m[SHA256_MAX_DATA_LENGTH]; // Message schedule array.

    // Step 1: Prepare the message schedule (first 16 words are directly from
    // the input data).
    for (i = 0, j = 0; i < 16; ++i, j += 4) {
        // Each 32-bit word is constructed from 4 consecutive 8-bit bytes from
        // the input data.
        m[i] = (uint32_t)(data[j] << 24) | (uint32_t)(data[j + 1] << 16) | (uint32_t)(data[j + 2] << 8) |
               (uint32_t)(data[j + 3]);
    }

    // Step 2: Extend the first 16 words into the remaining 48 words of the
    // message schedule. Each word is computed based on the previous words using
    // the SIG1 and SIG0 functions.
    for (; i < SHA256_MAX_DATA_LENGTH; ++i) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    // Step 3: Initialize the working variables with the current state values.
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    // Step 4: Perform the main hash computation (64 rounds).
    for (i = 0; i < SHA256_MAX_DATA_LENGTH; ++i) {
        // Calculate the temporary values.
        t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);

        // Rotate the working variables for the next round.
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    // Step 5: Add the resulting values back into the current state.
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void sha256_bytes_to_hex(uint8_t *src, size_t src_length, char *out, size_t out_length)
{
    // Check if the output buffer is large enough to hold the hex string
    if (out_length < (src_length * 2 + 1)) {
        perror("Output buffer is too small for the hex representation.\n");
        return; // Return early if the buffer is insufficient.
    }

    // Lookup table for converting bytes to hex
    static const char look_up[] = "0123456789abcdef";

    // Convert each byte to its hex representation
    for (size_t i = 0; i < src_length; ++i) {
        *out++ = look_up[*src >> 4];   // Upper nibble
        *out++ = look_up[*src & 0x0F]; // Lower nibble
        src++;
    }
    *out = 0; // Null-terminate the output string
}

void sha256_init(SHA256_ctx_t *ctx)
{
    // Error check: Ensure the input context is not NULL.
    if (!ctx) {
        perror("SHA256 context is NULL.\n");
        return; // Return early if the context is NULL to prevent crashes.
    }

    // Initialize the length of the data (in bytes) to 0.
    ctx->datalen = 0;

    // Initialize the total length of the input data (in bits) to 0.
    ctx->bitlen = 0;

    // Initialize the state variables (hash values) to the SHA-256 initial constants.
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

void sha256_update(SHA256_ctx_t *ctx, const uint8_t data[], size_t len)
{
    // Error check: Ensure the input context and data are not NULL.
    if (!ctx) {
        perror("SHA256 context is NULL.\n");
        return; // Return early if the context is NULL to prevent crashes.
    }

    if (!data) {
        perror("Input data is NULL.\n");
        return; // Return early if the data is NULL to prevent errors.
    }

    // Iterate over the input data, processing it in chunks.
    for (uint32_t i = 0; i < len; ++i) {
        // Add data to the context's buffer.
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;

        // If the buffer is full, process the data and reset the buffer.
        if (ctx->datalen == SHA256_MAX_DATA_LENGTH) {
            // Perform the SHA-256 transformation on the full data block.
            __sha256_transform(ctx, ctx->data);

            // Update the total length of the data processed so far in bits.
            ctx->bitlen += 512;

            // Reset the buffer length to 0 for the next chunk of data.
            ctx->datalen = 0;
        }
    }
}

void sha256_final(SHA256_ctx_t *ctx, uint8_t hash[])
{
    // Error check: Ensure the input context and hash are not NULL.
    if (!ctx) {
        perror("SHA256 context is NULL.\n");
        return; // Return early if the context is NULL to prevent crashes.
    }
    if (!hash) {
        perror("Output hash buffer is NULL.\n");
        return; // Return early if the output buffer is NULL to prevent errors.
    }

    // Get the current length of the data buffer.
    uint32_t i = ctx->datalen;

    // Step 1: Pad whatever data is left in the buffer.

    // If there's enough space in the buffer (less than 56 bytes used), pad
    // with 0x80 followed by zeros.
    if (ctx->datalen < 56) {
        // Append the padding byte (0x80).
        ctx->data[i++] = 0x80;

        // Pad the buffer with zeros until we reach 56 bytes.
        while (i < 56) {
            ctx->data[i++] = 0x00;
        }
    }
    // If there's not enough space, pad the remaining buffer and process it.
    else {
        // Append the padding byte (0x80).
        ctx->data[i++] = 0x80;

        // Fill the rest of the buffer with zeros.
        while (i < SHA256_MAX_DATA_LENGTH) {
            ctx->data[i++] = 0x00;
        }

        // Process the full buffer.
        __sha256_transform(ctx, ctx->data);

        // Reset the buffer for the next padding.
        memset(ctx->data, 0, 56);
    }

    // Step 2: Append the total message length in bits and process the final block.

    // Convert the total length from bytes to bits.
    ctx->bitlen += ctx->datalen * 8;

    // Append the lower 8 bits of the length.
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;

    // Process the final block.
    __sha256_transform(ctx, ctx->data);

    // Step 3: Copy the final state (hash) to the output buffer.

    // SHA-256 uses big-endian byte order, so we reverse the byte order when
    // copying.
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}
