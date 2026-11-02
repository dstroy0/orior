// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_run.c: a run's Rust tests: the targets the tests given reach, a cargo test started for each on a thread of its
// own, and the harness's report read: each result as it is printed, each failure's message and place, and the first
// error of a build that failed
#include "../command_line/callback_cfg.h"
#include "../programs/executables.h"
#include "cargo_report.h"
#include "test_runs_internal.h"

// A target the tests given reach: its first test found, how many tests it has, the tests requested of it by their
// places among those found, and whether each was requested by its name
typedef struct
{
    unsigned int first;
    unsigned int total;
    unsigned int first_at;
    unsigned int count;
    int named;
} CliCargoGroup;
// the longest list of names a run gives the harness, in characters; past it a target runs whole and only the results
// requested of it are reported
#define CLI_CARGO_NAMES_MOST 16000ull

// the most targets a run reaches
#define CLI_CARGO_GROUPS 1024u

static CliCargoTests s_tests;
static CliCargoGroup s_groups[CLI_CARGO_GROUPS];
static unsigned int s_order[CLI_CARGO_FOUND];

typedef struct
{
    const CliCargoFound *one;
    const CliCargoFound *other;
    const char *const *given;
    unsigned int count;
    const char *text;
    const char *root;
    const CliTestWorker *worker;
    const char *path;
    char *out;
    CliTestWorker *worker_out;
    const char *errors;
    unsigned long long size;
    CliWords *words;
    const CliCargoTarget *target;
    CliTestRun *run;
    const CliCargoGroup *group;
    const char *cargo;
    VerbaLinea *err_msg;
} CargoRunCtx;
// whether two tests found are of one target of one manifest
EMBED_INLINE int run_same_target(const CargoRunCtx *args)
{
    const CliCargoFound *other = args->other;
    return (cellul.eq(
               &(CatenaFinitaCfg){.src = args->one->manifest, .other = other->manifest, .cap = CLI_CELLS_POOL})) &&
           (args->one->target.kind == other->target.kind) &&
           ((args->one->target.name == NULL) ||
            (cellul.eq(&(CatenaFinitaCfg){
                .src = args->one->target.name, .other = other->target.name, .cap = CLI_CELLS_POOL})));
}

// whether `given`, `count` of them, contains `text`
EMBED_INLINE int run_free(const CargoRunCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->given[at], .other = args->text, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// the targets the tests given reach, under the run lock: each with the tests given of it, whether all of its tests are
// and whether each was requested by its name: their count
static unsigned int run_group(const CargoRunCtx *args)
{
    const char *root = args->root;
    unsigned int count = args->count;
    EMBED_CALL(cli_cargo_tests_table.cargo_find, CliCargoTestsCfg, .root = root, .tests = &s_tests);
    unsigned int groups = 0u;
    unsigned int group_of[CLI_CARGO_FOUND];
    for (unsigned int at = 0u; at < s_tests.count; at += 1u)
    {
        unsigned int group = 0u;
        while ((group < groups) &&
               !EMBED_CALL(run_same_target, CargoRunCtx, .one = &s_tests.found[s_groups[group].first],
                           .other = &s_tests.found[at]))
        {
            group += 1u;
        }
        if ((group == groups) && (groups < CLI_CARGO_GROUPS))
        {
            s_groups[groups] = (CliCargoGroup){at, 0u, 0u, 0u, 1};
            groups += 1u;
        }
        group_of[at] = group;
        s_groups[group].total += (group < groups) ? 1u : 0u;
    }
    unsigned int placed = 0u;
    unsigned int group_count = 0u;
    for (unsigned int group = 0u; group < groups; group += 1u)
    {
        CliCargoGroup made = s_groups[group];
        made.first_at = placed;
        for (unsigned int at = 0u; at < s_tests.count; at += 1u)
        {
            const CliCargoFound *const one = &s_tests.found[at];
            const int by_file =
                EMBED_CALL(run_free, CargoRunCtx, .given = args->given, .count = count, .text = one->test.file);
            if ((group_of[at] == group) &&
                (EMBED_CALL(run_free, CargoRunCtx, .given = args->given, .count = count, .text = one->test.id) ||
                 by_file))
            {
                s_order[placed] = at;
                placed += 1u;
                made.named = made.named && !by_file;
            }
        }
        made.count = placed - made.first_at;
        if (made.count > 0u)
        {
            s_groups[group_count] = made;
            group_count += 1u;
        }
    }
    return group_count;
}

unsigned int cli_cargo_targets_free(const CliCargoTestsGivenCfg *args)
{
    const unsigned int groups =
        EMBED_CALL(run_group, CargoRunCtx, .root = args->root, .given = args->given, .count = args->count);
    return groups;
}

// a path a panic or an error is written with, from the workspace's folder where it is not whole, as a path of the tree
// into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void run_tree_path(const CargoRunCtx *args)
{
    const char *path = args->path;
    char full[CLI_PATH_MAX_LEN];
    const int whole = (path[0] == '/') || (path[0] == '\\') ||
                      ((path[0] != '\0') && (path[1] == ':') && ((path[2] == '/') || (path[2] == '\\')));
    if (whole)
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->worker->workspace, .relative = path,
                   .joined = full);
    }
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->worker->run->root, .path = full,
               .relative_out = args->out);
}

