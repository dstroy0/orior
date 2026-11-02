// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql_internal.h: what the MySQL sources share: a connection's place, its socket, the packet read off it, and the
// columns of the result being read
#ifndef MYSQL_INTERNAL_H
#define MYSQL_INTERNAL_H
typedef struct CliMysqlSizeCtx CliMysqlSizeCtx;
typedef struct CliMysqlFailurePasswordCtx CliMysqlFailurePasswordCtx;
typedef struct CliMysqlSizePasswordCtx CliMysqlSizePasswordCtx;
typedef struct CliMysqlSizeBytesCtx CliMysqlSizeBytesCtx;
typedef struct CliMysqlSizeLengthCtx CliMysqlSizeLengthCtx;
typedef struct CliMysqlOutBigCtx CliMysqlOutBigCtx;
typedef struct CliMysqlSizeBytesAtCtx CliMysqlSizeBytesAtCtx;
typedef struct CliMysqlSizeOutCtx CliMysqlSizeOutCtx;
typedef struct CliMysqlFailureCtx CliMysqlFailureCtx;
typedef struct CliMysqlCtx CliMysqlCtx;
typedef struct CliMysqlACtx CliMysqlACtx;
typedef struct CliMysqlBigCtx CliMysqlBigCtx;
typedef struct CliMysqlBigOutCtx CliMysqlBigOutCtx;

#include "embed_dispatch_layout.h"
#include "mysql.h"

// the most connections at once; the bytes read off the socket at a time; the most bytes of one packet; and the bytes a
// row's values written in hex take
#define CLI_MYSQL_LINKS 8u
#define CLI_MYSQL_READ (1u << 16u)
#define CLI_MYSQL_PACKET (1u << 22u)
#define CLI_MYSQL_HEX (1u << 20u)

// the capabilities the client requests, and the server's status flags it reads
#define CLI_MYSQL_LONG_PASSWORD 0x1u
#define CLI_MYSQL_LONG_FLAG 0x4u
#define CLI_MYSQL_CONNECT_WITH_DB 0x8u
#define CLI_MYSQL_PROTOCOL_41 0x200u
#define CLI_MYSQL_TRANSACTIONS 0x2000u
#define CLI_MYSQL_SECURE_CONNECTION 0x8000u
#define CLI_MYSQL_MULTI_RESULTS 0x20000u
#define CLI_MYSQL_PLUGIN_AUTH 0x80000u
#define CLI_MYSQL_PLUGIN_AUTH_LENENC 0x200000u
#define CLI_MYSQL_IN_TRANSACTION 0x1u
#define CLI_MYSQL_MORE_RESULTS 0x8u

struct CliMysql
{
    unsigned int place;
    CliSocket socket;
    unsigned char in[CLI_MYSQL_READ];
    unsigned long long start;
    unsigned long long end;
    unsigned char packet[CLI_MYSQL_PACKET];
    unsigned long long size;
    unsigned char sequence;
    CliMysqlCancel cancel;
    char version[CLI_NAME_MAX_LEN];
    unsigned int status;
    unsigned int warnings;
    unsigned char codes[CLI_DB_COLUMNS];
    unsigned int charsets[CLI_DB_COLUMNS];
    int reading;
    int sealed;
    CliDbHead head;
    CliDbValue row[CLI_DB_COLUMNS];
    char hex[CLI_MYSQL_HEX];
};

// a packet read, its payloads joined where it spans more than one, into the connection's room: 1, or 0 with the
// failure given; `size` bytes of `payload` sent as packets of the connection's sequence: 1, or 0 with the failure
// given; what a server reported went wrong read from an error packet; a length-encoded number at `at`, moving past it:
// 1, or 0 where it is NULL or runs past the end; and a length-encoded string's start and length (mysql.c)
int cli_mysql_packet(const CliMysqlFailureCtx *args);
int cli_mysql_send(const CliMysqlFailureCtx *args);
void cli_mysql_failure_get(const CliMysqlFailureCtx *args);
int cli_mysql_lenenc(const CliMysqlSizeBytesAtCtx *args);
int cli_mysql_lenenc_bytes(const CliMysqlSizeBytesCtx *args);
// the answer to the scramble `size` bytes long the password gives, caching_sha2_password's where `caching`, else
// mysql_native_password's, into `answer`, sized for 32 bytes: its length, 0 for an empty password (mysql.c)
unsigned int cli_mysql_answer(const CliMysqlSizePasswordCtx *args);

// the handshake read and the sign-in made, as the account requires: 1, or 0 with the failure given (mysql_sign.c)
int cli_mysql_signed_in(const CliMysqlFailurePasswordCtx *args);

// the words of a number's room
#define CLI_BIG_WORDS 520u

