// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_python.c: a run's Python tests: the Python they run with and whether it has pytest, the shares the tests are
// spread over, and a process of pytest or unittest to each share, read on a thread of its own
#include "../command_line/cells.h"
#include "test_runs_internal.h"

#include "../text/text.h"

int cli_test_python(const CliTestRunsCfg *args)
{
    static CliTreeEnvironment s_environment;
    static CliFound s_found;
    int found = EMBED_CALL(cli_toolchains_table.environment, CliToolchainsCfg, .environment_out = &s_environment) &&
                s_environment.has_python;
    if (found)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->python, .src = s_environment.python, .bytes = CLI_PATH_MAX_LEN});
    }
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int manifest =
        found ? CLI_JSON_NONE
              : EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg, .json_out = &json,
                           .nodes_more = 0u, .text_more = 0ULL);
    for (unsigned int index = 0u;
         !found && (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                               .array = manifest, .index = index) != CLI_JSON_NONE);
         index += 1u)
    {
        const unsigned int tool = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = &json, .array = manifest, .index = index);
        const char *const id =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = tool, .name = "id"),
                       .length_pointer = NULL);
        if ((id == NULL) || (!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = "python", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&count, &g_cli_tests_cells);
        EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &json, .tool = tool,
                   .path_callback = (const char (*)[CLI_PATH_MAX_LEN])folders, .count = count, .found = &s_found);
        g_cli_cells->persistent_buf_release(folders, &g_cli_tests_cells);
        found = s_found.has_program;
        if (found)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->python, .src = s_found.program, .bytes = CLI_PATH_MAX_LEN});
        }
        break;
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return found;
}

int cli_test_has_pytest(const CliTestRunsPythonCtx *args)
{
    static char s_path[CLI_PATH_TEXT];
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path, .size = sizeof(s_path));
    const CliSetting settings[1] = {{"PATH", s_path}};
    const char *const words[4] = {args->python, "-c", "import pytest", NULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int has = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = args->root,
                               .settings = settings, .count = 1u, .run_result = run_result, .err_msg = &err_msg) &&
                    (run_result->code == 0LL);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return has;
}

typedef struct
{
    const char *id;
    CliTestWorker *worker;
    unsigned long long size;
    const char *outcome;
    const char *seen;
    unsigned long long seen_size;
    const char *errors;
    unsigned long long errors_size;
    const CliTestRun *run;
    const char *stem;
    unsigned int share;
    const char *extension;
    char *path;
    unsigned int count;
    unsigned int files;
    unsigned int file;
    unsigned int before;
    unsigned int shares;
    const unsigned int *share_of;
    const unsigned int *sizes;
    CliTestRun *run_out;
} TestPythonCtx;

// the files of a run's supplied tests: the file each test is of, each file's first test and how many tests it has, and
// the files in the order the shares take them
static unsigned int s_file_of[CLI_TEST_NAMED];
static unsigned int s_file_first[CLI_TEST_NAMED];
static unsigned int s_file_size[CLI_TEST_NAMED];
static unsigned int s_by_size[CLI_TEST_NAMED];
// the length of the file part of a test's name, before its first `::`
EMBED_INLINE unsigned long long test_file_length(const TestPythonCtx *args)
{
    const char *const path = cellul.find(
        &(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
    return (path != NULL) ? (unsigned long long)(path - args->id)
                          : cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL});
}

// the `count` supplied tests of `run` grouped by the file their names begin with, into s_file_of, s_file_first and
// s_file_size, the files in the order their first tests come: how many files
EMBED_INLINE unsigned int test_files_grouped(const TestPythonCtx *args)
{
    unsigned int files = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const unsigned long long length = EMBED_CALL(test_file_length, TestPythonCtx, .id = args->run->supplied[at]);
        unsigned int file = 0u;
        while (
            (file < files) &&
            !((EMBED_CALL(test_file_length, TestPythonCtx, .id = args->run->supplied[s_file_first[file]]) == length) &&
              EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->run->supplied[s_file_first[file]],
                         .other_pointer = args->run->supplied[at], .size = length)))
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
    return files;
}

