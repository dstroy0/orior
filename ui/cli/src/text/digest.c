// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// digest.c: SHA-256 as FIPS 180-4 sets it out, taken over pieces as they come; its HMAC as RFC 2104 and PBKDF2 as
// RFC 8018 set them out; SHA-1 as FIPS 180-4 and MD5 as RFC 1321 set them out, each over the bytes whole with their
// padding written block by block and no copy of the bytes made
#include "digest.h"

static const unsigned int s_round[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

static const unsigned int s_start[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

static unsigned int cli_right(unsigned int word, unsigned int count)
{
    return (word >> count) | (word << (32u - count));
}

static unsigned int cli_left(unsigned int word, unsigned int count)
{
    return (word << count) | (word >> (32u - count));
}

// one block of 64 bytes taken into the state
static void cli_sha256_block(unsigned int *state, const unsigned char *block)
{
    unsigned int words[64];
    for (unsigned int at = 0u; at < 16u; at += 1u)
    {
        words[at] = ((unsigned int)block[at * 4u] << 24u) | ((unsigned int)block[(at * 4u) + 1u] << 16u) |
                    ((unsigned int)block[(at * 4u) + 2u] << 8u) | (unsigned int)block[(at * 4u) + 3u];
    }
    for (unsigned int at = 16u; at < 64u; at += 1u)
    {
        const unsigned int low =
            cli_right(words[at - 15u], 7u) ^ cli_right(words[at - 15u], 18u) ^ (words[at - 15u] >> 3u);
        const unsigned int high =
            cli_right(words[at - 2u], 17u) ^ cli_right(words[at - 2u], 19u) ^ (words[at - 2u] >> 10u);
        words[at] = words[at - 16u] + low + words[at - 7u] + high;
    }
    unsigned int a = state[0];
    unsigned int b = state[1];
    unsigned int c = state[2];
    unsigned int d = state[3];
    unsigned int e = state[4];
    unsigned int f = state[5];
    unsigned int g = state[6];
    unsigned int h = state[7];
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        const unsigned int sum_e = cli_right(e, 6u) ^ cli_right(e, 11u) ^ cli_right(e, 25u);
        const unsigned int choose = (e & f) ^ (~e & g);
        const unsigned int first = h + sum_e + choose + s_round[at] + words[at];
        const unsigned int sum_a = cli_right(a, 2u) ^ cli_right(a, 13u) ^ cli_right(a, 22u);
        const unsigned int most = (a & b) ^ (a & c) ^ (b & c);
        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + sum_a + most;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void cli_sha256_begun(CliSha256 *sha)
{
    memor.cpy(&(MemoriaCfg){.dst = sha->state, .src = s_start, .bytes = sizeof(s_start)});
    sha->held = 0u;
    sha->total = 0ull;
}

void cli_sha256_taken(CliSha256 *sha, const void *bytes, unsigned long long size)
{
    const unsigned char *const from = (const unsigned char *)bytes;
    sha->total += size;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        sha->block[sha->held] = from[at];
        sha->held += 1u;
        if (sha->held == 64u)
        {
            cli_sha256_block(sha->state, sha->block);
            sha->held = 0u;
        }
    }
}

void cli_sha256_ended(CliSha256 *sha, unsigned char *digest)
{
    const unsigned long long bits = sha->total * 8ull;
    const unsigned char one = 0x80u;
    const unsigned char zero = 0u;
    cli_sha256_taken(sha, &one, 1ull);
    while (sha->held != 56u)
    {
        cli_sha256_taken(sha, &zero, 1ull);
    }
    unsigned char length[8];
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        length[at] = (unsigned char)((bits >> (56u - (8u * at))) & 0xffull);
    }
    cli_sha256_taken(sha, length, 8ull);
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        digest[at * 4u] = (unsigned char)(sha->state[at] >> 24u);
        digest[(at * 4u) + 1u] = (unsigned char)(sha->state[at] >> 16u);
        digest[(at * 4u) + 2u] = (unsigned char)(sha->state[at] >> 8u);
        digest[(at * 4u) + 3u] = (unsigned char)sha->state[at];
    }
}

void cli_sha256_bytes(const void *bytes, unsigned long long size, unsigned char *digest)
{
    CliSha256 sha;
    cli_sha256_begun(&sha);
    cli_sha256_taken(&sha, bytes, size);
    cli_sha256_ended(&sha, digest);
}

