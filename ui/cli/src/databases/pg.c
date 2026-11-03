// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg.c: a PostgreSQL connection's messages, read off the socket through a capacity of their own and sent whole; a
// statement started by the simple query protocol, its rows read a page at a time, and a statement running stopped by
// a cancel request
#include "../command_line/cells.h"
#include "../machine/net.h"
#include "../text/json_tree.h"
#include "pg_internal.h"

static CliPg *s_links[CLI_PG_LINKS];

typedef struct
{
    CliPg *pg;
    unsigned char *out;
    unsigned long long size;
} PgCtx;
// `size` bytes read whole off the connection, through its capacity: 1, or 0 where it broke
EMBED_INLINE int pg_read(const PgCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        if (args->pg->start == args->pg->end)
        {
            const long long got =
                EMBED_CALL(cli_capacity_internal.socket_some, CliCapacityCtx, .sock = &args->pg->socket,
                           .bytes_out = args->pg->in, .capacity = sizeof(args->pg->in));
            if (got <= 0LL)
            {
                return 0;
            }
            args->pg->start = 0ULL;
            args->pg->end = (unsigned long long)got;
        }
        const unsigned long long have = args->pg->end - args->pg->start;
        const unsigned long long piece = (have < args->size - at) ? have : args->size - at;
        if (args->out != NULL)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->out + at, .src = args->pg->in + args->pg->start, .bytes = (size_t)piece});
        }
        args->pg->start += piece;
        at += piece;
    }
    return 1;
}

int cli_pg_message(const CliPgKindCtx *args)
{
    unsigned char *kind = args->kind;
    CliDbFailure *failure = args->failure;
    unsigned char head[5];
    if (!EMBED_CALL(pg_read, PgCtx, .pg = args->pg, .out = head, .size = sizeof(head)))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the connection broke: the server closed it or did not answer");
        return 0;
    }
    const unsigned long long length = ((unsigned long long)head[1] << 24u) | ((unsigned long long)head[2] << 16u) |
                                      ((unsigned long long)head[3] << 8u) | (unsigned long long)head[4];
    const unsigned long long size = (length > 4ULL) ? length - 4ULL : 0ULL;
    if (size > sizeof(args->pg->message))
    {
        // a message past the capacity is read past, so that the connection reads on from the next
        (void)EMBED_CALL(pg_read, PgCtx, .pg = args->pg, .out = NULL, .size = size);
        char msg_buf[160];
        VerbaLinea line = {msg_buf, sizeof(msg_buf), 0ULL};
        mmgr_line_put(&line, "a message of ");
        mmgr_line_decimal(&line, size, 1u);
        mmgr_line_put(&line, " bytes is past the capacity of the buffer orior_cli reads one in");
        mmgr_line_finish(&line);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = msg_buf);
        return 0;
    }
    if (!EMBED_CALL(pg_read, PgCtx, .pg = args->pg, .out = args->pg->message, .size = size))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the connection broke: the server closed it or did not answer");
        return 0;
    }
    *kind = head[0];
    args->pg->size = size;
    return 1;
}

int cli_pg_send(const CliPgFailureKindCtx *args)
{
    CliDbFailure *failure = args->failure;
    unsigned long long length = 4ULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        length += args->sizes[at];
    }
    const unsigned char head[5] = {args->kind_value, (unsigned char)((length >> 24u) & 0xffu),
                                   (unsigned char)((length >> 16u) & 0xffu), (unsigned char)((length >> 8u) & 0xffu),
                                   (unsigned char)(length & 0xffu)};
    int sent = EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->pg->socket,
                          .bytes = head, .size = sizeof(head));
    for (unsigned int at = 0u; sent && (at < args->count); at += 1u)
    {
        sent = EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->pg->socket,
                          .bytes = args->parts[at], .size = args->sizes[at]);
    }
    if (!sent)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the connection broke: the server closed it");
    }
    return sent;
}

