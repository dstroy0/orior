// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_run.c: a run's Rust tests: the targets the tests given reach, a cargo test started for each on a thread of its
// own, and the harness's report read: each result as it is printed, each failure's message and place, and the first
// error of a build that failed
#include "../programs/executables.h"
#include "cargo_report.h"
#include "test_runs_internal.h"

// the longest list of names a run gives the harness, in characters; past it a target runs whole and only the results
// asked of it are told
#define CLI_CARGO_NAMES_MOST 16000ull

// the most targets a run reaches
#define CLI_CARGO_GROUPS 1024u

// A target the tests given reach: its first test found, how many tests it has, the tests asked of it by their places
// among those found, and whether each was asked by its name
typedef struct
{
    unsigned int first;
    unsigned int total;
    unsigned int asked_from;
    unsigned int asked;
    int named;
} CliCargoGroup;

static CliCargoTests s_tests;
static CliCargoGroup s_groups[CLI_CARGO_GROUPS];
static unsigned int s_asked[CLI_CARGO_FOUND];
static CliLock s_run_lock = CLI_LOCK_INIT;

// whether two tests found are of one target of one manifest
static int run_same_target(const CliCargoFound *one, const CliCargoFound *other)
{
    return (cellul.eq(&(CatenaFinitaCfg){.src = one->manifest, .other = other->manifest, .cap = CLI_CELLS_POOL})) &&
           (one->target.kind == other->target.kind) &&
           ((one->target.name == NULL) ||
            (cellul.eq(
                &(CatenaFinitaCfg){.src = one->target.name, .other = other->target.name, .cap = CLI_CELLS_POOL})));
}

// whether `given`, `count` of them, holds `text`
static int run_given(const char *const *given, unsigned int count, const char *text)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = given[at], .other = text, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// the targets the tests given reach, under the run lock: each with the tests given of it, whether all of its tests are
// and whether each was asked by its name: their count
static unsigned int run_grouped(const char *root, const char *const *given, unsigned int count)
{
    cli_cargo_found(root, &s_tests);
    unsigned int groups = 0u;
    unsigned int group_of[CLI_CARGO_FOUND];
    for (unsigned int at = 0u; at < s_tests.count; at += 1u)
    {
        unsigned int group = 0u;
        while ((group < groups) && !run_same_target(&s_tests.found[s_groups[group].first], &s_tests.found[at]))
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
    unsigned int kept = 0u;
    for (unsigned int group = 0u; group < groups; group += 1u)
    {
        CliCargoGroup made = s_groups[group];
        made.asked_from = placed;
        for (unsigned int at = 0u; at < s_tests.count; at += 1u)
        {
            const CliCargoFound *const one = &s_tests.found[at];
            const int by_file = run_given(given, count, one->test.file);
            if ((group_of[at] == group) && (run_given(given, count, one->test.id) || by_file))
            {
                s_asked[placed] = at;
                placed += 1u;
                made.named = made.named && !by_file;
            }
        }
        made.asked = placed - made.asked_from;
        if (made.asked > 0u)
        {
            s_groups[kept] = made;
            kept += 1u;
        }
    }
    return kept;
}

unsigned int cli_cargo_targets_given(const char *root, const char *const *given, unsigned int count)
{
    cli_lock_take(&s_run_lock);
    const unsigned int groups = run_grouped(root, given, count);
    cli_lock_give(&s_run_lock);
    return groups;
}

// a path a panic or an error is written with, from the workspace's folder where it is not whole, as a path of the tree
// into `out`, which holds CLI_PATH_LONGEST
static void run_tree_path(const CliTestWorker *worker, const char *path, char *out)
{
    char full[CLI_PATH_LONGEST];
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
        cli_full(worker->workspace, path, full);
    }
    cli_relative(worker->run->root, full, out);
}

// a line of the harness's own heard: the test's name and its outcome, told where a test of the target is of that name
static void run_line(CliTestWorker *worker, const char *text, unsigned long long size)
{
    unsigned long long name = 0ull;
    unsigned long long name_size = 0ull;
    const char *outcome = NULL;
    if (!cli_cargo_result_line(text, size, &name, &name_size, &outcome))
    {
        return;
    }
    CliTestRun *const run = worker->run;
    for (unsigned int at = worker->named; at < worker->named + worker->named_count; at += 1u)
    {
        const CliTestNamed *const test = &run->named[at];
        if ((cellul.len(&(CatenaFinitaCfg){.src = test->full, .cap = CLI_CELLS_POOL}) != name_size) ||
            (memor.cmp(&(MemoriaCfg){.src = test->full, .other = text + name, .bytes = name_size}) != 0))
        {
            continue;
        }
        if (cli_test_heard(run, worker->index, text + name, name_size, outcome))
        {
            cli_test_tally(run, outcome);
        }
        cli_test_result_told(worker, test->id, outcome, "", 0ull, NULL, -1ll, 0.0);
        return;
    }
}

