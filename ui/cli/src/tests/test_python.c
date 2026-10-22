// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_python.c: a run's Python tests: the Python they run with and whether it has pytest, the shares the tests are
// spread over, and a process of pytest or unittest to each share, read on a thread of its own
#include "../command_line/cells.h"
#include "test_runs_internal.h"

#include "../text/text.h"

int cli_test_python(char *python)
{
    static CliTreeEnvironment s_environment;
    static CliFound s_found;
    static CliLock s_python_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_python_lock);
    int found = cli_toolchains_environment(&s_environment) && s_environment.has_python;
    if (found)
    {
        memor.cpy(&(MemoriaCfg){.dst = python, .src = s_environment.python, .bytes = CLI_PATH_LONGEST});
    }
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    const unsigned int manifest = found ? CLI_JSON_NONE : cli_toolchains_manifest(&json, 0u, 0ull);
    for (unsigned int index = 0u; !found && (cli_json_item(&json, manifest, index) != CLI_JSON_NONE); index += 1u)
    {
        const unsigned int tool = cli_json_item(&json, manifest, index);
        const char *const id = cli_json_string(&json, cli_json_member(&json, tool, "id"), NULL);
        if ((id == NULL) || (!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = "python", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
        cli_toolchains_find(&json, tool, (const char (*)[CLI_PATH_LONGEST])folders, count, &s_found);
        cli_cell_given(folders);
        found = s_found.has_program;
        if (found)
        {
            memor.cpy(&(MemoriaCfg){.dst = python, .src = s_found.program, .bytes = CLI_PATH_LONGEST});
        }
        break;
    }
    cli_json_given(&json);
    cli_lock_give(&s_python_lock);
    return found;
}

int cli_test_has_pytest(const char *python, const char *root)
{
    static char s_path[CLI_PATH_TEXT];
    static CliLock s_path_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_path_lock);
    cli_toolchains_run_path(s_path, sizeof(s_path));
    const CliSetting settings[1] = {{"PATH", s_path}};
    const char *const words[4] = {python, "-c", "import pytest", NULL};
    CliRan *const ran = cli_ran_taken();
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const int has = cli_ran(words, root, settings, 1u, ran, &said) && (ran->code == 0ll);
    cli_ran_given(ran);
    cli_lock_give(&s_path_lock);
    return has;
}

// the length of the file part of a test's name, before its first `::`
static unsigned long long test_file_length(const char *id)
{
    const char *const path =
        cellul.find(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
    return (path != NULL) ? (unsigned long long)(path - id)
                          : cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL});
}

void cli_test_shares(CliTestRun *run, unsigned int count, unsigned int most)
{
    static unsigned int s_file_of[CLI_TEST_NAMED];
    static unsigned int s_file_first[CLI_TEST_NAMED];
    static unsigned int s_file_size[CLI_TEST_NAMED];
    static unsigned int s_by_size[CLI_TEST_NAMED];
    unsigned int files = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long length = test_file_length(run->given[at]);
        unsigned int file = 0u;
        while ((file < files) && !((test_file_length(run->given[s_file_first[file]]) == length) &&
                                   cli_text_same(run->given[s_file_first[file]], run->given[at], length)))
        {
            file += 1u;
        }
        if (file == files)
        {
            s_file_first[files] = at;
            s_file_size[files] = 0u;
            files += 1u;
        }
        s_file_of[at] = file;
        s_file_size[file] += 1u;
    }
    for (unsigned int file = 0u; file < files; file += 1u)
    {
        unsigned int place = file;
        while (place > 0u)
        {
            const unsigned int before = s_by_size[place - 1u];
            const unsigned long long before_length = test_file_length(run->given[s_file_first[before]]);
            const unsigned long long length = test_file_length(run->given[s_file_first[file]]);
            const embed_iword named_after =
                memor.cmp(&(MemoriaCfg){.src = run->given[s_file_first[before]],
                                        .other = run->given[s_file_first[file]],
                                        .bytes = (size_t)((before_length < length) ? before_length : length)});
            const int after = (s_file_size[before] < s_file_size[file]) ||
                              ((s_file_size[before] == s_file_size[file]) &&
                               ((named_after > 0) || ((named_after == 0) && (before_length > length))));
            if (!after)
            {
                break;
            }
            s_by_size[place] = before;
            place -= 1u;
        }
        s_by_size[place] = file;
    }
    unsigned int shares = (most < 1u) ? 1u : most;
    shares = (shares > ((files > 1u) ? files : 1u)) ? ((files > 1u) ? files : 1u) : shares;
    shares = (shares > CLI_TEST_WORKERS) ? CLI_TEST_WORKERS : shares;
    unsigned int share_of[CLI_TEST_NAMED];
    unsigned int sizes[CLI_TEST_WORKERS];
    memor.set(&(MemoriaCfg){.dst = sizes, .val = 0, .bytes = sizeof(sizes)});
    for (unsigned int at = 0u; at < files; at += 1u)
    {
        const unsigned int file = s_by_size[at];
        unsigned int least = 0u;
        for (unsigned int share = 1u; share < shares; share += 1u)
        {
            least = (sizes[share] < sizes[least]) ? share : least;
        }
        share_of[file] = least;
        sizes[least] += s_file_size[file];
    }
    run->shares = 0u;
    unsigned int placed = 0u;
    for (unsigned int share = 0u; share < shares; share += 1u)
    {
        if (sizes[share] == 0u)
        {
            continue;
        }
        run->share_from[run->shares] = placed;
        for (unsigned int at = 0u; at < files; at += 1u)
        {
            const unsigned int file = s_by_size[at];
            for (unsigned int one = 0u; (share_of[file] == share) && (one < count); one += 1u)
            {
                if (s_file_of[one] == file)
                {
                    run->order[placed] = one;
                    placed += 1u;
                }
            }
        }
        run->share_size[run->shares] = placed - run->share_from[run->shares];
        run->shares += 1u;
    }
}