// A number: its words, least first, and how many of them are used, the top one not 0
typedef struct
{
    unsigned int word[CLI_BIG_WORDS];
    unsigned int count;
} CliBig;

// a number read from `size` bytes, most significant first: 1, or 0 where it does not fit its room; a number written
// into `length` bytes, most significant first; the product of two: 1, or 0 where it does not fit; the remainder of a
// number by `modulus`; and `base` to the power `exponent` by `modulus`, while no sealing runs: 1, or 0 where a product
// does not fit; the `length` bytes of the mask MGF1 makes of `size` bytes of `seed` with SHA-1; and `size` bytes of
// `message` padded by OAEP with SHA-1 to `length` bytes with the 20 bytes of `seed`, into `padded` (mysql_rsa.c)
int cli_big_from_be(const CliMysqlCtx *args);
void cli_big_to_be(const CliMysqlBigOutCtx *args);
int cli_big_mul(const CliMysqlACtx *args);
void cli_big_rem(const CliMysqlBigCtx *args);
int cli_big_pow_mod(const CliMysqlOutBigCtx *args);
void cli_mysql_mgf1(const CliMysqlSizeOutCtx *args);
void cli_mysql_oaep(const CliMysqlSizeLengthCtx *args);

// `size` bytes of `message` encrypted by the RSA key `pem` contains, `pem_size` bytes, with OAEP padding by SHA-1, into
// `out`, sized for 1024 bytes, their count into `out_size`: 1, or 0 with why given (mysql_rsa.c)
int cli_mysql_rsa_seal(const CliMysqlSizeCtx *args);

struct CliMysqlSizeCtx
{
    unsigned long long size;
    unsigned char *out;
    const unsigned char *message;
    const char *pem;
    unsigned long long pem_size;
    unsigned long long *out_size;
    VerbaLinea *err_msg;
};

struct CliMysqlFailurePasswordCtx
{
    CliMysql *mysql;
    CliDbFailure *failure;
    const char *password;
    const char *host;
    unsigned int port;
    const char *user;
    const char *database;
};

struct CliMysqlSizePasswordCtx
{
    unsigned long long size;
    const char *password;
    const unsigned char *scramble;
    int caching;
    unsigned char *answer;
};

struct CliMysqlSizeBytesCtx
{
    unsigned long long size;
    const unsigned char *bytes;
    unsigned long long *at;
    unsigned long long *start;
    unsigned long long *length_pointer;
};

struct CliMysqlSizeLengthCtx
{
    unsigned long long size;
    unsigned long long length;
    const unsigned char *seed;
    const unsigned char *message;
    unsigned char *padded;
};

struct CliMysqlOutBigCtx
{
    CliBig *out_big_pointer;
    const CliBig *modulus;
    const CliBig *base;
    const CliBig *exponent;
};

struct CliMysqlSizeBytesAtCtx
{
    unsigned long long size;
    const unsigned char *bytes;
    unsigned long long *at;
    unsigned long long *value;
};

struct CliMysqlSizeOutCtx
{
    unsigned long long size;
    unsigned char *out;
    unsigned long long length;
    const unsigned char *seed;
};

struct CliMysqlFailureCtx
{
    CliMysql *mysql;
    CliDbFailure *failure;
    const unsigned char *payload;
    unsigned long long size;
};

struct CliMysqlCtx
{
    unsigned long long size;
    const unsigned char *bytes;
    CliBig *big_out;
};

struct CliMysqlACtx
{
    const CliBig *a;
    const CliBig *b;
    CliBig *out_big_pointer;
};

struct CliMysqlBigCtx
{
    const CliBig *big;
    CliBig *out_big_pointer;
    const CliBig *modulus;
};

struct CliMysqlBigOutCtx
{
    const CliBig *big;
    unsigned char *out;
    unsigned long long length;
};

typedef struct
{
    int (*packet)(const CliMysqlFailureCtx *args);
    int (*send)(const CliMysqlFailureCtx *args);
    void (*failure_get)(const CliMysqlFailureCtx *args);
} CliMysqlFailureNsTable;
EMBED_TABLE_LAYOUT(CliMysqlFailureNsTable, packet, send, failure_get);
EMBED_TABLE_STORAGE CliMysqlFailureNsTable cli_mysql_failure_internal EMBED_UNUSED = {
    .packet = cli_mysql_packet,
    .send = cli_mysql_send,
    .failure_get = cli_mysql_failure_get,
};

typedef struct
{
    int (*lenenc)(const CliMysqlSizeBytesAtCtx *args);
} CliMysqlSizeBytesAtNs;
EMBED_TABLE_LAYOUT(CliMysqlSizeBytesAtNs, lenenc);
EMBED_TABLE_STORAGE CliMysqlSizeBytesAtNs cli_mysql_size_bytes_at_internal EMBED_UNUSED = {
    .lenenc = cli_mysql_lenenc,
};