// a failure the harness reported told where its test is one of the target's heard as failed, at the place it panicked
static void run_failure_told(void *context, const char *name, unsigned long long name_size, const char *message,
                             unsigned long long message_size, const char *path, long long line)
{
    CliTestWorker *const worker = (CliTestWorker *)context;
    CliTestRun *const run = worker->run;
    const char *const heard = cli_test_heard_outcome(run, worker->index, name, name_size);
    for (unsigned int at = worker->named;
         (heard != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = heard, .other = "failed", .cap = CLI_CELLS_POOL})) &&
         (at < worker->named + worker->named_count);
         at += 1u)
    {
        const CliTestNamed *const test = &run->named[at];
        if ((cellul.len(&(CatenaFinitaCfg){.src = test->full, .cap = CLI_CELLS_POOL}) != name_size) ||
            (memor.cmp(&(MemoriaCfg){.src = test->full, .other = name, .bytes = name_size}) != 0))
        {
            continue;
        }
        char file[CLI_PATH_LONGEST];
        if (path != NULL)
        {
            run_tree_path(worker, path, file);
        }
        // a line of a panic counts from 1, and is told from 0
        cli_test_result_told(worker, test->id, "failed", message, message_size, (path != NULL) ? file : test->file,
                             (path != NULL) ? ((line > 0ll) ? line - 1ll : 0ll) : (long long)test->line, 0.0);
        return;
    }
}

// a target that ran no test says why: its build's first error fails each test given it
static void run_build_failed(CliTestWorker *worker, const char *errors, unsigned long long size)
{
    char message[8192];
    char path[CLI_PATH_LONGEST];
    long long line = -1ll;
    unsigned long long length = 0ull;
    if (!cli_cargo_build_error(errors, size, message, sizeof(message), &length, path, &line))
    {
        path[0] = '\0';
        line = -1ll;
        cli_cargo_last_line(errors, size, "cargo test ran no test", message, sizeof(message), &length);
    }
    char file[CLI_PATH_LONGEST];
    if (path[0] != '\0')
    {
        run_tree_path(worker, path, file);
    }
    CliTestRun *const run = worker->run;
    for (unsigned int at = worker->named; at < worker->named + worker->named_count; at += 1u)
    {
        cli_test_tally(run, "failed");
        cli_test_result_told(worker, run->named[at].id, "failed", message, length, (path[0] != '\0') ? file : NULL,
                             (path[0] != '\0') ? ((line > 0ll) ? line - 1ll : 0ll) : -1ll, 0.0);
    }
    cli_test_said(run, message, length);
}

// a cargo process read until it ends: each result told as the harness prints it, each failure as the process ends,
// and a build that failed told for each test given the target
static void run_worker(void *context)
{
    CliTestWorker *const worker = (CliTestWorker *)context;
    cli_test_lines_read(worker, run_line);
    cli_test_errors_waited(worker);
    unsigned long long errors_size = 0ull;
    unsigned long long seen_size = 0ull;
    const char *const errors = cli_test_file_read(worker->errors, &errors_size);
    const char *const seen = cli_test_file_read(worker->seen, &seen_size);
    cli_cargo_failures_each((seen != NULL) ? seen : "", seen_size, run_failure_told, worker);
    if (!cli_test_heard_any(worker->run, worker->index) && !cli_test_stopped())
    {
        run_build_failed(worker, (errors != NULL) ? errors : "", errors_size);
    }
    cli_test_file_let_go(errors, errors_size);
    cli_test_file_let_go(seen, seen_size);
    CliTestRun *const run = worker->run;
    cli_lock_take(&run->lock);
    run->going -= 1u;
    cli_wake_all(&run->wake);
    cli_lock_give(&run->lock);
}

// the words that ask Cargo for a target alone, put after the words
static void run_target_words(CliWords *words, const CliCargoTarget *target)
{
    cli_words_put(words, (target->kind == CLI_CARGO_LIB)   ? "--lib"
                         : (target->kind == CLI_CARGO_BIN) ? "--bin"
                                                           : "--test");
    if (target->kind != CLI_CARGO_LIB)
    {
        cli_words_put(words, target->name);
    }
}

