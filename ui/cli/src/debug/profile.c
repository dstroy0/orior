// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// profile.c: a profile run: the program started under the shim, here or by `ssh` on another machine, the shim given on
// its input, each line it writes reported as output or as the counts so far, and its end reported
#include "profile.h"
#include "../command_line/callback_cfg.h"
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"

// A profile: whether its place is taken, where it reports, its process and whether it still waits to be waited on, its
// readers still reading, under the profiles' lock, the code the shim reported the program ended with, and the cells its
// words and the PATH it runs with are allocated in, taken with its place and given back with it
typedef struct
{
    int taken;
    CliJsonSink sink;
    CliSpawned spawned;
    int running;
    unsigned int going;
    int has_code;
    long long code;
    CliThread output_thread;
    CliThread error_thread;
    CliWords *words;
    char *path;
} CliProfile;
typedef struct
{
    CliProfile *profile;
    const char *line;
    unsigned long long size;
} CliProfileLineSizeCfg;
// the profiles open at once, the one running and the one before it ending; the longest line read whole, and the bytes
// a line's cell is first taken with
#define CLI_PROFILES 2u
#define CLI_PROFILE_LINE (1u << 20u)
#define CLI_PROFILE_LINE_FIRST 4096ull

// The start of the lines the shim writes its counts on
#define CLI_PROFILE_MARK "\x1eorior-profile "

static CliProfile s_profiles[CLI_PROFILES];
static CliProfile *s_running;
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;

typedef struct
{
    CliProfile *profile;
    const char *line;
    unsigned long long size;
    int error;
    unsigned long long pipe;
    void (*each_line)(const CliProfileLineSizeCfg *args);
    CliJsonSink sink;
    VerbaLinea *out;
    const char *text;
} ProfileCtx;
// a profile's cells given back, under the profiles' lock
EMBED_INLINE void profile_cells_free(const ProfileCtx *args)
{
    g_cli_cells->persistent_buf_release(args->profile->words, &g_cli_profile_cells);
    g_cli_cells->persistent_buf_release(args->profile->path, &g_cli_profile_cells);
    args->profile->words = NULL;
    args->profile->path = NULL;
}

void cli_profile_stop(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    CliProfile *const profile = s_running;
    if ((profile != NULL) && profile->running)
    {
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &profile->spawned);
        (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &profile->spawned);
        profile->running = 0;
    }
    s_running = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// a line the program wrote reported as output, to its errors where `error`, in a cell of the most its quoting takes,
