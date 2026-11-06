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
    int reserved;
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
#define CLI_PROFILE_LINE_FIRST 4096ULL

// The start of the lines the shim writes its counts on
#define CLI_PROFILE_MARK "\x1eorior-profile "

static CliProfile s_profiles[CLI_PROFILES];
static CliProfile *s_running;
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

void cli_profile_stop(void)
{
    CliProfile *const profile = s_running;
    if ((profile != NULL) && profile->running)
    {
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &profile->spawned);
        (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &profile->spawned);
        profile->running = 0;
    }
    s_running = NULL;
}

// a line the program wrote reported as output, to its errors where `error`, in a cell of the most its quoting takes,
// each byte at most `\u` and four digits
EMBED_INLINE void profile_output_emit(const ProfileCtx *args)
{
    const unsigned long long capacity = (args->size * 6ULL) + 64ULL;
    char *const json_buf = g_cli_guard->temporary_buf_alloc(capacity, g_cli_profile_owner);
    if (json_buf == NULL)
    {
        return;
    }
    VerbaLinea out = {json_buf, capacity, 0ULL};
    mmgr_line_put(&out, "{\"kind\":\"output\",\"text\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->line, .length = args->size);
    mmgr_line_put(&out, args->error ? ",\"error\":true}" : ",\"error\":false}");
    if (mmgr_line_ok(&out))
    {
        EMBED_CALL(args->profile->sink.emit, CliEachJsonSizeCfg, .context = args->profile->sink.context,
                   .json = out.out, .size = out.at);
    }
    g_cli_guard->persistent_buf_release(json_buf, g_cli_profile_owner);
}

// a line of the program's output received: the counts so far where it starts with the mark, the code the program ended
// with recorded where the counts report it is done, and any other line reported as output
static void profile_line(const CliProfileLineSizeCfg *args)
{
    CliProfile *profile = args->profile;
    const char *line = args->line;
    unsigned long long size = args->size;
    const unsigned long long mark = sizeof(CLI_PROFILE_MARK) - 1ULL;
    if ((size < mark) || (memor.cmp(&(MemoriaCfg){.src = line, .other = CLI_PROFILE_MARK, .bytes = mark}) != 0))
    {
        EMBED_CALL(profile_output_emit, ProfileCtx, .profile = profile, .line = line, .size = size, .error = 0);
        return;
    }
    const char *const json = line + mark;
    const unsigned long long length = size - mark;
    // the counts compacted are no longer than they are written
    const unsigned long long capacity = length + 64ULL;
    char *const json_buf = g_cli_guard->temporary_buf_alloc(capacity, g_cli_profile_owner);
    if (json_buf == NULL)
    {
        return;
    }
    VerbaLinea out = {json_buf, capacity, 0ULL};
    mmgr_line_put(&out, "{\"kind\":\"counts\",\"counts\":");
    if (!EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = json, .size = length) ||
        !EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = json, .size = length, .out = &out))
    {
        mmgr_line_put(&out, "null");
    }
    mmgr_line_ch(&out, '}');
    unsigned long long at = 0ULL;
    unsigned long long done_size = 0ULL;
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = json, .size_value = length, .name = "done", .start = &at,
                   .length_pointer = &done_size) &&
        (done_size == 4ULL) && (memor.cmp(&(MemoriaCfg){.src = json + at, .other = "true", .bytes = 4u}) == 0) &&
        EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = json, .size_value = length, .name = "code", .start = &at,
                   .length_pointer = &done_size))
    {
        long long code = 0LL;
        int whole = (done_size > 0ULL);
        const int negative = whole && (json[at] == '-');
        unsigned long long letter = at + (negative ? 1ULL : 0ULL);
        while (whole && (letter < at + done_size))
        {
            whole = (json[letter] >= '0') && (json[letter] <= '9') && (code < 922337203685477580LL);
            code = (code * 10LL) + (long long)(json[letter] - '0');
            letter += 1ULL;
        }
        profile->has_code = whole;
        profile->code = negative ? -code : code;
    }
    if (mmgr_line_ok(&out))
    {
        EMBED_CALL(profile->sink.emit, CliEachJsonSizeCfg, .context = profile->sink.context, .json = out.out,
                   .size = out.at);
    }
    g_cli_guard->persistent_buf_release(json_buf, g_cli_profile_owner);
}

