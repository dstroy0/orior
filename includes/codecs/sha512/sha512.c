// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sha512.c: SHA-512 as FIPS 180-4 states it. The constants are section 4.2.3's and 5.3.5's, the functions section
// 4.1.3's, the padding section 5.1.2's and the compression section 6.4.2's
#include "sha512.h"
#include "memoria_operor/memoria_linea.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "sha512: a byte contains 8 bits");
_Static_assert(ULLONG_MAX >= 0xFFFFFFFFFFFFFFFFULL, "sha512: an unsigned long long represents a 64-bit word");

// the words of the hash value, of the message schedule and of a block
#define SHA512_STATE_WORDS 8u
#define SHA512_SCHEDULE_WORDS 80u
#define SHA512_BLOCK_WORDS 16u
// the bytes the padding ends on, the message length in bits
#define SHA512_LENGTH_BYTES 16u

// the first 64 bits of the fractional parts of the cube roots of the first 80 primes (section 4.2.3)
static const unsigned long long s_round_constant[SHA512_SCHEDULE_WORDS] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL,
    0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
    0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL, 0x983e5152ee66dfabULL,
    0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL,
    0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
    0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
    0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL, 0xca273eceea26619cULL,
    0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
    0x113f9804bef90daeULL, 0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL};

// the first 64 bits of the fractional parts of the square roots of the first 8 primes (section 5.3.5)
static const unsigned long long s_initial_state[SHA512_STATE_WORDS] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL};

// `word` rotated right by `count`, 0 < count < 64
static unsigned long long sha512_rotate(unsigned long long word, unsigned int count)
{
    return (word >> count) | (word << (64u - count));
}

// the eight big-endian bytes at `bytes` as one word
static unsigned long long sha512_word_read(const unsigned char *bytes)
{
    unsigned long long word = 0ULL;
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
    mmgr_bytes_copy(work, state, sizeof(work));
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
    mmgr_bytes_copy(sha->state, s_initial_state, sizeof(sha->state));
    sha->buffered = 0u;
    sha->taken = 0ULL;
}

void sha512_taken(Sha512 *sha, const void *data, size_t bytes)
{
    const unsigned char *at = (const unsigned char *)data;
    size_t left = bytes;
    sha->taken += bytes;
    while (left > 0u)
    {
        const size_t room = SHA512_BLOCK_BYTES - sha->buffered;
        const size_t piece = (left < room) ? left : room;
        mmgr_bytes_copy(sha->block + sha->buffered, at, piece);
        // a piece is at max the block's 128 bytes, which an unsigned int represents
        sha->buffered += (unsigned int)piece;
        at += piece;
        left -= piece;
        if (sha->buffered == SHA512_BLOCK_BYTES)
        {
            sha512_compress(sha->state, sha->block);
            sha->buffered = 0u;
        }
    }
}

void sha512_ended(Sha512 *sha, unsigned char digest[SHA512_BYTES])
{
    // the message is padded with a one bit, zeros, and its length in bits as 128 bits (section 5.1.2)
    const unsigned long long low_bits = sha->taken << 3u;
    const unsigned long long high_bits = sha->taken >> 61u;
    sha->block[sha->buffered] = 0x80u;
    sha->buffered += 1u;
    if (sha->buffered > (SHA512_BLOCK_BYTES - SHA512_LENGTH_BYTES))
    {
        mmgr_bytes_set(sha->block + sha->buffered, 0, SHA512_BLOCK_BYTES - sha->buffered);
        sha512_compress(sha->state, sha->block);
        sha->buffered = 0u;
    }
    mmgr_bytes_set(sha->block + sha->buffered, 0, SHA512_BLOCK_BYTES - sha->buffered);
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        // each byte of the length shifted down to its own retains only that byte
        sha->block[SHA512_BLOCK_BYTES - 16u + at] = (unsigned char)(high_bits >> (56u - (8u * at)));
        sha->block[SHA512_BLOCK_BYTES - 8u + at] = (unsigned char)(low_bits >> (56u - (8u * at)));
    }
    sha512_compress(sha->state, sha->block);
    for (unsigned int word = 0u; word < SHA512_STATE_WORDS; word += 1u)
    {
        for (unsigned int at = 0u; at < 8u; at += 1u)
        {
            // each byte of a word shifted down to its own retains only that byte
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
