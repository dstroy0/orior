// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_http.c: one request sent over HTTP/1.1 from this process, through the system's TLS for https, and its
// answer read whole: each answer of 1xx but 101 passed over, a chunked body, one of a length and one read to the
// connection's end, the most of it CLI_REQUESTS_BODY and the rest cut; gzip and deflate undone; and how long each
// part took
#include "../command_line/cells.h"
#include "../comms/tls.h"
#include "../machine/net.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "requests_internal.h"

// A connection: its socket, its TLS session where it is https, and the work state its bytes are read into
typedef struct
{
    CliSocket socket;
    CliTls *tls;
    CliRequestsWorkState *work_state;
} HttpConnection;
// how long a connection may take to open, and an answer to come, in milliseconds
#define CLI_REQUESTS_CONNECTING 15000u
#define CLI_REQUESTS_ANSWERING 60000u

// the longest line of a head, and the most read from the connection at once
#define CLI_REQUESTS_LINE (64u << 10u)
#define CLI_REQUESTS_CHUNK (16u << 10u)

typedef struct
{
    unsigned long long microseconds;
    HttpConnection *connection;
    VerbaLinea *err_msg;
    unsigned long long *line;
    unsigned long long *size;
    CliRequestsWorkState *work_state;
    unsigned long long put;
    unsigned long long length;
    int *cut;
    unsigned long long step;
    const char *host;
} RequestsHttpCtx;