// `size` bytes as lowercase hex
static void cli_hex(const unsigned char *bytes, unsigned int size, CliLine *text)
{
    for (unsigned int at = 0u; at < size; at += 1u)
    {
        cli_line_hex(text, bytes[at], 2u);
    }
}

void cli_sha256_text(const void *bytes, unsigned long long size, CliLine *text)
{
    unsigned char digest[32];
    cli_sha256_bytes(bytes, size, digest);
    cli_hex(digest, 32u, text);
}

void cli_hmac_sha256_bytes(const void *key, unsigned long long key_size, const void *message,
                           unsigned long long message_size, unsigned char *signed_digest)
{
    unsigned char block[64];
    memor.set(&(MemoriaCfg){.dst = block, .val = 0, .bytes = sizeof(block)});
    if (key_size > 64ull)
    {
        cli_sha256_bytes(key, key_size, block);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = block, .src = key, .bytes = (size_t)key_size});
    }
    unsigned char pad[64];
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        pad[at] = block[at] ^ 0x36u;
    }
    CliSha256 sha;
    unsigned char inner[32];
    cli_sha256_begun(&sha);
    cli_sha256_taken(&sha, pad, 64ull);
    cli_sha256_taken(&sha, message, message_size);
    cli_sha256_ended(&sha, inner);
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        pad[at] = block[at] ^ 0x5cu;
    }
    cli_sha256_begun(&sha);
    cli_sha256_taken(&sha, pad, 64ull);
    cli_sha256_taken(&sha, inner, 32ull);
    cli_sha256_ended(&sha, signed_digest);
}

void cli_hmac_sha256_text(const void *key, unsigned long long key_size, const void *message,
                          unsigned long long message_size, CliLine *text)
{
    unsigned char signed_digest[32];
    cli_hmac_sha256_bytes(key, key_size, message, message_size, signed_digest);
    cli_hex(signed_digest, 32u, text);
}

void cli_pbkdf2_sha256(const void *password, unsigned long long password_size, const void *salt,
                       unsigned long long salt_size, unsigned int rounds, unsigned char *derived)
{
    // the salt and the block's number, 1, big-endian, as the first round's message
    static unsigned char s_first[4096u + 4u];
    const unsigned long long kept = (salt_size < 4096ull) ? salt_size : 4096ull;
    memor.cpy(&(MemoriaCfg){.dst = s_first, .src = salt, .bytes = (size_t)kept});
    s_first[kept] = 0u;
    s_first[kept + 1u] = 0u;
    s_first[kept + 2u] = 0u;
    s_first[kept + 3u] = 1u;
    unsigned char last[32];
    cli_hmac_sha256_bytes(password, password_size, s_first, kept + 4ull, last);
    memor.cpy(&(MemoriaCfg){.dst = derived, .src = last, .bytes = 32u});
    for (unsigned int round = 1u; round < rounds; round += 1u)
    {
        unsigned char now[32];
        cli_hmac_sha256_bytes(password, password_size, last, 32ull, now);
        for (unsigned int at = 0u; at < 32u; at += 1u)
        {
            derived[at] ^= now[at];
        }
        memor.cpy(&(MemoriaCfg){.dst = last, .src = now, .bytes = 32u});
    }
}

// the padded message's block `index` of the `size` bytes at `bytes`: the bytes, the 0x80, zeros, and the length in
// bits, big-endian for SHA-1 and little for MD5, written into `block`
static void cli_padded_block(const unsigned char *bytes, unsigned long long size, unsigned long long index, int big,
                             unsigned char *block)
{
    const unsigned long long bits = size * 8ull;
    const unsigned long long blocks = ((size + 8ull) / 64ull) + 1ull;
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        const unsigned long long byte = (index * 64ull) + at;
        unsigned char value = (byte < size) ? bytes[byte] : ((byte == size) ? 0x80u : 0u);
        if ((index == (blocks - 1u)) && (at >= 56u))
        {
            const unsigned int shift = big ? (8u * (63u - at)) : (8u * (at - 56u));
            value = (unsigned char)((bits >> shift) & 0xffull);
        }
        block[at] = value;
    }
}

