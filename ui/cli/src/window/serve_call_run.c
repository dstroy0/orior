// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_run.c: the calls that run things: the catalog of jobs, a job started, here or detached as a process of its
// own where the server stores runs, and stopped; the libraries a program loads copied beside it; the detached runs
// listed and followed; the runs of the query read, readied, stopped and joined, a blob queued on its channel to the run
// named or to the run the window is joined to and the call answered at once, the backend's own threads handing it on;
// the cores reserved for the window's work; the tests found, run and stopped; a profile started and stopped; and the
// comms layer's parts, channels and door
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../comms/comms.h"
#include "../debug/profile.h"
#include "../machine/cores.h"
#include "../programs/binary.h"
#include "../runs/qry.h"
#include "../tests/test_runs.h"
#include "../tests/testing.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "serve_internal.h"

// the most tests and files one run is given
#define CLI_SERVE_GIVEN 65536u

static int catalog_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(cli_serve_err_msg_out_request_internal.catalog_read, CliServeErrMsgOutRequestCtx,
                      .request = arguments, .out = out, .err_msg = err_msg);
}

static int job_start(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const job = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "job", .err_msg = err_msg);
    if (job == NULL)
    {
        return 0;
    }
    char detached_dir_buf[CLI_PATH_MAX_LEN];
    const int has_detached_dir = g_cli_serve.has_detached_dir;
    memor.cpy(
        &(MemoriaCfg){.dst = detached_dir_buf, .src = g_cli_serve.detached_dir, .bytes = sizeof(detached_dir_buf)});
    return has_detached_dir ? EMBED_CALL(cli_serve_err_msg_out_internal.detached_start, CliServeErrMsgOutCtx,
                                         .detached_dir = detached_dir_buf, .job = job, .request = arguments, .out = out,
                                         .err_msg = err_msg)
                            : EMBED_CALL(cli_serve_err_msg_out_internal.job_start, CliServeErrMsgOutCtx, .job = job,
                                         .request = arguments, .out = out, .err_msg = err_msg);
}

// a library copied beside a program, written after those written
static void new_copy(const CliEachNameCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}));
}

static int new_libraries_copy(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    const char *const given = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "path", .err_msg = err_msg);
    if ((given == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                       .root = root, .err_msg = err_msg))
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
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = given, .joined = file);
    }
    CliBinaryReport *const report =
        EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx, .size = sizeof(*report),
                   .what = "the report of what a program loads", .err_msg = err_msg);
    unsigned int count = 0u;
    char (*const folders)[CLI_PATH_MAX_LEN] =
        (report != NULL) ? cli_toolchains_path_taken(&count, &g_cli_window_cells) : NULL;
    const unsigned long long start = args->out->at;
    mmgr_line_ch(args->out, '[');
    const int done = (report != NULL) &&
                     EMBED_CALL(cli_binary_path_orior_path.read, CliBinaryPathOriorPathCfg, .path = file,
                                .orior_path = (const char (*)[CLI_PATH_MAX_LEN])folders, .count = count,
                                .report = report, .err_msg = err_msg) &&
                     EMBED_CALL(cli_binary_path.copy_beside, CliBinaryPathCfg, .path = file, .report_read = report,
                                .copied = new_copy, .context = args->out, .err_msg = err_msg);
    mmgr_line_ch(args->out, ']');
    args->out->at = done ? args->out->at : start;
    g_cli_cells->persistent_buf_release(folders, &g_cli_window_cells);
    g_cli_cells->persistent_buf_release(report, &g_cli_window_cells);
    return done;
}

static int qry_runs(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_qry_out_root.list, CliQryOutRootCfg, .root = root, .out = out);
    return 1;
}

static int qry_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned long long from = 0ull;
    return EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                      .root = root, .full = full, .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "from",
                      .value = &from, .err_msg = err_msg) &&
           EMBED_CALL(cli_qry_path_from.read, CliQryPathFromCfg, .path = full, .from = from, .out = out,
                      .err_msg = err_msg);
}

static int qry_ready(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                    .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out,
                  EMBED_CALL(cli_qry_path_panicked.ready, CliQryPathPanickedCfg, .path = full) ? "true" : "false");
    return 1;
}

