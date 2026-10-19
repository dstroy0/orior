// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_runs.c: a run of a tree's tests: started with its Python's and cargo's processes over the machine's cores,
// what each process hears told, and the run ended once its processes are, its coverage and its counts told
#include "../command_line/cells.h"
#include "../machine/cores.h"
#include "../programs/executables.h"
#include "test_runs_internal.h"

#include "../text/text.h"

CliLock g_cli_test_lock = CLI_LOCK_INIT;
int g_cli_test_stopped;

static CliTestRun *s_runs[CLI_TEST_RUNS];
static CliWake s_runs_wake = CLI_WAKE_INIT;
static CliTestRun *s_going;
static unsigned long long s_last;
static unsigned long long s_scratch;

int cli_test_stopped(void)
{
    cli_lock_take(&g_cli_test_lock);
    const int stopped = g_cli_test_stopped;
    cli_lock_give(&g_cli_test_lock);
    return stopped;
}

const char *cli_test_put(CliTestRun *run, const char *text, unsigned long long length)
{
    cli_lock_take(&run->lock);
    const char *put = "";
    if (run->used + length + 1ull <= CLI_TEST_TEXT)
    {
        char *const at = run->text + run->used;
        memor.cpy(&(MemoriaCfg){.dst = at, .src = text, .bytes = length});
        at[length] = '\0';
        run->used += length + 1ull;
        put = at;
    }
    cli_lock_give(&run->lock);
    return put;
}

void cli_test_tally(CliTestRun *run, const char *outcome)
{
    cli_lock_take(&run->lock);
    run->passed +=
        (cellul.eq(&(CatenaFinitaCfg){.src = outcome, .other = "passed", .cap = CLI_CELLS_POOL})) ? 1ull : 0ull;
    run->failed +=
        (cellul.eq(&(CatenaFinitaCfg){.src = outcome, .other = "failed", .cap = CLI_CELLS_POOL})) ? 1ull : 0ull;
    run->skipped += ((!cellul.eq(&(CatenaFinitaCfg){.src = outcome, .other = "passed", .cap = CLI_CELLS_POOL})) &&
                     (!cellul.eq(&(CatenaFinitaCfg){.src = outcome, .other = "failed", .cap = CLI_CELLS_POOL})))
                        ? 1ull
                        : 0ull;
    cli_lock_give(&run->lock);
}

void cli_test_said(CliTestRun *run, const char *text, unsigned long long length)
{
    cli_lock_take(&run->lock);
    if (run->said[0] == '\0')
    {
        CliLine line = {run->said, sizeof(run->said), 0ull};
        cli_line_counted(&line, text, length);
        cli_line_closed(&line);
    }
    cli_lock_give(&run->lock);
}

// the place a process's name is heard at in the run's table, or the empty place it would take; CLI_TEST_HEARD where
// the table is full
static unsigned int test_heard_place(const CliTestRun *run, unsigned int worker, const char *name,
                                     unsigned long long length)
{
    unsigned long long hash = 1469598103934665603ull ^ worker;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        hash = (hash ^ (unsigned char)name[at]) * 1099511628211ull;
    }
    for (unsigned int probe = 0u; probe < CLI_TEST_HEARD; probe += 1u)
    {
        const unsigned int place = (unsigned int)((hash + probe) % CLI_TEST_HEARD);
        const unsigned long long kept = run->heard_name[place];
        // a name kept is its process's number, the name, and a 0, from one past its place in the texts
        if ((kept == 0ull) ||
            (((unsigned char)run->heard_text[kept - 1ull] == worker) &&
             cli_text_same(run->heard_text + kept, name, length) && (run->heard_text[kept + length] == '\0')))
        {
            return place;
        }
    }
    return CLI_TEST_HEARD;
}

