// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_runs.c: a run of a tree's tests: started with its Python's and cargo's processes over the machine's cores,
// what each process receives reported, and the run ended once its processes are, its coverage and its counts reported
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../machine/cores.h"
#include "../programs/executables.h"
#include "test_runs_internal.h"

#include "../text/text.h"

int g_cli_test_stopped;

static CliTestRun *s_runs[CLI_TEST_RUNS];
static CliWake s_runs_wake = CLI_WAKE_INIT;
static CliTestRun *s_going;
static unsigned long long s_last;
static unsigned long long s_scratch;

int cli_test_stop(void)
{
    const int stopped = g_cli_test_stopped;
    return stopped;
}

const char *cli_test_put(const CliTestRunsRunTextCtx *args)
{
    const char *put = "";
    if (args->run->used + args->length + 1ull <= CLI_TEST_TEXT)
    {
        char *const at = args->run->text + args->run->used;
        memor.cpy(&(MemoriaCfg){.dst = at, .src = args->text, .bytes = args->length});
        at[args->length] = '\0';
        args->run->used += args->length + 1ull;
        put = at;
    }
    return put;
}

void cli_test_outcome_count(const CliTestRunsRunLengthCtx *args)
{
    args->run->passed +=
        (cellul.eq(&(CatenaFinitaCfg){.src = args->outcome, .other = "passed", .cap = CLI_CELLS_POOL})) ? 1ull : 0ull;
    args->run->failed +=
        (cellul.eq(&(CatenaFinitaCfg){.src = args->outcome, .other = "failed", .cap = CLI_CELLS_POOL})) ? 1ull : 0ull;
    args->run->skipped +=
        ((!cellul.eq(&(CatenaFinitaCfg){.src = args->outcome, .other = "passed", .cap = CLI_CELLS_POOL})) &&
         (!cellul.eq(&(CatenaFinitaCfg){.src = args->outcome, .other = "failed", .cap = CLI_CELLS_POOL})))
            ? 1ull
            : 0ull;
}

void cli_test_message_set(const CliTestRunsRunTextCtx *args)
{
    if (args->run->message[0] == '\0')
    {
        VerbaLinea line = {args->run->message, sizeof(args->run->message), 0ull};
        mmgr_line_put_n(&line, args->text, args->length);
        mmgr_line_finish(&line);
    }
}

typedef struct
{
    const CliTestRun *run;
    unsigned int worker;
    const char *name;
    unsigned long long length;
    const char *root;
    unsigned int python_count;
    unsigned int rust_count;
    int cover;
    char *python;
    int *pytest;
    VerbaLinea *err_msg;
} TestRunsCtx;
// the place a process's name is received at in the run's table, or the empty place it would take; CLI_TEST_SEEN_MAX
// where the table is full
EMBED_INLINE unsigned int test_seen_place(const TestRunsCtx *args)
{
    unsigned long long hash = 1469598103934665603ull ^ args->worker;
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        hash = (hash ^ (unsigned char)args->name[at]) * 1099511628211ull;
    }
    for (unsigned int probe = 0u; probe < CLI_TEST_SEEN_MAX; probe += 1u)
    {
        const unsigned int place = (unsigned int)((hash + probe) % CLI_TEST_SEEN_MAX);
        const unsigned long long name_at = args->run->seen_name[place];
        // a name stored is its process's number, the name, and a 0, from one past its place in the texts
        if ((name_at == 0ull) ||
            (((unsigned char)args->run->seen_text[name_at - 1ull] == args->worker) &&
             EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->run->seen_text + name_at,
                        .other_pointer = args->name, .size = args->length) &&
             (args->run->seen_text[name_at + args->length] == '\0')))
        {
            return place;
        }
    }
    return CLI_TEST_SEEN_MAX;
}

int cli_test_visit(const CliTestRunsRunLengthCtx *args)
{
    const unsigned int place = EMBED_CALL(test_seen_place, TestRunsCtx, .run = args->run, .worker = args->worker_u32,
                                          .name = args->name, .length = args->length);
    int fresh = 0;
    if ((place < CLI_TEST_SEEN_MAX) && (args->run->seen_name[place] == 0ull) &&
        (args->run->seen_used + args->length + 2ull <= CLI_TEST_SEEN_TEXT_MAX))
    {
        args->run->seen_text[args->run->seen_used] = (char)args->worker_u32;
        memor.cpy(&(MemoriaCfg){
            .dst = args->run->seen_text + args->run->seen_used + 1ull, .src = args->name, .bytes = args->length});
        args->run->seen_text[args->run->seen_used + 1ull + args->length] = '\0';
        args->run->seen_name[place] = args->run->seen_used + 1ull;
        args->run->seen_outcome[place] = args->outcome;
        args->run->seen_used += args->length + 2ull;
        args->run->workers[args->worker_u32].seen_count += 1u;
        fresh = 1;
    }
    return fresh;
}