// whether the file `file` of `run`'s tests comes ahead of the file `before`: it has more tests, or as many and a name
// that sorts ahead of the other's
EMBED_INLINE int test_file_ahead(const TestPythonCtx *args)
{
    const char *const before_name = args->run->supplied[s_file_first[args->before]];
    const char *const name = args->run->supplied[s_file_first[args->file]];
    const unsigned long long before_length = EMBED_CALL(test_file_length, TestPythonCtx, .id = before_name);
    const unsigned long long length = EMBED_CALL(test_file_length, TestPythonCtx, .id = name);
    const embed_iword named_after = memor.cmp(&(MemoriaCfg){
        .src = before_name, .other = name, .bytes = (size_t)((before_length < length) ? before_length : length)});
    return (s_file_size[args->before] < s_file_size[args->file]) ||
           ((s_file_size[args->before] == s_file_size[args->file]) &&
            ((named_after > 0) || ((named_after == 0) && (before_length > length))));
}

// the `files` of `run`'s tests put into s_by_size, those with the most tests first and those with as many by name
EMBED_INLINE void test_files_sorted(const TestPythonCtx *args)
{
    for (unsigned int file = 0u; file < args->files; file += 1u)
    {
        unsigned int place = file;
        while ((place > 0u) && EMBED_CALL(test_file_ahead, TestPythonCtx, .run = args->run, .file = file,
                                          .before = s_by_size[place - 1u]))
        {
            s_by_size[place] = s_by_size[place - 1u];
            place -= 1u;
        }
        s_by_size[place] = file;
    }
}

// the `count` tests of `run_out` laid in its order share by share: each of the `shares` that `sizes` gives a test, its
// first place and size, its tests those of the `files` that `share_of` gives it, file after file in s_by_size's order
EMBED_INLINE void test_shares_laid(const TestPythonCtx *args)
{
    CliTestRun *const run = args->run_out;
    run->shares = 0u;
    unsigned int positioned = 0u;
    for (unsigned int share = 0u; share < args->shares; share += 1u)
    {
        if (args->sizes[share] == 0u)
        {
            continue;
        }
        run->share_from[run->shares] = positioned;
        for (unsigned int at = 0u; at < args->files; at += 1u)
        {
            const unsigned int file = s_by_size[at];
            for (unsigned int one = 0u; (args->share_of[file] == share) && (one < args->count); one += 1u)
            {
                if (s_file_of[one] == file)
                {
                    run->order[positioned] = one;
                    positioned += 1u;
                }
            }
        }
        run->share_size[run->shares] = positioned - run->share_from[run->shares];
        run->shares += 1u;
    }
}

void cli_test_shares(const CliTestRunsRunCountCtx *args)
{
    const unsigned int files = EMBED_CALL(test_files_grouped, TestPythonCtx, .run = args->run, .count = args->count);
    EMBED_CALL(test_files_sorted, TestPythonCtx, .run = args->run, .files = files);
    const unsigned int most_shares = (files > 1u) ? files : 1u;
    unsigned int shares = (args->most < 1u) ? 1u : args->most;
    shares = (shares > most_shares) ? most_shares : shares;
    shares = (shares > CLI_TEST_WORKERS) ? CLI_TEST_WORKERS : shares;
    unsigned int share_of[CLI_TEST_NAMED];
    unsigned int sizes[CLI_TEST_WORKERS];
    memor.set(&(MemoriaCfg){.dst = sizes, .val = 0, .bytes = sizeof(sizes)});
    for (unsigned int at = 0u; at < files; at += 1u)
    {
        const unsigned int file = s_by_size[at];
        unsigned int min = 0u;
        for (unsigned int share = 1u; share < shares; share += 1u)
        {
            min = (sizes[share] < sizes[min]) ? share : min;
        }
        share_of[file] = min;
        sizes[min] += s_file_size[file];
    }
    EMBED_CALL(test_shares_laid, TestPythonCtx, .run_out = args->run, .count = args->count, .files = files,
               .shares = shares, .share_of = share_of, .sizes = sizes);
}