// a result heard of a Python process: counted where it is new, and told
static void python_heard(CliTestWorker *worker, const char *id, unsigned long long size, const char *outcome)
{
    char named[CLI_PATH_LONGEST * 2u];
    CliLine line = {named, sizeof(named), 0ull};
    cli_line_counted(&line, id, size);
    cli_line_closed(&line);
    if (cli_test_heard(worker->run, worker->index, id, size, outcome))
    {
        cli_test_tally(worker->run, outcome);
    }
    cli_test_result_told(worker, named, outcome, "", 0ull, NULL, -1ll, 0.0);
}

// a line of pytest's verbose report heard
static void python_line(CliTestWorker *worker, const char *text, unsigned long long size)
{
    unsigned long long id = 0ull;
    unsigned long long id_size = 0ull;
    const char *outcome = NULL;
    if (worker->pytest && cli_test_pytest_line(text, size, &id, &id_size, &outcome))
    {
        python_heard(worker, text + id, id_size, outcome);
    }
}

// the last three lines a process wrote that are not blank, of its output then its errors, joined by "\n", said as why
// it ran no test
static void python_tail_said(CliTestWorker *worker, const char *seen, unsigned long long seen_size, const char *errors,
                             unsigned long long errors_size)
{
    const char *starts[3];
    unsigned long long sizes[3];
    unsigned int kept = 0u;
    const char *const texts[2] = {seen, errors};
    const unsigned long long text_sizes[2] = {seen_size, errors_size};
    for (unsigned int which = 0u; which < 2u; which += 1u)
    {
        unsigned long long at = 0ull;
        while (at < text_sizes[which])
        {
            const char *const text = texts[which];
            const char *const newline =
                memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = text_sizes[which] - at});
            unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : text_sizes[which];
            const unsigned long long start = at;
            at = (newline != NULL) ? end + 1ull : text_sizes[which];
            end -= ((newline != NULL) && (end > start) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
            unsigned long long letter = start;
            while ((letter < end) && (cellul.chr(&(CatenaFinitaCfg){
                                          .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = text[letter]}) != NULL))
            {
                letter += 1ull;
            }
            if (letter < end)
            {
                starts[kept % 3u] = text + start;
                sizes[kept % 3u] = end - start;
                kept += 1u;
            }
        }
    }
    char said[CLI_TEST_SAID];
    CliLine line = {said, sizeof(said), 0ull};
    for (unsigned int one = (kept > 3u) ? kept - 3u : 0u; one < kept; one += 1u)
    {
        cli_line_text(&line, (line.at > 0ull) ? "\n" : "");
        cli_line_counted(&line, starts[one % 3u], sizes[one % 3u]);
    }
    cli_test_said(worker->run, said, cli_line_fits(&line) ? line.at : sizeof(said));
}

// a Python process read until it ends: each result told as it is printed, then unittest's report of its errors, or
// pytest's report in JUnit's form, each failure's message and line told
static void python_worker(void *context)
{
    CliTestWorker *const worker = (CliTestWorker *)context;
    cli_test_lines_read(worker, python_line);
    cli_test_errors_waited(worker);
    unsigned long long errors_size = 0ull;
    const char *const errors = cli_test_file_read(worker->errors, &errors_size);
    if (!worker->pytest && (errors != NULL))
    {
        unsigned long long at = 0ull;
        while (at < errors_size)
        {
            const char *const newline =
                memor.chr(&(MemoriaCfg){.src = errors + at, .val = '\n', .bytes = errors_size - at});
            unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - errors) : errors_size;
            const unsigned long long start = at;
            at = (newline != NULL) ? end + 1ull : errors_size;
            end -= ((newline != NULL) && (end > start) && (errors[end - 1ull] == '\r')) ? 1ull : 0ull;
            char id[CLI_PATH_LONGEST * 2u];
            const char *outcome = NULL;
            if (cli_test_unittest_line(errors + start, end - start, id, sizeof(id), &outcome))
            {
                python_heard(worker, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}), outcome);
            }
        }
        cli_test_unittest_failures_told(worker, errors, errors_size);
    }
    else if (worker->pytest)
    {
        unsigned long long report_size = 0ull;
        const char *const report = cli_test_file_read(worker->report, &report_size);
        if (report != NULL)
        {
            cli_test_report_told(worker, report, report_size);
        }
        cli_test_file_let_go(report, report_size);
    }
    if (!cli_test_heard_any(worker->run, worker->index) && !cli_test_stopped())
    {
        unsigned long long seen_size = 0ull;
        const char *const seen = cli_test_file_read(worker->seen, &seen_size);
        python_tail_said(worker, (seen != NULL) ? seen : "", seen_size, (errors != NULL) ? errors : "", errors_size);
        cli_test_file_let_go(seen, seen_size);
    }
    cli_test_file_let_go(errors, errors_size);
    CliTestRun *const run = worker->run;
    cli_lock_take(&run->lock);
    run->going -= 1u;
    cli_wake_all(&run->wake);
    cli_lock_give(&run->lock);
}