void cli_pg_failure_get(const CliPgCtx *args)
{
    if (args->failure == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = args->failure, .val = 0, .bytes = sizeof(*args->failure)});
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const unsigned char *const nul =
            memor.chr(&(MemoriaCfg){.src = args->body + at, .val = 0, .bytes = (size_t)(args->size - at)});
        const unsigned long long end = (nul != NULL) ? (unsigned long long)(nul - args->body) : args->size;
        const unsigned long long kind = at;
        at = end + 1ULL;
        if (end <= kind)
        {
            continue;
        }
        char *field;
        switch (args->body[kind])
        {
        case 'M': {
            field = args->failure->message;
            break;
        }
        case 'C': {
            field = args->failure->code;
            break;
        }
        case 'D': {
            field = args->failure->detail;
            break;
        }
        case 'H': {
            field = args->failure->hint;
            break;
        }
        default: {
            field = NULL;
            break;
        }
        }
        const unsigned long long capacity =
            (args->body[kind] == 'C') ? sizeof(args->failure->code) : sizeof(args->failure->message);
        if (field != NULL)
        {
            VerbaLinea line = {field, capacity, 0ULL};
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line,
                       .text_bytes = args->body + kind + 1u, .length = end - kind - 1u);
            mmgr_line_finish(&line);
        }
        if (args->body[kind] == 'P')
        {
            // the place, where the field is a number alone, else 0
            unsigned long long value = 0ULL;
            int whole = (end > kind + 1u) && (end - kind - 1u <= 19ULL);
            unsigned long long digit = kind + 1u;
            while (whole && (digit < end))
            {
                whole = (args->body[digit] >= '0') && (args->body[digit] <= '9');
                value = (value * 10ULL) + (unsigned long long)(args->body[digit] - '0');
                digit += 1ULL;
            }
            args->failure->position = whole ? value : 0ULL;
        }
    }
}

void cli_pg_parameter_store(const CliPgBodyCtx *args)
{
    const unsigned char *const nul = memor.chr(&(MemoriaCfg){.src = args->body, .val = 0, .bytes = (size_t)args->size});
    if ((nul == NULL) || (args->pg->parameters == CLI_PG_PARAMETERS))
    {
        return;
    }
    const unsigned long long key = (unsigned long long)(nul - args->body);
    const unsigned char *const value = nul + 1;
    const unsigned char *const value_end =
        memor.chr(&(MemoriaCfg){.src = value, .val = 0, .bytes = (size_t)(args->size - key - 1u)});
    const unsigned long long value_size =
        (value_end != NULL) ? (unsigned long long)(value_end - value) : args->size - key - 1u;
    unsigned int place = args->pg->parameters;
    for (unsigned int at = 0u; at < args->pg->parameters; at += 1u)
    {
        place = ((cellul.len(&(CatenaFinitaCfg){.src = args->pg->names[at], .cap = CLI_CELLS_POOL}) == key) &&
                 (memor.cmp(&(MemoriaCfg){.src = args->pg->names[at], .other = args->body, .bytes = (size_t)key}) == 0))
                    ? at
                    : place;
    }
    VerbaLinea name = {args->pg->names[place], CLI_PG_PARAMETER, 0ULL};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &name, .text_bytes = args->body,
               .length = key);
    mmgr_line_finish(&name);
    VerbaLinea value_line = {args->pg->values[place], CLI_PG_PARAMETER, 0ULL};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &value_line, .text_bytes = value,
               .length = value_size);
    mmgr_line_finish(&value_line);
    args->pg->parameters += (place == args->pg->parameters) ? 1u : 0u;
}

