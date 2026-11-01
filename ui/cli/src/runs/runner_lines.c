// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_lines.c: a run's output and errors carried line by line as they come, each printed and stored, and the pages a
// job wrote found
#include "../command_line/cells.h"
#include "runner_internal.h"

#include "../text/text.h"

// the most bytes of one line carried whole: a longer one is printed in pieces of this size
#define CLI_CARRIED_LINE (1u << 18u)

void cli_run_line(CliRun *run, const char *stream, const char *text)
{
    const double ms = cli_run_since(run);
    cli_runs_take();
    cli_lines_put(run->lines, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    if (!run->has_first)
    {
        run->first = ms;
        run->has_first = 1;
    }
    cli_runs_give();
    const CliRunEvent event = {CLI_RUN_EVENT_LINE, run->run, NULL, stream, text, 0, 0ll, 0, NULL, 0u, NULL, ms};
    run->sink.emit(run->sink.context, &event);
}

// the bytes of the UTF-8 sequence at `at` before `end` that is whole and in its shortest form, 0 where none is
static unsigned int cli_sequence(const unsigned char *at, const unsigned char *end)
{
    const unsigned char lead = at[0];
    const unsigned int bytes = (lead < 0x80u)                         ? 1u
                               : ((lead >= 0xc2u) && (lead <= 0xdfu)) ? 2u
                               : ((lead >= 0xe0u) && (lead <= 0xefu)) ? 3u
                               : ((lead >= 0xf0u) && (lead <= 0xf4u)) ? 4u
                                                                      : 0u;
    if ((bytes == 0u) || ((unsigned long long)(end - at) < bytes))
    {
        return 0u;
    }
    for (unsigned int step = 1u; step < bytes; step += 1u)
    {
        if ((at[step] & 0xc0u) != 0x80u)
        {
            return 0u;
        }
    }
    const unsigned char second = (bytes > 1u) ? at[1] : 0x80u;
    const int shortest = !((lead == 0xe0u) && (second < 0xa0u)) && !((lead == 0xedu) && (second >= 0xa0u)) &&
                         !((lead == 0xf0u) && (second < 0x90u)) && !((lead == 0xf4u) && (second >= 0x90u));
    return shortest ? bytes : 0u;
}

// What one stream's carrier reads: its run, its pipe, its stream's name, whether it has ended, and the line it buffers
// so far, in a cell of the most a line carried whole holds, and its size
typedef struct
{
    CliRun *run;
    unsigned long long pipe;
    const char *stream;
    int ended;
    char *pending;
    unsigned long long pending_size;
} CliCarrier;

// one line of a stream printed: its `\r` and `\n` at the end left off, each byte that starts no letter read as U+FFFD,
// written into a cell of its size, a letter read as U+FFFD three bytes for one
static void cli_line_emitted(CliCarrier *carrier, const char *bytes, unsigned long long length)
{
    length = cli_text_back_past(bytes, length, "\n\r");
    const unsigned long long room = (3ull * length) + 1ull;
    char *const text = g_cli_cells->temporary_buf_alloc(room, &g_cli_runs_cells);
    if (text == NULL)
    {
        return;
    }
    VerbaLinea line = {text, room, 0ull};
    const unsigned char *at = (const unsigned char *)bytes;
    const unsigned char *const end = at + length;
    while (at < end)
    {
        const unsigned int size = cli_sequence(at, end);
        if (size == 0u)
        {
            mmgr_line_put(&line, "\xef\xbf\xbd");
            at += 1;
            continue;
        }
        mmgr_line_put_n(&line, (const char *)at, size);
        at += size;
    }
    mmgr_line_finish(&line);
    cli_run_line(carrier->run, carrier->stream, text);
    g_cli_cells->persistent_buf_release(text, &g_cli_runs_cells);
}

// the line buffered, with `size` bytes of `bytes` after it, printed: from the bytes where none is buffered, else from
// the buffer, which holds both, a line ending at the most it holds at the latest
static void cli_pending_emitted(CliCarrier *carrier, const char *bytes, unsigned long long size)
{
    if (carrier->pending_size == 0ull)
    {
        cli_line_emitted(carrier, bytes, size);
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = carrier->pending + carrier->pending_size, .src = bytes, .bytes = (size_t)size});
    cli_line_emitted(carrier, carrier->pending, carrier->pending_size + size);
    carrier->pending_size = 0ull;
}

static CliLock s_carried = CLI_LOCK_INIT;
static CliWake s_carried_wake = CLI_WAKE_INIT;

// a stream read to its end, each line printed as it is whole, a line of CLI_CARRIED_LINE bytes printed where it has
// come to that many with no end
static void cli_carried(CliCarrier *carrier)
{
    char piece[4096];
    for (;;)
    {
        const unsigned long long got = cli_spawn_read(carrier->pipe, piece, sizeof(piece));
        if (got == 0ull)
        {
            break;
        }
        unsigned long long from = 0ull;
        for (unsigned long long at = 0ull; at < got; at += 1ull)
        {
            const unsigned long long pending_size = carrier->pending_size + (at + 1ull - from);
            if ((piece[at] == '\n') || (pending_size == CLI_CARRIED_LINE))
            {
                cli_pending_emitted(carrier, piece + from, at + 1ull - from);
                from = at + 1ull;
            }
        }
        if ((got > from) && (carrier->pending != NULL))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = carrier->pending + carrier->pending_size, .src = piece + from, .bytes = (size_t)(got - from)});
            carrier->pending_size += got - from;
        }
        else if (got > from)
        {
            // a stream whose cell the pool could not meet is printed as it comes
            cli_line_emitted(carrier, piece + from, got - from);
        }
    }
    if (carrier->pending_size != 0ull)
    {
        cli_pending_emitted(carrier, "", 0ull);
    }
}

