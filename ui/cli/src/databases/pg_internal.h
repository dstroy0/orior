// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg_internal.h: what the PostgreSQL sources share: a connection's place, its socket, what is read off it, and what
// the server reported of itself and noted
#ifndef PG_INTERNAL_H
#define PG_INTERNAL_H
typedef struct CliPgFailurePasswordCtx CliPgFailurePasswordCtx;
typedef struct CliPgFailureCtx CliPgFailureCtx;
typedef struct CliPgFailureKindCtx CliPgFailureKindCtx;
typedef struct CliPgCtx CliPgCtx;
typedef struct CliPgKindCtx CliPgKindCtx;
typedef struct CliPgBodyCtx CliPgBodyCtx;

#include "embed_dispatch_layout.h"
#include "pg.h"

// the most connections at once; the bytes read off the socket at a time; the most bytes of one message's body; the
// parameters the server reports of itself recorded, and the bytes of each; and the notices of one statement recorded,
// and the bytes of each
#define CLI_PG_LINKS 8u
#define CLI_PG_READ (1u << 16u)
#define CLI_PG_MESSAGE (1u << 22u)
#define CLI_PG_PARAMETERS 64u
#define CLI_PG_PARAMETER 256u
#define CLI_PG_NOTICES 64u
#define CLI_PG_NOTICE 1024u

struct CliPg
{
    unsigned int place;
    CliSocket socket;
    unsigned char in[CLI_PG_READ];
    unsigned long long start;
    unsigned long long end;
    unsigned char message[CLI_PG_MESSAGE];
    unsigned long long size;
    CliPgCancel cancel;
    int has_cancel;
    char status;
    char names[CLI_PG_PARAMETERS][CLI_PG_PARAMETER];
    char values[CLI_PG_PARAMETERS][CLI_PG_PARAMETER];
    unsigned int parameters;
    char notices[CLI_PG_NOTICES][CLI_PG_NOTICE];
    unsigned int notice_count;
    int read_state;
    int has_failed;
    CliDbFailure failed;
    CliDbHead head;
    CliDbValue row[CLI_DB_COLUMNS];
};

// a message read off the connection, its kind into `kind` and its body into the connection's capacity: 1, or 0 with the
// failure given; a message of `kind` sent, `count` parts of its body, each `sizes` bytes of `parts`: 1, or 0 with the
// failure given; what a server reported went wrong read from a message's body; a parameter the server reported
// recorded; and a notice it gave recorded as the reader reads a failure (pg.c)
int cli_pg_message(const CliPgKindCtx *args);
int cli_pg_send(const CliPgFailureKindCtx *args);
void cli_pg_failure_get(const CliPgCtx *args);
void cli_pg_parameter_store(const CliPgBodyCtx *args);
void cli_pg_notice_store(const CliPgFailurePasswordCtx *args);

// the capacity one sign-in's texts are made in
#define CLI_PG_SIGN_TEXT 1024u

// the client's final message of SCRAM-SHA-256, and the server's signature it must answer with, base64, each into what
// sized for CLI_PG_SIGN_TEXT: 1, or 0 with the failure given (pg_sign.c)
int cli_pg_scram_final(const CliPgFailureCtx *args);

// the startup and sign-in, as the server requires: 1, or 0 with the failure given (pg_sign.c)
int cli_pg_signed_in(const CliPgFailurePasswordCtx *args);

struct CliPgFailurePasswordCtx
{
    CliPg *pg;
    CliDbFailure *failure;
    const char *password;
    const char *host;
    unsigned int port;
    const char *user;
    const char *database;
};

struct CliPgFailureCtx
{
    CliDbFailure *failure;
    const char *password;
    const char *first_bare;
    const char *nonce;
    const char *server_first;
    char *client_final;
    char *signature;
};

struct CliPgFailureKindCtx
{
    CliPg *pg;
    CliDbFailure *failure;
    unsigned char kind_value;
    const void *const *parts;
    const unsigned long long *sizes;
    unsigned int count;
};

struct CliPgCtx
{
    CliDbFailure *failure;
    const unsigned char *body;
    unsigned long long size;
};

struct CliPgKindCtx
{
    CliPg *pg;
    unsigned char *kind;
    CliDbFailure *failure;
};

struct CliPgBodyCtx
{
    CliPg *pg;
    const unsigned char *body;
    unsigned long long size;
};

typedef struct
{
    int (*message)(const CliPgKindCtx *args);
} CliPgKindNs;
EMBED_TABLE_LAYOUT(CliPgKindNs, message);
EMBED_TABLE_STORAGE CliPgKindNs cli_pg_kind_internal EMBED_UNUSED = {
    .message = cli_pg_message,
};

typedef struct
{
    int (*send)(const CliPgFailureKindCtx *args);
} CliPgFailureKindNs;
EMBED_TABLE_LAYOUT(CliPgFailureKindNs, send);
EMBED_TABLE_STORAGE CliPgFailureKindNs cli_pg_failure_kind_internal EMBED_UNUSED = {
    .send = cli_pg_send,
};

typedef struct
{
    void (*failure_get)(const CliPgCtx *args);
} CliPgNsTable;
EMBED_TABLE_LAYOUT(CliPgNsTable, failure_get);
EMBED_TABLE_STORAGE CliPgNsTable cli_pg_internal EMBED_UNUSED = {
    .failure_get = cli_pg_failure_get,
};

typedef struct
{
    void (*parameter_store)(const CliPgBodyCtx *args);
} CliPgBodyNs;
EMBED_TABLE_LAYOUT(CliPgBodyNs, parameter_store);
EMBED_TABLE_STORAGE CliPgBodyNs cli_pg_body_internal EMBED_UNUSED = {
    .parameter_store = cli_pg_parameter_store,
};

typedef struct
{
    void (*notice_store)(const CliPgFailurePasswordCtx *args);
    int (*signed_in)(const CliPgFailurePasswordCtx *args);
} CliPgFailurePasswordNs;
EMBED_TABLE_LAYOUT(CliPgFailurePasswordNs, notice_store, signed_in);
EMBED_TABLE_STORAGE CliPgFailurePasswordNs cli_pg_failure_password_internal EMBED_UNUSED = {
    .notice_store = cli_pg_notice_store,
    .signed_in = cli_pg_signed_in,
};

typedef struct
{
    int (*scram_final)(const CliPgFailureCtx *args);
} CliPgFailureNsTable;
EMBED_TABLE_LAYOUT(CliPgFailureNsTable, scram_final);
EMBED_TABLE_STORAGE CliPgFailureNsTable cli_pg_failure_internal EMBED_UNUSED = {
    .scram_final = cli_pg_scram_final,
};

#endif