// a line of the harness's own received: the test's name and its outcome, reported where a test of the target is of that
// name
static void run_line(const CliWorkerTextSizeCfg *args)
{
    CliTestWorker *worker = args->worker;
    unsigned long long size = args->size;
    unsigned long long name = 0ull;
    unsigned long long name_size = 0ull;
    const char *outcome = NULL;
    if (!EMBED_CALL(cli_cargo_report_line.cargo_result_line, CliCargoReportLineCfg, .line_text = args->text,
                    .size = size, .name = &name, .name_size = &name_size, .outcome = &outcome))
    {
        return;
    }
    CliTestRun *const run = worker->run;
    for (unsigned int at = worker->named; at < worker->named + worker->named_count; at += 1u)
    {
        const CliTestNamed *const test = &run->named[at];
        if ((cellul.len(&(CatenaFinitaCfg){.src = test->full, .cap = CLI_CELLS_POOL}) != name_size) ||
            (memor.cmp(&(MemoriaCfg){.src = test->full, .other = args->text + name, .bytes = name_size}) != 0))
        {
            continue;
        }
        if (EMBED_CALL(cli_test_runs_run_length_internal.test_visit, CliTestRunsRunLengthCtx, .run = run,
                       .worker_u32 = worker->index, .name = args->text + name, .length = name_size, .outcome = outcome))
        {
            EMBED_CALL(cli_test_runs_run_length_internal.test_outcome_count, CliTestRunsRunLengthCtx, .run = run,
                       .outcome = outcome);
        }
        EMBED_CALL(cli_test_runs_outcome_internal.test_result_emit, CliTestRunsOutcomeCtx, .worker = worker,
                   .id = test->id, .outcome = outcome, .message = "", .message_size = 0ull, .file = NULL,
                   .line_i64 = -1ll, .seconds = 0.0);
        return;
    }
}