// the lines of a pipe read until it ends, each in one cell of CLI_PROFILE_LINE, handed on with its "\r" before "\n"
// taken off, a longer line handed on in pieces of that length
static void profile_lines(const ProfileCtx *args)
{
    char *const line = g_cli_guard->temporary_buf_alloc(CLI_PROFILE_LINE, g_cli_profile_owner);
    const unsigned long long capacity = (line != NULL) ? CLI_PROFILE_LINE : 0ULL;
    unsigned long long used = 0ULL;
    char piece[4096];
    for (;;)
    {
        const unsigned long long count =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = args->pipe, .piece = piece, .capacity = sizeof(piece));
        if (count == 0ULL)
        {
            break;
        }
        unsigned long long at = 0ULL;
        while ((capacity != 0ULL) && (at < count))
        {
            const unsigned long long stop =
                EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = piece, .at = at, .size = count).stop;
            const unsigned long long left = capacity - used;
            const unsigned long long fits = ((stop - at) < left) ? (stop - at) : left;
            memor.cpy(&(MemoriaCfg){.dst = line + used, .src = piece + at, .bytes = (size_t)fits});
            used += fits;
            at += fits;
            if (at < stop)
            {
                // a line past the capacity is handed on as far as it reaches, the rest as a line after it
                EMBED_CALL(args->each_line, CliProfileLineSizeCfg, .profile = args->profile, .line = line,
                           .size = used);
                used = 0ULL;
            }
            else if (stop < count)
            {
                EMBED_CALL(args->each_line, CliProfileLineSizeCfg, .profile = args->profile, .line = line,
                           .size = ((used > 0ULL) && (line[used - 1ULL] == '\r')) ? used - 1ULL : used);
                used = 0ULL;
                at = stop + 1ULL;
            }
        }
    }
    if (used > 0ULL)
    {
        EMBED_CALL(args->each_line, CliProfileLineSizeCfg, .profile = args->profile, .line = line, .size = used);
    }
    g_cli_guard->persistent_buf_release(line, g_cli_profile_owner);
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
    args->profile->going -= 1u;
    if (args->profile->going == 0u)
    {
        EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &args->profile->spawned);
        g_cli_guard->persistent_buf_release(args->profile->words, g_cli_profile_owner);
        g_cli_guard->persistent_buf_release(args->profile->path, g_cli_profile_owner);
        args->profile->words = NULL;
        args->profile->path = NULL;
        args->profile->reserved = 0;
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
    }
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
    while (profile->going > 1u)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    long long status = 0LL;
    int has_status = 0;
    if (profile->running)
    {
        status = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &profile->spawned);
        has_status = (status >= 0LL);
        profile->running = 0;
    }
    const int has_code = profile->has_code || has_status;
    const long long code = profile->has_code ? profile->code : status;
    char done[256];
    VerbaLinea out = {done, sizeof(done), 0ULL};
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
    else if (code == 0LL)
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
EMBED_INLINE CliProfile *profile_alloc(const ProfileCtx *args)
{
    CliJsonSink sink = args->sink;
    CliProfile *profile = NULL;
    while (profile == NULL)
    {
        unsigned int at = 0u;
        while ((at < CLI_PROFILES) && (profile == NULL))
        {
            profile = s_profiles[at].reserved ? NULL : &s_profiles[at];
            at += 1u;
        }
        if (profile == NULL)
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
        }
    }
    profile->reserved = 1;
    profile->sink = sink;
    profile->running = 0;
    profile->going = 0u;
    profile->has_code = 0;
    profile->code = 0LL;
    memor.set(&(MemoriaCfg){.dst = &profile->spawned, .val = 0, .bytes = sizeof(profile->spawned)});
    profile->words = g_cli_guard->persistent_buf_alloc(sizeof(*profile->words), g_cli_profile_owner);
    profile->path = NULL;
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
    g_cli_guard->persistent_buf_release(args->profile->words, g_cli_profile_owner);
    g_cli_guard->persistent_buf_release(args->profile->path, g_cli_profile_owner);
    args->profile->words = NULL;
    args->profile->path = NULL;
    args->profile->reserved = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

// a text written as one word of a shell, in single quotes, each quote of it closed, escaped and opened again
EMBED_INLINE void profile_quote(const ProfileCtx *args)
{
    VerbaLinea *out = args->out;
    const char *text = args->text;
    mmgr_line_ch(out, '\'');
    EMBED_CALL(cli_text_set_table.append_replace, CliTextSetCfg, .out = out, .text = text, .set = "'", .with = "'\\''");
    mmgr_line_ch(out, '\'');
}

int cli_profile_start(const CliProfileCfg *args)
{
    cli_profile_stop();
    const char *trimmed = (args->remote != NULL) ? args->remote : "";
    trimmed += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = trimmed, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = trimmed, .cap = CLI_POOL_BYTES}), .set = " \t");
    const int far = (*trimmed != '\0');
    const char *const colon =
        far ? cellul.chr(&(CatenaFinitaCfg){.src = args->remote, .cap = CLI_POOL_BYTES, .byte = ':'}) : NULL;
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
    CliProfile *const profile = EMBED_CALL(profile_alloc, ProfileCtx, .sink = args->sink);
    // the PATH a program here runs with, as long as the system lets an environment's value be
    profile->path = ((profile->words != NULL) && !far)
                        ? g_cli_guard->persistent_buf_alloc(CLI_PATH_TEXT, g_cli_profile_owner)
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
        VerbaLinea host_line = {host, sizeof(host), 0ULL};
        mmgr_line_put_n(&host_line, args->remote, (unsigned long long)(colon - args->remote));
        mmgr_line_finish(&host_line);
        VerbaLinea shell_line = {shell, sizeof(shell), 0ULL};
        mmgr_line_put(&shell_line, "cd ");
        EMBED_CALL(profile_quote, ProfileCtx, .out = &shell_line, .text = colon + 1);
        mmgr_line_put(&shell_line, " && exec python3 - - ");
        EMBED_CALL(profile_quote, ProfileCtx, .out = &shell_line, .text = args->file);
        mmgr_line_finish(&shell_line);
        const char *const words[5] = {"ssh", "-o", "BatchMode=yes", host, shell};
        for (unsigned int at = 0u; at < 5u; at += 1u)
        {
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = profile->words,
                       .word_text = words[at]);
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
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = profile->words,
                       .word_text = words[at]);
        }
    }
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &profile->spawned,
                    .words = profile->words->word_text, .folder = far ? NULL : args->root, .settings = settings,
                    .count = settings_count, .detached = 1, .err_msg = args->err_msg))
    {
        EMBED_CALL(profile_let_go, ProfileCtx, .profile = profile);
        return 0;
    }
    const int supplied =
        EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = profile->spawned.in,
                   .bytes = g_cli_profile_shim,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_profile_shim, .cap = CLI_POOL_BYTES}));
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &profile->spawned);
    profile->running = 1;
    profile->going = 2u;
    s_running = profile;
    if (!supplied)
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
    return supplied;
}
