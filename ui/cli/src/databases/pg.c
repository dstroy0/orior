// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg.c: a PostgreSQL connection's messages, read off the socket through a room of their own and sent whole; a
// statement started by the simple query protocol, its rows read a page at a time, and a statement running stopped by
// a cancel request
#include "../command_line/cells.h"
#include "../machine/net.h"
#include "../text/json_tree.h"
#include "pg_internal.h"

static CliPg *s_links[CLI_PG_LINKS];
static CliLock s_links_lock = CLI_LOCK_INIT;

// `size` bytes read whole off the connection, through its room: 1, or 0 where it broke
static int pg_read(CliPg *pg, unsigned char *out, unsigned long long size)
{
    for (unsigned long long at = 0ull; at < size;)
    {
        if (pg->start == pg->end)
        {
            const long long got = cli_socket_some(&pg->socket, pg->in, sizeof(pg->in));
            if (got <= 0ll)
            {
                return 0;
            }
            pg->start = 0ull;
            pg->end = (unsigned long long)got;
        }
        const unsigned long long have = pg->end - pg->start;
        const unsigned long long piece = (have < size - at) ? have : size - at;
        if (out != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = out + at, .src = pg->in + pg->start, .bytes = (size_t)piece});
        }
        pg->start += piece;
        at += piece;
    }
    return 1;
}

int cli_pg_message(CliPg *pg, unsigned char *kind, CliDbFailure *failure)
{
    unsigned char head[5];
    if (!pg_read(pg, head, sizeof(head)))
    {
        cli_db_failure_said(failure, "the connection broke: the server closed it or did not answer");
        return 0;
    }
    const unsigned long long length = ((unsigned long long)head[1] << 24u) | ((unsigned long long)head[2] << 16u) |
                                      ((unsigned long long)head[3] << 8u) | (unsigned long long)head[4];
    const unsigned long long size = (length > 4ull) ? length - 4ull : 0ull;
    if (size > sizeof(pg->message))
    {
        // a message past the room is read past, so that the connection reads on from the next
        (void)pg_read(pg, NULL, size);
        char said[160];
        CliLine line = {said, sizeof(said), 0ull};
        cli_line_text(&line, "a message of ");
        cli_line_decimal(&line, size, 1u);
        cli_line_text(&line, " bytes is past the room orior reads one in");
        cli_line_closed(&line);
        cli_db_failure_said(failure, said);
        return 0;
    }
    if (!pg_read(pg, pg->message, size))
    {
        cli_db_failure_said(failure, "the connection broke: the server closed it or did not answer");
        return 0;
    }
    *kind = head[0];
    pg->size = size;
    return 1;
}

int cli_pg_sent(CliPg *pg, unsigned char kind, const void *const *parts, const unsigned long long *sizes,
                unsigned int count, CliDbFailure *failure)
{
    unsigned long long length = 4ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        length += sizes[at];
    }
    const unsigned char head[5] = {kind, (unsigned char)((length >> 24u) & 0xffu),
                                   (unsigned char)((length >> 16u) & 0xffu), (unsigned char)((length >> 8u) & 0xffu),
                                   (unsigned char)(length & 0xffu)};
    int sent = cli_socket_written(&pg->socket, head, sizeof(head));
    for (unsigned int at = 0u; sent && (at < count); at += 1u)
    {
        sent = cli_socket_written(&pg->socket, parts[at], sizes[at]);
    }
    if (!sent)
    {
        cli_db_failure_said(failure, "the connection broke: the server closed it");
    }
    return sent;
}

void cli_pg_failure_of(const unsigned char *body, unsigned long long size, CliDbFailure *failure)
{
    memor.set(&(MemoriaCfg){.dst = failure, .val = 0, .bytes = sizeof(*failure)});
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned char *const nul =
            memor.chr(&(MemoriaCfg){.src = body + at, .val = 0, .bytes = (size_t)(size - at)});
        const unsigned long long end = (nul != NULL) ? (unsigned long long)(nul - body) : size;
        if (end > at)
        {
            char *const field = (body[at] == 'M')   ? failure->message
                                : (body[at] == 'C') ? failure->code
                                : (body[at] == 'D') ? failure->detail
                                : (body[at] == 'H') ? failure->hint
                                                    : NULL;
            const unsigned long long room = (body[at] == 'C') ? sizeof(failure->code) : sizeof(failure->message);
            if (field != NULL)
            {
                CliLine line = {field, room, 0ull};
                cli_lossy(&line, body + at + 1u, end - at - 1u);
                cli_line_closed(&line);
            }
            if (body[at] == 'P')
            {
                // the place, where the field is a number alone, else 0
                unsigned long long value = 0ull;
                int whole = (end > at + 1u) && (end - at - 1u <= 19ull);
                for (unsigned long long digit = at + 1u; whole && (digit < end); digit += 1ull)
                {
                    whole = (body[digit] >= '0') && (body[digit] <= '9');
                    value = (value * 10ull) + (unsigned long long)(body[digit] - '0');
                }
                failure->position = whole ? value : 0ull;
            }
        }
        at = end + 1ull;
    }
}