// a result received of a Python process: counted where it is new, and reported
EMBED_INLINE void python_result_visit(const TestPythonCtx *args)
{
    CliTestWorker *worker = args->worker;
    const char *id = args->id;
    const char *outcome = args->outcome;
    char named[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {named, sizeof(named), 0ULL};
    mmgr_line_put_n(&line, id, args->size);
    mmgr_line_finish(&line);
    if (EMBED_CALL(cli_test_runs_run_length_internal.test_visit, CliTestRunsRunLengthCtx, .run = worker->run,
                   .worker_u32 = worker->index, .name = id, .length = args->size, .outcome = outcome))
    {
        EMBED_CALL(cli_test_runs_run_length_internal.test_outcome_count, CliTestRunsRunLengthCtx, .run = worker->run,
                   .outcome = outcome);
    }
    EMBED_CALL(cli_test_runs_outcome_internal.test_result_emit, CliTestRunsOutcomeCtx, .worker = worker, .id = named,
               .outcome = outcome, .message = "", .message_size = 0ULL, .file = NULL, .line_i64 = -1LL, .seconds = 0.0);
}

// a line of pytest's verbose report received
static void python_line(const CliWorkerTextSizeCfg *args)
{
    CliTestWorker *worker = args->worker;
    unsigned long long size = args->size;
    unsigned long long id = 0ULL;
    unsigned long long id_size = 0ULL;
    const char *outcome = NULL;
    if (worker->pytest &&
        EMBED_CALL(cli_test_runs_size_internal.test_pytest_line, CliTestRunsSizeCtx, .line = args->text, .size = size,
                   .id_u64s = &id, .id_size = &id_size, .outcome_pointer = &outcome))
    {
        EMBED_CALL(python_result_visit, TestPythonCtx, .worker = worker, .id = args->text + id, .size = id_size,
                   .outcome = outcome);
    }
}

// the last three lines a process wrote that are not blank, of its output then its errors, joined by "\n", given as why
// it ran no test
EMBED_INLINE void python_tail_message(const TestPythonCtx *args)
{
    const char *starts[3];
    unsigned long long sizes[3];
    unsigned int line_count = 0u;
    const char *const texts[2] = {args->seen, args->errors};
    const unsigned long long text_sizes[2] = {args->seen_size, args->errors_size};
    for (unsigned int which = 0u; which < 2u; which += 1u)
    {
        unsigned long long at = 0ULL;
        while (at < text_sizes[which])
        {
            const char *const text = texts[which];
            const CliTextLine bounds =
                EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = text_sizes[which]);
            const unsigned long long end = bounds.cut;
            const unsigned long long start = at;
            at = bounds.next;
            unsigned long long letter = start;
            while ((letter < end) && (cellul.chr(&(CatenaFinitaCfg){
                                          .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = text[letter]}) != NULL))
            {
                letter += 1ULL;
            }
            if (letter < end)
            {
                starts[line_count % 3u] = text + start;
                sizes[line_count % 3u] = end - start;
                line_count += 1u;
            }
        }
    }
    char message_buf[CLI_TEST_MESSAGE_MAX_LEN];
    VerbaLinea line = {message_buf, sizeof(message_buf), 0ULL};
    for (unsigned int one = (line_count > 3u) ? line_count - 3u : 0u; one < line_count; one += 1u)
    {
        mmgr_line_put(&line, (line.at > 0ULL) ? "\n" : "");
        mmgr_line_put_n(&line, starts[one % 3u], sizes[one % 3u]);
    }
    EMBED_CALL(cli_test_runs_run_text_internal.test_message_set, CliTestRunsRunTextCtx, .run = args->worker->run,
               .text = message_buf, .length = mmgr_line_ok(&line) ? line.at : sizeof(message_buf));
}

