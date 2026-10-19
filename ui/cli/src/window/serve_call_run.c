// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_run.c: the calls that run things: the catalog of jobs, a job started, here or kept as a process of its own
// where the server keeps runs, and stopped; the libraries a program loads copied beside it; the kept runs listed and
// followed; the runs of the query read, readied, stopped and joined, a blob queued on its channel to the run named or
// to the run the window is joined to and the call answered at once, the backend's own threads handing it on; the cores
// kept for the window's work; the tests found, run and stopped; a profile started and stopped; and the comms layer's
// parts, channels and door
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

static int catalog_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    return cli_serve_catalog_read(args, out, said);
}

static int job_start(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const job = cli_serve_string(args, "job", said);
    if (job == NULL)
    {
        return 0;
    }
    char keep[CLI_PATH_LONGEST];
    cli_lock_take(&g_cli_serve.lock);
    const int keeping = g_cli_serve.has_keeping;
    memor.cpy(&(MemoriaCfg){.dst = keep, .src = g_cli_serve.keeping, .bytes = sizeof(keep)});
    cli_lock_give(&g_cli_serve.lock);
    return keeping ? cli_serve_kept_start(keep, job, args, out, said) : cli_serve_job_start(job, args, out, said);
}

// a library copied beside a program, written after those written
static void made_copied(void *context, const char *name)
{
    CliLine *const out = context;
    cli_line_text(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    cli_json_quoted_lossy(out, (const unsigned char *)name,
                          cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
}

static int made_libraries_copy(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static CliBinaryReport s_report;
    static CliLock s_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    const char *const given = cli_serve_string(args, "path", said);
    if ((given == NULL) || !cli_serve_tree(root, said))
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
    cli_lock_take(&s_lock);
    unsigned int count = 0u;
    char (*const folders)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
    const unsigned long long start = out->at;
    cli_line_character(out, '[');
    const int done = cli_binary_read(file, (const char (*)[CLI_PATH_LONGEST])folders, count, &s_report, said) &&
                     cli_binary_copy_beside(file, &s_report, made_copied, out, said);
    cli_line_character(out, ']');
    out->at = done ? out->at : start;
    cli_cell_given(folders);
    cli_lock_give(&s_lock);
    return done;
}

static int qry_runs(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_qry_listed(root, out);
    return 1;
}

static int qry_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned long long from = 0ull;
    return cli_serve_at(args, root, full, said) && cli_serve_whole(args, "from", &from, said) &&
           cli_qry_read(full, from, out, said);
}

static int qry_ready(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    if (!cli_serve_at(args, root, full, said))
    {
        return 0;
    }
    cli_line_text(out, cli_qry_ready(full) ? "true" : "false");
    return 1;
}

static int qry_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    if (!cli_serve_at(args, root, full, said) || !cli_qry_stop(full, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int qry_append(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *const s_channels[CLI_CHANNELS] = {"primary", "control", "signal"};
    char root[CLI_PATH_LONGEST];
    char run[CLI_PATH_LONGEST];
    const char *const name = cli_serve_string(args, "name", said);
    const char *const text = (name != NULL) ? cli_serve_string(args, "text", said) : NULL;
    if (text == NULL)
    {
        return 0;
    }
    const char *const path = cli_serve_string_or(args, "path", NULL);
    if (path != NULL)
    {
        if (!cli_serve_tree(root, said))
        {
            return 0;
        }
        cli_full(root, path, run);
    }
    else if (!cli_qry_joined(run))
    {
        cli_line_text(said, "the window is joined to no run");
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
        cli_line_text(said, "channel: unknown variant `");
        cli_line_text(said, channel);
        cli_line_text(said, "`, expected one of `primary`, `control`, `signal`");
        return 0;
    }
    unsigned long long length = 0ull;
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    // the priority is cut to its low word, as the backend's priorities are 0 to 2
    const unsigned int priority = (unsigned int)cli_serve_whole_or(args, "priority", 2ull);
    cli_qry_queue(at, run, name, text, length, priority);
    cli_line_text(out, "null");
    return 1;
}

static int qry_append_file(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char run[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    const char *const file = cli_serve_string(args, "file", said);
    if ((file == NULL) || !cli_serve_at(args, root, run, said))
    {
        return 0;
    }
    cli_full(root, file, full);
    unsigned long long size = 0ull;
    const unsigned char *const mapped = qry_member_file_mapped(full, &size);
    if ((mapped == NULL) && (!cli_file_size(full, &size) || (size > 0ull)))
    {
        cli_line_text(said, file);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    char name[CLI_PATH_LONGEST];
    CliLine named = {name, sizeof(name), 0ull};
    for (const char *at = file; *at != '\0'; at += 1)
    {
        cli_line_character(&named, (*at == '\\') ? '/' : *at);
    }
    (void)cli_line_closed(&named);
    cli_qry_queue(CLI_CHANNEL_PRIMARY, run, name, (mapped != NULL) ? mapped : (const unsigned char *)"", size, 2u);
    if (mapped != NULL)
    {
        qry_member_file_unmapped(mapped, size);
    }
    cli_line_text(out, "null");
    return 1;
}

// the run the window is joined to written, `null` where none
static void qry_joined_written(CliLine *out)
{
    char run[CLI_PATH_LONGEST];
    if (cli_qry_joined(run))
    {
        cli_json_quoted_lossy(out, (const unsigned char *)run,
                              cellul.len(&(CatenaFinitaCfg){.src = run, .cap = CLI_CELLS_POOL}));
        return;
    }
    cli_line_text(out, "null");
}

static int qry_join(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char run[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string_or(args, "path", NULL);
    if (path != NULL)
    {
        if (!cli_serve_tree(root, said))
        {
            return 0;
        }
        cli_full(root, path, run);
    }
    cli_qry_join_window((path != NULL) ? run : NULL);
    qry_joined_written(out);
    return 1;
}

static int cores_keep(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long kept = 0ull;
    if (!cli_serve_whole(args, "kept", &kept, said))
    {
        return 0;
    }
    // the cores kept are cut to their low word, as no machine has more
    cli_cores_keep((unsigned int)kept);
    cli_line_text(out, "{\"kept\":");
    cli_line_decimal(out, cli_cores_kept(), 1u);
    cli_line_text(out, ",\"work\":");
    cli_line_decimal(out, cli_cores_for_work(), 1u);
    cli_line_text(out, ",\"machine\":");
    cli_line_decimal(out, cli_cores_machine(), 1u);
    cli_line_character(out, '}');
    return 1;
}

static int qry_window(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_line_text(out, "{\"path\":");
    qry_joined_written(out);
    cli_line_text(out, ",\"queued\":");
    cli_qry_queued(out);
    cli_line_character(out, '}');
    return 1;
}

static int qry_joined(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    const char *const name = cli_serve_string(args, "name", said);
    if ((name == NULL) || !cli_serve_at(args, root, full, said))
    {
        return 0;
    }
    // the joined bytes, a cell taken for the call
    char *const joined = cli_serve_cell(16u << 20u, "a run's joined bytes", said);
    if (joined == NULL)
    {
        return 0;
    }
    CliLine bytes = {joined, 16u << 20u, 0ull};
    const int done = cli_qry_joined_bytes(full, name, &bytes, said);
    if (done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)joined, cli_line_fits(&bytes) ? bytes.at : 0ull);
    }
    cli_cell_given(joined);
    return done;
}

// a test found written after those written
static void tests_written(void *context, const CliTest *test)
{
    CliLine *const out = context;
    cli_line_text(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    cli_test_written(test, out);
}

static int tests_found(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_line_character(out, '[');
    cli_tests_found(root, tests_written, out);
    cli_line_character(out, ']');
    return 1;
}

// what a test run tells sent as tests-run
static void tests_told(void *context, const char *heard, unsigned long long size)
{
    (void)context;
    cli_serve_emitted("tests-run", heard, size);
}

static int tests_run(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *s_given[CLI_SERVE_GIVEN];
    char root[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    int parallel = 0;
    int cover = 0;
    if (!cli_serve_strings(args, "given", s_given, CLI_SERVE_GIVEN, &count, said) ||
        !cli_serve_bool(args, "parallel", &parallel, said) || !cli_serve_bool(args, "cover", &cover, said) ||
        !cli_serve_tree(root, said))
    {
        return 0;
    }
    const CliTell tell = {tests_told, NULL};
    unsigned long long run = 0ull;
    if (!cli_test_runs_start(root, s_given, count, parallel, cover, tell, &run, said))
    {
        return 0;
    }
    cli_line_decimal(out, run, 1u);
    return 1;
}

static int tests_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_test_runs_stop();
    cli_line_text(out, "null");
    return 1;
}

// what a profile tells sent as profile
static void profile_told(void *context, const char *heard, unsigned long long size)
{
    (void)context;
    cli_serve_emitted("profile", heard, size);
}

static int profile_start(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    const CliTell tell = {profile_told, NULL};
    if ((path == NULL) || !cli_serve_tree(root, said) ||
        !cli_profile_start(root, path, cli_serve_string_or(args, "remote", NULL), tell, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int profile_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_profile_stop();
    cli_line_text(out, "null");
    return 1;
}

static int comms_parts(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_comms_parts(out);
    return 1;
}

static int comms_channels(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_comms_channels(out);
    return 1;
}

static int comms_door(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char address[CLI_PATH_LONGEST];
    if (!cli_serve_open_door(address, said))
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
    {"runs_kept", cli_serve_runs_kept},
    {"run_follow", cli_serve_run_follow},
    {"qry_runs", qry_runs},
    {"qry_read", qry_read},
    {"qry_ready", qry_ready},
    {"qry_stop", qry_stop},
    {"qry_append", qry_append},
    {"qry_append_file", qry_append_file},
    {"qry_join", qry_join},
    {"cores_keep", cores_keep},
    {"qry_window", qry_window},
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