int cli_test_heard(CliTestRun *run, unsigned int worker, const char *name, unsigned long long length,
                   const char *outcome)
{
    cli_lock_take(&run->lock);
    const unsigned int place = test_heard_place(run, worker, name, length);
    int fresh = 0;
    if ((place < CLI_TEST_HEARD) && (run->heard_name[place] == 0ull) &&
        (run->heard_used + length + 2ull <= CLI_TEST_HEARD_TEXT))
    {
        run->heard_text[run->heard_used] = (char)worker;
        memor.cpy(&(MemoriaCfg){.dst = run->heard_text + run->heard_used + 1ull, .src = name, .bytes = length});
        run->heard_text[run->heard_used + 1ull + length] = '\0';
        run->heard_name[place] = run->heard_used + 1ull;
        run->heard_outcome[place] = outcome;
        run->heard_used += length + 2ull;
        run->workers[worker].heard += 1u;
        fresh = 1;
    }
    cli_lock_give(&run->lock);
    return fresh;
}

const char *cli_test_heard_outcome(CliTestRun *run, unsigned int worker, const char *name, unsigned long long length)
{
    cli_lock_take(&run->lock);
    const unsigned int place = test_heard_place(run, worker, name, length);
    const char *const outcome =
        ((place < CLI_TEST_HEARD) && (run->heard_name[place] != 0ull)) ? run->heard_outcome[place] : NULL;
    cli_lock_give(&run->lock);
    return outcome;
}

int cli_test_heard_any(CliTestRun *run, unsigned int worker)
{
    cli_lock_take(&run->lock);
    const int any = (run->workers[worker].heard > 0u);
    cli_lock_give(&run->lock);
    return any;
}

