// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// profile.c: a profile run: the program started under the shim, here or by `ssh` on another machine, the shim given on
// its input, each line it writes reported as output or as the counts so far, and its end reported
#include "profile.h"
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"

// the profiles open at once, the one running and the one before it ending; the longest line read whole, and the bytes
// a line's cell is first taken with
#define CLI_PROFILES 2u
#define CLI_PROFILE_LINE (1u << 20u)
#define CLI_PROFILE_LINE_FIRST 4096ull

// The start of the lines the shim writes its counts on
#define CLI_PROFILE_MARK "\x1eorior-profile "

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

static CliProfile s_profiles[CLI_PROFILES];
static CliProfile *s_running;
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;

// a profile's cells given back, under the profiles' lock
static void profile_cells_given(CliProfile *profile)
{
    g_cli_cells->persistent_buf_release(profile->words, &g_cli_profile_cells);
    g_cli_cells->persistent_buf_release(profile->path, &g_cli_profile_cells);
    profile->words = NULL;
    profile->path = NULL;
}

void cli_profile_stop(void)
{
    cli_lock_take(&s_lock);
    CliProfile *const profile = s_running;
    if ((profile != NULL) && profile->running)
    {
        cli_spawn_killed(&profile->spawned);
        (void)cli_spawn_wait(&profile->spawned);
        profile->running = 0;
    }
    s_running = NULL;
    cli_lock_give(&s_lock);
}