void cli_pg_notice_store(const CliPgFailurePasswordCtx *args)
{
    CliDbFailure notice;
    EMBED_CALL(cli_pg_internal.failure_get, CliPgCtx, .body = args->pg->message, .size = args->pg->size,
               .failure = &notice);
    if (args->pg->notice_count < CLI_PG_NOTICES)
    {
        VerbaLinea line = {args->pg->notices[args->pg->notice_count], CLI_PG_NOTICE, 0ULL};
        EMBED_CALL(cli_db_wire_out.db_failure_write, CliDbWireOutCfg, .failure_read = &notice, .out = &line);
        mmgr_line_finish(&line);
        args->pg->notice_count += 1u;
    }
}

CliPg *cli_pg_connect(const CliPgHostCfg *args)
{
    CliDbFailure *failure = args->failure;
    char address[CLI_NAME_MAX_LEN + 16u];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    VerbaLinea named = {address, sizeof(address), 0ULL};
    mmgr_line_put(&named, args->host);
    mmgr_line_ch(&named, ':');
    mmgr_line_decimal(&named, args->port, 1u);
    mmgr_line_finish(&named);
    if (!EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = address, .purpose = "a PostgreSQL connection",
                    .err_msg = &err_msg))
    {
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = why);
        return NULL;
    }
    CliPg *pg = NULL;
    unsigned int at = 0u;
    while ((at < CLI_PG_LINKS) && (s_links[at] != NULL))
    {
        at += 1u;
    }
    pg = (at < CLI_PG_LINKS) ? g_cli_cells->persistent_buf_alloc(sizeof(CliPg), &g_cli_db_cells) : NULL;
    if (pg != NULL)
    {
        pg->place = at;
        s_links[at] = pg;
    }
    if (pg == NULL)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "orior_cli has as many PostgreSQL connections as it can");
        return NULL;
    }
    pg->start = 0ULL;
    pg->end = 0ULL;
    pg->has_cancel = 0;
    pg->status = 'I';
    pg->parameters = 0u;
    pg->notice_count = 0u;
    pg->read_state = 0;
    pg->has_failed = 0;
    // the wait is at most what a socket's wait counts, in milliseconds
    const unsigned int within = (args->wait > 0xffffffffULL) ? 0xffffffffu : (unsigned int)args->wait;
    if (!EMBED_CALL(cli_err_msg_internal.socket_connected_within, CliErrMsgCtx, .sock_out = &pg->socket,
                    .host = args->host, .port = args->port, .milliseconds = within, .err_msg = &err_msg))
    {
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = why);
        s_links[pg->place] = NULL;
        mmgr_zero_buf(pg, sizeof(*pg));
        g_cli_cells->persistent_buf_release(pg, &g_cli_db_cells);
        return NULL;
    }
    EMBED_CALL(cli_sock_internal.socket_wait, CliSockCtx, .sock = &pg->socket, .milliseconds = within);
    if (!EMBED_CALL(cli_pg_failure_password_internal.signed_in, CliPgFailurePasswordCtx, .pg = pg, .host = args->host,
                    .port = args->port, .user = args->user, .password = args->password, .database = args->database,
                    .failure = failure))
    {
        EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &pg->socket);
        s_links[pg->place] = NULL;
        mmgr_zero_buf(pg, sizeof(*pg));
        g_cli_cells->persistent_buf_release(pg, &g_cli_db_cells);
        return NULL;
    }
    EMBED_CALL(cli_sock_internal.socket_wait, CliSockCtx, .sock = &pg->socket, .milliseconds = 0u);
    return pg;
}

void cli_pg_close(const CliPgFailureSqlCfg *args)
{
    CliDbFailure failure;
    (void)EMBED_CALL(cli_pg_failure_kind_internal.send, CliPgFailureKindCtx, .pg = args->pg, .kind_value = 'X',
                     .parts = NULL, .sizes = NULL, .count = 0u, .failure = &failure);
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &args->pg->socket);
    s_links[args->pg->place] = NULL;
    mmgr_zero_buf(args->pg, sizeof(*args->pg));
    g_cli_cells->persistent_buf_release(args->pg, &g_cli_db_cells);
}