// a Python process read until it ends: each result reported as it is printed, then unittest's report of its errors, or
// pytest's report in JUnit's form, each failure's message and line reported
static void python_worker(void *context)
{
    CliTestWorker *const worker = (CliTestWorker *)context;
    EMBED_CALL(cli_test_runs_worker_line_internal.test_lines_read, CliTestRunsWorkerLineCtx, .worker = worker,
               .line_callback = python_line);
    EMBED_CALL(cli_test_runs_worker_line_internal.test_errors_wait, CliTestRunsWorkerLineCtx, .worker = worker);
    unsigned long long errors_size = 0ULL;
    const char *const errors = EMBED_CALL(cli_test_runs_path_internal.test_file_read, CliTestRunsPathCtx,
                                          .path = worker->errors, .size_pointer = &errors_size);
    if (!worker->pytest && (errors != NULL))
    {
        unsigned long long at = 0ULL;
        while (at < errors_size)
        {
            const CliTextLine bounds =
                EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = errors, .at = at, .size = errors_size);
            unsigned long long end = bounds.stop;
            const unsigned long long start = at;
            at = bounds.next;
            end -= bounds.stop - bounds.cut;
            char id[CLI_PATH_MAX_LEN * 2u];
            const char *outcome = NULL;
            if (EMBED_CALL(cli_test_runs_size_line_internal.test_unittest_line, CliTestRunsSizeLineCtx,
                           .line = errors + start, .size = end - start, .id_out = id, .capacity = sizeof(id),
                           .outcome_pointer = &outcome))
            {
                EMBED_CALL(python_result_visit, TestPythonCtx, .worker = worker, .id = id,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}),
                           .outcome = outcome);
            }
        }
        EMBED_CALL(cli_test_runs_text_worker_internal.test_unittest_failures_emit, CliTestRunsTextWorkerCtx,
                   .worker = worker, .text = errors, .size = errors_size);
    }
    else if (worker->pytest)
    {
        unsigned long long report_size = 0ULL;
        const char *const report = EMBED_CALL(cli_test_runs_path_internal.test_file_read, CliTestRunsPathCtx,
                                              .path = worker->report, .size_pointer = &report_size);
        if (report != NULL)
        {
            EMBED_CALL(cli_test_runs_worker_internal.test_report_emit, CliTestRunsWorkerCtx, .worker = worker,
                       .xml = report, .size = report_size);
        }
        EMBED_CALL(cli_test_runs_text_worker_internal.test_file_let_go, CliTestRunsTextWorkerCtx, .text = report,
                   .size = report_size);
    }
    if (!EMBED_CALL(cli_test_runs_run_length_internal.test_seen_any, CliTestRunsRunLengthCtx, .run = worker->run,
                    .worker_u32 = worker->index) &&
        !cli_test_stop())
    {
        unsigned long long seen_size = 0ULL;
        const char *const seen = EMBED_CALL(cli_test_runs_path_internal.test_file_read, CliTestRunsPathCtx,
                                            .path = worker->seen, .size_pointer = &seen_size);
        EMBED_CALL(python_tail_message, TestPythonCtx, .worker = worker, .seen = (seen != NULL) ? seen : "",
                   .seen_size = (seen != NULL) ? seen_size : 0ULL, .errors = (errors != NULL) ? errors : "",
                   .errors_size = (errors != NULL) ? errors_size : 0ULL);
        EMBED_CALL(cli_test_runs_text_worker_internal.test_file_let_go, CliTestRunsTextWorkerCtx, .text = seen,
                   .size = seen_size);
    }
    EMBED_CALL(cli_test_runs_text_worker_internal.test_file_let_go, CliTestRunsTextWorkerCtx, .text = errors,
               .size = errors_size);
    CliTestRun *const run = worker->run;
    run->going -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &run->wake);
}