// a failure the harness reported reported where its test is one of the target's received as failed, at the place it
// panicked
static void run_failure_emit(const CliEachNameMessagePathLineCfg *args)
{
    const char *name = args->name;
    const char *message = args->message;
    unsigned long long message_size = args->message_size;
    const char *path = args->path;
    long long line = args->line;
    CliTestWorker *const worker = (CliTestWorker *)args->context;
    CliTestRun *const run = worker->run;
    const char *const seen_outcome =
        EMBED_CALL(cli_test_runs_run_length_internal.test_seen_outcome, CliTestRunsRunLengthCtx, .run = run,
                   .worker_u32 = worker->index, .name = name, .length = args->name_size);
    for (unsigned int at = worker->named;
         (seen_outcome != NULL) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = seen_outcome, .other = "failed", .cap = CLI_CELLS_POOL})) &&
         (at < worker->named + worker->named_count);
         at += 1u)
    {
        const CliTestNamed *const test = &run->named[at];
        if ((cellul.len(&(CatenaFinitaCfg){.src = test->full, .cap = CLI_CELLS_POOL}) != args->name_size) ||
            (memor.cmp(&(MemoriaCfg){.src = test->full, .other = name, .bytes = args->name_size}) != 0))
        {
            continue;
        }
        char file[CLI_PATH_MAX_LEN];
        const char *reported = test->file;
        long long reported_line = (long long)test->line;
        if (path != NULL)
        {
            EMBED_CALL(run_tree_path, CargoRunCtx, .worker = worker, .path = path, .out = file);
            reported = file;
            // a line of a panic counts from 1, and is reported from 0
            reported_line = (line > 0ll) ? line - 1ll : 0ll;
        }
        EMBED_CALL(cli_test_runs_outcome_internal.test_result_emit, CliTestRunsOutcomeCtx, .worker = worker,
                   .id = test->id, .outcome = "failed", .message = message, .message_size = message_size,
                   .file = reported, .line_i64 = reported_line, .seconds = 0.0);
        return;
    }
}

// a target that ran no test reports why: its build's first error fails each test given it
EMBED_INLINE void run_build_fail(const CargoRunCtx *args)
{
    CliTestWorker *worker = args->worker_out;
    unsigned long long size = args->size;
    char message[8192];
    char path[CLI_PATH_MAX_LEN];
    long long line = -1ll;
    unsigned long long length = 0ull;
    if (!EMBED_CALL(cli_cargo_report_size.cargo_build_error, CliCargoReportSizeCfg, .output = args->errors,
                    .size = size, .error = message, .room = sizeof(message), .length = &length, .path = path,
                    .line = &line))
    {
        path[0] = '\0';
        line = -1ll;
        EMBED_CALL(cli_cargo_report_size_text.cargo_last_line, CliCargoReportSizeTextCfg, .text = args->errors,
                   .size = size, .none = "cargo test ran no test", .out_out = message, .room = sizeof(message),
                   .length = &length);
    }
    char file[CLI_PATH_MAX_LEN];
    if (path[0] != '\0')
    {
        EMBED_CALL(run_tree_path, CargoRunCtx, .worker = worker, .path = path, .out = file);
    }
    CliTestRun *const run = worker->run;
    for (unsigned int at = worker->named; at < worker->named + worker->named_count; at += 1u)
    {
        EMBED_CALL(cli_test_runs_run_length_internal.test_outcome_count, CliTestRunsRunLengthCtx, .run = run,
                   .outcome = "failed");
        EMBED_CALL(cli_test_runs_outcome_internal.test_result_emit, CliTestRunsOutcomeCtx, .worker = worker,
                   .id = run->named[at].id, .outcome = "failed", .message = message, .message_size = length,
                   .file = (path[0] != '\0') ? file : NULL,
                   .line_i64 = (path[0] != '\0') ? ((line > 0ll) ? line - 1ll : 0ll) : -1ll, .seconds = 0.0);
    }
    EMBED_CALL(cli_test_runs_run_text_internal.test_message_set, CliTestRunsRunTextCtx, .run = run, .text = message,
               .length = length);
}