void cli_pg_parameter_kept(CliPg *pg, const unsigned char *body, unsigned long long size)
{
    const unsigned char *const nul = memor.chr(&(MemoriaCfg){.src = body, .val = 0, .bytes = (size_t)size});
    if ((nul == NULL) || (pg->parameters == CLI_PG_PARAMETERS))
    {
        return;
    }
    const unsigned long long key = (unsigned long long)(nul - body);
    const unsigned char *const value = nul + 1;
    const unsigned char *const value_end =
        memor.chr(&(MemoriaCfg){.src = value, .val = 0, .bytes = (size_t)(size - key - 1u)});
    const unsigned long long value_size =
        (value_end != NULL) ? (unsigned long long)(value_end - value) : size - key - 1u;
    unsigned int place = pg->parameters;
    for (unsigned int at = 0u; at < pg->parameters; at += 1u)
    {
        place = ((cellul.len(&(CatenaFinitaCfg){.src = pg->names[at], .cap = CLI_CELLS_POOL}) == key) &&
                 (memor.cmp(&(MemoriaCfg){.src = pg->names[at], .other = body, .bytes = (size_t)key}) == 0))
                    ? at
                    : place;
    }
    CliLine name = {pg->names[place], CLI_PG_PARAMETER, 0ull};
    cli_lossy(&name, body, key);
    cli_line_closed(&name);
    CliLine kept = {pg->values[place], CLI_PG_PARAMETER, 0ull};
    cli_lossy(&kept, value, value_size);
    cli_line_closed(&kept);
    pg->parameters += (place == pg->parameters) ? 1u : 0u;
}

void cli_pg_notice_kept(CliPg *pg)
{
    CliDbFailure said;
    cli_pg_failure_of(pg->message, pg->size, &said);
    if (pg->notice_count < CLI_PG_NOTICES)
    {
        CliLine line = {pg->notices[pg->notice_count], CLI_PG_NOTICE, 0ull};
        cli_db_failure_written(&said, &line);
        cli_line_closed(&line);
        pg->notice_count += 1u;
    }
}

CliPg *cli_pg_connected(const char *host, unsigned int port, const char *user, const char *password,
                        const char *database, unsigned long long wait, CliDbFailure *failure)
{
    char address[CLI_NAME_LONGEST + 16u];
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine named = {address, sizeof(address), 0ull};
    cli_line_text(&named, host);
    cli_line_character(&named, ':');
    cli_line_decimal(&named, port, 1u);
    cli_line_closed(&named);
    if (!cli_net_reach(address, "a PostgreSQL connection", &said))
    {
        cli_line_closed(&said);
        cli_db_failure_said(failure, why);
        return NULL;
    }
    CliPg *pg = NULL;
    cli_lock_take(&s_links_lock);
    unsigned int at = 0u;
    while ((at < CLI_PG_LINKS) && (s_links[at] != NULL))
    {
        at += 1u;
    }
    pg = (at < CLI_PG_LINKS) ? cli_cell_taken(sizeof(CliPg), "a PostgreSQL connection") : NULL;
    if (pg != NULL)
    {
        pg->place = at;
        s_links[at] = pg;
    }
    cli_lock_give(&s_links_lock);
    if (pg == NULL)
    {
        cli_db_failure_said(failure, "orior holds as many PostgreSQL connections as it can");
        return NULL;
    }
    pg->start = 0ull;
    pg->end = 0ull;
    pg->has_cancel = 0;
    pg->status = 'I';
    pg->parameters = 0u;
    pg->notice_count = 0u;
    pg->reading = 0;
    pg->has_failed = 0;
    // the wait is at most what a socket's wait counts, in milliseconds
    const unsigned int within = (wait > 0xffffffffull) ? 0xffffffffu : (unsigned int)wait;
    if (!cli_socket_connected_within(&pg->socket, host, port, within, &said))
    {
        cli_line_closed(&said);
        cli_db_failure_said(failure, why);
        cli_lock_take(&s_links_lock);
        s_links[pg->place] = NULL;
        cli_lock_give(&s_links_lock);
        cli_cell_given(pg);
        return NULL;
    }
    cli_socket_waited(&pg->socket, within);
    if (!cli_pg_signed_in(pg, host, port, user, password, database, failure))
    {
        cli_socket_closed(&pg->socket);
        cli_lock_take(&s_links_lock);
        s_links[pg->place] = NULL;
        cli_lock_give(&s_links_lock);
        cli_cell_given(pg);
        return NULL;
    }
    cli_socket_waited(&pg->socket, 0u);
    return pg;
}

void cli_pg_closed(CliPg *pg)
{
    CliDbFailure failure;
    (void)cli_pg_sent(pg, 'X', NULL, NULL, 0u, &failure);
    cli_socket_closed(&pg->socket);
    cli_lock_take(&s_links_lock);
    s_links[pg->place] = NULL;
    cli_lock_give(&s_links_lock);
    cli_cell_given(pg);
}