static int qry_stop(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                    .root = root, .full = full, .err_msg = err_msg) ||
        !EMBED_CALL(cli_qry_path_out.stop, CliQryPathOutCfg, .path = full, .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int qry_append(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    static const char *const s_channels[CLI_CHANNELS] = {"primary", "control", "signal"};
    char root[CLI_PATH_MAX_LEN];
    char run[CLI_PATH_MAX_LEN];
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    const char *const text = (name != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "text", .err_msg = err_msg)
                                            : NULL;
    if (text == NULL)
    {
        return 0;
    }
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .otherwise = NULL);
    if (path != NULL)
    {
        if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                        .err_msg = err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = run);
    }
    else if (!EMBED_CALL(cli_qry.join, CliQryCfg, .path_out = run))
    {
        mmgr_line_put(err_msg, "the window is joined to no run");
        return 0;
    }
    const char *const channel = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                           .request = arguments, .key = "channel", .otherwise = "primary");
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
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    // the priority is cut to its low word, as the backend's priorities are 0 to 2
    const unsigned int priority =
        (unsigned int)EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                 .key = "priority", .otherwise_u64 = 2ull);
    EMBED_CALL(cli_qry_path_name.queue, CliQryPathNameCfg, .channel_named = at, .path = run, .name = name,
               .bytes = text, .size = length, .priority = priority);
    mmgr_line_put(args->out, "null");
    return 1;
}

static int qry_append_file(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char run[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const file = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "file", .err_msg = err_msg);
    if ((file == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx,
                                      .request = arguments, .root = root, .full = run, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = file, .joined = full);
    unsigned long long size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = full,
                                                   .size_pointer = &size, .owner = &g_cli_window_cells);
    if ((mapped == NULL) &&
        (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = full, .size_pointer = &size) ||
         (size > 0ull)))
    {
        mmgr_line_put(err_msg, file);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    char name[CLI_PATH_MAX_LEN];
    VerbaLinea named = {name, sizeof(name), 0ull};
    (void)EMBED_CALL(cli_text_from.put_swap, CliTextFromCfg, .out = &named, .text = file, .from = '\\', .to = '/');
    (void)mmgr_line_finish(&named);
    EMBED_CALL(cli_qry_path_name.queue, CliQryPathNameCfg, .channel_named = CLI_CHANNEL_PRIMARY, .path = run,
               .name = name, .bytes = (mapped != NULL) ? mapped : (const unsigned char *)"", .size = size,
               .priority = 2u);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_window_cells);
    mmgr_line_put(out, "null");
    return 1;
}

typedef struct
{
    VerbaLinea *out;
} ServeCallRunCtx;
// the run the window is joined to written, `null` where none
EMBED_INLINE void qry_joined_write(const ServeCallRunCtx *args)
{
    VerbaLinea *out = args->out;
    char run[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_qry.join, CliQryCfg, .path_out = run))
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)run,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = run, .cap = CLI_CELLS_POOL}));
        return;
    }
    mmgr_line_put(out, "null");
}

static int qry_join(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char run[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .otherwise = NULL);
    if (path != NULL)
    {
        if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                        .err_msg = err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = run);
    }
    EMBED_CALL(cli_qry_path_panicked.join_window, CliQryPathPanickedCfg, .path = (path != NULL) ? run : NULL);
    EMBED_CALL(qry_joined_write, ServeCallRunCtx, .out = out);
    return 1;
}

static int cores_reserve(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long reserved = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "reserved",
                    .value = &reserved, .err_msg = err_msg))
    {
        return 0;
    }
    // the cores reserved are cut to their low word, as no machine has more
    EMBED_CALL(cli_cores.reserve, CliCoresCfg, .reserved = (unsigned int)reserved);
    mmgr_line_put(args->out, "{\"reserved\":");
    mmgr_line_decimal(args->out, cli_cores_on_reserve(), 1u);
    mmgr_line_put(args->out, ",\"work\":");
    mmgr_line_decimal(args->out, cli_cores_for_work(), 1u);
    mmgr_line_put(args->out, ",\"machine\":");
    mmgr_line_decimal(args->out, cli_cores_machine(), 1u);
    mmgr_line_ch(args->out, '}');
    return 1;
}

// the cellblock's own account, `{"pool", "persistent", "temporary"}`, the pool's bytes and its two peaks, beside what
// the system reports the process has
static int cells_usage(const CliServeAnswerCfg *args)
{
    unsigned long long pool = 0ull;
    unsigned long long persistent = 0ull;
    unsigned long long temporary = 0ull;
    EMBED_CALL(cli_cells_table.report, CliCellsCfg, .pool = &pool, .persistent = &persistent, .temporary = &temporary);
    mmgr_line_put(args->out, "{\"pool\":");
    mmgr_line_decimal(args->out, pool, 1u);
    mmgr_line_put(args->out, ",\"persistent\":");
    mmgr_line_decimal(args->out, persistent, 1u);
    mmgr_line_put(args->out, ",\"temporary\":");
    mmgr_line_decimal(args->out, temporary, 1u);
    mmgr_line_ch(args->out, '}');
    return 1;
}

// every owner of a cell on the cellblock's list, for the audit of the cellblock
static int cells_owners(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, "{\"owners\":");
    EMBED_CALL(cli_cells_table.owners_write, CliCellsCfg, .out = out);
    mmgr_line_ch(out, '}');
    return 1;
}

