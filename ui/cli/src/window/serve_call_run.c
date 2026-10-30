// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_run.c: the calls that run things: the catalog of jobs, a job started, here or detached as a process of its
// own where the server stores runs, and stopped; the libraries a program loads copied beside it; the detached runs
// listed and followed; the runs of the query read, readied, stopped and joined, a blob queued on its channel to the run
// named or to the run the window is joined to and the call answered at once, the backend's own threads handing it on;
// the cores reserved for the window's work; the tests found, run and stopped; a profile started and stopped; and the
// comms layer's parts, channels and door
#include "../comms/comms.h"
#include "../debug/profile.h"
#include "../machine/cores.h"
#include "../programs/binary.h"
#include "../runs/qry.h"
#include "../tests/test_runs.h"
#include "../tests/testing.h"
#include "../toolchains/toolchains.h"
#include "serve_internal.h"

// the most tests and files one run is given
#define CLI_SERVE_GIVEN 65536u

static int catalog_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    return cli_serve_catalog_read(args, out, err_msg);
}

static int job_start(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const job = cli_serve_string(args, "job", err_msg);
    if (job == NULL)
    {
        return 0;
    }
    char detached_dir_buf[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_serve.lock);
    const int has_detached_dir = g_cli_serve.has_detached_dir;
    memor.cpy(
        &(MemoriaCfg){.dst = detached_dir_buf, .src = g_cli_serve.detached_dir, .bytes = sizeof(detached_dir_buf)});
    cli_lock_give(&g_cli_serve.lock);
    return has_detached_dir ? cli_serve_detached_start(detached_dir_buf, job, args, out, err_msg)
                            : cli_serve_job_start(job, args, out, err_msg);
}

// a library copied beside a program, written after those written
static void made_copied(void *context, const char *name)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    cli_json_quoted_lossy(out, (const unsigned char *)name,
                          cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
}

static int made_libraries_copy(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    const char *const given = cli_serve_string(args, "path", err_msg);
    if ((given == NULL) || !cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    const char first = given[0];
    const int lettered = ((first >= 'a') && (first <= 'z')) || ((first >= 'A') && (first <= 'Z'));
    const int whole = (first == '/') || (first == '\\') || (lettered && (given[1] == ':'));
    if (whole)
    {
        memor.cpy(&(MemoriaCfg){.dst = file,
                                .src = given,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = given, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        cli_full(root, given, file);
    }
    CliBinaryReport *const report = cli_serve_cell(sizeof(*report), "the report of what a program loads", err_msg);
    unsigned int count = 0u;
    char (*const folders)[CLI_PATH_MAX_LEN] =
        (report != NULL) ? cli_toolchains_path_taken(&count, &g_cli_window_cells) : NULL;
    const unsigned long long start = out->at;
    mmgr_line_ch(out, '[');
    const int done = (report != NULL) &&
                     cli_binary_read(file, (const char (*)[CLI_PATH_MAX_LEN])folders, count, report, err_msg) &&
                     cli_binary_copy_beside(file, report, made_copied, out, err_msg);
    mmgr_line_ch(out, ']');
    out->at = done ? out->at : start;
    g_cli_cells->persistent_buf_release(folders, &g_cli_window_cells);
    g_cli_cells->persistent_buf_release(report, &g_cli_window_cells);
    return done;
}

static int qry_runs(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    cli_qry_listed(root, out);
    return 1;
}

static int qry_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned long long from = 0ull;
    return cli_serve_at(args, root, full, err_msg) && cli_serve_whole(args, "from", &from, err_msg) &&
           cli_qry_read(full, from, out, err_msg);
}

static int qry_ready(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!cli_serve_at(args, root, full, err_msg))
    {
        return 0;
    }
    mmgr_line_put(out, cli_qry_ready(full) ? "true" : "false");
    return 1;
}

static int qry_stop(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!cli_serve_at(args, root, full, err_msg) || !cli_qry_stop(full, err_msg))
    {
        return 0;
    }
    mmgr_line_put(out, "null");
    return 1;
}

