// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// digest.h: SHA-256, as FIPS 180-4 sets it out, which `sha256sum` gives on a machine files are sent to: a file's
// digest here and there shows whether the two contain the same bytes. SHA-1, MD5 and PBKDF2 are the ones a database's
// sign-in requires: MySQL's password scramble, PostgreSQL's md5 password and its SCRAM-SHA-256 (digest.c)
#ifndef DIGEST_H
#define DIGEST_H
typedef struct CliDigestPasswordCfg CliDigestPasswordCfg;
typedef struct CliDigestKeyCfg CliDigestKeyCfg;
typedef struct CliDigestTextCfg CliDigestTextCfg;
typedef struct CliDigestBytesCfg CliDigestBytesCfg;
typedef struct CliDigestBytesSizeCfg CliDigestBytesSizeCfg;
typedef struct CliDigestShaCfg CliDigestShaCfg;
typedef struct CliDigestCfg CliDigestCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// A SHA-256 taken over pieces as they come: the state, the bytes of the block not yet whole, and the count taken
typedef struct
{
    unsigned int state[8];
    unsigned char block[64];
    unsigned int buffered;
    unsigned long long total;
} CliSha256;

void cli_sha256_begin(const CliDigestCfg *args);
void cli_sha256_take(const CliDigestShaCfg *args);
void cli_sha256_end(const CliDigestCfg *args);
// the SHA-256 of `size` bytes, its 32 bytes, and as the 64 lowercase hex digits `sha256sum` writes
void cli_sha256_bytes(const CliDigestBytesSizeCfg *args);
void cli_sha256_text(const CliDigestBytesCfg *args);
// the HMAC of `message` with `key`, by SHA-256, as RFC 2104 sets it out, its 32 bytes and in lowercase hex: the
// signature Jupyter's messages carry
void cli_hmac_sha256_bytes(const CliDigestKeyCfg *args);
void cli_hmac_sha256_text(const CliDigestTextCfg *args);
// the first 32 bytes PBKDF2 derives from `password` and `salt` in `rounds`, by HMAC-SHA-256, as RFC 8018 sets it out:
// SCRAM's salted password
void cli_pbkdf2_sha256(const CliDigestPasswordCfg *args);
// the SHA-1 of `size` bytes, its 20 bytes; the MD5 of them, as RFC 1321 sets it out, in lowercase hex
void cli_sha1_bytes(const CliDigestBytesSizeCfg *args);
void cli_md5_text(const CliDigestBytesCfg *args);

struct CliDigestPasswordCfg
{
    const void *password;
    unsigned long long password_size;
    const void *salt;
    unsigned long long salt_size;
    unsigned int rounds;
    unsigned char *derived;
};

struct CliDigestKeyCfg
{
    const void *key;
    unsigned long long key_size;
    const void *message;
    unsigned long long message_size;
    unsigned char *signed_digest;
};

struct CliDigestTextCfg
{
    VerbaLinea *text;
    const void *key;
    unsigned long long key_size;
    const void *message;
    unsigned long long message_size;
};

struct CliDigestBytesCfg
{
    const void *bytes;
    unsigned long long size;
    VerbaLinea *text;
};

struct CliDigestBytesSizeCfg
{
    const void *bytes;
    unsigned long long size;
    unsigned char *digest;
};

struct CliDigestShaCfg
{
    CliSha256 *sha;
    const void *bytes;
    unsigned long long size;
};

struct CliDigestCfg
{
    CliSha256 *sha;
    unsigned char *digest;
};

typedef struct
{
    void (*sha256_begin)(const CliDigestCfg *args);
    void (*sha256_end)(const CliDigestCfg *args);
} CliDigestNs;
EMBED_TABLE_LAYOUT(CliDigestNs, sha256_begin, sha256_end);
EMBED_TABLE_STORAGE CliDigestNs cli_digest EMBED_UNUSED = {
    .sha256_begin = cli_sha256_begin,
    .sha256_end = cli_sha256_end,
};

typedef struct
{
    void (*sha256_take)(const CliDigestShaCfg *args);
} CliDigestShaNs;
EMBED_TABLE_LAYOUT(CliDigestShaNs, sha256_take);
EMBED_TABLE_STORAGE CliDigestShaNs cli_digest_sha EMBED_UNUSED = {
    .sha256_take = cli_sha256_take,
};

typedef struct
{
    void (*sha256_bytes)(const CliDigestBytesSizeCfg *args);
    void (*sha1_bytes)(const CliDigestBytesSizeCfg *args);
} CliDigestBytesSizeNs;
EMBED_TABLE_LAYOUT(CliDigestBytesSizeNs, sha256_bytes, sha1_bytes);
EMBED_TABLE_STORAGE CliDigestBytesSizeNs cli_digest_bytes_size EMBED_UNUSED = {
    .sha256_bytes = cli_sha256_bytes,
    .sha1_bytes = cli_sha1_bytes,
};

typedef struct
{
    void (*sha256_text)(const CliDigestBytesCfg *args);
    void (*md5_text)(const CliDigestBytesCfg *args);
} CliDigestBytesNs;
EMBED_TABLE_LAYOUT(CliDigestBytesNs, sha256_text, md5_text);
EMBED_TABLE_STORAGE CliDigestBytesNs cli_digest_bytes EMBED_UNUSED = {
    .sha256_text = cli_sha256_text,
    .md5_text = cli_md5_text,
};

typedef struct
{
    void (*hmac_sha256_bytes)(const CliDigestKeyCfg *args);
} CliDigestKeyNs;
EMBED_TABLE_LAYOUT(CliDigestKeyNs, hmac_sha256_bytes);
EMBED_TABLE_STORAGE CliDigestKeyNs cli_digest_key EMBED_UNUSED = {
    .hmac_sha256_bytes = cli_hmac_sha256_bytes,
};

typedef struct
{
    void (*hmac_sha256_text)(const CliDigestTextCfg *args);
} CliDigestTextNs;
EMBED_TABLE_LAYOUT(CliDigestTextNs, hmac_sha256_text);
EMBED_TABLE_STORAGE CliDigestTextNs cli_digest_text EMBED_UNUSED = {
    .hmac_sha256_text = cli_hmac_sha256_text,
};

typedef struct
{
    void (*pbkdf2_sha256)(const CliDigestPasswordCfg *args);
} CliDigestPasswordNs;
EMBED_TABLE_LAYOUT(CliDigestPasswordNs, pbkdf2_sha256);
EMBED_TABLE_STORAGE CliDigestPasswordNs cli_digest_password EMBED_UNUSED = {
    .pbkdf2_sha256 = cli_pbkdf2_sha256,
};

#endif
