// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql_internal.h: what the MySQL sources share: a connection's place, its socket, the packet read off it, and the
// columns of the result being read
#ifndef MYSQL_INTERNAL_H
#define MYSQL_INTERNAL_H

#include "mysql.h"

// the most connections at once; the bytes read off the socket at a time; the most bytes of one packet; and the bytes a
// row's values written in hex take
#define CLI_MYSQL_LINKS 8u
#define CLI_MYSQL_READ (1u << 16u)
#define CLI_MYSQL_PACKET (1u << 22u)
#define CLI_MYSQL_HEX (1u << 20u)

// the capabilities the client asks for, and the server's status flags it reads
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
    char version[CLI_NAME_LONGEST];
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
// failure said; `size` bytes of `payload` sent as packets of the connection's sequence: 1, or 0 with the failure said;
// what a server said went wrong read from an error packet; a length-encoded number at `at`, moving past it: 1, or 0
// where it is NULL or runs past the end; and a length-encoded string's start and length (mysql.c)
int cli_mysql_packet(CliMysql *mysql, CliDbFailure *failure);
int cli_mysql_sent(CliMysql *mysql, const unsigned char *payload, unsigned long long size, CliDbFailure *failure);
void cli_mysql_failure_of(const unsigned char *payload, unsigned long long size, CliDbFailure *failure);
int cli_mysql_lenenc(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
                     unsigned long long *value);
int cli_mysql_lenenc_bytes(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
                           unsigned long long *start, unsigned long long *length);
// the answer to the scramble `size` bytes long the password gives, caching_sha2_password's where `caching`, else
// mysql_native_password's, into `answer`, which holds 32 bytes: its length, 0 for an empty password (mysql.c)
unsigned int cli_mysql_answer(const char *password, const unsigned char *scramble, unsigned long long size, int caching,
                              unsigned char *answer);

// the handshake read and the sign-in made, as the account asks: 1, or 0 with the failure said (mysql_sign.c)
int cli_mysql_signed_in(CliMysql *mysql, const char *host, unsigned int port, const char *user, const char *password,
                        const char *database, CliDbFailure *failure);

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
int cli_big_from_be(CliBig *big, const unsigned char *bytes, unsigned long long size);
void cli_big_to_be(const CliBig *big, unsigned char *out, unsigned long long length);
int cli_big_mul(const CliBig *a, const CliBig *b, CliBig *out);
void cli_big_rem(const CliBig *big, const CliBig *modulus, CliBig *out);
int cli_big_pow_mod(const CliBig *base, const CliBig *exponent, const CliBig *modulus, CliBig *out);
void cli_mysql_mgf1(const unsigned char *seed, unsigned long long size, unsigned char *out, unsigned long long length);
void cli_mysql_oaep(const unsigned char *message, unsigned long long size, const unsigned char *seed,
                    unsigned long long length, unsigned char *padded);

// `size` bytes of `message` encrypted by the RSA key `pem` holds, `pem_size` bytes, with OAEP padding by SHA-1, into
// `out`, which holds 1024 bytes, their count into `out_size`: 1, or 0 with why said (mysql_rsa.c)
int cli_mysql_rsa_sealed(const char *pem, unsigned long long pem_size, const unsigned char *message,
                         unsigned long long size, unsigned char *out, unsigned long long *out_size, CliLine *said);

#endif
