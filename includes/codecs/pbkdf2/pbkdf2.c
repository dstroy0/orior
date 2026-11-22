// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pbkdf2.c: HMAC-SHA-512 as RFC 2104 section 2 states it, and PBKDF2 over it as RFC 8018 section 5.2 states it
#include "pbkdf2.h"
#include "memoria_operor/memoria_linea.h"

#include "../sha512/sha512.h"


// the bytes of the block of SHA-512, which is B of RFC 2104
#define HMAC_BLOCK_BYTES SHA512_BLOCK_BYTES
// the bytes of INT(i) of RFC 8018 section 5.2
#define PBKDF2_INDEX_BYTES 4u

_Static_assert(HMAC_SHA512_BYTES == SHA512_BYTES, "pbkdf2: the tag of HMAC-SHA-512 is one SHA-512 digest");

// The two digests under way after the padded key, by RFC 2104 steps (1), (2) and (5). The inner one holds
// K XOR ipad, and the outer one holds K XOR opad. A copy of each starts every tag under the same key.
typedef struct
{
    Sha512 inner;
    Sha512 outer;
} HmacKey;

// the key of RFC 2104 into `prepared`. Its own hash replaces a key longer than B, as RFC 2104 section 2 says
static void hmac_key_prepare(HmacKey *prepared, const void *key, size_t key_bytes)
{
    // step (1): append zeros to the key to make a B byte string
    unsigned char block[HMAC_BLOCK_BYTES];
    mmgr_bytes_set(block, 0, sizeof(block));
    if (key_bytes > HMAC_BLOCK_BYTES)
    {
        sha512(key, key_bytes, block);
    }
    else if (key_bytes > 0u)
    {
        mmgr_bytes_copy(block, key, key_bytes);
    }

    // steps (2) and (5): the B byte string XOR ipad, and XOR opad, each taken as the first block of its digest
    unsigned char inner_pad[HMAC_BLOCK_BYTES];
    unsigned char outer_pad[HMAC_BLOCK_BYTES];
    for (size_t at = 0u; at < HMAC_BLOCK_BYTES; at += 1u)
    {
        inner_pad[at] = (unsigned char)(block[at] ^ 0x36u);
        outer_pad[at] = (unsigned char)(block[at] ^ 0x5Cu);
    }
    sha512_begun(&prepared->inner);
    sha512_taken(&prepared->inner, inner_pad, sizeof(inner_pad));
    sha512_begun(&prepared->outer);
    sha512_taken(&prepared->outer, outer_pad, sizeof(outer_pad));
}

// the tag under `prepared` of the `first_bytes` bytes at `first` and then the `second_bytes` bytes at `second`.
// Two parts let PBKDF2 tag S || INT(i) with no copy of the salt
static void hmac_tag(const HmacKey *prepared, const void *first, size_t first_bytes, const void *second,
                     size_t second_bytes, unsigned char tag[HMAC_SHA512_BYTES])
{
    // steps (3) and (4): append the text to K XOR ipad, and apply H
    Sha512 inner = prepared->inner;
    sha512_taken(&inner, first, first_bytes);
    sha512_taken(&inner, second, second_bytes);
    unsigned char inner_digest[SHA512_BYTES];
    sha512_ended(&inner, inner_digest);

    // steps (6) and (7): append the result of step (4) to K XOR opad, and apply H
    Sha512 outer = prepared->outer;
    sha512_taken(&outer, inner_digest, sizeof(inner_digest));
    sha512_ended(&outer, tag);
}

void hmac_sha512(const void *key, size_t key_bytes, const void *message, size_t message_bytes,
                 unsigned char tag[HMAC_SHA512_BYTES])
{
    HmacKey prepared;
    hmac_key_prepare(&prepared, key, key_bytes);
    hmac_tag(&prepared, message, message_bytes, NULL, 0u, tag);
}

int pbkdf2_hmac_sha512(const void *password, size_t password_bytes, const void *salt, size_t salt_bytes,
                       unsigned long iterations, unsigned char *derived, size_t derived_bytes)
{
    // RFC 8018 gives c and dkLen as positive integers. Step 1: l = CEIL(dkLen / hLen) must not exceed 2^32 - 1
    if ((iterations == 0u) || (derived_bytes == 0u) || (((derived_bytes - 1u) / HMAC_SHA512_BYTES) >= 0xFFFFFFFFu))
    {
        return 0;
    }

    HmacKey prepared;
    hmac_key_prepare(&prepared, password, password_bytes);

    // steps 2 to 4: each block T_i = U_1 XOR ... XOR U_c, with U_1 = PRF(P, S || INT(i)) and U_j = PRF(P, U_(j-1)).
    // The blocks join in order, and the last one gives only the bytes that dkLen still needs
    size_t done = 0u;
    unsigned long index = 1u;
    while (done < derived_bytes)
    {
        // INT(i): the four bytes of i, most significant first
        const unsigned char counter[PBKDF2_INDEX_BYTES] = {
            (unsigned char)((index >> 24u) & 0xFFu), (unsigned char)((index >> 16u) & 0xFFu),
            (unsigned char)((index >> 8u) & 0xFFu), (unsigned char)(index & 0xFFu)};
        unsigned char chained[HMAC_SHA512_BYTES];
        unsigned char block[HMAC_SHA512_BYTES];
        hmac_tag(&prepared, salt, salt_bytes, counter, sizeof(counter), chained);
        mmgr_bytes_copy(block, chained, sizeof(block));

        for (unsigned long round = 1u; round < iterations; round += 1u)
        {
            hmac_tag(&prepared, chained, sizeof(chained), NULL, 0u, chained);
            for (size_t at = 0u; at < HMAC_SHA512_BYTES; at += 1u)
            {
                block[at] ^= chained[at];
            }
        }

        const size_t left = derived_bytes - done;
        const size_t taken = (left < HMAC_SHA512_BYTES) ? left : HMAC_SHA512_BYTES;
        mmgr_bytes_copy(derived + done, block, taken);
        done += taken;
        index += 1u;
    }
    return 1;
}