// each byte at most `\u` and four digits
EMBED_INLINE void profile_output_emit(const ProfileCtx *args)
{
    const unsigned long long room = (args->size * 6ull) + 64ull;
    char *const json_buf = g_cli_cells->temporary_buf_alloc(room, &g_cli_profile_cells);
    if (json_buf == NULL)
    {
        return;
    }
    VerbaLinea out = {json_buf, room, 0ull};
    mmgr_line_put(&out, "{\"kind\":\"output\",\"text\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->line, .length = args->size);
    mmgr_line_put(&out, args->error ? ",\"error\":true}" : ",\"error\":false}");
    if (mmgr_line_ok(&out))
    {
        EMBED_CALL(args->profile->sink.emit, CliEachJsonSizeCfg, .context = args->profile->sink.context,
                   .json = out.out, .size = out.at);
    }
    g_cli_cells->persistent_buf_release(json_buf, &g_cli_profile_cells);
}

// a line of the program's output received: the counts so far where it starts with the mark, the code the program ended
// with recorded where the counts report it is done, and any other line reported as output
static void profile_line(const CliProfileLineSizeCfg *args)
{
    CliProfile *profile = args->profile;
    const char *line = args->line;
    unsigned long long size = args->size;
    const unsigned long long mark = sizeof(CLI_PROFILE_MARK) - 1ull;
    if ((size < mark) || (memor.cmp(&(MemoriaCfg){.src = line, .other = CLI_PROFILE_MARK, .bytes = mark}) != 0))
    {
        EMBED_CALL(profile_output_emit, ProfileCtx, .profile = profile, .line = line, .size = size, .error = 0);
        return;
    }
    const char *const json = line + mark;
    const unsigned long long length = size - mark;
    // the counts compacted are no longer than they are written
    const unsigned long long room = length + 64ull;
    char *const json_buf = g_cli_cells->temporary_buf_alloc(room, &g_cli_profile_cells);
    if (json_buf == NULL)
    {
        return;
    }
    VerbaLinea out = {json_buf, room, 0ull};
    mmgr_line_put(&out, "{\"kind\":\"counts\",\"counts\":");
    if (!EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg, .bytes = json, .size = length) ||
        !EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = json, .size = length, .out = &out))
    {
        mmgr_line_put(&out, "null");
    }
    mmgr_line_ch(&out, '}');
    unsigned long long at = 0ull;
    unsigned long long done_size = 0ull;
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = json, .size_value = length, .name = "done", .start = &at,
                   .length_pointer = &done_size) &&
        (done_size == 4ull) && (memor.cmp(&(MemoriaCfg){.src = json + at, .other = "true", .bytes = 4u}) == 0) &&
        EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = json, .size_value = length, .name = "code", .start = &at,
                   .length_pointer = &done_size))
    {
        long long code = 0ll;
        int whole = (done_size > 0ull);
        const int negative = whole && (json[at] == '-');
        for (unsigned long long letter = at + (negative ? 1ull : 0ull); whole && (letter < at + done_size);
             letter += 1ull)
        {
            whole = (json[letter] >= '0') && (json[letter] <= '9') && (code < 922337203685477580ll);
            code = (code * 10ll) + (long long)(json[letter] - '0');
        }
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
        profile->has_code = whole;
        profile->code = negative ? -code : code;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    }
    if (mmgr_line_ok(&out))
    {
        EMBED_CALL(profile->sink.emit, CliEachJsonSizeCfg, .context = profile->sink.context, .json = out.out,
                   .size = out.at);
    }
    g_cli_cells->persistent_buf_release(json_buf, &g_cli_profile_cells);
}

// the lines of a pipe read until it ends, each in one cell of CLI_PROFILE_LINE, handed on with its "\r" before "\n"
// taken off, a longer line handed on in pieces of that length
static void profile_lines(const ProfileCtx *args)
{
    char *const line = g_cli_cells->temporary_buf_alloc(CLI_PROFILE_LINE, &g_cli_profile_cells);
    const unsigned long long room = (line != NULL) ? CLI_PROFILE_LINE : 0ull;
    unsigned long long used = 0ull;
    char piece[4096];
    for (;;)
    {
        const unsigned long long count = EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                                                    .pipe_handle = args->pipe, .piece = piece, .room = sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        unsigned long long at = 0ull;
        while ((room != 0ull) && (at < count))
        {
            const unsigned long long stop =
                EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = piece, .at = at, .size = count).stop;
            const unsigned long long left = room - used;
            const unsigned long long fits = ((stop - at) < left) ? (stop - at) : left;
            memor.cpy(&(MemoriaCfg){.dst = line + used, .src = piece + at, .bytes = (size_t)fits});
            used += fits;
            at += fits;
            if (at < stop)
            {
                // a line past the room is handed on as far as it reaches, the rest as a line after it
                EMBED_CALL(args->each_line, CliProfileLineSizeCfg, .profile = args->profile, .line = line,
                           .size = used);
                used = 0ull;
            }
            else if (stop < count)
            {
                EMBED_CALL(args->each_line, CliProfileLineSizeCfg, .profile = args->profile, .line = line,
                           .size = ((used > 0ull) && (line[used - 1ull] == '\r')) ? used - 1ull : used);
                used = 0ull;
                at = stop + 1ull;
            }
        }
    }
    if (used > 0ull)
    {
        EMBED_CALL(args->each_line, CliProfileLineSizeCfg, .profile = args->profile, .line = line, .size = used);
    }
    g_cli_cells->persistent_buf_release(line, &g_cli_profile_cells);
}

// a line the program wrote to its errors reported as output
static void profile_error_line(const CliProfileLineSizeCfg *args)
{
    CliProfile *profile = args->profile;
    const char *line = args->line;
    unsigned long long size = args->size;
    EMBED_CALL(profile_output_emit, ProfileCtx, .profile = profile, .line = line, .size = size, .error = 1);
}