static int qry_window(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, "{\"path\":");
    EMBED_CALL(qry_joined_write, ServeCallRunCtx, .out = out);
    mmgr_line_put(out, ",\"queued\":");
    EMBED_CALL(cli_qry_out_root.queue, CliQryOutRootCfg, .out = out);
    mmgr_line_ch(out, '}');
    return 1;
}

static int qry_on_join(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    if ((name == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx,
                                      .request = arguments, .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    // the joined bytes, a cell taken for the call
    char *const joined = EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx, .size = 16u << 20u,
                                    .what = "a run's joined bytes", .err_msg = err_msg);
    if (joined == NULL)
    {
        return 0;
    }
    VerbaLinea bytes = {joined, 16u << 20u, 0ull};
    const int done = EMBED_CALL(cli_qry_path_out.joined_bytes, CliQryPathOutCfg, .path = full, .name = name,
                                .out = &bytes, .err_msg = err_msg);
    if (done)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)joined, .length = mmgr_line_ok(&bytes) ? bytes.at : 0ull);
    }
    g_cli_cells->persistent_buf_release(joined, &g_cli_window_cells);
    return done;
}

// a test found written after those written
static void tests_write(const CliTestEachCfg *args)
{
    const CliTest *test = args->test;
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    EMBED_CALL(cli_testing.test_write, CliTestingCfg, .test = test, .out = out);
}

static int tests_find(const CliServeAnswerCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_ch(args->out, '[');
    EMBED_CALL(cli_testing_root.tests_find, CliTestingRootCfg, .root = root, .each = tests_write, .context = args->out);
    mmgr_line_ch(args->out, ']');
    return 1;
}

// what a test run reports sent as tests-run
static void tests_event(const CliEachJsonSizeCfg *args)
{
    unsigned long long size = args->size;
    EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "tests-run", .body = args->json, .size = size);
}

static int tests_run(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char **given = NULL;
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    int parallel = 0;
    int cover = 0;
    const CliJsonSink sink = {tests_event, NULL};
    unsigned long long run = 0ull;
    const int started =
        EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = arguments, .key = "given",
                   .most = CLI_SERVE_GIVEN, .items_pointer = &given, .count = &count, .err_msg = err_msg) &&
        EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "parallel",
                   .value_i32s = &parallel, .err_msg = err_msg) &&
        EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "cover",
                   .value_i32s = &cover, .err_msg = err_msg) &&
        EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                   .err_msg = err_msg) &&
        EMBED_CALL(cli_test_runs_root.start, CliTestRunsRootCfg, .root = root, .given = given, .count = count,
                   .parallel = parallel, .cover = cover, .sink = sink, .number = &run, .err_msg = err_msg);
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = given);
    if (started)
    {
        mmgr_line_decimal(args->out, run, 1u);
    }
    return started;
}

static int tests_stop(const CliServeAnswerCfg *args)
{
    cli_test_runs_stop();
    mmgr_line_put(args->out, "null");
    return 1;
}

// what a profile reports sent as profile
static void profile_event(const CliEachJsonSizeCfg *args)
{
    unsigned long long size = args->size;
    EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "profile", .body = args->json, .size = size);
}

static int profile_start(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    const CliJsonSink sink = {profile_event, NULL};
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_profile.start, CliProfileCfg, .root = root, .file = path,
                    .remote = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                         .key = "remote", .otherwise = NULL),
                    .sink = sink, .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int profile_stop(const CliServeAnswerCfg *args)
{
    cli_profile_stop();
    mmgr_line_put(args->out, "null");
    return 1;
}

static int comms_parts(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_comms_table.parts, CliCommsCfg, .out = out);
    return 1;
}

static int comms_channels(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_comms_table.channels, CliCommsCfg, .out = out);
    return 1;
}

static int comms_door(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char address[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_address.open_door, CliServeErrMsgAddressCfg, .address = address,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)address,
               .length = cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}));
    return 1;
}

const CliServeCall g_cli_serve_run_calls[] = {
    {"catalog_read", catalog_read},
    {"job_start", job_start},
    {"job_stop", cli_serve_job_stop},
    {"made_libraries_copy", new_libraries_copy},
    {"runs_detached", cli_serve_runs_detach},
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
    {"qry_joined", qry_on_join},
    {"tests_found", tests_find},
    {"tests_run", tests_run},
    {"tests_stop", tests_stop},
    {"profile_start", profile_start},
    {"profile_stop", profile_stop},
    {"comms_parts", comms_parts},
    {"comms_channels", comms_channels},
    {"comms_door", comms_door},
};
const unsigned int g_cli_serve_run_count = sizeof(g_cli_serve_run_calls) / sizeof(g_cli_serve_run_calls[0]);