// a line the program wrote reported as output, to its errors where `error`, in a cell of the most its quoting takes,
// each byte at most `\u` and four digits
static void profile_output_emitted(CliProfile *profile, const char *line, unsigned long long size, int error)
{
    const unsigned long long room = (size * 6ull) + 64ull;
    char *const json_buf = g_cli_cells->temporary_buf_alloc(room, &g_cli_profile_cells);
    if (json_buf == NULL)
    {
        return;
    }
    VerbaLinea out = {json_buf, room, 0ull};
    mmgr_line_put(&out, "{\"kind\":\"output\",\"text\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)line, size);
    mmgr_line_put(&out, error ? ",\"error\":true}" : ",\"error\":false}");
    if (mmgr_line_ok(&out))
    {
        profile->sink.emit(profile->sink.context, out.out, out.at);
    }
    g_cli_cells->persistent_buf_release(json_buf, &g_cli_profile_cells);
}

// a line of the program's output received: the counts so far where it starts with the mark, the code the program ended
// with recorded where the counts report it is done, and any other line reported as output
static void profile_line(CliProfile *profile, const char *line, unsigned long long size)
{
    const unsigned long long mark = sizeof(CLI_PROFILE_MARK) - 1ull;
    if ((size < mark) || (memor.cmp(&(MemoriaCfg){.src = line, .other = CLI_PROFILE_MARK, .bytes = mark}) != 0))
    {
        profile_output_emitted(profile, line, size, 0);
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
    if (!cli_utf8_whole(json, length) || !cli_json_compacted(json, length, &out))
    {
        mmgr_line_put(&out, "null");
    }
    mmgr_line_ch(&out, '}');
    unsigned long long at = 0ull;
    unsigned long long done_size = 0ull;
    if (cli_lsp_member(json, length, "done", &at, &done_size) && (done_size == 4ull) &&
        (memor.cmp(&(MemoriaCfg){.src = json + at, .other = "true", .bytes = 4u}) == 0) &&
        cli_lsp_member(json, length, "code", &at, &done_size))
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
        cli_lock_take(&s_lock);
        profile->has_code = whole;
        profile->code = negative ? -code : code;
        cli_lock_give(&s_lock);
    }
    if (mmgr_line_ok(&out))
    {
        profile->sink.emit(profile->sink.context, out.out, out.at);
    }
    g_cli_cells->persistent_buf_release(json_buf, &g_cli_profile_cells);
}

// the lines of a pipe read until it ends, each in a cell grown as it fills, handed on with its "\r" before "\n" taken
// off, a line longer than CLI_PROFILE_LINE, or than the cell could be grown to, handed on in pieces of that length
static void profile_lines(CliProfile *profile, unsigned long long pipe,
                          void (*each_line)(CliProfile *profile, const char *line, unsigned long long size))
{
    char *line = NULL;
    unsigned long long room = 0ull;
    unsigned long long used = 0ull;
    char piece[4096];
    for (;;)
    {
        const unsigned long long count = cli_spawn_read(pipe, piece, sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            if ((used == room) && (room < CLI_PROFILE_LINE) && (piece[at] != '\n'))
            {
                const unsigned long long doubled = (room == 0ull) ? CLI_PROFILE_LINE_FIRST : (2ull * room);
                const unsigned long long wanted = (doubled < CLI_PROFILE_LINE) ? doubled : CLI_PROFILE_LINE;
                char *const grown = g_cli_cells->persistent_buf_alloc(wanted, &g_cli_profile_cells);
                if (grown != NULL)
                {
                    memor.cpy(&(MemoriaCfg){.dst = grown, .src = line, .bytes = (size_t)used});
                    g_cli_cells->persistent_buf_release(line, &g_cli_profile_cells);
                    line = grown;
                    room = wanted;
                }
            }
            if ((piece[at] == '\n') || ((used == room) && (used > 0ull)))
            {
                each_line(profile, line,
                          ((piece[at] == '\n') && (used > 0ull) && (line[used - 1ull] == '\r')) ? used - 1ull : used);
                used = 0ull;
            }
            if ((piece[at] != '\n') && (used < room))
            {
                line[used] = piece[at];
                used += 1ull;
            }
        }
    }
    if (used > 0ull)
    {
        each_line(profile, line, used);
    }
    g_cli_cells->persistent_buf_release(line, &g_cli_profile_cells);
}

// a line the program wrote to its errors reported as output
static void profile_error_line(CliProfile *profile, const char *line, unsigned long long size)
{
    profile_output_emitted(profile, line, size, 1);
}

// a reader's end: reported to the profile, and its place given back with the last
static void profile_reader_ended(CliProfile *profile)
{
    cli_lock_take(&s_lock);
    profile->going -= 1u;
    if (profile->going == 0u)
    {
        cli_spawn_closed(&profile->spawned);
        profile_cells_given(profile);
        profile->taken = 0;
        cli_wake_all(&s_wake);
    }
    cli_lock_give(&s_lock);
}

// what the program writes to its errors read until it ends
static void profile_errors_read(void *context)
{
    CliProfile *const profile = (CliProfile *)context;
    profile_lines(profile, profile->spawned.err, profile_error_line);
    profile_reader_ended(profile);
}

// what the program writes read until it ends, then its errors waited for, its process waited on where it was not
// stopped, and its end reported: the code it ended with, and that it ended, ended with another code, or was stopped
static void profile_output_read(void *context)
{
    CliProfile *const profile = (CliProfile *)context;
    profile_lines(profile, profile->spawned.out, profile_line);
    cli_lock_take(&s_lock);
    while (profile->going > 1u)
    {
        cli_wake_wait(&s_wake, &s_lock);
    }
    long long status = 0ll;
    int has_status = 0;
    if (profile->running)
    {
        status = cli_spawn_wait(&profile->spawned);
        has_status = (status >= 0ll);
        profile->running = 0;
    }
    const int has_code = profile->has_code || has_status;
    const long long code = profile->has_code ? profile->code : status;
    cli_lock_give(&s_lock);
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
    profile->sink.emit(profile->sink.context, out.out, out.at);
    profile_reader_ended(profile);
}

// a profile's place taken once one is free, made empty
static CliProfile *profile_taken(CliJsonSink sink)
{
    cli_lock_take(&s_lock);
    CliProfile *profile = NULL;
    while (profile == NULL)
    {
        for (unsigned int at = 0u; (at < CLI_PROFILES) && (profile == NULL); at += 1u)
        {
            profile = s_profiles[at].taken ? NULL : &s_profiles[at];
        }
        if (profile == NULL)
        {
            cli_wake_wait(&s_wake, &s_lock);
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
    cli_lock_give(&s_lock);
    if (profile->words != NULL)
    {
        profile->words->count = 0u;
        profile->words->used = 0u;
        profile->words->full = 0;
    }
    return profile;
}

// a profile's place given back where its process did not start
static void profile_let_go(CliProfile *profile)
{
    cli_lock_take(&s_lock);
    profile_cells_given(profile);
    profile->taken = 0;
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
}

// a text written as one word of a shell, in single quotes, each quote of it closed, escaped and opened again
static void profile_quoted(VerbaLinea *out, const char *text)
{
    mmgr_line_ch(out, '\'');
    for (const char *letter = text; *letter != '\0'; letter += 1)
    {
        mmgr_line_put(out, (*letter == '\'') ? "'\\''" : "");
        mmgr_line_put_n(out, letter, (*letter == '\'') ? 0ull : 1ull);
    }
    mmgr_line_ch(out, '\'');
}

int cli_profile_start(const char *root, const char *file, const char *remote, CliJsonSink sink, VerbaLinea *err_msg)
{
    cli_profile_stop();
    const char *trimmed = (remote != NULL) ? remote : "";
    while ((*trimmed == ' ') || (*trimmed == '\t'))
    {
        trimmed += 1;
    }
    const int far = (*trimmed != '\0');
    const char *const colon =
        far ? cellul.chr(&(CatenaFinitaCfg){.src = remote, .cap = CLI_CELLS_POOL, .byte = ':'}) : NULL;
    if (far && (colon == NULL))
    {
        mmgr_line_put(err_msg, remote);
        mmgr_line_put(err_msg, " names no folder: user@host:folder");
        return 0;
    }
    char python[CLI_PATH_MAX_LEN];
    if (!far && !cli_test_python(python))
    {
        mmgr_line_put(err_msg, "no Python is found: File, Toolchains takes its folder");
        return 0;
    }
    CliProfile *const profile = profile_taken(sink);
    // the PATH a program here runs with, as long as the system lets an environment's value be
    profile->path =
        ((profile->words != NULL) && !far) ? g_cli_cells->persistent_buf_alloc(CLI_PATH_TEXT, &g_cli_profile_cells) : NULL;
    if ((profile->words == NULL) || (!far && (profile->path == NULL)))
    {
        profile_let_go(profile);
        mmgr_line_put(err_msg, "the cells a profile is run in could not be taken");
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
        mmgr_line_put_n(&host_line, remote, (unsigned long long)(colon - remote));
        mmgr_line_finish(&host_line);
        VerbaLinea shell_line = {shell, sizeof(shell), 0ull};
        mmgr_line_put(&shell_line, "cd ");
        profile_quoted(&shell_line, colon + 1);
        mmgr_line_put(&shell_line, " && exec python3 - - ");
        profile_quoted(&shell_line, file);
        mmgr_line_finish(&shell_line);
        const char *const words[5] = {"ssh", "-o", "BatchMode=yes", host, shell};
        for (unsigned int at = 0u; at < 5u; at += 1u)
        {
            cli_words_put(profile->words, words[at]);
        }
    }
    else
    {
        cli_full(root, file, full);
        cli_toolchains_run_path(profile->path, CLI_PATH_TEXT);
        settings_count = 2u;
        const char *const words[4] = {python, "-", root, full};
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            cli_words_put(profile->words, words[at]);
        }
    }
    if (!cli_spawn_fed(&profile->spawned, profile->words->word, far ? NULL : root, settings, settings_count, 1,
                       err_msg))
    {
        profile_let_go(profile);
        return 0;
    }
    const int given =
        cli_spawn_written(profile->spawned.in, g_cli_profile_shim,
                          cellul.len(&(CatenaFinitaCfg){.src = g_cli_profile_shim, .cap = CLI_CELLS_POOL}));
    cli_spawn_input_closed(&profile->spawned);
    cli_lock_take(&s_lock);
    profile->running = 1;
    profile->going = 2u;
    s_running = profile;
    cli_lock_give(&s_lock);
    if (!given)
    {
        mmgr_line_put(err_msg, "the shim could not be given to Python");
        cli_profile_stop();
    }
    profile->error_thread = (CliThread){profile_errors_read, profile};
    profile->output_thread = (CliThread){profile_output_read, profile};
    if (!cli_thread_started(&profile->error_thread))
    {
        profile_errors_read(profile);
    }
    if (!cli_thread_started(&profile->output_thread))
    {
        profile_output_read(profile);
    }
    return given;
}