// the cargo test of one target started for the tests given of it, with the words rust-analyzer gives an editor, and
// read on a thread of its own: 1, or 0 with why said
static int run_target_started(CliTestRun *run, const CliCargoGroup *group, const char *cargo, CliLine *said)
{
    static CliWords s_words;
    static char s_path[CLI_PATH_TEXT];
    if (run->worker_count == CLI_TEST_WORKERS)
    {
        cli_line_text(said, "no more processes are run at once");
        return 0;
    }
    CliTestWorker *const worker = &run->workers[run->worker_count];
    const CliCargoFound *const first = &s_tests.found[s_asked[group->asked_from]];
    char package[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char manifest[CLI_PATH_LONGEST];
    const size_t folder_size = cellul.len(&(CatenaFinitaCfg){.src = first->manifest, .cap = CLI_CELLS_POOL}) - 10u;
    CliLine folder_line = {folder, sizeof(folder), 0ull};
    cli_line_counted(&folder_line, first->manifest, folder_size);
    cli_line_closed(&folder_line);
    cli_full(run->root, folder, package);
    cli_full(run->root, first->manifest, manifest);
    cli_cargo_workspace_of(run->root, package, worker->workspace);
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    const char *const head[3] = {cargo, "test", "--package"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_words_put(&s_words, head[at]);
    }
    cli_words_put(&s_words, first->package);
    run_target_words(&s_words, &first->target);
    const char *const middle[5] = {"--no-fail-fast", "--color", "never", "--manifest-path", manifest};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        cli_words_put(&s_words, middle[at]);
    }
    cli_words_put(&s_words, "--");
    worker->named = run->named_count;
    unsigned long long names = 0ull;
    for (unsigned int at = 0u; (at < group->asked) && (run->named_count < CLI_TEST_NAMED); at += 1u)
    {
        const CliCargoFound *const one = &s_tests.found[s_asked[group->asked_from + at]];
        run->named[run->named_count] = (CliTestNamed){
            cli_test_put(run, one->test.id, cellul.len(&(CatenaFinitaCfg){.src = one->test.id, .cap = CLI_CELLS_POOL})),
            cli_test_put(run, one->test.file,
                         cellul.len(&(CatenaFinitaCfg){.src = one->test.file, .cap = CLI_CELLS_POOL})),
            one->test.line,
            cli_test_put(run, one->full, cellul.len(&(CatenaFinitaCfg){.src = one->full, .cap = CLI_CELLS_POOL}))};
        run->named_count += 1u;
        names += cellul.len(&(CatenaFinitaCfg){.src = one->full, .cap = CLI_CELLS_POOL});
    }
    worker->named_count = run->named_count - worker->named;
    if ((group->asked != group->total) && (names < CLI_CARGO_NAMES_MOST))
    {
        for (unsigned int at = worker->named; at < run->named_count; at += 1u)
        {
            cli_words_put(&s_words, run->named[at].full);
        }
        cli_words_put(&s_words, "--exact");
    }
    const char *const tail[3] = {"--show-output", "--color", "never"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_words_put(&s_words, tail[at]);
    }
    if (group->named)
    {
        cli_words_put(&s_words, "--include-ignored");
    }
    cli_toolchains_run_path(s_path, sizeof(s_path));
    const CliSetting settings[2] = {{"PATH", s_path}, {"CARGO_TERM_COLOR", "never"}};
    worker->run = run;
    worker->index = run->worker_count;
    worker->cargo = 1;
    worker->pytest = 0;
    worker->heard = 0u;
    char name[64];
    CliLine name_line = {name, sizeof(name), 0ull};
    cli_line_text(&name_line, "cargo-");
    cli_line_decimal(&name_line, worker->index, 1u);
    const unsigned long long stem = name_line.at;
    cli_line_text(&name_line, ".err");
    cli_line_closed(&name_line);
    cli_full(run->scratch, name, worker->errors);
    name_line.at = stem;
    cli_line_text(&name_line, ".out");
    cli_line_closed(&name_line);
    cli_full(run->scratch, name, worker->seen);
    cli_lock_take(&g_cli_test_lock);
    const int started =
        cli_spawn_as(CLI_PRIORITY_GUEST, &worker->spawned, s_words.word, package, settings, 2u, 1, said);
    run->worker_count += started ? 1u : 0u;
    cli_lock_give(&g_cli_test_lock);
    if (!started)
    {
        return 0;
    }
    cli_test_errors_started(worker);
    cli_lock_take(&run->lock);
    run->going += 1u;
    cli_lock_give(&run->lock);
    worker->thread = (CliThread){run_worker, worker, CLI_PRIORITY_GUEST};
    if (!cli_thread_started(&worker->thread))
    {
        run_worker(worker);
    }
    return 1;
}

int cli_cargo_workers_started(CliTestRun *run, const char *const *given, unsigned int count, CliLine *said)
{
    char cargo[CLI_PATH_LONGEST];
    if (!cli_executables_program("rust", "cargo", cargo, said))
    {
        return 0;
    }
    cli_lock_take(&s_run_lock);
    const unsigned int groups = run_grouped(run->root, given, count);
    int started = 1;
    for (unsigned int group = 0u; started && (group < groups); group += 1u)
    {
        started = run_target_started(run, &s_groups[group], cargo, said);
    }
    // each worker took its tests' names into the run, so the tests found are given back once every one is started
    cli_cargo_tests_given(&s_tests);
    cli_lock_give(&s_run_lock);
    return started;
}