// a reader's end: reported to the profile, and its place given back with the last
EMBED_INLINE void profile_reader_end(const ProfileCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    args->profile->going -= 1u;
    if (args->profile->going == 0u)
    {
        EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &args->profile->spawned);
        profile_cells_free(args);
        args->profile->taken = 0;
        EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_wake);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// what the program writes to its errors read until it ends
static void profile_errors_read(void *context)
{
    CliProfile *const profile = (CliProfile *)context;
    EMBED_CALL(profile_lines, ProfileCtx, .profile = profile, .pipe = profile->spawned.err,
               .each_line = profile_error_line);
    EMBED_CALL(profile_reader_end, ProfileCtx, .profile = profile);
}

// what the program writes read until it ends, then its errors waited for, its process waited on where it was not
// stopped, and its end reported: the code it ended with, and that it ended, ended with another code, or was stopped
static void profile_output_read(void *context)
{
    CliProfile *const profile = (CliProfile *)context;
    EMBED_CALL(profile_lines, ProfileCtx, .profile = profile, .pipe = profile->spawned.out, .each_line = profile_line);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    while (profile->going > 1u)
    {
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_wake, .lock = &s_lock);
    }
    long long status = 0ll;
    int has_status = 0;
    if (profile->running)
    {
        status = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &profile->spawned);
        has_status = (status >= 0ll);
        profile->running = 0;
    }
    const int has_code = profile->has_code || has_status;
    const long long code = profile->has_code ? profile->code : status;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    char done[256];
    VerbaLinea out = {done, sizeof(done), 0ull};
    mmgr_line_put(&out, "{\"kind\":\"done\",\"code\":");
    if (has_code)
    {
        mmgr_line_decimal_signed(&out, code);
    }
    else
    {
        mmgr_line_put(&out, "null");
    }
    mmgr_line_put(&out, ",\"message\":\"");
    if (!has_code)
    {
        mmgr_line_put(&out, "The run was stopped.");
    }
    else if (code == 0ll)
    {
        mmgr_line_put(&out, "The run ended.");
    }
    else
    {
        mmgr_line_put(&out, "The run ended with exit code ");
        mmgr_line_decimal_signed(&out, code);
        mmgr_line_ch(&out, '.');
    }
    mmgr_line_put(&out, "\"}");
    EMBED_CALL(profile->sink.emit, CliEachJsonSizeCfg, .context = profile->sink.context, .json = out.out,
               .size = out.at);
    EMBED_CALL(profile_reader_end, ProfileCtx, .profile = profile);
}

// a profile's place taken once one is free, made empty
EMBED_INLINE CliProfile *profile_take(const ProfileCtx *args)
{
    CliJsonSink sink = args->sink;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    CliProfile *profile = NULL;
    while (profile == NULL)
    {
        for (unsigned int at = 0u; (at < CLI_PROFILES) && (profile == NULL); at += 1u)
        {
            profile = s_profiles[at].taken ? NULL : &s_profiles[at];
        }
        if (profile == NULL)
        {
            EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_wake, .lock = &s_lock);
        }
    }
    profile->taken = 1;
    profile->sink = sink;
    profile->running = 0;
    profile->going = 0u;
    profile->has_code = 0;
    profile->code = 0ll;
    memor.set(&(MemoriaCfg){.dst = &profile->spawned, .val = 0, .bytes = sizeof(profile->spawned)});
    profile->words = g_cli_cells->persistent_buf_alloc(sizeof(*profile->words), &g_cli_profile_cells);
    profile->path = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (profile->words != NULL)
    {
        profile->words->count = 0u;
        profile->words->used = 0u;
        profile->words->full = 0;
    }
    return profile;
}

