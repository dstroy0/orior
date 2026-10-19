// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_kept.c: the runs a server answering a window over a link keeps on this machine: each run a process of its own
// whose lines are kept, for a window that joins again to read them. A run is started as such a process and followed,
// each line it said told from its first as run-lines and its end as run-end, as they come, the lines of one reading
// going as one event; a run whose process is gone with no end said ends as stopped. A run is stopped by its stop file,
// which its own process watches for. The kept runs are listed in serve_kept_list.c
#include "../runs/runner.h"
#include "serve_internal.h"

#include <stdlib.h>

// how often a follower reads, in milliseconds, and how many quiet readings pass between asks whether the run's
// process still runs
#define CLI_SERVE_FOLLOW_EVERY 50u
#define CLI_SERVE_FOLLOW_ASKS 20u
// the most runs followed at once
#define CLI_SERVE_FOLLOWERS 32u

// A follower, a cell taken as it starts and given back as its thread ends: its place among the followers, the run's
// files and number, what it read and has yet to take, the lines it sends, and its thread
typedef struct
{
    unsigned int place;
    char base[CLI_PATH_LONGEST];
    unsigned long long run;
    char held[CLI_SERVE_KEPT_LINE];
    unsigned long long holding;
    char lines[CLI_SERVE_KEPT_LINE];
    CliThread thread;
} CliServeFollower;

static CliServeFollower *s_follower[CLI_SERVE_FOLLOWERS];
static CliLock s_lock = CLI_LOCK_INIT;
static unsigned long long s_last;

// a kept run's number: the milliseconds since 1970 it started at, each after the last
static unsigned long long kept_number(void)
{
    cli_lock_take(&s_lock);
    const unsigned long long now = cli_wall_milliseconds();
    s_last = (now > s_last) ? now : s_last + 1ull;
    const unsigned long long number = s_last;
    cli_lock_give(&s_lock);
    return number;
}

// a line of a kept run's said file taken: a start told at once, a line put among the lines read, and an end into
// `ended`; 1 where the run ended
static int kept_line_taken(const char *line, unsigned long long size, CliLine *lines, CliLine *ended)
{
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    if (cli_lsp_member(line, size, "end", &at, &length))
    {
        cli_line_counted(ended, line + at, length);
        return 1;
    }
    if (cli_lsp_member(line, size, "start", &at, &length))
    {
        cli_serve_emitted("run-start", line + at, length);
    }
    else if (cli_lsp_member(line, size, "line", &at, &length))
    {
        cli_line_text(lines, (lines->at > 1ull) ? "," : "");
        cli_line_counted(lines, line + at, length);
    }
    return 0;
}