// a file of the run's folder named for a share into `path`, which holds CLI_PATH_LONGEST
static void python_file(const CliTestRun *run, const char *stem, unsigned int share, const char *extension, char *path)
{
    char name[64];
    CliLine line = {name, sizeof(name), 0ull};
    cli_line_text(&line, stem);
    cli_line_decimal(&line, share, 1u);
    cli_line_text(&line, extension);
    cli_line_closed(&line);
    cli_full(run->scratch, name, path);
}

int cli_python_workers_started(CliTestRun *run, const char *python, int pytest, int cover, CliLine *said)
{
    static CliWords s_words;
    static char s_path[CLI_PATH_TEXT];
    cli_toolchains_run_path(s_path, sizeof(s_path));
    const CliSetting settings[2] = {{"PATH", s_path}, {"PYTHONIOENCODING", "utf-8"}};
    for (unsigned int share = 0u; share < run->shares; share += 1u)
    {
        if (run->worker_count == CLI_TEST_WORKERS)
        {
            cli_line_text(said, "no more processes are run at once");
            return 0;
        }
        CliTestWorker *const worker = &run->workers[run->worker_count];
        python_file(run, "report-", share, ".xml", worker->report);
        python_file(run, "coverage-", share, ".json", worker->covered);
        python_file(run, "python-", share, ".err", worker->errors);
        python_file(run, "python-", share, ".out", worker->seen);
        s_words.count = 0u;
        s_words.used = 0u;
        s_words.full = 0;
        cli_words_put(&s_words, python);
        if (pytest)
        {
            char junit[CLI_PATH_LONGEST + 16u];
            memor.cpy(&(MemoriaCfg){
                .dst = junit,
                .src = "--junitxml=",
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "--junitxml=", .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){
                .dst = junit + 11,
                .src = worker->report,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = worker->report, .cap = CLI_CELLS_POOL}) + 1u});
            const char *const head[4] = {"-c", g_cli_test_shim, worker->covered, run->root};
            const char *const module[2] = {"-m", "pytest"};
            for (unsigned int at = 0u; at < (cover ? 4u : 2u); at += 1u)
            {
                cli_words_put(&s_words, cover ? head[at] : module[at]);
            }
            // the process starts in the run's root, and pytest is held to it as its rootdir, so the ids its lines and
            // its report give are the ids the run was given though a folder above holds a configuration of its own
            const char *const words[8] = {
                "-v", "-p", "no:cacheprovider", "--tb=short", "-o", "junit_family=xunit1", "--rootdir=.", junit};
            for (unsigned int at = 0u; at < 8u; at += 1u)
            {
                cli_words_put(&s_words, words[at]);
            }
        }
        else
        {
            const char *const words[3] = {"-m", "unittest", "-v"};
            for (unsigned int at = 0u; at < 3u; at += 1u)
            {
                cli_words_put(&s_words, words[at]);
            }
        }
        for (unsigned int at = 0u; at < run->share_size[share]; at += 1u)
        {
            const char *const id = run->given[run->order[run->share_from[share] + at]];
            if (pytest)
            {
                cli_words_put(&s_words, id);
            }
            else
            {
                cli_test_dotted(&s_words, id);
            }
        }
        worker->run = run;
        worker->index = run->worker_count;
        worker->cargo = 0;
        worker->pytest = pytest;
        worker->heard = 0u;
        worker->named = run->share_from[share];
        worker->named_count = run->share_size[share];
        cli_lock_take(&g_cli_test_lock);
        const int started =
            cli_spawn_as(CLI_PRIORITY_GUEST, &worker->spawned, s_words.word, run->root, settings, 2u, 1, said);
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
        worker->thread = (CliThread){python_worker, worker, CLI_PRIORITY_GUEST};
        if (!cli_thread_started(&worker->thread))
        {
            python_worker(worker);
        }
    }
    return 1;
}