// a cargo process read until it ends: each result reported as the harness prints it, each failure as the process ends,
// and a build that failed reported for each test given the target
static void run_worker(void *context)
{
    CliTestWorker *const worker = (CliTestWorker *)context;
    EMBED_CALL(cli_test_runs_worker_line_internal.test_lines_read, CliTestRunsWorkerLineCtx, .worker = worker,
               .line_callback = run_line);
    EMBED_CALL(cli_test_runs_worker_line_internal.test_errors_wait, CliTestRunsWorkerLineCtx, .worker = worker);
    unsigned long long errors_size = 0ull;
    unsigned long long seen_size = 0ull;
    const char *const errors = EMBED_CALL(cli_test_runs_path_internal.test_file_read, CliTestRunsPathCtx,
                                          .path = worker->errors, .size_pointer = &errors_size);
    const char *const seen = EMBED_CALL(cli_test_runs_path_internal.test_file_read, CliTestRunsPathCtx,
                                        .path = worker->seen, .size_pointer = &seen_size);
    EMBED_CALL(cli_cargo_report_size_out.cargo_failures_each, CliCargoReportSizeOutCfg,
               .out = (seen != NULL) ? seen : "", .size = seen_size, .each = run_failure_emit, .context = worker);
    if (!EMBED_CALL(cli_test_runs_run_length_internal.test_seen_any, CliTestRunsRunLengthCtx, .run = worker->run,
                    .worker_u32 = worker->index) &&
        !cli_test_stop())
    {
        EMBED_CALL(run_build_fail, CargoRunCtx, .worker_out = worker, .errors = (errors != NULL) ? errors : "",
                   .size = errors_size);
    }
    EMBED_CALL(cli_test_runs_text_worker_internal.test_file_let_go, CliTestRunsTextWorkerCtx, .text = errors,
               .size = errors_size);
    EMBED_CALL(cli_test_runs_text_worker_internal.test_file_let_go, CliTestRunsTextWorkerCtx, .text = seen,
               .size = seen_size);
    CliTestRun *const run = worker->run;
    run->going -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &run->wake);
}

// the words that request from Cargo a target alone, put after the words
EMBED_INLINE void run_target_words(const CargoRunCtx *args)
{
    CliWords *words = args->words;
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
               .word = (args->target->kind == CLI_CARGO_LIB)   ? "--lib"
                       : (args->target->kind == CLI_CARGO_BIN) ? "--bin"
                                                               : "--test");
    if (args->target->kind != CLI_CARGO_LIB)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                   .word = args->target->name);
    }
}