void cli_test_result_told(CliTestWorker *worker, const char *id, const char *outcome, const char *message,
                          unsigned long long message_size, const char *file, long long line, double seconds)
{
    CliLine out = {worker->event, sizeof(worker->event), 0ull};
    cli_line_text(&out, "{\"kind\":\"result\",\"run\":");
    cli_line_decimal(&out, worker->run->number, 1u);
    cli_line_text(&out, ",\"outcome\":{\"id\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)id,
                          cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    cli_line_text(&out, ",\"outcome\":");
    cli_json_quoted(&out, outcome, cellul.len(&(CatenaFinitaCfg){.src = outcome, .cap = CLI_CELLS_POOL}));
    cli_line_text(&out, ",\"message\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)message, message_size);
    cli_line_text(&out, ",\"file\":");
    if (file == NULL)
    {
        cli_line_text(&out, "null");
    }
    else
    {
        cli_json_quoted_lossy(&out, (const unsigned char *)file,
                              cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(&out, ",\"line\":");
    if (line < 0ll)
    {
        cli_line_text(&out, "null");
    }
    else
    {
        cli_line_decimal(&out, (unsigned long long)line, 1u);
    }
    cli_line_text(&out, ",\"seconds\":");
    cli_json_double(&out, seconds);
    cli_line_text(&out, "}}");
    if (cli_line_fits(&out))
    {
        worker->run->tell.told(worker->run->tell.context, out.out, out.at);
    }
}

// a run ended once each of its threads has: its coverage told, its processes waited for, its counts and what it says
// told, its folder taken away, and its place given back
static void test_ended(void *context)
{
    CliTestRun *const run = (CliTestRun *)context;
    cli_lock_take(&run->lock);
    while (run->going > 0u)
    {
        cli_wake_wait(&run->wake, &run->lock);
    }
    cli_lock_give(&run->lock);
    cli_test_coverage_told(run);
    cli_lock_take(&g_cli_test_lock);
    for (unsigned int at = 0u; at < run->worker_count; at += 1u)
    {
        CliTestWorker *const worker = &run->workers[at];
        if (worker->spawned.process != 0ull)
        {
            (void)cli_spawn_wait(&worker->spawned);
            cli_spawn_closed(&worker->spawned);
        }
    }
    const int stopped = g_cli_test_stopped;
    s_going = (s_going == run) ? NULL : s_going;
    cli_lock_give(&g_cli_test_lock);
    char done[CLI_TEST_SAID + 512u];
    CliLine out = {done, sizeof(done), 0ull};
    cli_line_text(&out, "{\"kind\":\"done\",\"run\":");
    cli_line_decimal(&out, run->number, 1u);
    cli_line_text(&out, ",\"passed\":");
    cli_line_decimal(&out, run->passed, 1u);
    cli_line_text(&out, ",\"failed\":");
    cli_line_decimal(&out, run->failed, 1u);
    cli_line_text(&out, ",\"skipped\":");
    cli_line_decimal(&out, run->skipped, 1u);
    cli_line_text(&out, ",\"seconds\":");
    cli_json_double(&out, (double)(cli_clock_milliseconds() - run->started) / 1000.0);
    cli_line_text(&out, ",\"said\":");
    const char *const said = stopped ? "The run was stopped" : run->said;
    cli_json_quoted_lossy(&out, (const unsigned char *)said,
                          cellul.len(&(CatenaFinitaCfg){.src = said, .cap = CLI_CELLS_POOL}));
    cli_line_character(&out, '}');
    run->tell.told(run->tell.context, out.out, out.at);
    if (run->scratch[0] != '\0')
    {
        (void)query_removed(run->scratch);
    }
    cli_lock_take(&g_cli_test_lock);
    s_runs[run->place] = NULL;
    cli_wake_all(&s_runs_wake);
    cli_lock_give(&g_cli_test_lock);
    cli_cell_given(run);
}

void cli_test_runs_stop(void)
{
    cli_lock_take(&g_cli_test_lock);
    g_cli_test_stopped = 1;
    CliTestRun *const run = s_going;
    for (unsigned int at = 0u; (run != NULL) && (at < run->worker_count); at += 1u)
    {
        if (run->workers[at].spawned.process != 0ull)
        {
            cli_spawn_killed(&run->workers[at].spawned);
        }
    }
    s_going = NULL;
    cli_lock_give(&g_cli_test_lock);
}

// a place for a run taken once one is free, the run a cell given back as it ends, made empty, its number the next: it,
// or NULL where no room for it is left
static CliTestRun *test_run_taken(void)
{
    cli_lock_take(&g_cli_test_lock);
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
            cli_wake_wait(&s_runs_wake, &g_cli_test_lock);
        }
    }
    run = cli_cell_taken(sizeof(CliTestRun), "a run of tests");
    if (run == NULL)
    {
        cli_lock_give(&g_cli_test_lock);
        return NULL;
    }
    run->place = at;
    s_runs[at] = run;
    s_last += 1ull;
    run->number = s_last;
    g_cli_test_stopped = 0;
    s_going = run;
    cli_lock_give(&g_cli_test_lock);
    run->lock = (CliLock)CLI_LOCK_INIT;
    run->wake = (CliWake)CLI_WAKE_INIT;
    run->going = 0u;
    run->passed = 0ull;
    run->failed = 0ull;
    run->skipped = 0ull;
    run->said[0] = '\0';
    run->worker_count = 0u;
    run->named_count = 0u;
    run->shares = 0u;
    run->used = 0ull;
    run->heard_used = 0ull;
    run->scratch[0] = '\0';
    memor.set(&(MemoriaCfg){.dst = run->heard_name, .val = 0, .bytes = sizeof(run->heard_name)});
    return run;
}

// what is wrong with the tests given before a run starts said: 1 where nothing is, with the Python into `python` where
// Python's tests are given and whether it has pytest into `pytest`
static int test_checked(const char *root, unsigned int python_count, unsigned int rust_count, int cover, char *python,
                        int *pytest, CliLine *said)
{
    if (cover && (rust_count > 0u))
    {
        cli_line_text(said, "Coverage is recorded for Python's tests alone: run Rust's without it");
        return 0;
    }
    char cargo[CLI_PATH_LONGEST];
    if ((rust_count > 0u) && !cli_executables_program("rust", "cargo", cargo, said))
    {
        return 0;
    }
    python[0] = '\0';
    if ((python_count > 0u) && !cli_test_python(python))
    {
        cli_line_text(said, "No Python is found to run the tests: File, Toolchains finds one");
        return 0;
    }
    *pytest = (python[0] != '\0') && cli_test_has_pytest(python, root);
    if (cover && !*pytest)
    {
        cli_line_text(said, "Coverage is recorded by a run of pytest, which the tree's Python has not: python -m pip "
                            "install pytest");
        return 0;
    }
    return 1;
}

int cli_test_runs_start(const char *root, const char *const *given, unsigned int count, int parallel, int cover,
                        CliTell tell, unsigned long long *number, CliLine *said)
{
    static const char *s_rust[CLI_TEST_NAMED];
    static const char *s_python[CLI_TEST_NAMED];
    cli_test_runs_stop();
    if (count == 0u)
    {
        cli_line_text(said, "No test is given to run");
        return 0;
    }
    unsigned int rust_count = 0u;
    unsigned int python_count = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cli_cargo_is_rust(given[at]) && (rust_count < CLI_TEST_NAMED))
        {
            s_rust[rust_count] = given[at];
            rust_count += 1u;
        }
        else if (!cli_cargo_is_rust(given[at]) && (python_count < CLI_TEST_NAMED))
        {
            s_python[python_count] = given[at];
            python_count += 1u;
        }
    }
    char python[CLI_PATH_LONGEST];
    int pytest = 0;
    if (!test_checked(root, python_count, rust_count, cover, python, &pytest, said))
    {
        return 0;
    }
    CliTestRun *const run = test_run_taken();
    if (run == NULL)
    {
        cli_line_text(said, "a run of tests could not be taken");
        return 0;
    }
    run->tell = tell;
    memor.cpy(&(MemoriaCfg){.dst = run->root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    for (unsigned int at = 0u; at < python_count; at += 1u)
    {
        run->given[at] =
            cli_test_put(run, s_python[at], cellul.len(&(CatenaFinitaCfg){.src = s_python[at], .cap = CLI_CELLS_POOL}));
    }
    if (python_count > 0u)
    {
        cli_test_shares(run, python_count, parallel ? cli_cores_for_work() : 1u);
    }
    const unsigned int cargo_runs = (rust_count > 0u) ? cli_cargo_targets_given(root, s_rust, rust_count) : 0u;
    const char *const python_runner = pytest ? "pytest" : "unittest";
    char started[512];
    CliLine out = {started, sizeof(started), 0ull};
    cli_line_text(&out, "{\"kind\":\"started\",\"run\":");
    cli_line_decimal(&out, run->number, 1u);
    cli_line_text(&out, ",\"given\":");
    cli_line_decimal(&out, python_count + rust_count, 1u);
    cli_line_text(&out, ",\"workers\":");
    cli_line_decimal(&out, run->shares + cargo_runs, 1u);
    cli_line_text(&out, ",\"runner\":\"");
    cli_line_text(&out, (run->shares == 0u) ? "cargo test" : python_runner);
    cli_line_text(&out, ((run->shares > 0u) && (cargo_runs > 0u)) ? " and cargo test\"}" : "\"}");
    tell.told(tell.context, out.out, out.at);
    char temporary[CLI_PATH_LONGEST];
    char name[96];
    CliLine name_line = {name, sizeof(name), 0ull};
    cli_line_text(&name_line, "orior-tests-");
    cli_line_decimal(&name_line, cli_process_own(), 1u);
    cli_line_character(&name_line, '-');
    cli_lock_take(&g_cli_test_lock);
    cli_line_decimal(&name_line, s_scratch, 1u);
    s_scratch += 1ull;
    cli_lock_give(&g_cli_test_lock);
    cli_line_closed(&name_line);
    cli_temp_folder(temporary);
    cli_full(temporary, name, run->scratch);
    run->started = cli_clock_milliseconds();
    int going = query_folder_made(run->scratch);
    if (!going)
    {
        cli_line_text(said, run->scratch);
        cli_line_text(said, ": it could not be made");
        run->scratch[0] = '\0';
    }
    going = going && ((rust_count == 0u) || cli_cargo_workers_started(run, s_rust, rust_count, said));
    going = going && ((python_count == 0u) || cli_python_workers_started(run, python, pytest, cover, said));
    // a run that could not start each of its processes is stopped, and ends as one that was
    if (!going)
    {
        cli_test_runs_stop();
    }
    run->ender = (CliThread){test_ended, run};
    if (!cli_thread_started(&run->ender))
    {
        test_ended(run);
    }
    *number = run->number;
    return going;
}
