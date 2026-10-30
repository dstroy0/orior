// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_detached.c: the runs a server answering a window over a link stores on this machine: each run a process of its
// own whose lines are stored, for a window that joins again to read them. A run is started as such a process and
// followed, each line it printed sent from its first as run-lines and its end as run-end, as they come, the lines of
// one reading going as one event; a run whose process is gone with no end reported ends as stopped. A run is stopped by
// its stop file, which its own process watches for. The detached runs are listed in serve_detached_list.c
#include "../runs/runner.h"
#include "serve_internal.h"

#include <stdlib.h>

// how often a follower reads, in milliseconds, and how many quiet readings pass between checks whether the run's
// process still runs
#define CLI_SERVE_FOLLOW_EVERY 50u
#define CLI_SERVE_FOLLOW_TRIES 20u
// how many checks a run's process may go without writing its number before the run is taken as never started
#define CLI_SERVE_UNSTARTED_TRIES 5u
// the most runs followed at once
#define CLI_SERVE_FOLLOWERS 32u

// A follower, a cell taken as it starts and given back as its thread ends: its place among the followers, the run's
// files and number, what it read and has yet to take, the lines it sends, and its thread
typedef struct
{
    unsigned int place;
    char base[CLI_PATH_MAX_LEN];
    unsigned long long run;
    char pending[CLI_SERVE_DETACHED_LINE];
    unsigned long long pending_size;
    char lines[CLI_SERVE_DETACHED_LINE];
    CliThread thread;
} CliServeFollower;

static CliServeFollower *s_follower[CLI_SERVE_FOLLOWERS];
static CliLock s_lock = CLI_LOCK_INIT;
static unsigned long long s_last;

// a detached run's number: the milliseconds since 1970 it started at, each after the last
static unsigned long long detached_number(void)
{
    cli_lock_take(&s_lock);
    const unsigned long long now = cli_wall_milliseconds();
    s_last = (now > s_last) ? now : s_last + 1ull;
    const unsigned long long number = s_last;
    cli_lock_give(&s_lock);
    return number;
}

// a line of a detached run's output file taken: a start sent at once, a line put among the lines read, and an end into
// `ended`; 1 where the run ended
static int detached_line_taken(const char *line, unsigned long long size, VerbaLinea *lines, VerbaLinea *ended)
{
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    if (cli_lsp_member(line, size, "end", &at, &length))
    {
        mmgr_line_put_n(ended, line + at, length);
        return 1;
    }
    if (cli_lsp_member(line, size, "start", &at, &length))
    {
        cli_serve_emitted("run-start", line + at, length);
    }
    else if (cli_lsp_member(line, size, "line", &at, &length))
    {
        mmgr_line_put(lines, (lines->at > 1ull) ? "," : "");
        mmgr_line_put_n(lines, line + at, length);
    }
    return 0;
}

