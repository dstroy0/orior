// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_detached.c: the runs a server answering a window over a link stores on this machine: each run a process of its
// own whose lines are stored, for a window that joins again to read them. A run is started as such a process and
// followed, each line it printed sent from its first as run-lines and its end as run-end, as they come, the lines of
// one reading going as one event; a run whose process is gone with no end reported ends as stopped. A run is stopped by
// its stop file, which its own process watches for. The detached runs are listed in serve_detached_list.c
#include "../command_line/cells.h"
#include "../runs/runner.h"
#include "../text/text.h"
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const unsigned long long now = cli_wall_milliseconds();
    s_last = (now > s_last) ? now : s_last + 1ull;
    const unsigned long long number = s_last;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return number;
}

typedef struct
{
    const char *line;
    unsigned long long size;
    VerbaLinea *lines;
    VerbaLinea *ended;
    const char *base;
} ServeDetachedCtx;
// a line of a detached run's output file taken: a start sent at once, a line put among the lines read, and an end into
// `ended`; 1 where the run ended
EMBED_INLINE int detached_line_take(const ServeDetachedCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->line, .size_value = size, .name = "end",
                   .start = &at, .length_pointer = &length))
    {
        mmgr_line_put_n(args->ended, args->line + at, length);
        return 1;
    }
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->line, .size_value = size, .name = "start",
                   .start = &at, .length_pointer = &length))
    {
        EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "run-start", .body = args->line + at,
                   .size = length);
    }
    else if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->line, .size_value = size, .name = "line",
                        .start = &at, .length_pointer = &length))
    {
        mmgr_line_put(args->lines, (args->lines->at > 1ull) ? "," : "");
        mmgr_line_put_n(args->lines, args->line + at, length);
    }
    return 0;
}

// a detached run followed until its end, on a thread of its own, its follower its context
static void detached_follower(void *context)
{
    CliServeFollower *const one = context;
    char lines_file[CLI_PATH_MAX_LEN];
    char pid_file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = one->base, .kind = "output",
               .path_out = lines_file);
    EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = one->base, .kind = "pid",
               .path_out = pid_file);
    unsigned long long at = 0ull;
    unsigned int quiet = 0u;
    unsigned int unstarted = 0u;
    one->pending_size = 0ull;
    for (;;)
    {
        unsigned long long size = 0ull;
        const unsigned long long whole =
            EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = lines_file, .size_pointer = &size)
                ? size
                : 0ull;
        unsigned long long read = 0ull;
        if (whole > at)
        {
            unsigned long long mapped_size = 0ull;
            const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = lines_file,
                                                           .size_pointer = &mapped_size, .owner = &g_cli_window_cells);
            if (mapped != NULL)
            {
                const unsigned long long room = sizeof(one->pending) - one->pending_size;
                read = (mapped_size > at) ? mapped_size - at : 0ull;
                read = (read < room) ? read : room;
                memor.cpy(
                    &(MemoriaCfg){.dst = one->pending + one->pending_size, .src = mapped + at, .bytes = (size_t)read});
                one->pending_size += read;
                EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
                           .owner = &g_cli_window_cells);
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
                    finished = EMBED_CALL(detached_line_take, ServeDetachedCtx, .line = one->pending + start,
                                          .size = cut - start, .lines = &lines, .ended = &ended);
                    start = cut + 1ull;
                }
            }
            memor.move_down(&(MemoriaCfg){
                .dst = one->pending, .src = one->pending + start, .bytes = (size_t)(one->pending_size - start)});
            one->pending_size -= start;
            mmgr_line_ch(&lines, ']');
            if ((lines.at > 2ull) && mmgr_line_ok(&lines))
            {
                EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "run-lines", .body = one->lines,
                           .size = lines.at);
            }
            if (finished)
            {
                EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "run-end", .body = ended_room,
                           .size = ended.at);
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
                EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "run-end", .body = ended,
                           .size = line.at);
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
            if (number && (pid <= 0xffffffffull) &&
                !EMBED_CALL(cli_pid_internal.process_running, CliPidCtx, .pid = (unsigned int)pid) && (whole == at))
            {
                char ended[256];
                VerbaLinea line = {ended, sizeof(ended), 0ull};
                mmgr_line_put(&line, "{\"run\":");
                mmgr_line_decimal(&line, one->run, 1u);
                mmgr_line_put(&line, ",\"code\":null,\"stopped\":true,\"views\":[],\"ms\":0.0}");
                EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "run-end", .body = ended,
                           .size = line.at);
                break;
            }
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = CLI_SERVE_FOLLOW_EVERY);
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_follower[one->place] = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    g_cli_cells->persistent_buf_release(one, &g_cli_window_cells);
}

// the detached run `base` followed on a thread of its own: 1, or 0 where every follower is taken or no room for one is
// left
EMBED_INLINE int detached_follow(const ServeDetachedCtx *args)
{
    const char *name = args->base;
    const unsigned long long name_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->base,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_past != 0ull) ? (args->base + name_past) : name;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
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
        mmgr_line_put(&line, args->base);
        (void)mmgr_line_finish(&line);
        one->run = strtoull(name, NULL, 10);
        one->thread = (CliThread){detached_follower, one};
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &one->thread))
        {
            s_follower[at] = NULL;
            g_cli_cells->persistent_buf_release(one, &g_cli_window_cells);
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return one != NULL;
}

