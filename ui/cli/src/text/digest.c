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

typedef struct
{
    unsigned int word;
    unsigned int count;
    unsigned int *state;
    const unsigned char *block;
    const unsigned char *bytes;
    unsigned int size;
    VerbaLinea *text;
    unsigned long long size_u64;
    unsigned long long index;
    int big;
    unsigned char *block_out;
} DigestCtx;
EMBED_INLINE unsigned int cli_right(const DigestCtx *args)
{
    return (args->word >> args->count) | (args->word << (32u - args->count));
}

EMBED_INLINE unsigned int cli_leave(const DigestCtx *args)
{
    return (args->word << args->count) | (args->word >> (32u - args->count));
}

// one block of 64 bytes taken into the state
EMBED_INLINE void cli_sha256_block(const DigestCtx *args)
{
    unsigned int words[64];
    for (unsigned int at = 0u; at < 16u; at += 1u)
    {
        words[at] = ((unsigned int)args->block[at * 4u] << 24u) | ((unsigned int)args->block[(at * 4u) + 1u] << 16u) |
                    ((unsigned int)args->block[(at * 4u) + 2u] << 8u) | (unsigned int)args->block[(at * 4u) + 3u];
    }
    for (unsigned int at = 16u; at < 64u; at += 1u)
    {
        const unsigned int low = EMBED_CALL(cli_right, DigestCtx, .word = words[at - 15u], .count = 7u) ^
                                 EMBED_CALL(cli_right, DigestCtx, .word = words[at - 15u], .count = 18u) ^
                                 (words[at - 15u] >> 3u);
        const unsigned int high = EMBED_CALL(cli_right, DigestCtx, .word = words[at - 2u], .count = 17u) ^
                                  EMBED_CALL(cli_right, DigestCtx, .word = words[at - 2u], .count = 19u) ^
                                  (words[at - 2u] >> 10u);
        words[at] = words[at - 16u] + low + words[at - 7u] + high;
    }
    unsigned int a = args->state[0];
    unsigned int b = args->state[1];
    unsigned int c = args->state[2];
    unsigned int d = args->state[3];
    unsigned int e = args->state[4];
    unsigned int f = args->state[5];
    unsigned int g = args->state[6];
    unsigned int h = args->state[7];
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        const unsigned int sum_e = EMBED_CALL(cli_right, DigestCtx, .word = e, .count = 6u) ^
                                   EMBED_CALL(cli_right, DigestCtx, .word = e, .count = 11u) ^
                                   EMBED_CALL(cli_right, DigestCtx, .word = e, .count = 25u);
        const unsigned int choose = (e & f) ^ (~e & g);
        const unsigned int first = h + sum_e + choose + s_round[at] + words[at];
        const unsigned int sum_a = EMBED_CALL(cli_right, DigestCtx, .word = a, .count = 2u) ^
                                   EMBED_CALL(cli_right, DigestCtx, .word = a, .count = 13u) ^
                                   EMBED_CALL(cli_right, DigestCtx, .word = a, .count = 22u);
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
    args->state[0] += a;
    args->state[1] += b;
    args->state[2] += c;
    args->state[3] += d;
    args->state[4] += e;
    args->state[5] += f;
    args->state[6] += g;
    args->state[7] += h;
}

void cli_sha256_begin(const CliDigestCfg *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->sha->state, .src = s_start, .bytes = sizeof(s_start)});
    args->sha->buffered = 0u;
    args->sha->total = 0ull;
}

void cli_sha256_take(const CliDigestShaCfg *args)
{
    const unsigned char *const from = (const unsigned char *)args->bytes;
    args->sha->total += args->size;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        args->sha->block[args->sha->buffered] = from[at];
        args->sha->buffered += 1u;
        if (args->sha->buffered == 64u)
        {
            EMBED_CALL(cli_sha256_block, DigestCtx, .state = args->sha->state, .block = args->sha->block);
            args->sha->buffered = 0u;
        }
    }
}

