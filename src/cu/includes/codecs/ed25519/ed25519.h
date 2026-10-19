// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ed25519.h: Ed25519 signatures as RFC 8032 states them (section 5.1): a public key made from a 32-byte seed, a
// message signed with the seed, and a signature held to its public key and message. A signature whose S is not below
// the group's order is refused, so no signature has a second form that also holds
#ifndef ED25519_H
#define ED25519_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define ED25519_SEED_BYTES 32u
#define ED25519_PUBLIC_BYTES 32u
#define ED25519_SIGNATURE_BYTES 64u

    // the public key of `seed` into `public_key` (section 5.1.5)
    void ed25519_public_of(const unsigned char seed[ED25519_SEED_BYTES],
                           unsigned char public_key[ED25519_PUBLIC_BYTES]);
    // the `bytes` bytes at `message` signed with `seed`, whose public key is `public_key`, into `signature`
    // (section 5.1.6)
    void ed25519_signed(const unsigned char seed[ED25519_SEED_BYTES],
                        const unsigned char public_key[ED25519_PUBLIC_BYTES], const void *message, size_t bytes,
                        unsigned char signature[ED25519_SIGNATURE_BYTES]);
    // whether `signature` holds for the `bytes` bytes at `message` and `public_key` (section 5.1.7): 1, or 0
    int ed25519_holds(const unsigned char public_key[ED25519_PUBLIC_BYTES], const void *message, size_t bytes,
                      const unsigned char signature[ED25519_SIGNATURE_BYTES]);

#ifdef __cplusplus
}
#endif

#endif