// a kept run followed until its end, on a thread of its own, its follower its context
static void kept_follower(void *context)
{
    CliServeFollower *const one = context;
    char said_file[CLI_PATH_LONGEST];
    char pid_file[CLI_PATH_LONGEST];
    cli_serve_kept_file(one->base, "said", said_file);
    cli_serve_kept_file(one->base, "pid", pid_file);
    unsigned long long at = 0ull;
    unsigned int quiet = 0u;
    one->holding = 0ull;
    for (;;)
    {
        unsigned long long size = 0ull;
        const unsigned long long whole = cli_file_size(said_file, &size) ? size : 0ull;
        unsigned long long read = 0ull;
        if (whole > at)
        {
            unsigned long long mapped_size = 0ull;
            const unsigned char *const mapped = qry_member_file_mapped(said_file, &mapped_size);
            if (mapped != NULL)
            {
                const unsigned long long room = sizeof(one->held) - one->holding;
                read = (mapped_size > at) ? mapped_size - at : 0ull;
                read = (read < room) ? read : room;
                memor.cpy(&(MemoriaCfg){.dst = one->held + one->holding, .src = mapped + at, .bytes = (size_t)read});
                one->holding += read;
                qry_member_file_unmapped(mapped, mapped_size);
            }
        }
        if (read > 0ull)
        {
            at += read;
            quiet = 0u;
            // the lines of one reading go to the page as one event
            CliLine lines = {one->lines, sizeof(one->lines), 0ull};
            cli_line_character(&lines, '[');
            char ended_room[1u << 16u];
            CliLine ended = {ended_room, sizeof(ended_room), 0ull};
            int finished = 0;
            unsigned long long start = 0ull;
            for (unsigned long long cut = 0ull; !finished && (cut < one->holding); cut += 1ull)
            {
                if (one->held[cut] == '\n')
                {
                    finished = kept_line_taken(one->held + start, cut - start, &lines, &ended);
                    start = cut + 1ull;
                }
            }
            memor.move_down(
                &(MemoriaCfg){.dst = one->held, .src = one->held + start, .bytes = (size_t)(one->holding - start)});
            one->holding -= start;
            cli_line_character(&lines, ']');
            if ((lines.at > 2ull) && cli_line_fits(&lines))
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
        if ((quiet % CLI_SERVE_FOLLOW_ASKS) == 0u)
        {
            char pid_text[32];
            const unsigned long long pid_size =
                qry_member_file_read(pid_file, (unsigned char *)pid_text, sizeof(pid_text) - 1u);
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
                CliLine line = {ended, sizeof(ended), 0ull};
                cli_line_text(&line, "{\"run\":");
                cli_line_decimal(&line, one->run, 1u);
                cli_line_text(&line, ",\"code\":null,\"stopped\":true,\"views\":[],\"ms\":0.0}");
                cli_serve_emitted("run-end", ended, line.at);
                break;
            }
        }
        query_paused(CLI_SERVE_FOLLOW_EVERY);
    }
    cli_lock_take(&s_lock);
    s_follower[one->place] = NULL;
    cli_lock_give(&s_lock);
    cli_cell_given(one);
}

// the kept run `base` followed on a thread of its own: 1, or 0 where every follower is taken or no room for one is left
static int kept_followed(const char *base)
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
    CliServeFollower *const one =
        (at < CLI_SERVE_FOLLOWERS) ? cli_cell_taken(sizeof(CliServeFollower), "a run's follower") : NULL;
    if (one != NULL)
    {
        s_follower[at] = one;
        one->place = at;
        CliLine line = {one->base, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, base);
        (void)cli_line_closed(&line);
        one->run = strtoull(name, NULL, 10);
        one->thread = (CliThread){kept_follower, one};
        if (!cli_thread_started(&one->thread))
        {
            s_follower[at] = NULL;
            cli_cell_given(one);
        }
    }
    cli_lock_give(&s_lock);
    return one != NULL;
}

