// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg_cancel.c: what a PostgreSQL connection read of the server, its head, its state, its parameters and its notices;
// the request that stops the statement it runs, sent on a connection of its own; and a string written as PostgreSQL
// reads it back
#include "../machine/net.h"
#include "pg_internal.h"

const CliDbHead *cli_pg_head(const CliPg *pg)
{
    return &pg->head;
}

char cli_pg_status(const CliPg *pg)
{
    return pg->status;
}

const char *cli_pg_parameter(const CliPg *pg, const char *name)
{
    for (unsigned int at = 0u; at < pg->parameters; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = pg->names[at], .other = name, .cap = CLI_CELLS_POOL}))
        {
            return pg->values[at];
        }
    }
    return NULL;
}

const char *cli_pg_notice(const CliPg *pg, unsigned int at)
{
    return (at < pg->notice_count) ? pg->notices[at] : NULL;
}

int cli_pg_canceller(const CliPg *pg, CliPgCancel *cancel)
{
    if (pg->has_cancel)
    {
        *cancel = pg->cancel;
    }
    return pg->has_cancel;
}

int cli_pg_cancel_sent(const CliPgCancel *cancel, CliLine *said)
{
    char address[CLI_NAME_LONGEST + 16u];
    CliLine named = {address, sizeof(address), 0ull};
    cli_line_text(&named, cancel->host);
    cli_line_character(&named, ':');
    cli_line_decimal(&named, cancel->port, 1u);
    cli_line_closed(&named);
    CliSocket socket;
    if (!cli_net_reach(address, "stopping a PostgreSQL statement", said) ||
        !cli_socket_connected(&socket, cancel->host, cancel->port, said))
    {
        return 0;
    }
    // the request is its length, the cancel code 80877102, the backend's number and its secret, each big-endian
    unsigned char request[16u + sizeof(cancel->secret)];
    const unsigned long long length = 12ull + cancel->secret_size;
    const unsigned long long words[3] = {length, 80877102ull, cancel->pid};
    for (unsigned int word = 0u; word < 3u; word += 1u)
    {
        for (unsigned int byte = 0u; byte < 4u; byte += 1u)
        {
            request[(word * 4u) + byte] = (unsigned char)((words[word] >> (24u - (8u * byte))) & 0xffu);
        }
    }
    memor.cpy(&(MemoriaCfg){.dst = request + 12u, .src = cancel->secret, .bytes = cancel->secret_size});
    if (!cli_socket_written(&socket, request, length))
    {
        cli_socket_closed(&socket);
        cli_line_text(said, "the cancel request could not be sent");
        return 0;
    }
    // the server closes the connection once it has the request
    unsigned char rest[1];
    (void)cli_socket_some(&socket, rest, sizeof(rest));
    cli_socket_closed(&socket);
    return 1;
}

void cli_pg_literal(const char *text, CliLine *out)
{
    cli_line_character(out, '\'');
    for (const char *letter = text; *letter != '\0'; letter += 1)
    {
        cli_line_text(out, (*letter == '\'') ? "''" : "");
        if (*letter != '\'')
        {
            cli_line_character(out, *letter);
        }
    }
    cli_line_character(out, '\'');
}