// a detached run followed until its end, on a thread of its own, its follower its context
static void detached_follower(void *context)
{
    CliServeFollower *const one = context;
    char lines_file[CLI_PATH_MAX_LEN];
    char pid_file[CLI_PATH_MAX_LEN];
    cli_serve_detached_file(one->base, "output", lines_file);
    cli_serve_detached_file(one->base, "pid", pid_file);
    unsigned long long at = 0ull;
    unsigned int quiet = 0u;
    unsigned int unstarted = 0u;
    one->pending_size = 0ull;
    for (;;)
    {
        unsigned long long size = 0ull;
        const unsigned long long whole = cli_file_size(lines_file, &size) ? size : 0ull;
        unsigned long long read = 0ull;
        if (whole > at)
        {
            unsigned long long mapped_size = 0ull;
            const unsigned char *const mapped = cli_file_read(lines_file, &mapped_size);
            if (mapped != NULL)
            {
                const unsigned long long room = sizeof(one->pending) - one->pending_size;
                read = (mapped_size > at) ? mapped_size - at : 0ull;
                read = (read < room) ? read : room;
                memor.cpy(
                    &(MemoriaCfg){.dst = one->pending + one->pending_size, .src = mapped + at, .bytes = (size_t)read});
                one->pending_size += read;
                cli_file_let_go(mapped, mapped_size);
            }
        }
        if (read > 0ull)
        {
            at += read;
            quiet = 0u;
            // the lines of one reading go to the page as one event
            VerbaLinea lines = {one->lines, sizeof(one->lines), 0ull};
            mmgr_line_ch(&lines, '[');
            char ended_room[1u << 16u];
            VerbaLinea ended = {ended_room, sizeof(ended_room), 0ull};
            int finished = 0;
            unsigned long long start = 0ull;
            for (unsigned long long cut = 0ull; !finished && (cut < one->pending_size); cut += 1ull)
            {
                if (one->pending[cut] == '\n')
                {
                    finished = detached_line_taken(one->pending + start, cut - start, &lines, &ended);
                    start = cut + 1ull;
                }
            }
            memor.move_down(&(MemoriaCfg){
                .dst = one->pending, .src = one->pending + start, .bytes = (size_t)(one->pending_size - start)});
            one->pending_size -= start;
            mmgr_line_ch(&lines, ']');
            if ((lines.at > 2ull) && mmgr_line_ok(&lines))
            {
                cli_serve_emitted("run-lines", one->lines, lines.at);
            }
            if (finished)
            {
                cli_serve_emitted("run-end", ended_room, ended.at);
                break;
            }
            continue;
        }
        quiet += 1u;
        if ((quiet % CLI_SERVE_FOLLOW_TRIES) == 0u)
        {
            char pid_text[32];
            const unsigned long long pid_size =
                qry_member_file_read(pid_file, (unsigned char *)pid_text, sizeof(pid_text) - 1u);
            // a run whose process has written no number of its own by CLI_SERVE_UNSTARTED_TRIES checks, and printed
            // nothing, never started: it ends as stopped, never followed without end
            unstarted = (pid_size == QRY_MEMBER_UNREAD) ? unstarted + 1u : 0u;
            if ((unstarted >= CLI_SERVE_UNSTARTED_TRIES) && (at == 0ull) && (whole == 0ull))
            {
                char ended[256];
                VerbaLinea line = {ended, sizeof(ended), 0ull};
                mmgr_line_put(&line, "{\"run\":");
                mmgr_line_decimal(&line, one->run, 1u);
                mmgr_line_put(&line, ",\"code\":null,\"stopped\":true,\"views\":[],\"ms\":0.0}");
                cli_serve_emitted("run-end", ended, line.at);
                break;
            }
            unsigned long long pid = 0ull;
            int number = (pid_size != QRY_MEMBER_UNREAD);
            for (unsigned long long digit = 0ull; number && (digit < pid_size); digit += 1ull)
            {
                const char letter = pid_text[digit];
                const int white = (letter == ' ') || (letter == '\n') || (letter == '\r') || (letter == '\t');
                number = white || ((letter >= '0') && (letter <= '9'));
                pid = (number && !white) ? (pid * 10ull) + (unsigned long long)(letter - '0') : pid;
            }
            if (number && (pid <= 0xffffffffull) && !cli_process_running((unsigned int)pid) && (whole == at))
            {
                char ended[256];
                VerbaLinea line = {ended, sizeof(ended), 0ull};
                mmgr_line_put(&line, "{\"run\":");
                mmgr_line_decimal(&line, one->run, 1u);
                mmgr_line_put(&line, ",\"code\":null,\"stopped\":true,\"views\":[],\"ms\":0.0}");
                cli_serve_emitted("run-end", ended, line.at);
                break;
            }
        }
        query_paused(CLI_SERVE_FOLLOW_EVERY);
    }
    cli_lock_take(&s_lock);
    s_follower[one->place] = NULL;
    cli_lock_give(&s_lock);
    cli_cell_release(one, &g_cli_window_cells);
}

// the detached run `base` followed on a thread of its own: 1, or 0 where every follower is taken or no room for one is
// left
static int detached_followed(const char *base)
{
    const char *name = base;
    for (const char *at = base; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? at + 1 : name;
    }
    cli_lock_take(&s_lock);
    unsigned int at = 0u;
    while ((at < CLI_SERVE_FOLLOWERS) && (s_follower[at] != NULL))
    {
        at += 1u;
    }
    CliServeFollower *const one = (at < CLI_SERVE_FOLLOWERS)
                                      ? g_cli_cells->persistent_buf_alloc(sizeof(CliServeFollower), &g_cli_window_cells)
                                      : NULL;
    if (one != NULL)
    {
        s_follower[at] = one;
        one->place = at;
        VerbaLinea line = {one->base, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, base);
        (void)mmgr_line_finish(&line);
        one->run = strtoull(name, NULL, 10);
        one->thread = (CliThread){detached_follower, one};
        if (!cli_thread_started(&one->thread))
        {
            s_follower[at] = NULL;
            cli_cell_release(one, &g_cli_window_cells);
        }
    }
    cli_lock_give(&s_lock);
    return one != NULL;
}

