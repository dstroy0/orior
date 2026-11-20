// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pbkdf2.h: HMAC-SHA-512 as RFC 2104 section 2 states it, and PBKDF2 over it as RFC 8018 section 5.2 states it. The
// hash is the SHA-512 of includes/codecs/sha512
#ifndef PBKDF2_H
#define PBKDF2_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the bytes of an HMAC-SHA-512 tag, which is hLen of RFC 8018 for this PRF
#define HMAC_SHA512_BYTES 64u

    // the HMAC-SHA-512 tag of the `message_bytes` bytes at `message` under the `key_bytes` bytes at `key`, into `tag`
    void hmac_sha512(const void *key, size_t key_bytes, const void *message, size_t message_bytes,
                     unsigned char tag[HMAC_SHA512_BYTES]);

    // `derived_bytes` bytes of PBKDF2 with HMAC-SHA-512 as the PRF, over the password, the salt and `iterations`
    // iterations, into `derived`. Returns 1. Returns 0 and writes nothing where RFC 8018 step 1 refuses the length,
    // or where `iterations` is zero, which RFC 8018 does not allow
    int pbkdf2_hmac_sha512(const void *password, size_t password_bytes, const void *salt, size_t salt_bytes,
                           unsigned long iterations, unsigned char *derived, size_t derived_bytes);

#ifdef __cplusplus
}
#endif

#endif
