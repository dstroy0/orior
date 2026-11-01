// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link.c: the window's one link to a tree on another machine: made on a thread of its own and made again as it
// drops, sooner at first and then every 30 s; the server's answers handed to the calls waiting on them by id and its
// events handed on; and how it stands reported as the event "link"
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "link_internal.h"

CliLink g_cli_link;
char *g_cli_link_line;
static char s_state_json[CLI_LINK_STANDING];
// the link's state, and the server's input, which a writer takes first
CliLock g_cli_link_lock = CLI_LOCK_INIT;
CliLock g_cli_link_writing = CLI_LOCK_INIT;
CliWake g_cli_link_changed = CLI_WAKE_INIT;

// the link's state as `{state, machine, address, message}` written into `out`
static void link_stood(const char *state, const char *detail, unsigned long long size, VerbaLinea *out)
{
    char text[CLI_PATH_MAX_LEN * 3u];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(out, "{\"state\":");
    cli_json_quoted(out, state, cellul.len(&(CatenaFinitaCfg){.src = state, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"machine\":");
    cli_link_machine(&g_cli_link.address, &line);
    cli_json_quoted(out, text, line.at);
    mmgr_line_put(out, ",\"address\":");
    line.at = 0ull;
    cli_link_address_text(&g_cli_link.address, &line);
    cli_json_quoted(out, text, line.at);
    mmgr_line_put(out, ",\"message\":");
    cli_json_quoted_lossy(out, (const unsigned char *)detail, size);
    mmgr_line_ch(out, '}');
}

// the link's state reported and stored as how it stands; only the keeper's thread reports
void cli_link_state_set(const char *state, const char *detail, unsigned long long size)
{
    VerbaLinea state_json = {s_state_json, sizeof(s_state_json), 0ull};
    link_stood(state, detail, size, &state_json);
    cli_lock_take(&g_cli_link_lock);
    memor.cpy(&(MemoriaCfg){.dst = g_cli_link.standing, .src = s_state_json, .bytes = (size_t)state_json.at});
    g_cli_link.standing_size = state_json.at;
    cli_lock_give(&g_cli_link_lock);
    g_cli_link.emit(g_cli_link.context, "link", s_state_json, state_json.at);
}

// what preparing the server reports, reported as the link's state
void cli_link_stated(void *context, const char *state, const char *detail)
{
    (void)context;
    cli_link_state_set(state, detail, cellul.len(&(CatenaFinitaCfg){.src = detail, .cap = CLI_CELLS_POOL}));
}

// what the transport prints on its errors read until it ends, its last CLI_LINK_STDERR_TAIL_LEN bytes retained
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
            if (g_cli_link.stderr_tail_size == CLI_LINK_STDERR_TAIL_LEN)
            {
                unsigned long long cut = 1ull;
                while ((cut < g_cli_link.stderr_tail_size) &&
                       (((unsigned char)g_cli_link.stderr_tail[cut] & 0xc0u) == 0x80u))
                {
                    cut += 1ull;
                }
                memor.move_down(&(MemoriaCfg){.dst = g_cli_link.stderr_tail,
                                              .src = g_cli_link.stderr_tail + cut,
                                              .bytes = (size_t)(g_cli_link.stderr_tail_size - cut)});
                g_cli_link.stderr_tail_size -= cut;
            }
            g_cli_link.stderr_tail[g_cli_link.stderr_tail_size] = piece[at];
            g_cli_link.stderr_tail_size += 1ull;
        }
        cli_lock_give(&g_cli_link_lock);
    }
    cli_lock_take(&g_cli_link_lock);
    g_cli_link.reading_errors = 0;
    cli_wake_all(&g_cli_link_changed);
    cli_lock_give(&g_cli_link_lock);
}

// the end of what the transport printed, trimmed, into `out`
void cli_link_stderr_written(VerbaLinea *out)
{
    cli_lock_take(&g_cli_link_lock);
    unsigned long long start = 0ull;
    unsigned long long end = g_cli_link.stderr_tail_size;
    start = cli_text_past_any(g_cli_link.stderr_tail, start, end, CLI_TEXT_WHITE);
    end = start + cli_text_back_past(g_cli_link.stderr_tail + start, end - start, CLI_TEXT_WHITE);
    cli_lossy(out, (const unsigned char *)g_cli_link.stderr_tail + start, end - start);
    cli_lock_give(&g_cli_link_lock);
}

// the next line the server wrote, its line's end taken off, into g_cli_link_line, `*size` bytes, `*whole` 0 where it
// was longer than g_cli_link_line fits: 1, or 0 at the output's end
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

// the call waiting on `id` answered by the line, the lock acquired: its answer, or its failure
static void link_answered(CliLinkCall *call, const char *line, unsigned long long size, int whole)
{
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char error[CLI_ERR_MSG_MAX_LEN];
    call->done = 2;
    if (!whole)
    {
        mmgr_line_put(call->err_msg, "the answer is larger than orior_cli supports");
    }
    else if (cli_lsp_member(line, size, "error", &start, &length))
    {
        const int text = cli_lsp_string(line + start, length, error, sizeof(error));
        mmgr_line_put(call->err_msg, text ? error : "the call failed");
    }
    else
    {
        call->done = 1;
        if (!cli_lsp_member(line, size, "ok", &start, &length) || !cli_json_compacted(line + start, length, call->out))
        {
            mmgr_line_put(call->out, "null");
        }
    }
    cli_wake_all(&g_cli_link_changed);
}

// a line the server wrote handed on: an event to the link's teller, an answer to the call waiting on its id
void cli_link_recv(unsigned long long size, int whole)
{
    VerbaLinea none = {NULL, 0ull, 0ull};
    if (whole && !cli_json_compacted(g_cli_link_line, size, &none))
    {
        return;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char event[CLI_NAME_MAX_LEN];
    if (whole && cli_lsp_member(g_cli_link_line, size, "event", &start, &length) &&
        cli_lsp_string(g_cli_link_line + start, length, event, sizeof(event)))
    {
        const int body = cli_lsp_member(g_cli_link_line, size, "body", &start, &length);
        g_cli_link.emit(g_cli_link.context, event, body ? (g_cli_link_line + start) : "null", body ? length : 4u);
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

void cli_link_path_word(const char *path, VerbaLinea *out)
{
    char slashed[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = slashed, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    cli_text_swapped(slashed, cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_CELLS_POOL}), '\\', '/');
    cli_link_quote(slashed, cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_CELLS_POOL}), out);
}