static void cli_errors_carried(void *context)
{
    CliCarrier *const carrier = (CliCarrier *)context;
    cli_carried(carrier);
    cli_lock_take(&s_carried);
    carrier->ended = 1;
    cli_wake_all(&s_carried_wake);
    cli_lock_give(&s_carried);
}

void cli_run_carry(CliRun *run, const CliSpawned *spawned)
{
    // the step's two carriers, retained as long as the step goes, each with a cell of the most a line holds
    CliCarrier out_carrier = {run, spawned->out, "stdout", 0, NULL, 0ull};
    CliCarrier err_carrier = {run, spawned->err, "stderr", 0, NULL, 0ull};
    out_carrier.pending = g_cli_cells->temporary_buf_alloc(CLI_CARRIED_LINE, &g_cli_runs_cells);
    err_carrier.pending = g_cli_cells->temporary_buf_alloc(CLI_CARRIED_LINE, &g_cli_runs_cells);
    CliCarrier *const out = &out_carrier;
    CliCarrier *const err = &err_carrier;
    CliThread errors = {cli_errors_carried, err};
    const int threaded = cli_thread_started(&errors);
    if (!threaded)
    {
        cli_carried(err);
    }
    cli_carried(out);
    cli_lock_take(&s_carried);
    while (threaded && !err->ended)
    {
        cli_wake_wait(&s_carried_wake, &s_carried);
    }
    cli_lock_give(&s_carried);
    g_cli_cells->persistent_buf_release(out_carrier.pending, &g_cli_runs_cells);
    g_cli_cells->persistent_buf_release(err_carrier.pending, &g_cli_runs_cells);
}

// a path put among the pages where it is in the tree, by its path there, once
static void cli_page_put(const char *root, const char *path, char (*views)[CLI_PATH_MAX_LEN], unsigned int *count)
{
    char full[CLI_PATH_MAX_LEN];
    char relative[CLI_PATH_MAX_LEN];
    if (!query_path_full(path, full, sizeof(full)) || !query_is_file(full))
    {
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    if (!cli_text_same(full, root, length) ||
        ((full[length] != '/') && (full[length] != '\\') && (full[length] != '\0')))
    {
        return;
    }
    cli_relative(root, full, relative);
    unsigned int place = *count;
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = views[at], .other = relative, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (*count >= CLI_VIEWS_MOST)
    {
        return;
    }
    while ((place > 0u) && (cli_text_order(views[place - 1u], relative) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = views[place],
            .src = views[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = views[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = views[place],
                            .src = relative,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_CELLS_POOL}) + 1u});
    *count += 1u;
}

// The .html files of the view folder found
typedef struct
{
    const char *root;
    const char *folder;
    char (*views)[CLI_PATH_MAX_LEN];
    unsigned int *count;
} CliPageWalk;

static void cli_page_each(void *context, const char *name, int is_folder)
{
    const CliPageWalk *const walk = (const CliPageWalk *)context;
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    if (!is_folder && (dot != NULL) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".html", .cap = CLI_CELLS_POOL})))
    {
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ull};
        mmgr_line_put(&line, walk->folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, name);
        if (mmgr_line_finish(&line) != 0ull)
        {
            cli_page_put(walk->root, path, walk->views, walk->count);
        }
    }
}

unsigned int cli_run_pages(const char *root, const char *view_out, const CliLines *lines,
                           char (*views)[CLI_PATH_MAX_LEN])
{
    unsigned int count = 0u;
    CliPageWalk walk = {root, view_out, views, &count};
    query_folder_each(view_out, cli_page_each, &walk);
    for (unsigned int index = 0u; index < lines->count; index += 1u)
    {
        const char *at = cli_lines_at(lines, index);
        while (at[0] != '\0')
        {
            at += cli_text_past_any(at, 0ull, cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}),
                                    CLI_TEXT_WHITE);
            const char *end = at;
            end += cli_text_to_any(end, 0ull, cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}),
                                   CLI_TEXT_WHITE);
            // the word trimmed of the quotes, commas and colons about it
            const char *from = at;
            const char *to = end;
            while ((from < to) &&
                   (cellul.chr(&(CatenaFinitaCfg){.src = "\"',:", .cap = CLI_CELLS_POOL, .byte = from[0]}) != NULL))
            {
                from += 1;
            }
            while ((to > from) &&
                   (cellul.chr(&(CatenaFinitaCfg){.src = "\"',:", .cap = CLI_CELLS_POOL, .byte = to[-1]}) != NULL))
            {
                to -= 1;
            }
            if (((to - from) >= 5) && (memor.cmp(&(MemoriaCfg){.src = to - 5, .other = ".html", .bytes = 5u}) == 0))
            {
                char word[CLI_PATH_MAX_LEN];
                char path[CLI_PATH_MAX_LEN];
                VerbaLinea line = {word, sizeof(word), 0ull};
                mmgr_line_put_n(&line, from, (unsigned long long)(to - from));
                if (mmgr_line_finish(&line) != 0ull)
                {
                    const int whole = (word[0] == '/') || (word[0] == '\\') || ((word[0] != '\0') && (word[1] == ':'));
                    if (whole)
                    {
                        memor.cpy(&(MemoriaCfg){
                            .dst = path,
                            .src = word,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}) + 1u});
                    }
                    else
                    {
                        cli_full(root, word, path);
                    }
                    cli_page_put(root, path, views, &count);
                }
            }
            at = end;
        }
    }
    return count;
}