// a file of the run's folder named for a share into `path`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void python_file(const TestPythonCtx *args)
{
    char name[64];
    VerbaLinea line = {name, sizeof(name), 0ULL};
    mmgr_line_put(&line, args->stem);
    mmgr_line_decimal(&line, args->share, 1u);
    mmgr_line_put(&line, args->extension);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->run->scratch, .relative = name, .joined = args->path);
}

int cli_python_workers_start(const CliTestRunsRunCtx *args)
{
    CliTestRun *run = args->run;
    int pytest = args->pytest;
    static CliWords s_words;
    static char s_path[CLI_PATH_TEXT];
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path, .size = sizeof(s_path));
    const CliSetting settings[2] = {{"PATH", s_path}, {"PYTHONIOENCODING", "utf-8"}};
    for (unsigned int share = 0u; share < run->shares; share += 1u)
    {
        if (run->worker_count == CLI_TEST_WORKERS)
        {
            mmgr_line_put(args->err_msg, "no more processes are run at once");
            return 0;
        }
        CliTestWorker *const worker = &run->workers[run->worker_count];
        EMBED_CALL(python_file, TestPythonCtx, .run = run, .stem = "report-", .share = share, .extension = ".xml",
                   .path = worker->report);
        EMBED_CALL(python_file, TestPythonCtx, .run = run, .stem = "coverage-", .share = share, .extension = ".json",
                   .path = worker->covered);
        EMBED_CALL(python_file, TestPythonCtx, .run = run, .stem = "python-", .share = share, .extension = ".err",
                   .path = worker->errors);
        EMBED_CALL(python_file, TestPythonCtx, .run = run, .stem = "python-", .share = share, .extension = ".out",
                   .path = worker->seen);
        s_words.count = 0u;
        s_words.used = 0u;
        s_words.full = 0;
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = args->python);
        if (pytest)
        {
            char junit[CLI_PATH_MAX_LEN + 16u];
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
            for (unsigned int at = 0u; at < (args->cover ? 4u : 2u); at += 1u)
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                           .word_text = args->cover ? head[at] : module[at]);
            }
            // the process starts in the run's root, and pytest is bound to it as its rootdir: the ids its lines and
            // its report give are the ids the run was given though a folder above contains a configuration of its own
            const char *const words[8] = {
                "-v", "-p", "no:cacheprovider", "--tb=short", "-o", "junit_family=xunit1", "--rootdir=.", junit};
            for (unsigned int at = 0u; at < 8u; at += 1u)
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                           .word_text = words[at]);
            }
        }
        else
        {
            const char *const words[3] = {"-m", "unittest", "-v"};
            for (unsigned int at = 0u; at < 3u; at += 1u)
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                           .word_text = words[at]);
            }
        }
        for (unsigned int at = 0u; at < run->share_size[share]; at += 1u)
        {
            const char *const id = run->supplied[run->order[run->share_from[share] + at]];
            if (pytest)
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                           .word_text = id);
            }
            else
            {
                EMBED_CALL(cli_test_runs_internal.test_dot, CliTestRunsCtx, .words = &s_words, .id = id);
            }
        }
        worker->run = run;
        worker->index = run->worker_count;
        worker->cargo = 0;
        worker->pytest = pytest;
        worker->seen_count = 0u;
        worker->named = run->share_from[share];
        worker->named_count = run->share_size[share];
        const int started =
            EMBED_CALL(cli_words_err_msg_internal.spawn_as, CliWordsErrMsgCtx, .priority = CLI_PRIORITY_GUEST,
                       .spawned = &worker->spawned, .words = s_words.word_text, .folder = run->root,
                       .settings = settings, .count = 2u, .detached = 1, .err_msg = args->err_msg);
        run->worker_count += started ? 1u : 0u;
        if (!started)
        {
            return 0;
        }
        EMBED_CALL(cli_test_runs_worker_line_internal.test_errors_start, CliTestRunsWorkerLineCtx, .worker = worker);
        run->going += 1u;
        worker->thread = (CliThread){python_worker, worker, CLI_PRIORITY_GUEST};
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &worker->thread))
        {
            python_worker(worker);
        }
    }
    return 1;
}
