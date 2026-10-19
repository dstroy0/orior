// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sha512.c: SHA-512 as FIPS 180-4 states it. The constants are section 4.2.3's and 5.3.5's, the functions section
// 4.1.3's, the padding section 5.1.2's and the compression section 6.4.2's
#include "sha512.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "sha512: a byte holds 8 bits");
_Static_assert(ULLONG_MAX >= 0xFFFFFFFFFFFFFFFFull, "sha512: an unsigned long long holds a 64-bit word");

// the words of the hash value, of the message schedule and of a block
#define SHA512_STATE_WORDS 8u
#define SHA512_SCHEDULE_WORDS 80u
#define SHA512_BLOCK_WORDS 16u
// the bytes the padding ends on, the message length in bits
#define SHA512_LENGTH_BYTES 16u

// the first 64 bits of the fractional parts of the cube roots of the first 80 primes (section 4.2.3)
static const unsigned long long s_round_constant[SHA512_SCHEDULE_WORDS] = {
    0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull, 0x3956c25bf348b538ull,
    0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull, 0xd807aa98a3030242ull, 0x12835b0145706fbeull,
    0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull, 0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull,
    0xc19bf174cf692694ull, 0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
    0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull, 0x983e5152ee66dfabull,
    0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull, 0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull,
    0x06ca6351e003826full, 0x142929670a0e6e70ull, 0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull,
    0x53380d139d95b3dfull, 0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
    0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull, 0xd192e819d6ef5218ull,
    0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull, 0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull,
    0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull, 0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull,
    0x682e6ff3d6b2b8a3ull, 0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
    0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull, 0xca273eceea26619cull,
    0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull, 0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull,
    0x113f9804bef90daeull, 0x1b710b35131c471bull, 0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull,
    0x431d67c49c100d4cull, 0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull};

// the first 64 bits of the fractional parts of the square roots of the first 8 primes (section 5.3.5)
static const unsigned long long s_initial_state[SHA512_STATE_WORDS] = {
    0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
    0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull};

// `word` rotated right by `count`, 0 < count < 64
static unsigned long long sha512_rotate(unsigned long long word, unsigned int count)
{
    return (word >> count) | (word << (64u - count));
}

// the eight big-endian bytes at `bytes` as one word
static unsigned long long sha512_word_read(const unsigned char *bytes)
{
    unsigned long long word = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        word = (word << 8u) | bytes[at];
    }
    return word;
}

// the hash value `state` carried through the 128-byte block at `block` (section 6.4.2)
static void sha512_compress(unsigned long long state[SHA512_STATE_WORDS], const unsigned char block[SHA512_BLOCK_BYTES])
{
    unsigned long long schedule[SHA512_SCHEDULE_WORDS];
    for (unsigned int round = 0u; round < SHA512_BLOCK_WORDS; round += 1u)
    {
        schedule[round] = sha512_word_read(&block[8u * round]);
    }
    for (unsigned int round = SHA512_BLOCK_WORDS; round < SHA512_SCHEDULE_WORDS; round += 1u)
    {
        const unsigned long long far = schedule[round - 15u];
        const unsigned long long near = schedule[round - 2u];
        const unsigned long long small_sigma0 = sha512_rotate(far, 1u) ^ sha512_rotate(far, 8u) ^ (far >> 7u);
        const unsigned long long small_sigma1 = sha512_rotate(near, 19u) ^ sha512_rotate(near, 61u) ^ (near >> 6u);
        schedule[round] = small_sigma1 + schedule[round - 7u] + small_sigma0 + schedule[round - 16u];
    }
    unsigned long long work[SHA512_STATE_WORDS];
    memcpy(work, state, sizeof(work));
    for (unsigned int round = 0u; round < SHA512_SCHEDULE_WORDS; round += 1u)
    {
        const unsigned long long big_sigma1 =
            sha512_rotate(work[4], 14u) ^ sha512_rotate(work[4], 18u) ^ sha512_rotate(work[4], 41u);
        const unsigned long long choose = (work[4] & work[5]) ^ (~work[4] & work[6]);
        const unsigned long long first = work[7] + big_sigma1 + choose + s_round_constant[round] + schedule[round];
        const unsigned long long big_sigma0 =
            sha512_rotate(work[0], 28u) ^ sha512_rotate(work[0], 34u) ^ sha512_rotate(work[0], 39u);
        const unsigned long long majority = (work[0] & work[1]) ^ (work[0] & work[2]) ^ (work[1] & work[2]);
        const unsigned long long second = big_sigma0 + majority;
        work[7] = work[6];
        work[6] = work[5];
        work[5] = work[4];
        work[4] = work[3] + first;
        work[3] = work[2];
        work[2] = work[1];
        work[1] = work[0];
        work[0] = first + second;
    }
    for (unsigned int word = 0u; word < SHA512_STATE_WORDS; word += 1u)
    {
        state[word] += work[word];
    }
}

void sha512_begun(Sha512 *sha)
{
    memcpy(sha->state, s_initial_state, sizeof(sha->state));
    sha->held = 0u;
    sha->taken = 0ull;
}

void sha512_taken(Sha512 *sha, const void *data, size_t bytes)
{
    const unsigned char *at = (const unsigned char *)data;
    size_t left = bytes;
    sha->taken += bytes;
    while (left > 0u)
    {
        const size_t room = SHA512_BLOCK_BYTES - sha->held;
        const size_t piece = (left < room) ? left : room;
        memcpy(sha->block + sha->held, at, piece);
        // a piece is at most the block's 128 bytes, which an unsigned int holds
        sha->held += (unsigned int)piece;
        at += piece;
        left -= piece;
        if (sha->held == SHA512_BLOCK_BYTES)
        {
            sha512_compress(sha->state, sha->block);
            sha->held = 0u;
        }
    }
}

void sha512_ended(Sha512 *sha, unsigned char digest[SHA512_BYTES])
{
    // the message is padded with a one bit, zeros, and its length in bits as 128 bits (section 5.1.2)
    const unsigned long long low_bits = sha->taken << 3u;
    const unsigned long long high_bits = sha->taken >> 61u;
    sha->block[sha->held] = 0x80u;
    sha->held += 1u;
    if (sha->held > (SHA512_BLOCK_BYTES - SHA512_LENGTH_BYTES))
    {
        memset(sha->block + sha->held, 0, SHA512_BLOCK_BYTES - sha->held);
        sha512_compress(sha->state, sha->block);
        sha->held = 0u;
    }
    memset(sha->block + sha->held, 0, SHA512_BLOCK_BYTES - sha->held);
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        // each byte of the length shifted down to its own keeps only that byte
        sha->block[SHA512_BLOCK_BYTES - 16u + at] = (unsigned char)(high_bits >> (56u - (8u * at)));
        sha->block[SHA512_BLOCK_BYTES - 8u + at] = (unsigned char)(low_bits >> (56u - (8u * at)));
    }
    sha512_compress(sha->state, sha->block);
    for (unsigned int word = 0u; word < SHA512_STATE_WORDS; word += 1u)
    {
        for (unsigned int at = 0u; at < 8u; at += 1u)
        {
            // each byte of a word shifted down to its own keeps only that byte
            digest[(8u * word) + at] = (unsigned char)(sha->state[word] >> (56u - (8u * at)));
        }
    }
}

void sha512(const void *data, size_t bytes, unsigned char digest[SHA512_BYTES])
{
    Sha512 sha;
    sha512_begun(&sha);
    sha512_taken(&sha, data, bytes);
    sha512_ended(&sha, digest);
}
