// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// digest.h: SHA-256, as FIPS 180-4 sets it out, which `sha256sum` gives on a machine files are sent to: a file's
// digest here and there says whether the two hold the same bytes. SHA-1, MD5 and PBKDF2 are the ones a database's
// sign-in asks for: MySQL's password scramble, PostgreSQL's md5 password and its SCRAM-SHA-256 (digest.c)
#ifndef DIGEST_H
#define DIGEST_H

#include "../command_line/cli_internal.h"

// A SHA-256 taken over pieces as they come: the state, the bytes of the block not yet whole, and the count taken
typedef struct
{
    unsigned int state[8];
    unsigned char block[64];
    unsigned int held;
    unsigned long long total;
} CliSha256;

void cli_sha256_begun(CliSha256 *sha);
void cli_sha256_taken(CliSha256 *sha, const void *bytes, unsigned long long size);
void cli_sha256_ended(CliSha256 *sha, unsigned char *digest);
// the SHA-256 of `size` bytes, its 32 bytes, and as the 64 lowercase hex digits `sha256sum` writes
void cli_sha256_bytes(const void *bytes, unsigned long long size, unsigned char *digest);
void cli_sha256_text(const void *bytes, unsigned long long size, CliLine *text);
// the HMAC of `message` with `key`, by SHA-256, as RFC 2104 sets it out, its 32 bytes and in lowercase hex: the
// signature Jupyter's messages carry
void cli_hmac_sha256_bytes(const void *key, unsigned long long key_size, const void *message,
                           unsigned long long message_size, unsigned char *signed_digest);
void cli_hmac_sha256_text(const void *key, unsigned long long key_size, const void *message,
                          unsigned long long message_size, CliLine *text);
// the first 32 bytes PBKDF2 derives from `password` and `salt` in `rounds`, by HMAC-SHA-256, as RFC 8018 sets it out:
// SCRAM's salted password
void cli_pbkdf2_sha256(const void *password, unsigned long long password_size, const void *salt,
                       unsigned long long salt_size, unsigned int rounds, unsigned char *derived);
// the SHA-1 of `size` bytes, its 20 bytes; the MD5 of them, as RFC 1321 sets it out, in lowercase hex
void cli_sha1_bytes(const void *bytes, unsigned long long size, unsigned char *digest);
void cli_md5_text(const void *bytes, unsigned long long size, CliLine *text);

#endif