void cli_sha1_bytes(const void *bytes, unsigned long long size, unsigned char *digest)
{
    unsigned int state[5] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
    const unsigned long long blocks = ((size + 8ull) / 64ull) + 1ull;
    for (unsigned long long index = 0ull; index < blocks; index += 1ull)
    {
        unsigned char block[64];
        cli_padded_block((const unsigned char *)bytes, size, index, 1, block);
        unsigned int words[80];
        for (unsigned int at = 0u; at < 16u; at += 1u)
        {
            words[at] = ((unsigned int)block[at * 4u] << 24u) | ((unsigned int)block[(at * 4u) + 1u] << 16u) |
                        ((unsigned int)block[(at * 4u) + 2u] << 8u) | (unsigned int)block[(at * 4u) + 3u];
        }
        for (unsigned int at = 16u; at < 80u; at += 1u)
        {
            words[at] = cli_left(words[at - 3u] ^ words[at - 8u] ^ words[at - 14u] ^ words[at - 16u], 1u);
        }
        unsigned int a = state[0];
        unsigned int b = state[1];
        unsigned int c = state[2];
        unsigned int d = state[3];
        unsigned int e = state[4];
        for (unsigned int at = 0u; at < 80u; at += 1u)
        {
            const unsigned int mixed = (at < 20u)   ? ((b & c) | (~b & d))
                                       : (at < 40u) ? (b ^ c ^ d)
                                       : (at < 60u) ? ((b & c) | (b & d) | (c & d))
                                                    : (b ^ c ^ d);
            const unsigned int constant = (at < 20u)   ? 0x5a827999u
                                          : (at < 40u) ? 0x6ed9eba1u
                                          : (at < 60u) ? 0x8f1bbcdcu
                                                       : 0xca62c1d6u;
            const unsigned int next = cli_left(a, 5u) + mixed + e + constant + words[at];
            e = d;
            d = c;
            c = cli_left(b, 30u);
            b = a;
            a = next;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        digest[at * 4u] = (unsigned char)(state[at] >> 24u);
        digest[(at * 4u) + 1u] = (unsigned char)(state[at] >> 16u);
        digest[(at * 4u) + 2u] = (unsigned char)(state[at] >> 8u);
        digest[(at * 4u) + 3u] = (unsigned char)state[at];
    }
}

static const unsigned int s_md5_round[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
    0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
    0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
    0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u};

static const unsigned int s_md5_shift[16] = {7u, 12u, 17u, 22u, 5u, 9u, 14u, 20u, 4u, 11u, 16u, 23u, 6u, 10u, 15u, 21u};

void cli_md5_text(const void *bytes, unsigned long long size, CliLine *text)
{
    unsigned int state[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
    const unsigned long long blocks = ((size + 8ull) / 64ull) + 1ull;
    for (unsigned long long index = 0ull; index < blocks; index += 1ull)
    {
        unsigned char block[64];
        cli_padded_block((const unsigned char *)bytes, size, index, 0, block);
        unsigned int words[16];
        for (unsigned int at = 0u; at < 16u; at += 1u)
        {
            words[at] = (unsigned int)block[at * 4u] | ((unsigned int)block[(at * 4u) + 1u] << 8u) |
                        ((unsigned int)block[(at * 4u) + 2u] << 16u) | ((unsigned int)block[(at * 4u) + 3u] << 24u);
        }
        unsigned int a = state[0];
        unsigned int b = state[1];
        unsigned int c = state[2];
        unsigned int d = state[3];
        for (unsigned int at = 0u; at < 64u; at += 1u)
        {
            const unsigned int quarter = at / 16u;
            const unsigned int mixed = (quarter == 0u)   ? ((b & c) | (~b & d))
                                       : (quarter == 1u) ? ((d & b) | (~d & c))
                                       : (quarter == 2u) ? (b ^ c ^ d)
                                                         : (c ^ (b | ~d));
            const unsigned int word = (quarter == 0u)   ? at
                                      : (quarter == 1u) ? (((5u * at) + 1u) % 16u)
                                      : (quarter == 2u) ? (((3u * at) + 5u) % 16u)
                                                        : ((7u * at) % 16u);
            const unsigned int turned =
                cli_left(a + mixed + s_md5_round[at] + words[word], s_md5_shift[(quarter * 4u) + (at % 4u)]);
            a = d;
            d = c;
            c = b;
            b = b + turned;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        for (unsigned int byte = 0u; byte < 4u; byte += 1u)
        {
            cli_line_hex(text, (state[at] >> (8u * byte)) & 0xffu, 2u);
        }
    }
}
