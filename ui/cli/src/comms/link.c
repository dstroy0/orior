// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link.c: the window's one link to a tree on another machine: made on a thread of its own and made again as it
// drops, sooner at first and then every 30 s; the server's answers handed to the calls waiting on them by id and its
// events handed on; and how it stands told as the event "link"
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "link_internal.h"

CliLink g_cli_link;
char *g_cli_link_line;
static char s_told[CLI_LINK_STANDING];
// the link's state, and the server's input, which a writer takes first
CliLock g_cli_link_lock = CLI_LOCK_INIT;
CliLock g_cli_link_writing = CLI_LOCK_INIT;
CliWake g_cli_link_changed = CLI_WAKE_INIT;

// the link's state as `{state, machine, address, said}` written into `out`
static void link_stood(const char *state, const char *said, unsigned long long size, CliLine *out)
{
    char text[CLI_PATH_LONGEST * 3u];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_text(out, "{\"state\":");
    cli_json_quoted(out, state, cellul.len(&(CatenaFinitaCfg){.src = state, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"machine\":");
    cli_link_machine(&g_cli_link.address, &line);
    cli_json_quoted(out, text, line.at);
    cli_line_text(out, ",\"address\":");
    line.at = 0ull;
    cli_link_address_text(&g_cli_link.address, &line);
    cli_json_quoted(out, text, line.at);
    cli_line_text(out, ",\"said\":");
    cli_json_quoted_lossy(out, (const unsigned char *)said, size);
    cli_line_character(out, '}');
}

// the link's state told and kept as how it stands; only the keeper's thread tells
void cli_link_tell(const char *state, const char *said, unsigned long long size)
{
    CliLine told = {s_told, sizeof(s_told), 0ull};
    link_stood(state, said, size, &told);
    cli_lock_take(&g_cli_link_lock);
    memor.cpy(&(MemoriaCfg){.dst = g_cli_link.standing, .src = s_told, .bytes = (size_t)told.at});
    g_cli_link.standing_size = told.at;
    cli_lock_give(&g_cli_link_lock);
    g_cli_link.told(g_cli_link.context, "link", s_told, told.at);
}

// what preparing the server tells, told as the link's state
void cli_link_stated(void *context, const char *state, const char *said)
{
    (void)context;
    cli_link_tell(state, said, cellul.len(&(CatenaFinitaCfg){.src = said, .cap = CLI_CELLS_POOL}));
}

// what the transport says on its errors read until it ends, its last CLI_LINK_SAID bytes kept
void cli_link_errors_read(void *context)
{
    (void)context;
    char piece[4096];
    for (;;)
    {
        const unsigned long long count = cli_spawn_read(g_cli_link.spawned.err, piece, sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        cli_lock_take(&g_cli_link_lock);
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            if (g_cli_link.said_size == CLI_LINK_SAID)
            {
                unsigned long long cut = 1ull;
                while ((cut < g_cli_link.said_size) && (((unsigned char)g_cli_link.said[cut] & 0xc0u) == 0x80u))
                {
                    cut += 1ull;
                }
                memor.move_down(&(MemoriaCfg){.dst = g_cli_link.said,
                                              .src = g_cli_link.said + cut,
                                              .bytes = (size_t)(g_cli_link.said_size - cut)});
                g_cli_link.said_size -= cut;
            }
            g_cli_link.said[g_cli_link.said_size] = piece[at];
            g_cli_link.said_size += 1ull;
        }
        cli_lock_give(&g_cli_link_lock);
    }
    cli_lock_take(&g_cli_link_lock);
    g_cli_link.reading_errors = 0;
    cli_wake_all(&g_cli_link_changed);
    cli_lock_give(&g_cli_link_lock);
}

// the end of what the transport said, trimmed, into `out`
void cli_link_said(CliLine *out)
{
    cli_lock_take(&g_cli_link_lock);
    unsigned long long start = 0ull;
    unsigned long long end = g_cli_link.said_size;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\v\f", .cap = CLI_CELLS_POOL, .byte = g_cli_link.said[start]}) != NULL) &&
           (g_cli_link.said[start] != '\0'))
    {
        start += 1u;
    }
    while ((end > start) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\v\f", .cap = CLI_CELLS_POOL, .byte = g_cli_link.said[end - 1u]}) != NULL) &&
           (g_cli_link.said[end - 1u] != '\0'))
    {
        end -= 1u;
    }
    cli_lossy(out, (const unsigned char *)g_cli_link.said + start, end - start);
    cli_lock_give(&g_cli_link_lock);
}

// the next line the server wrote, its line's end taken off, into g_cli_link_line, `*size` bytes, `*whole` 0 where it
// was longer than g_cli_link_line holds: 1, or 0 at the output's end
int cli_link_line(unsigned long long *size, int *whole)
{
    *size = 0ull;
    *whole = 1;
    int any = 0;
    for (;;)
    {
        if (g_cli_link.piece_at == g_cli_link.piece_end)
        {
            const unsigned long long count =
                cli_spawn_read(g_cli_link.spawned.out, g_cli_link.piece, sizeof(g_cli_link.piece));
            if (count == 0ull)
            {
                return any;
            }
            g_cli_link.piece_at = 0ull;
            g_cli_link.piece_end = count;
        }
        any = 1;
        const char *const from = g_cli_link.piece + g_cli_link.piece_at;
        const char *const newline = memor.chr(
            &(MemoriaCfg){.src = from, .val = '\n', .bytes = (size_t)(g_cli_link.piece_end - g_cli_link.piece_at)});
        const unsigned long long take =
            (newline != NULL) ? (unsigned long long)(newline - from) : (g_cli_link.piece_end - g_cli_link.piece_at);
        const unsigned long long fits = (take < CLI_LINK_LINE - *size) ? take : (CLI_LINK_LINE - *size);
        memor.cpy(&(MemoriaCfg){.dst = g_cli_link_line + *size, .src = from, .bytes = (size_t)fits});
        *size += fits;
        *whole = *whole && (fits == take);
        g_cli_link.piece_at += take;
        if (newline != NULL)
        {
            g_cli_link.piece_at += 1u;
            *size -= (*whole && (*size > 0u) && (g_cli_link_line[*size - 1u] == '\r')) ? 1u : 0u;
            return 1;
        }
    }
}

// the id a line gives as an unsigned integer into `id`: 1, or 0 where it gives none
static int link_id(const char *line, unsigned long long size, unsigned long long *id)
{
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    if (!cli_lsp_member(line, size, "id", &start, &length) || (length == 0u))
    {
        return 0;
    }
    *id = 0ull;
    for (unsigned long long at = start; at < start + length; at += 1u)
    {
        const char letter = line[at];
        if ((letter < '0') || (letter > '9') ||
            (*id > (0xffffffffffffffffull - (unsigned long long)(letter - '0')) / 10u))
        {
            return 0;
        }
        *id = (*id * 10u) + (unsigned long long)(letter - '0');
    }
    return (length == 1u) || (line[start] != '0');
}

// the call waiting on `id` answered by the line, the lock held: its answer, or its failure
static void link_answered(CliLinkCall *call, const char *line, unsigned long long size, int whole)
{
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char error[CLI_SAID_LONGEST];
    call->done = 2;
    if (!whole)
    {
        cli_line_text(call->said, "the answer is larger than orior holds");
    }
    else if (cli_lsp_member(line, size, "error", &start, &length))
    {
        const int text = cli_lsp_string(line + start, length, error, sizeof(error));
        cli_line_text(call->said, text ? error : "the call failed");
    }
    else
    {
        call->done = 1;
        if (!cli_lsp_member(line, size, "ok", &start, &length) || !cli_json_compacted(line + start, length, call->out))
        {
            cli_line_text(call->out, "null");
        }
    }
    cli_wake_all(&g_cli_link_changed);
}

// a line the server wrote handed on: an event to the link's teller, an answer to the call waiting on its id
void cli_link_heard(unsigned long long size, int whole)
{
    CliLine none = {NULL, 0ull, 0ull};
    if (whole && !cli_json_compacted(g_cli_link_line, size, &none))
    {
        return;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char event[CLI_NAME_LONGEST];
    if (whole && cli_lsp_member(g_cli_link_line, size, "event", &start, &length) &&
        cli_lsp_string(g_cli_link_line + start, length, event, sizeof(event)))
    {
        const int body = cli_lsp_member(g_cli_link_line, size, "body", &start, &length);
        g_cli_link.told(g_cli_link.context, event, body ? (g_cli_link_line + start) : "null", body ? length : 4u);
        return;
    }
    unsigned long long id = 0ull;
    if (!link_id(g_cli_link_line, size, &id))
    {
        return;
    }
    cli_lock_take(&g_cli_link_lock);
    for (unsigned int at = 0u; at < CLI_LINK_CALLS; at += 1u)
    {
        CliLinkCall *const call = &g_cli_link.calls[at];
        if (call->used && !call->done && (call->id == id))
        {
            link_answered(call, g_cli_link_line, size, whole);
            break;
        }
    }
    cli_lock_give(&g_cli_link_lock);
}
