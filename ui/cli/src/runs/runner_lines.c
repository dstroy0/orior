// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_lines.c: a run's output and errors carried line by line as they come, each printed and stored, and the pages a
// job wrote found
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "runner_internal.h"

#include "../text/text.h"

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
// the most bytes of one line carried whole: a longer one is printed in pieces of this size
#define CLI_CARRIED_LINE (1u << 18u)

void cli_run_line(const CliRunnerRunStreamCtx *args)
{
    const char *text = args->text;
    const double ms = EMBED_CALL(cli_runner_internal.run_since, CliRunnerCtx, .run_read = args->run);
    cli_runs_take();
    EMBED_CALL(cli_runner_lines.lines_put, CliRunnerLinesCfg, .lines = args->run->lines, .text = text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    if (!args->run->has_first)
    {
        args->run->first = ms;
        args->run->has_first = 1;
    }
    cli_runs_give();
    const CliRunEvent event = {
        CLI_RUN_EVENT_LINE, args->run->run, NULL, args->stream, text, 0, 0ll, 0, NULL, 0u, NULL, ms};
    EMBED_CALL(args->run->sink.emit, CliEachEventCfg, .context = args->run->sink.context, .event = &event);
}

typedef struct
{
    const unsigned char *at;
    const unsigned char *end;
    CliCarrier *carrier;
    const char *bytes;
    unsigned long long length;
    unsigned long long size;
    const char *root;
    const char *path;
    char (*views)[CLI_PATH_MAX_LEN];
    unsigned int *count;
} RunnerLinesCtx;
// the bytes of the UTF-8 sequence at `at` before `end` that is whole and in its shortest form, 0 where none is
EMBED_INLINE unsigned int cli_sequence(const RunnerLinesCtx *args)
{
    const unsigned char lead = args->at[0];
    const unsigned int bytes = (lead < 0x80u)                         ? 1u
                               : ((lead >= 0xc2u) && (lead <= 0xdfu)) ? 2u
                               : ((lead >= 0xe0u) && (lead <= 0xefu)) ? 3u
                               : ((lead >= 0xf0u) && (lead <= 0xf4u)) ? 4u
                                                                      : 0u;
    if ((bytes == 0u) || ((unsigned long long)(args->end - args->at) < bytes))
    {
        return 0u;
    }
    for (unsigned int step = 1u; step < bytes; step += 1u)
    {
        if ((args->at[step] & 0xc0u) != 0x80u)
        {
            return 0u;
        }
    }
    const unsigned char second = (bytes > 1u) ? args->at[1] : 0x80u;
    const int shortest = !((lead == 0xe0u) && (second < 0xa0u)) && !((lead == 0xedu) && (second >= 0xa0u)) &&
                         !((lead == 0xf0u) && (second < 0x90u)) && !((lead == 0xf4u) && (second >= 0x90u));
    return shortest ? bytes : 0u;
}

// one line of a stream printed: its `\r` and `\n` at the end left off, each byte that starts no letter read as U+FFFD,
// written into a cell of its size, a letter read as U+FFFD three bytes for one
EMBED_INLINE void cli_line_emit(const RunnerLinesCtx *args)
{
    unsigned long long length = args->length;
    length = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = args->bytes, .at = length,
                        .set = "\n\r");
    const unsigned long long room = (3ull * length) + 1ull;
    char *const text = g_cli_cells->temporary_buf_alloc(room, &g_cli_runs_cells);
    if (text == NULL)
    {
        return;
    }
    VerbaLinea line = {text, room, 0ull};
    const unsigned char *at = (const unsigned char *)args->bytes;
    const unsigned char *const end = at + length;
    while (at < end)
    {
        const unsigned int size = EMBED_CALL(cli_sequence, RunnerLinesCtx, .at = at, .end = end);
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
    EMBED_CALL(cli_runner_run_stream_internal.run_line, CliRunnerRunStreamCtx, .run = args->carrier->run,
               .stream = args->carrier->stream, .text = text);
    g_cli_cells->persistent_buf_release(text, &g_cli_runs_cells);
}

// the line buffered, with `size` bytes of `bytes` after it, printed: from the bytes where none is buffered, else from
// the buffer, which holds both, a line ending at the most it holds at the latest
EMBED_INLINE void cli_pending_emit(const RunnerLinesCtx *args)
{
    const char *bytes = args->bytes;
    if (args->carrier->pending_size == 0ull)
    {
        EMBED_CALL(cli_line_emit, RunnerLinesCtx, .carrier = args->carrier, .bytes = bytes, .length = args->size);
        return;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = args->carrier->pending + args->carrier->pending_size, .src = bytes, .bytes = (size_t)args->size});
    EMBED_CALL(cli_line_emit, RunnerLinesCtx, .carrier = args->carrier, .bytes = args->carrier->pending,
               .length = args->carrier->pending_size + args->size);
    args->carrier->pending_size = 0ull;
}

static CliLock s_carried = CLI_LOCK_INIT;
static CliWake s_carried_wake = CLI_WAKE_INIT;