int cli_serve_kept_start(const char *keep, const char *job, const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    CliCatalog *const catalog = cli_serve_catalog_taken(root, said);
    if (catalog == NULL)
    {
        return 0;
    }
    const unsigned int found = cli_catalog_find(catalog, job);
    const CliValues *const values = cli_serve_values_of(args);
    const int checked = (found != CLI_CATALOG_NONE) && cli_runner_check(catalog, found, values, said);
    cli_serve_catalog_given();
    if (found == CLI_CATALOG_NONE)
    {
        cli_line_text(said, "no job ");
        cli_line_text(said, job);
    }
    if (!checked)
    {
        return 0;
    }
    if (!query_folder_made(keep))
    {
        cli_line_text(said, keep);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    const unsigned long long run = kept_number();
    char base[CLI_PATH_LONGEST];
    char number[32];
    CliLine named = {number, sizeof(number), 0ull};
    cli_line_decimal(&named, run, 1u);
    (void)cli_line_closed(&named);
    cli_full(keep, number, base);
    char job_file[CLI_PATH_LONGEST];
    cli_serve_kept_file(base, "job", job_file);
    static char s_spec[1u << 20u];
    static CliLock s_spec_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_spec_lock);
    CliLine spec = {s_spec, sizeof(s_spec), 0ull};
    cli_line_text(&spec, "{\"job\":");
    cli_json_quoted(&spec, job, cellul.len(&(CatenaFinitaCfg){.src = job, .cap = CLI_CELLS_POOL}));
    cli_line_text(&spec, ",\"root\":");
    cli_json_quoted_lossy(&spec, (const unsigned char *)root,
                          cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    cli_line_text(&spec, ",\"values\":");
    const unsigned int given = cli_serve_arg(args, "values");
    if (given != CLI_JSON_NONE)
    {
        cli_json_write(args->json, given, &spec, 0u, 1);
    }
    else
    {
        cli_line_text(&spec, "{}");
    }
    cli_line_character(&spec, '}');
    const int written = cli_line_fits(&spec) && query_file_written(job_file, s_spec, spec.at);
    cli_lock_give(&s_spec_lock);
    if (!written)
    {
        cli_line_text(said, job_file);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    char program[CLI_PATH_LONGEST];
    if (!cli_program_own(program, sizeof(program)))
    {
        cli_line_text(said, "orior's program could not be found");
        return 0;
    }
    const char *const words[4] = {program, "keep-run", base, NULL};
    CliSpawned spawned;
    if (!cli_spawn(&spawned, words, NULL, NULL, 0u, 1, said))
    {
        return 0;
    }
    (void)kept_followed(base);
    cli_line_decimal(out, run, 1u);
    return 1;
}

int cli_serve_run_follow(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long run = 0ull;
    char keep[CLI_PATH_LONGEST];
    cli_lock_take(&g_cli_serve.lock);
    const int keeping = g_cli_serve.has_keeping;
    memor.cpy(&(MemoriaCfg){.dst = keep, .src = g_cli_serve.keeping, .bytes = sizeof(keep)});
    cli_lock_give(&g_cli_serve.lock);
    if (!keeping)
    {
        cli_line_text(said, "this tree keeps no runs");
        return 0;
    }
    if (!cli_serve_whole(args, "run", &run, said))
    {
        return 0;
    }
    char number[32];
    CliLine named = {number, sizeof(number), 0ull};
    cli_line_decimal(&named, run, 1u);
    (void)cli_line_closed(&named);
    char base[CLI_PATH_LONGEST];
    cli_full(keep, number, base);
    (void)kept_followed(base);
    cli_line_text(out, "null");
    return 1;
}

int cli_serve_job_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long run = 0ull;
    if (!cli_serve_whole(args, "run", &run, said))
    {
        return 0;
    }
    char keep[CLI_PATH_LONGEST];
    cli_lock_take(&g_cli_serve.lock);
    const int keeping = g_cli_serve.has_keeping;
    memor.cpy(&(MemoriaCfg){.dst = keep, .src = g_cli_serve.keeping, .bytes = sizeof(keep)});
    cli_lock_give(&g_cli_serve.lock);
    if (keeping)
    {
        char number[32];
        CliLine named = {number, sizeof(number), 0ull};
        cli_line_decimal(&named, run, 1u);
        (void)cli_line_closed(&named);
        char base[CLI_PATH_LONGEST];
        char job_file[CLI_PATH_LONGEST];
        cli_full(keep, number, base);
        cli_serve_kept_file(base, "job", job_file);
        if (query_is_file(job_file))
        {
            char stop_file[CLI_PATH_LONGEST];
            cli_serve_kept_file(base, "stop", stop_file);
            if (!query_file_written(stop_file, "", 0ull))
            {
                cli_line_text(said, stop_file);
                cli_line_text(said, ": it could not be written");
                return 0;
            }
            cli_line_text(out, "null");
            return 1;
        }
    }
    const int stopped = cli_runs_stop(run, said);
    cli_line_text(out, stopped ? "null" : "");
    return stopped;
}