// the cargo test of one target started for the tests given of it, with the words rust-analyzer gives an editor, and
// read on a thread of its own: 1, or 0 with why given
EMBED_INLINE int run_target_start(const CargoRunCtx *args)
{
    CliTestRun *run = args->run;
    const char *cargo = args->cargo;
    VerbaLinea *err_msg = args->err_msg;
    static CliWords s_words;
    static char s_path[CLI_PATH_TEXT];
    if (run->worker_count == CLI_TEST_WORKERS)
    {
        mmgr_line_put(err_msg, "no more processes are run at once");
        return 0;
    }
    CliTestWorker *const worker = &run->workers[run->worker_count];
    const CliCargoFound *const first = &s_tests.found[s_order[args->group->first_at]];
    char package[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char manifest[CLI_PATH_MAX_LEN];
    const size_t folder_size = cellul.len(&(CatenaFinitaCfg){.src = first->manifest, .cap = CLI_CELLS_POOL}) - 10u;
    VerbaLinea folder_line = {folder, sizeof(folder), 0ull};
    mmgr_line_put_n(&folder_line, first->manifest, folder_size);
    mmgr_line_finish(&folder_line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = run->root, .relative = folder, .joined = package);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = run->root, .relative = first->manifest, .joined = manifest);
    EMBED_CALL(cli_cargo_report.cargo_workspace_get, CliCargoReportCfg, .root = run->root, .package = package,
               .workspace = worker->workspace);
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    const char *const head[3] = {cargo, "test", "--package"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = head[at]);
    }
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = first->package);
    EMBED_CALL(run_target_words, CargoRunCtx, .words = &s_words, .target = &first->target);
    const char *const middle[5] = {"--no-fail-fast", "--color", "never", "--manifest-path", manifest};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = middle[at]);
    }
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = "--");
    worker->named = run->named_count;
    unsigned long long names = 0ull;
    for (unsigned int at = 0u; (at < args->group->count) && (run->named_count < CLI_TEST_NAMED); at += 1u)
    {
        const CliCargoFound *const one = &s_tests.found[s_order[args->group->first_at + at]];
        run->named[run->named_count] = (CliTestNamed){
            EMBED_CALL(cli_test_runs_run_text_internal.test_put, CliTestRunsRunTextCtx, .run = run,
                       .text = one->test.id,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = one->test.id, .cap = CLI_CELLS_POOL})),
            EMBED_CALL(cli_test_runs_run_text_internal.test_put, CliTestRunsRunTextCtx, .run = run,
                       .text = one->test.file,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = one->test.file, .cap = CLI_CELLS_POOL})),
            one->test.line,
            EMBED_CALL(cli_test_runs_run_text_internal.test_put, CliTestRunsRunTextCtx, .run = run, .text = one->full,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = one->full, .cap = CLI_CELLS_POOL}))};
        run->named_count += 1u;
        names += cellul.len(&(CatenaFinitaCfg){.src = one->full, .cap = CLI_CELLS_POOL});
    }
    worker->named_count = run->named_count - worker->named;
    if ((args->group->count != args->group->total) && (names < CLI_CARGO_NAMES_MOST))
    {
        for (unsigned int at = worker->named; at < run->named_count; at += 1u)
        {
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                       .word = run->named[at].full);
        }
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = "--exact");
    }
    const char *const tail[3] = {"--show-output", "--color", "never"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = tail[at]);
    }
    if (args->group->named)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = "--include-ignored");
    }
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path, .size = sizeof(s_path));
    const CliSetting settings[2] = {{"PATH", s_path}, {"CARGO_TERM_COLOR", "never"}};
    worker->run = run;
    worker->index = run->worker_count;
    worker->cargo = 1;
    worker->pytest = 0;
    worker->seen_count = 0u;
    char name[64];
    VerbaLinea name_line = {name, sizeof(name), 0ull};
    mmgr_line_put(&name_line, "cargo-");
    mmgr_line_decimal(&name_line, worker->index, 1u);
    const unsigned long long stem = name_line.at;
    mmgr_line_put(&name_line, ".err");
    mmgr_line_finish(&name_line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = run->scratch, .relative = name, .joined = worker->errors);
    name_line.at = stem;
    mmgr_line_put(&name_line, ".out");
    mmgr_line_finish(&name_line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = run->scratch, .relative = name, .joined = worker->seen);
    const int started =
        EMBED_CALL(cli_words_err_msg_internal.spawn_as, CliWordsErrMsgCtx, .priority = CLI_PRIORITY_GUEST,
                   .spawned = &worker->spawned, .words = s_words.word, .folder = package, .settings = settings,
                   .count = 2u, .detached = 1, .err_msg = err_msg);
    run->worker_count += started ? 1u : 0u;
    if (!started)
    {
        return 0;
    }
    EMBED_CALL(cli_test_runs_worker_line_internal.test_errors_start, CliTestRunsWorkerLineCtx, .worker = worker);
    run->going += 1u;
    worker->thread = (CliThread){run_worker, worker, CLI_PRIORITY_GUEST};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &worker->thread))
    {
        run_worker(worker);
    }
    return 1;
}

int cli_cargo_workers_start(const CliTestRunsRunGivenCtx *args)
{
    char cargo[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "rust", .name = "cargo", .path = cargo,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const unsigned int groups =
        EMBED_CALL(run_group, CargoRunCtx, .root = args->run->root, .given = args->given, .count = args->count);
    int started = 1;
    for (unsigned int group = 0u; started && (group < groups); group += 1u)
    {
        started = EMBED_CALL(run_target_start, CargoRunCtx, .run = args->run, .group = &s_groups[group], .cargo = cargo,
                             .err_msg = args->err_msg);
    }
    // each worker took its tests' names into the run, so the tests found are given back once every one is started
    EMBED_CALL(cli_cargo_tests_table.free, CliCargoTestsCfg, .tests = &s_tests);
    return started;
}