int cli_serve_detached_start(const CliServeErrMsgOutCtx *args)
{
    const char *job = args->job;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    CliCatalog *const catalog = EMBED_CALL(cli_serve_err_msg_root_internal.catalog_take, CliServeErrMsgRootCtx,
                                           .root_read = root, .err_msg = err_msg);
    if (catalog == NULL)
    {
        return 0;
    }
    const unsigned int found = EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = catalog, .id = job);
    const CliValues *const values = cli_serve_err_msg_out_internal.values_get(args);
    const int checked =
        (found != CLI_CATALOG_NONE) && EMBED_CALL(cli_runner_err_msg.check, CliRunnerErrMsgCfg, .catalog = catalog,
                                                  .job = found, .values = values, .err_msg = err_msg);
    cli_serve_catalog_free();
    if (found == CLI_CATALOG_NONE)
    {
        mmgr_line_put(err_msg, "no job ");
        mmgr_line_put(err_msg, job);
    }
    if (!checked)
    {
        return 0;
    }
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->detached_dir))
    {
        mmgr_line_put(err_msg, args->detached_dir);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    const unsigned long long run = detached_number();
    char base[CLI_PATH_MAX_LEN];
    char number[32];
    VerbaLinea named = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&named, run, 1u);
    (void)mmgr_line_finish(&named);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->detached_dir, .relative = number, .joined = base);
    char job_file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = base, .kind = "job",
               .path_out = job_file);
    static char s_spec[1u << 20u];
    static CliLock s_spec_lock = CLI_LOCK_INIT;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_spec_lock);
    VerbaLinea spec = {s_spec, sizeof(s_spec), 0ull};
    mmgr_line_put(&spec, "{\"job\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &spec, .text = job,
               .length = cellul.len(&(CatenaFinitaCfg){.src = job, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&spec, ",\"root\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &spec,
               .text_bytes = (const unsigned char *)root,
               .length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&spec, ",\"values\":");
    const unsigned int given =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = args->request, .key = "values");
    if (given != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->request->json,
                   .node = given, .out = &spec, .indent = 0u, .sorted = 1);
    }
    else
    {
        mmgr_line_put(&spec, "{}");
    }
    mmgr_line_ch(&spec, '}');
    const int written =
        mmgr_line_ok(&spec) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                          .path = job_file, .bytes = s_spec, .size = spec.at);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_spec_lock);
    if (!written)
    {
        mmgr_line_put(err_msg, job_file);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    char program[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_room_internal.program_own, CliPathRoomCtx, .path_out = program,
                    .room_size = sizeof(program)))
    {
        mmgr_line_put(err_msg, "orior's program could not be found");
        return 0;
    }
    const char *const words[4] = {program, "detached-run", base, NULL};
    CliSpawned spawned;
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned, .words = words,
                    .folder = NULL, .settings = NULL, .count = 0u, .detached = 1, .err_msg = err_msg))
    {
        return 0;
    }
    (void)EMBED_CALL(detached_follow, ServeDetachedCtx, .base = base);
    mmgr_line_decimal(out, run, 1u);
    return 1;
}

int cli_serve_run_follow(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long run = 0ull;
    char detached_dir[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    const int has_detached_dir = g_cli_serve.has_detached_dir;
    memor.cpy(&(MemoriaCfg){.dst = detached_dir, .src = g_cli_serve.detached_dir, .bytes = sizeof(detached_dir)});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    if (!has_detached_dir)
    {
        mmgr_line_put(err_msg, "this tree stores no runs");
        return 0;
    }
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "run",
                    .value = &run, .err_msg = err_msg))
    {
        return 0;
    }
    char number[32];
    VerbaLinea named = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&named, run, 1u);
    (void)mmgr_line_finish(&named);
    char base[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = detached_dir, .relative = number, .joined = base);
    (void)EMBED_CALL(detached_follow, ServeDetachedCtx, .base = base);
    mmgr_line_put(args->out, "null");
    return 1;
}

int cli_serve_job_stop(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long run = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "run",
                    .value = &run, .err_msg = err_msg))
    {
        return 0;
    }
    char detached_dir[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    const int has_detached_dir = g_cli_serve.has_detached_dir;
    memor.cpy(&(MemoriaCfg){.dst = detached_dir, .src = g_cli_serve.detached_dir, .bytes = sizeof(detached_dir)});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    if (has_detached_dir)
    {
        char number[32];
        VerbaLinea named = {number, sizeof(number), 0ull};
        mmgr_line_decimal(&named, run, 1u);
        (void)mmgr_line_finish(&named);
        char base[CLI_PATH_MAX_LEN];
        char job_file[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = detached_dir, .relative = number, .joined = base);
        EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = base, .kind = "job",
                   .path_out = job_file);
        if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = job_file))
        {
            char stop_file[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = base, .kind = "stop",
                       .path_out = stop_file);
            if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = stop_file,
                            .bytes = "", .size = 0ull))
            {
                mmgr_line_put(err_msg, stop_file);
                mmgr_line_put(err_msg, ": it could not be written");
                return 0;
            }
            mmgr_line_put(args->out, "null");
            return 1;
        }
    }
    const int stopped = EMBED_CALL(cli_runner_err_msg_run_number.runs_stop, CliRunnerErrMsgRunNumberCfg,
                                   .run_number = run, .err_msg = err_msg);
    mmgr_line_put(args->out, stopped ? "null" : "");
    return stopped;
}