const char *cli_test_seen_outcome(const CliTestRunsRunLengthCtx *args)
{
    const unsigned int place = EMBED_CALL(test_seen_place, TestRunsCtx, .run = args->run, .worker = args->worker_u32,
                                          .name = args->name, .length = args->length);
    const char *const outcome =
        ((place < CLI_TEST_SEEN_MAX) && (args->run->seen_name[place] != 0ull)) ? args->run->seen_outcome[place] : NULL;
    return outcome;
}

int cli_test_seen_any(const CliTestRunsRunLengthCtx *args)
{
    const int any = (args->run->workers[args->worker_u32].seen_count > 0u);
    return any;
}

void cli_test_result_emit(const CliTestRunsOutcomeCtx *args)
{
    VerbaLinea out = {args->worker->event, sizeof(args->worker->event), 0ull};
    mmgr_line_put(&out, "{\"kind\":\"result\",\"run\":");
    mmgr_line_decimal(&out, args->worker->run->number, 1u);
    mmgr_line_put(&out, ",\"outcome\":{\"id\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&out, ",\"outcome\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &out, .text = args->outcome,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->outcome, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&out, ",\"message\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->message, .length = args->message_size);
    mmgr_line_put(&out, ",\"file\":");
    if (args->file == NULL)
    {
        mmgr_line_put(&out, "null");
    }
    else
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
                   .text_bytes = (const unsigned char *)args->file,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(&out, ",\"line\":");
    if (args->line_i64 < 0ll)
    {
        mmgr_line_put(&out, "null");
    }
    else
    {
        mmgr_line_decimal(&out, (unsigned long long)args->line_i64, 1u);
    }
    mmgr_line_put(&out, ",\"seconds\":");
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = &out, .value_f64 = args->seconds);
    mmgr_line_put(&out, "}}");
    if (mmgr_line_ok(&out))
    {
        EMBED_CALL(args->worker->run->sink.emit, CliEachJsonSizeCfg, .context = args->worker->run->sink.context,
                   .json = out.out, .size = out.at);
    }
}

// a run ended once each of its threads has: its coverage reported, its processes waited for, its counts and what it
// reports reported, its folder taken away, and its place given back
static void test_end(void *context)
{
    CliTestRun *const run = (CliTestRun *)context;
    while (run->going > 0u)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &run->wake);
    }
    EMBED_CALL(cli_test_runs_run_text_internal.test_coverage_emit, CliTestRunsRunTextCtx, .run = run);
    for (unsigned int at = 0u; at < run->worker_count; at += 1u)
    {
        CliTestWorker *const worker = &run->workers[at];
        if (worker->spawned.process != 0ull)
        {
            (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &worker->spawned);
            EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &worker->spawned);
        }
    }
    const int stopped = g_cli_test_stopped;
    s_going = (s_going == run) ? NULL : s_going;
    char done[CLI_TEST_MESSAGE_MAX_LEN + 512u];
    VerbaLinea out = {done, sizeof(done), 0ull};
    mmgr_line_put(&out, "{\"kind\":\"done\",\"run\":");
    mmgr_line_decimal(&out, run->number, 1u);
    mmgr_line_put(&out, ",\"passed\":");
    mmgr_line_decimal(&out, run->passed, 1u);
    mmgr_line_put(&out, ",\"failed\":");
    mmgr_line_decimal(&out, run->failed, 1u);
    mmgr_line_put(&out, ",\"skipped\":");
    mmgr_line_decimal(&out, run->skipped, 1u);
    mmgr_line_put(&out, ",\"seconds\":");
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = &out,
               .value_f64 = (double)(cli_clock_milliseconds() - run->started) / 1000.0);
    mmgr_line_put(&out, ",\"message\":");
    const char *const message = stopped ? "The run was stopped" : run->message;
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)message,
               .length = cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&out, '}');
    EMBED_CALL(run->sink.emit, CliEachJsonSizeCfg, .context = run->sink.context, .json = out.out, .size = out.at);
    if (run->scratch[0] != '\0')
    {
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = run->scratch);
    }
    s_runs[run->place] = NULL;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_runs_wake);
    g_cli_cells->persistent_buf_release(run, &g_cli_tests_cells);
}