void cli_sha256_end(const CliDigestCfg *args)
{
    const unsigned long long bits = args->sha->total * 8ull;
    const unsigned char one = 0x80u;
    const unsigned char zero = 0u;
    EMBED_CALL(cli_digest_sha.sha256_take, CliDigestShaCfg, .sha = args->sha, .bytes = &one, .size = 1ull);
    while (args->sha->buffered != 56u)
    {
        EMBED_CALL(cli_digest_sha.sha256_take, CliDigestShaCfg, .sha = args->sha, .bytes = &zero, .size = 1ull);
    }
    unsigned char length[8];
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        length[at] = (unsigned char)((bits >> (56u - (8u * at))) & 0xffull);
    }
    EMBED_CALL(cli_digest_sha.sha256_take, CliDigestShaCfg, .sha = args->sha, .bytes = length, .size = 8ull);
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        args->digest[at * 4u] = (unsigned char)(args->sha->state[at] >> 24u);
        args->digest[(at * 4u) + 1u] = (unsigned char)(args->sha->state[at] >> 16u);
        args->digest[(at * 4u) + 2u] = (unsigned char)(args->sha->state[at] >> 8u);
        args->digest[(at * 4u) + 3u] = (unsigned char)args->sha->state[at];
    }
}

void cli_sha256_bytes(const CliDigestBytesSizeCfg *args)
{
    CliSha256 sha;
    EMBED_CALL(cli_digest.sha256_begin, CliDigestCfg, .sha = &sha);
    EMBED_CALL(cli_digest_sha.sha256_take, CliDigestShaCfg, .sha = &sha, .bytes = args->bytes, .size = args->size);
    EMBED_CALL(cli_digest.sha256_end, CliDigestCfg, .sha = &sha, .digest = args->digest);
}

// `size` bytes as lowercase hex
EMBED_INLINE void cli_hex(const DigestCtx *args)
{
    for (unsigned int at = 0u; at < args->size; at += 1u)
    {
        mmgr_line_hex(args->text, args->bytes[at], 2u);
    }
}

void cli_sha256_text(const CliDigestBytesCfg *args)
{
    unsigned char digest[32];
    EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = args->bytes, .size = args->size,
               .digest = digest);
    EMBED_CALL(cli_hex, DigestCtx, .bytes = digest, .size = 32u, .text = args->text);
}

void cli_hmac_sha256_bytes(const CliDigestKeyCfg *args)
{
    unsigned char block[64];
    memor.set(&(MemoriaCfg){.dst = block, .val = 0, .bytes = sizeof(block)});
    if (args->key_size > 64ull)
    {
        EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = args->key,
                   .size = args->key_size, .digest = block);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = block, .src = args->key, .bytes = (size_t)args->key_size});
    }
    unsigned char pad[64];
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        pad[at] = block[at] ^ 0x36u;
    }
    CliSha256 sha;
    unsigned char inner[32];
    EMBED_CALL(cli_digest.sha256_begin, CliDigestCfg, .sha = &sha);
    EMBED_CALL(cli_digest_sha.sha256_take, CliDigestShaCfg, .sha = &sha, .bytes = pad, .size = 64ull);
    EMBED_CALL(cli_digest_sha.sha256_take, CliDigestShaCfg, .sha = &sha, .bytes = args->message,
               .size = args->message_size);
    EMBED_CALL(cli_digest.sha256_end, CliDigestCfg, .sha = &sha, .digest = inner);
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        pad[at] = block[at] ^ 0x5cu;
    }
    EMBED_CALL(cli_digest.sha256_begin, CliDigestCfg, .sha = &sha);
    EMBED_CALL(cli_digest_sha.sha256_take, CliDigestShaCfg, .sha = &sha, .bytes = pad, .size = 64ull);
    EMBED_CALL(cli_digest_sha.sha256_take, CliDigestShaCfg, .sha = &sha, .bytes = inner, .size = 32ull);
    EMBED_CALL(cli_digest.sha256_end, CliDigestCfg, .sha = &sha, .digest = args->signed_digest);
}

void cli_hmac_sha256_text(const CliDigestTextCfg *args)
{
    unsigned char signed_digest[32];
    EMBED_CALL(cli_digest_key.hmac_sha256_bytes, CliDigestKeyCfg, .key = args->key, .key_size = args->key_size,
               .message = args->message, .message_size = args->message_size, .signed_digest = signed_digest);
    EMBED_CALL(cli_hex, DigestCtx, .bytes = signed_digest, .size = 32u, .text = args->text);
}

