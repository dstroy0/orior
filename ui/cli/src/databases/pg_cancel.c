// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg_cancel.c: what a PostgreSQL connection read of the server, its head, its state, its parameters and its notices;
// the request that stops the statement it runs, sent on a connection of its own; and a string written as PostgreSQL
// reads it back
#include "../machine/net.h"
#include "../text/text.h"
#include "pg_internal.h"

const CliDbHead *cli_pg_head(const CliPgCancelCfg *args)
{
    return &args->pg_read->head;
}

char cli_pg_status(const CliPgCancelCfg *args)
{
    return args->pg_read->status;
}

const char *cli_pg_parameter(const CliPgNameCfg *args)
{
    for (unsigned int at = 0u; at < args->pg_read->parameters; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->pg_read->names[at], .other = args->name, .cap = CLI_CELLS_POOL}))
        {
            return args->pg_read->values[at];
        }
    }
    return NULL;
}

const char *cli_pg_notice(const CliPgAtCfg *args)
{
    return (args->at < args->pg_read->notice_count) ? args->pg_read->notices[args->at] : NULL;
}

int cli_pg_canceller(const CliPgCancelCfg *args)
{
    CliPgCancel *cancel = args->cancel;
    if (args->pg_read->has_cancel)
    {
        *cancel = args->pg_read->cancel;
    }
    return args->pg_read->has_cancel;
}

int cli_pg_cancel_send(const CliPgCfg *args)
{
    char address[CLI_NAME_MAX_LEN + 16u];
    VerbaLinea named = {address, sizeof(address), 0ull};
    mmgr_line_put(&named, args->cancel_read->host);
    mmgr_line_ch(&named, ':');
    mmgr_line_decimal(&named, args->cancel_read->port, 1u);
    mmgr_line_finish(&named);
    CliSocket socket;
    if (!EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = address,
                    .purpose = "stopping a PostgreSQL statement", .err_msg = args->err_msg) ||
        !EMBED_CALL(cli_err_msg_internal.socket_connect, CliErrMsgCtx, .sock_out = &socket,
                    .host = args->cancel_read->host, .port = args->cancel_read->port, .err_msg = args->err_msg))
    {
        return 0;
    }
    // the request is its length, the cancel code 80877102, the backend's number and its secret, each big-endian
    unsigned char request[16u + sizeof(args->cancel_read->secret)];
    const unsigned long long length = 12ull + args->cancel_read->secret_size;
    const unsigned long long words[3] = {length, 80877102ull, args->cancel_read->pid};
    for (unsigned int word = 0u; word < 3u; word += 1u)
    {
        for (unsigned int byte = 0u; byte < 4u; byte += 1u)
        {
            request[(word * 4u) + byte] = (unsigned char)((words[word] >> (24u - (8u * byte))) & 0xffu);
        }
    }
    memor.cpy(
        &(MemoriaCfg){.dst = request + 12u, .src = args->cancel_read->secret, .bytes = args->cancel_read->secret_size});
    if (!EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &socket, .bytes = request,
                    .size = length))
    {
        EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &socket);
        mmgr_line_put(args->err_msg, "the cancel request could not be sent");
        return 0;
    }
    // the server closes the connection once it has the request
    unsigned char rest[1];
    (void)EMBED_CALL(cli_capacity_internal.socket_some, CliCapacityCtx, .sock = &socket, .bytes_out = rest,
                     .capacity = sizeof(rest));
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &socket);
    return 1;
}

void cli_pg_literal(const CliPgTextCfg *args)
{
    mmgr_line_ch(args->out, '\'');
    EMBED_CALL(cli_text_set_table.append_double, CliTextSetCfg, .out = args->out, .text = args->text, .set = "'");
    mmgr_line_ch(args->out, '\'');
}