typedef struct
{
    int (*lenenc_bytes)(const CliMysqlSizeBytesCtx *args);
} CliMysqlSizeBytesNs;
EMBED_TABLE_LAYOUT(CliMysqlSizeBytesNs, lenenc_bytes);
EMBED_TABLE_STORAGE CliMysqlSizeBytesNs cli_mysql_size_bytes_internal EMBED_UNUSED = {
    .lenenc_bytes = cli_mysql_lenenc_bytes,
};

typedef struct
{
    unsigned int (*answer)(const CliMysqlSizePasswordCtx *args);
} CliMysqlSizePasswordNs;
EMBED_TABLE_LAYOUT(CliMysqlSizePasswordNs, answer);
EMBED_TABLE_STORAGE CliMysqlSizePasswordNs cli_mysql_size_password_internal EMBED_UNUSED = {
    .answer = cli_mysql_answer,
};

typedef struct
{
    int (*signed_in)(const CliMysqlFailurePasswordCtx *args);
} CliMysqlFailurePasswordNs;
EMBED_TABLE_LAYOUT(CliMysqlFailurePasswordNs, signed_in);
EMBED_TABLE_STORAGE CliMysqlFailurePasswordNs cli_mysql_failure_password_internal EMBED_UNUSED = {
    .signed_in = cli_mysql_signed_in,
};

typedef struct
{
    int (*big_from_be)(const CliMysqlCtx *args);
} CliMysqlNsTable;
EMBED_TABLE_LAYOUT(CliMysqlNsTable, big_from_be);
EMBED_TABLE_STORAGE CliMysqlNsTable cli_mysql_internal EMBED_UNUSED = {
    .big_from_be = cli_big_from_be,
};

typedef struct
{
    void (*big_to_be)(const CliMysqlBigOutCtx *args);
} CliMysqlBigOutNs;
EMBED_TABLE_LAYOUT(CliMysqlBigOutNs, big_to_be);
EMBED_TABLE_STORAGE CliMysqlBigOutNs cli_mysql_big_out_internal EMBED_UNUSED = {
    .big_to_be = cli_big_to_be,
};

typedef struct
{
    int (*big_mul)(const CliMysqlACtx *args);
} CliMysqlANs;
EMBED_TABLE_LAYOUT(CliMysqlANs, big_mul);
EMBED_TABLE_STORAGE CliMysqlANs cli_mysql_a_internal EMBED_UNUSED = {
    .big_mul = cli_big_mul,
};

typedef struct
{
    void (*big_rem)(const CliMysqlBigCtx *args);
} CliMysqlBigNs;
EMBED_TABLE_LAYOUT(CliMysqlBigNs, big_rem);
EMBED_TABLE_STORAGE CliMysqlBigNs cli_mysql_big_internal EMBED_UNUSED = {
    .big_rem = cli_big_rem,
};

typedef struct
{
    int (*big_pow_mod)(const CliMysqlOutBigCtx *args);
} CliMysqlOutBigNs;
EMBED_TABLE_LAYOUT(CliMysqlOutBigNs, big_pow_mod);
EMBED_TABLE_STORAGE CliMysqlOutBigNs cli_mysql_out_big_internal EMBED_UNUSED = {
    .big_pow_mod = cli_big_pow_mod,
};

typedef struct
{
    void (*mgf1)(const CliMysqlSizeOutCtx *args);
} CliMysqlSizeOutNs;
EMBED_TABLE_LAYOUT(CliMysqlSizeOutNs, mgf1);
EMBED_TABLE_STORAGE CliMysqlSizeOutNs cli_mysql_size_out_internal EMBED_UNUSED = {
    .mgf1 = cli_mysql_mgf1,
};

typedef struct
{
    void (*oaep)(const CliMysqlSizeLengthCtx *args);
} CliMysqlSizeLengthNs;
EMBED_TABLE_LAYOUT(CliMysqlSizeLengthNs, oaep);
EMBED_TABLE_STORAGE CliMysqlSizeLengthNs cli_mysql_size_length_internal EMBED_UNUSED = {
    .oaep = cli_mysql_oaep,
};

typedef struct
{
    int (*rsa_seal)(const CliMysqlSizeCtx *args);
} CliMysqlSizeNs;
EMBED_TABLE_LAYOUT(CliMysqlSizeNs, rsa_seal);
EMBED_TABLE_STORAGE CliMysqlSizeNs cli_mysql_size_internal EMBED_UNUSED = {
    .rsa_seal = cli_mysql_rsa_seal,
};

#endif
