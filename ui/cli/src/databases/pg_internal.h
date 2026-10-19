// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg_internal.h: what the PostgreSQL sources share: a connection's place, its socket, what is read off it, and what
// the server said of itself and noted
#ifndef PG_INTERNAL_H
#define PG_INTERNAL_H

#include "pg.h"

// the most connections at once; the bytes read off the socket at a time; the most bytes of one message's body; the
// parameters the server says of itself kept, and the bytes of each; and the notices of one statement kept, and the
// bytes of each
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
    int reading;
    int has_failed;
    CliDbFailure failed;
    CliDbHead head;
    CliDbValue row[CLI_DB_COLUMNS];
};

// a message read off the connection, its kind into `kind` and its body into the connection's room: 1, or 0 with the
// failure said; a message of `kind` sent, `count` parts of its body, each `sizes` bytes of `parts`: 1, or 0 with the
// failure said; what a server said went wrong read from a message's body; a parameter the server said kept; and a
// notice it gave kept as the reader reads a failure (pg.c)
int cli_pg_message(CliPg *pg, unsigned char *kind, CliDbFailure *failure);
int cli_pg_sent(CliPg *pg, unsigned char kind, const void *const *parts, const unsigned long long *sizes,
                unsigned int count, CliDbFailure *failure);
void cli_pg_failure_of(const unsigned char *body, unsigned long long size, CliDbFailure *failure);
void cli_pg_parameter_kept(CliPg *pg, const unsigned char *body, unsigned long long size);
void cli_pg_notice_kept(CliPg *pg);

// the room one sign-in's texts are made in
#define CLI_PG_SIGN_TEXT 1024u

// the client's final message of SCRAM-SHA-256, and the server's signature it must answer with, base64, each into what
// holds CLI_PG_SIGN_TEXT: 1, or 0 with the failure said (pg_sign.c)
int cli_pg_scram_final(const char *password, const char *first_bare, const char *nonce, const char *server_first,
                       char *client_final, char *signature, CliDbFailure *failure);

// the startup and sign-in, as the server asks: 1, or 0 with the failure said (pg_sign.c)
int cli_pg_signed_in(CliPg *pg, const char *host, unsigned int port, const char *user, const char *password,
                     const char *database, CliDbFailure *failure);

#endif