void cli_pbkdf2_sha256(const CliDigestPasswordCfg *args)
{
    // the salt and the block's number, 1, big-endian, as the first round's message
    static unsigned char s_first[4096u + 4u];
    const unsigned long long salt_len = (args->salt_size < 4096ull) ? args->salt_size : 4096ull;
    memor.cpy(&(MemoriaCfg){.dst = s_first, .src = args->salt, .bytes = (size_t)salt_len});
    s_first[salt_len] = 0u;
    s_first[salt_len + 1u] = 0u;
    s_first[salt_len + 2u] = 0u;
    s_first[salt_len + 3u] = 1u;
    unsigned char last[32];
    EMBED_CALL(cli_digest_key.hmac_sha256_bytes, CliDigestKeyCfg, .key = args->password,
               .key_size = args->password_size, .message = s_first, .message_size = salt_len + 4ull,
               .signed_digest = last);
    memor.cpy(&(MemoriaCfg){.dst = args->derived, .src = last, .bytes = 32u});
    for (unsigned int round = 1u; round < args->rounds; round += 1u)
    {
        unsigned char now[32];
        EMBED_CALL(cli_digest_key.hmac_sha256_bytes, CliDigestKeyCfg, .key = args->password,
                   .key_size = args->password_size, .message = last, .message_size = 32ull, .signed_digest = now);
        for (unsigned int at = 0u; at < 32u; at += 1u)
        {
            args->derived[at] ^= now[at];
        }
        memor.cpy(&(MemoriaCfg){.dst = last, .src = now, .bytes = 32u});
    }
}

// the padded message's block `index` of the `size` bytes at `bytes`: the bytes, the 0x80, zeros, and the length in
// bits, big-endian for SHA-1 and little for MD5, written into `block`
EMBED_INLINE void cli_padded_block(const DigestCtx *args)
{
    const unsigned long long bits = args->size_u64 * 8ull;
    const unsigned long long blocks = ((args->size_u64 + 8ull) / 64ull) + 1ull;
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        const unsigned long long byte = (args->index * 64ull) + at;
        unsigned char value = (byte < args->size_u64) ? args->bytes[byte] : ((byte == args->size_u64) ? 0x80u : 0u);
        if ((args->index == (blocks - 1u)) && (at >= 56u))
        {
            const unsigned int shift = args->big ? (8u * (63u - at)) : (8u * (at - 56u));
            value = (unsigned char)((bits >> shift) & 0xffull);
        }
        args->block_out[at] = value;
    }
}

void cli_sha1_bytes(const CliDigestBytesSizeCfg *args)
{
    unsigned int state[5] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
    const unsigned long long blocks = ((args->size + 8ull) / 64ull) + 1ull;
    for (unsigned long long index = 0ull; index < blocks; index += 1ull)
    {
        unsigned char block[64];
        EMBED_CALL(cli_padded_block, DigestCtx, .bytes = (const unsigned char *)args->bytes, .size_u64 = args->size,
                   .index = index, .big = 1, .block_out = block);
        unsigned int words[80];
        for (unsigned int at = 0u; at < 16u; at += 1u)
        {
            words[at] = ((unsigned int)block[at * 4u] << 24u) | ((unsigned int)block[(at * 4u) + 1u] << 16u) |
                        ((unsigned int)block[(at * 4u) + 2u] << 8u) | (unsigned int)block[(at * 4u) + 3u];
        }
        for (unsigned int at = 16u; at < 80u; at += 1u)
        {
            words[at] =
                EMBED_CALL(cli_leave, DigestCtx,
                           .word = words[at - 3u] ^ words[at - 8u] ^ words[at - 14u] ^ words[at - 16u], .count = 1u);
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
            const unsigned int next =
                EMBED_CALL(cli_leave, DigestCtx, .word = a, .count = 5u) + mixed + e + constant + words[at];
            e = d;
            d = c;
            c = EMBED_CALL(cli_leave, DigestCtx, .word = b, .count = 30u);
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
        args->digest[at * 4u] = (unsigned char)(state[at] >> 24u);
        args->digest[(at * 4u) + 1u] = (unsigned char)(state[at] >> 16u);
        args->digest[(at * 4u) + 2u] = (unsigned char)(state[at] >> 8u);
        args->digest[(at * 4u) + 3u] = (unsigned char)state[at];
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

void cli_md5_text(const CliDigestBytesCfg *args)
{
    unsigned int state[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
    const unsigned long long blocks = ((args->size + 8ull) / 64ull) + 1ull;
    for (unsigned long long index = 0ull; index < blocks; index += 1ull)
    {
        unsigned char block[64];
        EMBED_CALL(cli_padded_block, DigestCtx, .bytes = (const unsigned char *)args->bytes, .size_u64 = args->size,
                   .index = index, .big = 0, .block_out = block);
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
                EMBED_CALL(cli_leave, DigestCtx, .word = a + mixed + s_md5_round[at] + words[word],
                           .count = s_md5_shift[(quarter * 4u) + (at % 4u)]);
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
            mmgr_line_hex(args->text, (state[at] >> (8u * byte)) & 0xffu, 2u);
        }
    }
}