int cli_serve_detached_start(const char *detached_dir, const char *job, const CliServeArgs *args, VerbaLinea *out,
                             VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    CliCatalog *const catalog = cli_serve_catalog_taken(root, err_msg);
    if (catalog == NULL)
    {
        return 0;
    }
    const unsigned int found = cli_catalog_find(catalog, job);
    const CliValues *const values = cli_serve_values_of(args);
    const int checked = (found != CLI_CATALOG_NONE) && cli_runner_check(catalog, found, values, err_msg);
    cli_serve_catalog_given();
    if (found == CLI_CATALOG_NONE)
    {
        mmgr_line_put(err_msg, "no job ");
        mmgr_line_put(err_msg, job);
    }
    if (!checked)
    {
        return 0;
    }
    if (!query_folder_made(detached_dir))
    {
        mmgr_line_put(err_msg, detached_dir);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    const unsigned long long run = detached_number();
    char base[CLI_PATH_MAX_LEN];
    char number[32];
    VerbaLinea named = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&named, run, 1u);
    (void)mmgr_line_finish(&named);
    cli_full(detached_dir, number, base);
    char job_file[CLI_PATH_MAX_LEN];
    cli_serve_detached_file(base, "job", job_file);
    static char s_spec[1u << 20u];
    static CliLock s_spec_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_spec_lock);
    VerbaLinea spec = {s_spec, sizeof(s_spec), 0ull};
    mmgr_line_put(&spec, "{\"job\":");
    cli_json_quoted(&spec, job, cellul.len(&(CatenaFinitaCfg){.src = job, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&spec, ",\"root\":");
    cli_json_quoted_lossy(&spec, (const unsigned char *)root,
                          cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&spec, ",\"values\":");
    const unsigned int given = cli_serve_arg(args, "values");
    if (given != CLI_JSON_NONE)
    {
        cli_json_write(args->json, given, &spec, 0u, 1);
    }
    else
    {
        mmgr_line_put(&spec, "{}");
    }
    mmgr_line_ch(&spec, '}');
    const int written = mmgr_line_ok(&spec) && query_file_written(job_file, s_spec, spec.at);
    cli_lock_give(&s_spec_lock);
    if (!written)
    {
        mmgr_line_put(err_msg, job_file);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    char program[CLI_PATH_MAX_LEN];
    if (!cli_program_own(program, sizeof(program)))
    {
        mmgr_line_put(err_msg, "orior's program could not be found");
        return 0;
    }
    const char *const words[4] = {program, "detached-run", base, NULL};
    CliSpawned spawned;
    if (!cli_spawn(&spawned, words, NULL, NULL, 0u, 1, err_msg))
    {
        return 0;
    }
    (void)detached_followed(base);
    mmgr_line_decimal(out, run, 1u);
    return 1;
}

int cli_serve_run_follow(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long run = 0ull;
    char detached_dir[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_serve.lock);
    const int has_detached_dir = g_cli_serve.has_detached_dir;
    memor.cpy(&(MemoriaCfg){.dst = detached_dir, .src = g_cli_serve.detached_dir, .bytes = sizeof(detached_dir)});
    cli_lock_give(&g_cli_serve.lock);
    if (!has_detached_dir)
    {
        mmgr_line_put(err_msg, "this tree stores no runs");
        return 0;
    }
    if (!cli_serve_whole(args, "run", &run, err_msg))
    {
        return 0;
    }
    char number[32];
    VerbaLinea named = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&named, run, 1u);
    (void)mmgr_line_finish(&named);
    char base[CLI_PATH_MAX_LEN];
    cli_full(detached_dir, number, base);
    (void)detached_followed(base);
    mmgr_line_put(out, "null");
    return 1;
}

int cli_serve_job_stop(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long run = 0ull;
    if (!cli_serve_whole(args, "run", &run, err_msg))
    {
        return 0;
    }
    char detached_dir[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_serve.lock);
    const int has_detached_dir = g_cli_serve.has_detached_dir;
    memor.cpy(&(MemoriaCfg){.dst = detached_dir, .src = g_cli_serve.detached_dir, .bytes = sizeof(detached_dir)});
    cli_lock_give(&g_cli_serve.lock);
    if (has_detached_dir)
    {
        char number[32];
        VerbaLinea named = {number, sizeof(number), 0ull};
        mmgr_line_decimal(&named, run, 1u);
        (void)mmgr_line_finish(&named);
        char base[CLI_PATH_MAX_LEN];
        char job_file[CLI_PATH_MAX_LEN];
        cli_full(detached_dir, number, base);
        cli_serve_detached_file(base, "job", job_file);
        if (query_is_file(job_file))
        {
            char stop_file[CLI_PATH_MAX_LEN];
            cli_serve_detached_file(base, "stop", stop_file);
            if (!query_file_written(stop_file, "", 0ull))
            {
                mmgr_line_put(err_msg, stop_file);
                mmgr_line_put(err_msg, ": it could not be written");
                return 0;
            }
            mmgr_line_put(out, "null");
            return 1;
        }
    }
    const int stopped = cli_runs_stop(run, err_msg);
    mmgr_line_put(out, stopped ? "null" : "");
    return stopped;
}