static int qry_append(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    static const char *const s_channels[CLI_CHANNELS] = {"primary", "control", "signal"};
    char root[CLI_PATH_MAX_LEN];
    char run[CLI_PATH_MAX_LEN];
    const char *const name = cli_serve_string(args, "name", err_msg);
    const char *const text = (name != NULL) ? cli_serve_string(args, "text", err_msg) : NULL;
    if (text == NULL)
    {
        return 0;
    }
    const char *const path = cli_serve_string_or(args, "path", NULL);
    if (path != NULL)
    {
        if (!cli_serve_tree(root, err_msg))
        {
            return 0;
        }
        cli_full(root, path, run);
    }
    else if (!cli_qry_joined(run))
    {
        mmgr_line_put(err_msg, "the window is joined to no run");
        return 0;
    }
    const char *const channel = cli_serve_string_or(args, "channel", "primary");
    unsigned int at = 0u;
    while ((at < CLI_CHANNELS) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = s_channels[at], .other = channel, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    if (at == CLI_CHANNELS)
    {
        mmgr_line_put(err_msg, "channel: unknown variant `");
        mmgr_line_put(err_msg, channel);
        mmgr_line_put(err_msg, "`, expected one of `primary`, `control`, `signal`");
        return 0;
    }
    unsigned long long length = 0ull;
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    // the priority is cut to its low word, as the backend's priorities are 0 to 2
    const unsigned int priority = (unsigned int)cli_serve_whole_or(args, "priority", 2ull);
    cli_qry_queue(at, run, name, text, length, priority);
    mmgr_line_put(out, "null");
    return 1;
}

static int qry_append_file(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char run[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const file = cli_serve_string(args, "file", err_msg);
    if ((file == NULL) || !cli_serve_at(args, root, run, err_msg))
    {
        return 0;
    }
    cli_full(root, file, full);
    unsigned long long size = 0ull;
    const unsigned char *const mapped = cli_file_read(full, &size);
    if ((mapped == NULL) && (!cli_file_size(full, &size) || (size > 0ull)))
    {
        mmgr_line_put(err_msg, file);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    char name[CLI_PATH_MAX_LEN];
    VerbaLinea named = {name, sizeof(name), 0ull};
    for (const char *at = file; *at != '\0'; at += 1)
    {
        mmgr_line_ch(&named, (*at == '\\') ? '/' : *at);
    }
    (void)mmgr_line_finish(&named);
    cli_qry_queue(CLI_CHANNEL_PRIMARY, run, name, (mapped != NULL) ? mapped : (const unsigned char *)"", size, 2u);
    if (mapped != NULL)
    {
        cli_file_let_go(mapped, size);
    }
    mmgr_line_put(out, "null");
    return 1;
}

// the run the window is joined to written, `null` where none
static void qry_joined_written(VerbaLinea *out)
{
    char run[CLI_PATH_MAX_LEN];
    if (cli_qry_joined(run))
    {
        cli_json_quoted_lossy(out, (const unsigned char *)run,
                              cellul.len(&(CatenaFinitaCfg){.src = run, .cap = CLI_CELLS_POOL}));
        return;
    }
    mmgr_line_put(out, "null");
}

static int qry_join(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char run[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string_or(args, "path", NULL);
    if (path != NULL)
    {
        if (!cli_serve_tree(root, err_msg))
        {
            return 0;
        }
        cli_full(root, path, run);
    }
    cli_qry_join_window((path != NULL) ? run : NULL);
    qry_joined_written(out);
    return 1;
}

static int cores_reserve(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long reserved = 0ull;
    if (!cli_serve_whole(args, "reserved", &reserved, err_msg))
    {
        return 0;
    }
    // the cores reserved are cut to their low word, as no machine has more
    cli_cores_reserve((unsigned int)reserved);
    mmgr_line_put(out, "{\"reserved\":");
    mmgr_line_decimal(out, cli_cores_reserved(), 1u);
    mmgr_line_put(out, ",\"work\":");
    mmgr_line_decimal(out, cli_cores_for_work(), 1u);
    mmgr_line_put(out, ",\"machine\":");
    mmgr_line_decimal(out, cli_cores_machine(), 1u);
    mmgr_line_ch(out, '}');
    return 1;
}

// the cellblock's own account, `{"pool", "persistent", "temporary"}`, the pool's bytes and its two peaks, beside what
// the system reports the process has
static int cells_usage(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    unsigned long long pool = 0ull;
    unsigned long long persistent = 0ull;
    unsigned long long temporary = 0ull;
    cli_cells_report(&pool, &persistent, &temporary);
    mmgr_line_put(out, "{\"pool\":");
    mmgr_line_decimal(out, pool, 1u);
    mmgr_line_put(out, ",\"persistent\":");
    mmgr_line_decimal(out, persistent, 1u);
    mmgr_line_put(out, ",\"temporary\":");
    mmgr_line_decimal(out, temporary, 1u);
    mmgr_line_ch(out, '}');
    return 1;
}

// every owner of a cell on the cellblock's list, for the audit of the cellblock
static int cells_owners(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    mmgr_line_put(out, "{\"owners\":");
    cli_cells_owners_written(out);
    mmgr_line_ch(out, '}');
    return 1;
}

static int qry_window(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    mmgr_line_put(out, "{\"path\":");
    qry_joined_written(out);
    mmgr_line_put(out, ",\"queued\":");
    cli_qry_queued(out);
    mmgr_line_ch(out, '}');
    return 1;
}

static int qry_joined(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const name = cli_serve_string(args, "name", err_msg);
    if ((name == NULL) || !cli_serve_at(args, root, full, err_msg))
    {
        return 0;
    }
    // the joined bytes, a cell taken for the call
    char *const joined = cli_serve_cell(16u << 20u, "a run's joined bytes", err_msg);
    if (joined == NULL)
    {
        return 0;
    }
    VerbaLinea bytes = {joined, 16u << 20u, 0ull};
    const int done = cli_qry_joined_bytes(full, name, &bytes, err_msg);
    if (done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)joined, mmgr_line_ok(&bytes) ? bytes.at : 0ull);
    }
    g_cli_cells->persistent_buf_release(joined, &g_cli_window_cells);
    return done;
}

// a test found written after those written
static void tests_written(void *context, const CliTest *test)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    cli_test_written(test, out);
}

static int tests_found(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    mmgr_line_ch(out, '[');
    cli_tests_found(root, tests_written, out);
    mmgr_line_ch(out, ']');
    return 1;
}

// what a test run reports sent as tests-run
static void tests_event(void *context, const char *json, unsigned long long size)
{
    (void)context;
    cli_serve_emitted("tests-run", json, size);
}

static int tests_run(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char **given = NULL;
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    int parallel = 0;
    int cover = 0;
    const CliJsonSink sink = {tests_event, NULL};
    unsigned long long run = 0ull;
    const int started = cli_serve_strings_taken(args, "given", CLI_SERVE_GIVEN, &given, &count, err_msg) &&
                        cli_serve_bool(args, "parallel", &parallel, err_msg) &&
                        cli_serve_bool(args, "cover", &cover, err_msg) && cli_serve_tree(root, err_msg) &&
                        cli_test_runs_start(root, given, count, parallel, cover, sink, &run, err_msg);
    cli_serve_strings_given(given);
    if (started)
    {
        mmgr_line_decimal(out, run, 1u);
    }
    return started;
}

static int tests_stop(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_test_runs_stop();
    mmgr_line_put(out, "null");
    return 1;
}

// what a profile reports sent as profile
static void profile_event(void *context, const char *json, unsigned long long size)
{
    (void)context;
    cli_serve_emitted("profile", json, size);
}

static int profile_start(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string(args, "path", err_msg);
    const CliJsonSink sink = {profile_event, NULL};
    if ((path == NULL) || !cli_serve_tree(root, err_msg) ||
        !cli_profile_start(root, path, cli_serve_string_or(args, "remote", NULL), sink, err_msg))
    {
        return 0;
    }
    mmgr_line_put(out, "null");
    return 1;
}

static int profile_stop(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_profile_stop();
    mmgr_line_put(out, "null");
    return 1;
}

static int comms_parts(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_comms_parts(out);
    return 1;
}

static int comms_channels(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_comms_channels(out);
    return 1;
}

static int comms_door(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char address[CLI_PATH_MAX_LEN];
    if (!cli_serve_open_door(address, err_msg))
    {
        return 0;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)address,
                          cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}));
    return 1;
}

const CliServeCall g_cli_serve_run_calls[] = {
    {"catalog_read", catalog_read},
    {"job_start", job_start},
    {"job_stop", cli_serve_job_stop},
    {"made_libraries_copy", made_libraries_copy},
    {"runs_detached", cli_serve_runs_detached},
    {"run_follow", cli_serve_run_follow},
    {"qry_runs", qry_runs},
    {"qry_read", qry_read},
    {"qry_ready", qry_ready},
    {"qry_stop", qry_stop},
    {"qry_append", qry_append},
    {"qry_append_file", qry_append_file},
    {"qry_join", qry_join},
    {"cores_reserve", cores_reserve},
    {"qry_window", qry_window},
    {"cells_usage", cells_usage},
    {"cells_owners", cells_owners},
    {"qry_joined", qry_joined},
    {"tests_found", tests_found},
    {"tests_run", tests_run},
    {"tests_stop", tests_stop},
    {"profile_start", profile_start},
    {"profile_stop", profile_stop},
    {"comms_parts", comms_parts},
    {"comms_channels", comms_channels},
    {"comms_door", comms_door},
};
const unsigned int g_cli_serve_run_count = sizeof(g_cli_serve_run_calls) / sizeof(g_cli_serve_run_calls[0]);
