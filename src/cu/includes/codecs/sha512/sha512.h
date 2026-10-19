// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sha512.h: SHA-512 as FIPS 180-4 states it (section 6.4), over a message of whole bytes, taken whole or in pieces
#ifndef SHA512_H
#define SHA512_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the bytes of a digest, and of the block the compression takes
#define SHA512_BYTES 64u
#define SHA512_BLOCK_BYTES 128u

    // A digest under way: the hash value, the bytes of a block not yet whole, how many of them, and the bytes taken
    typedef struct
    {
        unsigned long long state[8];
        unsigned char block[SHA512_BLOCK_BYTES];
        unsigned int held;
        unsigned long long taken;
    } Sha512;

    // a digest begun; `bytes` bytes at `data` taken after those before; and the digest into `digest`
    void sha512_begun(Sha512 *sha);
    void sha512_taken(Sha512 *sha, const void *data, size_t bytes);
    void sha512_ended(Sha512 *sha, unsigned char digest[SHA512_BYTES]);

    // the digest of the `bytes` bytes at `data` into `digest`
    void sha512(const void *data, size_t bytes, unsigned char digest[SHA512_BYTES]);

#ifdef __cplusplus
}
#endif

#endif