void cli_test_runs_stop(void)
{
    g_cli_test_stopped = 1;
    CliTestRun *const run = s_going;
    for (unsigned int at = 0u; (run != NULL) && (at < run->worker_count); at += 1u)
    {
        if (run->workers[at].spawned.process != 0ull)
        {
            EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &run->workers[at].spawned);
        }
    }
    s_going = NULL;
}

// a place for a run taken once one is free, the run a cell given back as it ends, made empty, its number the next: it,
// or NULL where no room for it is left
static CliTestRun *test_run_take(void)
{
    CliTestRun *run = NULL;
    unsigned int at = CLI_TEST_RUNS;
    while (at == CLI_TEST_RUNS)
    {
        at = 0u;
        while ((at < CLI_TEST_RUNS) && (s_runs[at] != NULL))
        {
            at += 1u;
        }
        if (at == CLI_TEST_RUNS)
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_runs_wake);
        }
    }
    run = g_cli_cells->persistent_buf_alloc(sizeof(CliTestRun), &g_cli_tests_cells);
    if (run == NULL)
    {
        return NULL;
    }
    run->place = at;
    s_runs[at] = run;
    s_last += 1ull;
    run->number = s_last;
    g_cli_test_stopped = 0;
    s_going = run;
    run->wake = (CliWake)CLI_WAKE_INIT;
    run->going = 0u;
    run->passed = 0ull;
    run->failed = 0ull;
    run->skipped = 0ull;
    run->message[0] = '\0';
    run->worker_count = 0u;
    run->named_count = 0u;
    run->shares = 0u;
    run->used = 0ull;
    run->seen_used = 0ull;
    run->scratch[0] = '\0';
    memor.set(&(MemoriaCfg){.dst = run->seen_name, .val = 0, .bytes = sizeof(run->seen_name)});
    return run;
}

// what is wrong with the tests given before a run starts reported: 1 where nothing is, with the Python into `python`
// where Python's tests are given and whether it has pytest into `pytest`
EMBED_INLINE int test_check(const TestRunsCtx *args)
{
    const char *root = args->root;
    char *python = args->python;
    int *pytest = args->pytest;
    VerbaLinea *err_msg = args->err_msg;
    if (args->cover && (args->rust_count > 0u))
    {
        mmgr_line_put(err_msg, "Coverage is recorded for Python's tests alone: run Rust's without it");
        return 0;
    }
    char cargo[CLI_PATH_MAX_LEN];
    if ((args->rust_count > 0u) && !EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "rust",
                                               .name = "cargo", .path = cargo, .err_msg = err_msg))
    {
        return 0;
    }
    python[0] = '\0';
    if ((args->python_count > 0u) && !EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        mmgr_line_put(err_msg, "No Python is found to run the tests: File, Toolchains finds one");
        return 0;
    }
    *pytest = (python[0] != '\0') && EMBED_CALL(cli_test_runs_python_internal.test_has_pytest, CliTestRunsPythonCtx,
                                                .python = python, .root = root);
    if (args->cover && !*pytest)
    {
        mmgr_line_put(err_msg,
                      "Coverage is recorded by a run of pytest, which the tree's Python has not: python -m pip "
                      "install pytest");
        return 0;
    }
    return 1;
}