// a stream read to its end, each line printed as it is whole, a line of CLI_CARRIED_LINE bytes printed where it has
// come to that many with no end
EMBED_INLINE void cli_on_carry(const RunnerLinesCtx *args)
{
    char piece[4096];
    for (;;)
    {
        const unsigned long long got =
            EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                       .pipe_handle = args->carrier->pipe, .piece = piece, .room = sizeof(piece));
        if (got == 0ull)
        {
            break;
        }
        unsigned long long from = 0ull;
        for (unsigned long long at = 0ull; at < got; at += 1ull)
        {
            const unsigned long long pending_size = args->carrier->pending_size + (at + 1ull - from);
            if ((piece[at] == '\n') || (pending_size == CLI_CARRIED_LINE))
            {
                EMBED_CALL(cli_pending_emit, RunnerLinesCtx, .carrier = args->carrier, .bytes = piece + from,
                           .size = at + 1ull - from);
                from = at + 1ull;
            }
        }
        if ((got > from) && (args->carrier->pending != NULL))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->carrier->pending + args->carrier->pending_size,
                                    .src = piece + from,
                                    .bytes = (size_t)(got - from)});
            args->carrier->pending_size += got - from;
        }
        else if (got > from)
        {
            // a stream whose cell the pool could not meet is printed as it comes
            EMBED_CALL(cli_line_emit, RunnerLinesCtx, .carrier = args->carrier, .bytes = piece + from,
                       .length = got - from);
        }
    }
    if (args->carrier->pending_size != 0ull)
    {
        EMBED_CALL(cli_pending_emit, RunnerLinesCtx, .carrier = args->carrier, .bytes = "", .size = 0ull);
    }
}

static void cli_errors_carry(void *context)
{
    CliCarrier *const carrier = (CliCarrier *)context;
    EMBED_CALL(cli_on_carry, RunnerLinesCtx, .carrier = carrier);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_carried);
    carrier->ended = 1;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_carried_wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_carried);
}

void cli_run_carry(const CliRunnerRunSpawnedCtx *args)
{
    // the step's two carriers, retained as long as the step goes, each with a cell of the most a line holds
    CliCarrier out_carrier = {args->run, args->spawned->out, "stdout", 0, NULL, 0ull};
    CliCarrier err_carrier = {args->run, args->spawned->err, "stderr", 0, NULL, 0ull};
    out_carrier.pending = g_cli_cells->temporary_buf_alloc(CLI_CARRIED_LINE, &g_cli_runs_cells);
    err_carrier.pending = g_cli_cells->temporary_buf_alloc(CLI_CARRIED_LINE, &g_cli_runs_cells);
    CliCarrier *const out = &out_carrier;
    CliCarrier *const err = &err_carrier;
    CliThread errors = {cli_errors_carry, err};
    const int threaded = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &errors);
    if (!threaded)
    {
        EMBED_CALL(cli_on_carry, RunnerLinesCtx, .carrier = err);
    }
    EMBED_CALL(cli_on_carry, RunnerLinesCtx, .carrier = out);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_carried);
    while (threaded && !err->ended)
    {
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_carried_wake, .lock = &s_carried);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_carried);
    g_cli_cells->persistent_buf_release(out_carrier.pending, &g_cli_runs_cells);
    g_cli_cells->persistent_buf_release(err_carrier.pending, &g_cli_runs_cells);
}

// a path put among the pages where it is in the tree, by its path there, once
static void cli_page_put(const RunnerLinesCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    unsigned int *count = args->count;
    char full[CLI_PATH_MAX_LEN];
    char relative[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = path, .full = full,
                    .room = sizeof(full)) ||
        !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = full))
    {
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = full, .other_pointer = root, .size = length) ||
        ((full[length] != '/') && (full[length] != '\\') && (full[length] != '\0')))
    {
        return;
    }
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = full, .relative_out = relative);
    unsigned int place = *count;
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->views[at], .other = relative, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (*count >= CLI_VIEWS_MOST)
    {
        return;
    }
    while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->views[place - 1u],
                                       .other_pointer = relative) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->views[place],
            .src = args->views[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->views[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->views[place],
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

static void cli_page_each(const CliEachNameIsFolderCfg *args)
{
    const CliPageWalk *const walk = (const CliPageWalk *)args->context;
    const char *const dot = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->name,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}), .byte = '.');
    if (!args->is_folder && (dot != NULL) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".html", .cap = CLI_CELLS_POOL})))
    {
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ull};
        mmgr_line_put(&line, walk->folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, args->name);
        if (mmgr_line_finish(&line) != 0ull)
        {
            EMBED_CALL(cli_page_put, RunnerLinesCtx, .root = walk->root, .path = path, .views = walk->views,
                       .count = walk->count);
        }
    }
}

unsigned int cli_run_pages(const CliRunnerRootCtx *args)
{
    const char *root = args->root;
    const CliLines *lines = args->lines;
    char (*views)[CLI_PATH_MAX_LEN] = args->views;
    unsigned int count = 0u;
    CliPageWalk walk = {root, args->view_out, views, &count};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->view_out,
               .each = cli_page_each, .context_bytess = &walk);
    for (unsigned int index = 0u; index < lines->count; index += 1u)
    {
        const char *at =
            EMBED_CALL(cli_runner_lines_index.lines_at, CliRunnerLinesIndexCfg, .lines_read = lines, .index = index);
        while (at[0] != '\0')
        {
            at += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ull,
                             .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}),
                             .set = CLI_TEXT_WHITE);
            const char *end = at;
            end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ull,
                              .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}),
                              .set = CLI_TEXT_WHITE);
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
                        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = word, .joined = path);
                    }
                    EMBED_CALL(cli_page_put, RunnerLinesCtx, .root = root, .path = path, .views = views,
                               .count = &count);
                }
            }
            at = end;
        }
    }
    return count;
}