// a profile's place given back where its process did not start
EMBED_INLINE void profile_let_go(const ProfileCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    profile_cells_free(args);
    args->profile->taken = 0;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// a text written as one word of a shell, in single quotes, each quote of it closed, escaped and opened again
EMBED_INLINE void profile_quote(const ProfileCtx *args)
{
    VerbaLinea *out = args->out;
    const char *text = args->text;
    mmgr_line_ch(out, '\'');
    EMBED_CALL(cli_text_set_table.put_replace, CliTextSetCfg, .out = out, .text = text, .set = "'", .with = "'\\''");
    mmgr_line_ch(out, '\'');
}

int cli_profile_start(const CliProfileCfg *args)
{
    cli_profile_stop();
    const char *trimmed = (args->remote != NULL) ? args->remote : "";
    trimmed += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = trimmed, .at = 0ull,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = trimmed, .cap = CLI_CELLS_POOL}), .set = " \t");
    const int far = (*trimmed != '\0');
    const char *const colon =
        far ? cellul.chr(&(CatenaFinitaCfg){.src = args->remote, .cap = CLI_CELLS_POOL, .byte = ':'}) : NULL;
    if (far && (colon == NULL))
    {
        mmgr_line_put(args->err_msg, args->remote);
        mmgr_line_put(args->err_msg, " names no folder: user@host:folder");
        return 0;
    }
    char python[CLI_PATH_MAX_LEN];
    if (!far && !EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        mmgr_line_put(args->err_msg, "no Python is found: File, Toolchains takes its folder");
        return 0;
    }
    CliProfile *const profile = EMBED_CALL(profile_take, ProfileCtx, .sink = args->sink);
    // the PATH a program here runs with, as long as the system lets an environment's value be
    profile->path = ((profile->words != NULL) && !far)
                        ? g_cli_cells->persistent_buf_alloc(CLI_PATH_TEXT, &g_cli_profile_cells)
                        : NULL;
    if ((profile->words == NULL) || (!far && (profile->path == NULL)))
    {
        EMBED_CALL(profile_let_go, ProfileCtx, .profile = profile);
        mmgr_line_put(args->err_msg, "the cells a profile is run in could not be taken");
        return 0;
    }
    char host[CLI_PATH_MAX_LEN];
    char shell[CLI_PATH_MAX_LEN * 3u];
    char full[CLI_PATH_MAX_LEN];
    unsigned int settings_count = 1u;
    CliSetting settings[2] = {{"PYTHONUNBUFFERED", "1"}, {"PATH", profile->path}};
    if (far)
    {
        VerbaLinea host_line = {host, sizeof(host), 0ull};
        mmgr_line_put_n(&host_line, args->remote, (unsigned long long)(colon - args->remote));
        mmgr_line_finish(&host_line);
        VerbaLinea shell_line = {shell, sizeof(shell), 0ull};
        mmgr_line_put(&shell_line, "cd ");
        EMBED_CALL(profile_quote, ProfileCtx, .out = &shell_line, .text = colon + 1);
        mmgr_line_put(&shell_line, " && exec python3 - - ");
        EMBED_CALL(profile_quote, ProfileCtx, .out = &shell_line, .text = args->file);
        mmgr_line_finish(&shell_line);
        const char *const words[5] = {"ssh", "-o", "BatchMode=yes", host, shell};
        for (unsigned int at = 0u; at < 5u; at += 1u)
        {
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = profile->words,
                       .word = words[at]);
        }
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->file, .joined = full);
        EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = profile->path,
                   .size = CLI_PATH_TEXT);
        settings_count = 2u;
        const char *const words[4] = {python, "-", args->root, full};
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = profile->words,
                       .word = words[at]);
        }
    }
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &profile->spawned,
                    .words = profile->words->word, .folder = far ? NULL : args->root, .settings = settings,
                    .count = settings_count, .detached = 1, .err_msg = args->err_msg))
    {
        EMBED_CALL(profile_let_go, ProfileCtx, .profile = profile);
        return 0;
    }
    const int given =
        EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = profile->spawned.in,
                   .bytes = g_cli_profile_shim,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_profile_shim, .cap = CLI_CELLS_POOL}));
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &profile->spawned);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    profile->running = 1;
    profile->going = 2u;
    s_running = profile;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (!given)
    {
        mmgr_line_put(args->err_msg, "the shim could not be given to Python");
        cli_profile_stop();
    }
    profile->error_thread = (CliThread){profile_errors_read, profile};
    profile->output_thread = (CliThread){profile_output_read, profile};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &profile->error_thread))
    {
        profile_errors_read(profile);
    }
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &profile->output_thread))
    {
        profile_output_read(profile);
    }
    return given;
}