int cli_test_runs_start(const CliTestRunsRootCfg *args)
{
    const char *root = args->root;
    const char *const *given = args->given;
    unsigned int count = args->count;
    CliJsonSink sink = args->sink;
    unsigned long long *number = args->number;
    static const char *s_rust[CLI_TEST_NAMED];
    static const char *s_python[CLI_TEST_NAMED];
    cli_test_runs_stop();
    if (count == 0u)
    {
        mmgr_line_put(args->err_msg, "No test is given to run");
        return 0;
    }
    unsigned int rust_count = 0u;
    unsigned int python_count = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = given[at]) &&
            (rust_count < CLI_TEST_NAMED))
        {
            s_rust[rust_count] = given[at];
            rust_count += 1u;
        }
        else if (!EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = given[at]) &&
                 (python_count < CLI_TEST_NAMED))
        {
            s_python[python_count] = given[at];
            python_count += 1u;
        }
    }
    char python[CLI_PATH_MAX_LEN];
    int pytest = 0;
    if (!EMBED_CALL(test_check, TestRunsCtx, .root = root, .python_count = python_count, .rust_count = rust_count,
                    .cover = args->cover, .python = python, .pytest = &pytest, .err_msg = args->err_msg))
    {
        return 0;
    }
    CliTestRun *const run = test_run_take();
    if (run == NULL)
    {
        mmgr_line_put(args->err_msg, "a run of tests could not be taken");
        return 0;
    }
    run->sink = sink;
    memor.cpy(&(MemoriaCfg){.dst = run->root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    for (unsigned int at = 0u; at < python_count; at += 1u)
    {
        run->given[at] = EMBED_CALL(
            cli_test_runs_run_text_internal.test_put, CliTestRunsRunTextCtx, .run = run, .text = s_python[at],
            .length = cellul.len(&(CatenaFinitaCfg){.src = s_python[at], .cap = CLI_CELLS_POOL}));
    }
    if (python_count > 0u)
    {
        EMBED_CALL(cli_test_runs_run_count_internal.test_shares, CliTestRunsRunCountCtx, .run = run,
                   .count = python_count, .most = args->parallel ? cli_cores_for_work() : 1u);
    }
    const unsigned int cargo_runs =
        (rust_count > 0u) ? EMBED_CALL(cli_cargo_tests_given_table.cargo_targets_free, CliCargoTestsGivenCfg,
                                       .root = root, .given = s_rust, .count = rust_count)
                          : 0u;
    const char *const python_runner = pytest ? "pytest" : "unittest";
    char started[512];
    VerbaLinea out = {started, sizeof(started), 0ull};
    mmgr_line_put(&out, "{\"kind\":\"started\",\"run\":");
    mmgr_line_decimal(&out, run->number, 1u);
    mmgr_line_put(&out, ",\"given\":");
    mmgr_line_decimal(&out, python_count + rust_count, 1u);
    mmgr_line_put(&out, ",\"workers\":");
    mmgr_line_decimal(&out, run->shares + cargo_runs, 1u);
    mmgr_line_put(&out, ",\"runner\":\"");
    mmgr_line_put(&out, (run->shares == 0u) ? "cargo test" : python_runner);
    mmgr_line_put(&out, ((run->shares > 0u) && (cargo_runs > 0u)) ? " and cargo test\"}" : "\"}");
    EMBED_CALL(sink.emit, CliEachJsonSizeCfg, .context = sink.context, .json = out.out, .size = out.at);
    char temporary[CLI_PATH_MAX_LEN];
    char name[96];
    VerbaLinea name_line = {name, sizeof(name), 0ull};
    mmgr_line_put(&name_line, "orior-tests-");
    mmgr_line_decimal(&name_line, cli_process_own(), 1u);
    mmgr_line_ch(&name_line, '-');
    mmgr_line_decimal(&name_line, s_scratch, 1u);
    s_scratch += 1ull;
    mmgr_line_finish(&name_line);
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = temporary);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = temporary, .relative = name, .joined = run->scratch);
    run->started = cli_clock_milliseconds();
    int going = EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = run->scratch);
    if (!going)
    {
        mmgr_line_put(args->err_msg, run->scratch);
        mmgr_line_put(args->err_msg, ": it could not be made");
        run->scratch[0] = '\0';
    }
    going = going && ((rust_count == 0u) ||
                      EMBED_CALL(cli_test_runs_run_given_internal.cargo_workers_start, CliTestRunsRunGivenCtx,
                                 .run = run, .given = s_rust, .count = rust_count, .err_msg = args->err_msg));
    going = going && ((python_count == 0u) ||
                      EMBED_CALL(cli_test_runs_run_internal.python_workers_start, CliTestRunsRunCtx, .run = run,
                                 .python = python, .pytest = pytest, .cover = args->cover, .err_msg = args->err_msg));
    // a run that could not start each of its processes is stopped, and ends as one that was
    if (!going)
    {
        cli_test_runs_stop();
    }
    run->ender = (CliThread){test_end, run};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &run->ender))
    {
        test_end(run);
    }
    *number = run->number;
    return going;
}
