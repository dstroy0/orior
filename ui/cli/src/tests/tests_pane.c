// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tests_pane.c: the tests pane's rows. The files and tests are read on the window's thread and stand in a cell of their
// own taken as the first read comes; a run's reports come on the run's thread and write each test's outcome through an
// atomic word, the message's text and the file's message count before it, so the window reads a whole result or none.
// The coverage is published through its count the same way. A read is refused while a run goes, so the run's thread
// never meets rows being rebuilt
#include "testing.h"

#include "../command_line/cells.h"
#include "../command_line/command_line.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "cargo_tests.h"
#include "embed_atomic.h"
#include "test_runs.h"

// the most files, tests and covered files, the bytes their texts take, the bytes the failures' messages take, the
// bytes of a runner's name and of a run's end message, and the most tests or files handed to a run
#define TESTS_FILES 1024u
#define TESTS_TESTS 16384u
#define TESTS_COVERS 1024u
#define TESTS_TEXT (2u << 20u)
#define TESTS_MESSAGES (512u * 1024u)
#define TESTS_RUNNER 32u
#define TESTS_MESSAGE 256u
#define TESTS_SUPPLIED 16384u

// A file of tests: its path's place in the text, the bytes of its folder before its name, its first test and how many,
// whether its rows are closed, and how many of its tests failed with a message
typedef struct
{
    unsigned int path;
    unsigned long long folder_length;
    unsigned int first;
    unsigned int count;
    int closed;
    embed_atomic_u32 messages;
} TestsFile;

// A test: its name's and its label's places in the text and the label's bytes, the line its name stands on, whether it
// was handed to the run going, its outcome, its time, its message's place in the messages and its bytes, and where it
// failed, the line where it is placed
typedef struct
{
    unsigned int id;
    unsigned int label;
    unsigned long long label_length;
    unsigned long long line;
    int given;
    embed_atomic_u32 outcome;
    unsigned int milliseconds;
    unsigned int message;
    unsigned long long message_length;
    unsigned long long fail_line;
    int fail_placed;
} TestsTest;

// A file's coverage: its path's place in the text, the lines that ran, those missed and the first missed
typedef struct
{
    unsigned int path;
    unsigned int ran;
    unsigned int missed;
    unsigned long long first_missed;
} TestsCover;

// The pane's data: the files and tests and how many, the covered files and how many, the text their paths and labels
// are copied into and how much of it is used, the messages and how much of them, the run going's runner and its
// processes, and the last run's end: its counts, its seconds in tenths and its message
typedef struct
{
    TestsFile file[TESTS_FILES];
    unsigned int files;
    TestsTest test[TESTS_TESTS];
    unsigned int tests;
    TestsCover cover[TESTS_COVERS];
    embed_atomic_u32 covers;
    char text[TESTS_TEXT];
    unsigned int used;
    char messages[TESTS_MESSAGES];
    unsigned int messages_used;
    char runner[TESTS_RUNNER];
    unsigned int workers;
    unsigned long long passed;
    unsigned long long failed;
    unsigned long long skipped;
    unsigned long long tenths;
    char message[TESTS_MESSAGE];
} Tests;

static Tests *s_tests;
static embed_atomic_u32 s_running;
static embed_atomic_u32 s_ended;
static embed_atomic_u32 s_changed;
static void (*s_woken)(void);
static int s_loaded;
static int s_parallel = 1;
static const char *s_supplied[TESTS_SUPPLIED];

typedef struct
{
    const char *text;
    unsigned long long length;
    const CliJson *json;
    unsigned int node;
    const char *name;
    unsigned int file;
    unsigned int at;
    unsigned int *file_out;
} TestsPaneCtx;

// `length` bytes of `text` copied after the texts copied so far with a NUL after them: their place, or TESTS_TEXT
// where they do not fit
EMBED_INLINE unsigned int tests_text_append(const TestsPaneCtx *args)
{
    Tests *const tests = s_tests;
    if (args->length + 1ULL > (unsigned long long)(TESTS_TEXT - tests->used))
    {
        return TESTS_TEXT;
    }
    const unsigned int at = tests->used;
    memor.cpy(&(MemoriaCfg){.dst = tests->text + at, .src = args->text, .bytes = args->length});
    tests->text[at + args->length] = '\0';
    // a text that fits the bytes is counted by an unsigned int
    tests->used = at + (unsigned int)args->length + 1u;
    return at;
}

// a test the finder found: a file row where its file is not the last file's, and the test under it with its label,
// its class and its name joined as its language joins them
static void tests_found(const CliTestEachCfg *args)
{
    Tests *const tests = s_tests;
    const CliTest *const test = args->test;
    const unsigned long long file_length = cellul.len(&(CatenaFinitaCfg){.src = test->file, .cap = CLI_PATH_MAX_LEN});
    if ((tests->files == 0u) ||
        !cellul.eq(&(CatenaFinitaCfg){
            .src = tests->text + tests->file[tests->files - 1u].path, .other = test->file, .cap = CLI_PATH_MAX_LEN}))
    {
        if (tests->files >= TESTS_FILES)
        {
            return;
        }
        const unsigned int path =
            EMBED_CALL(tests_text_append, TestsPaneCtx, .text = test->file, .length = file_length);
        if (path == TESTS_TEXT)
        {
            return;
        }
        unsigned long long cut = 0ULL;
        for (unsigned long long each = 0ULL; each < file_length; each += 1ULL)
        {
            cut = (test->file[each] == '/') ? each : cut;
        }
        tests->file[tests->files] = (TestsFile){path, cut, tests->tests, 0u, 0, EMBED_ATOMIC_INIT(0u)};
        tests->files += 1u;
    }
    if (tests->tests >= TESTS_TESTS)
    {
        return;
    }
    char label[CLI_PATH_MAX_LEN];
    VerbaLinea line = {label, sizeof(label), 0ULL};
    if (test->class_name != NULL)
    {
        mmgr_line_put(&line, test->class_name);
        mmgr_line_put(&line,
                      EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = test->id) ? "::" : ".");
    }
    mmgr_line_put(&line, test->name);
    if (!mmgr_line_ok(&line))
    {
        return;
    }
    const unsigned int id =
        EMBED_CALL(tests_text_append, TestsPaneCtx, .text = test->id,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = test->id, .cap = CLI_CELLS_POOL}));
    const unsigned int label_at = EMBED_CALL(tests_text_append, TestsPaneCtx, .text = label, .length = line.at);
    if ((id == TESTS_TEXT) || (label_at == TESTS_TEXT))
    {
        return;
    }
    tests->test[tests->tests] =
        (TestsTest){id, label_at, line.at, test->line, 0, EMBED_ATOMIC_INIT(CLI_TESTS_NONE), 0u, 0u, 0ULL, 0ULL, 0};
    tests->tests += 1u;
    tests->file[tests->files - 1u].count += 1u;
}

// the key the Parallel choice is saved under in orior's folder
#define TESTS_PARALLEL_KEY "tests.parallel"

void cli_tests_pane_begin(const CliTestsPaneCfg *args)
{
    s_woken = args->woken;
    const char *const saved = (args->entries != NULL) ? cli_state_entry_value(args->entries, TESTS_PARALLEL_KEY) : NULL;
    s_parallel = (saved == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = saved, .other = "false", .cap = 8ULL});
}

void cli_tests_pane_read(const CliTestsPaneCfg *args)
{
    if (EMBED_ATOMIC_LOAD(&s_running) != 0u)
    {
        return;
    }
    if (s_tests == NULL)
    {
        s_tests = g_cli_cells->persistent_buf_alloc(sizeof(Tests), &g_cli_tests_pane_cells);
        if (s_tests == NULL)
        {
            return;
        }
        memor.set(&(MemoriaCfg){.dst = s_tests, .val = 0, .bytes = sizeof(Tests)});
    }
    s_tests->files = 0u;
    s_tests->tests = 0u;
    s_tests->used = 0u;
    s_tests->messages_used = 0u;
    if (args->root != NULL)
    {
        EMBED_CALL(cli_testing_root.tests_find, CliTestingRootCfg, .root = args->root, .each = tests_found,
                   .context = NULL);
    }
    s_loaded = 1;
}

int cli_tests_pane_loaded(const CliTestsPaneCfg *args)
{
    (void)args;
    return s_loaded;
}

// how many rows the file `file` takes: its own, and where open its tests and their failures' messages
EMBED_INLINE unsigned int tests_file_rows(const TestsPaneCtx *args)
{
    const TestsFile *const file = &s_tests->file[args->file];
    return 1u + (file->closed ? 0u : file->count + EMBED_ATOMIC_LOAD(&file->messages));
}

unsigned int cli_tests_pane_rows(const CliTestsPaneCfg *args)
{
    (void)args;
    const Tests *const tests = s_tests;
    if (tests == NULL)
    {
        return 0u;
    }
    unsigned int count = 0u;
    for (unsigned int file = 0u; file < tests->files; file += 1u)
    {
        count += EMBED_CALL(tests_file_rows, TestsPaneCtx, .file = file);
    }
    const unsigned int covers = EMBED_ATOMIC_LOAD(&tests->covers);
    return count + ((covers != 0u) ? 1u + covers : 0u);
}

// the worst outcome among the file's tests, and its counts
EMBED_INLINE void tests_file_row(const TestsPaneCtx *args)
{
    const Tests *const tests = s_tests;
    const TestsFile *const file = &tests->file[args->file];
    CliTestsRow *const row = (CliTestsRow *)args->text;
    unsigned int worst = CLI_TESTS_SKIPPED + 1u;
    for (unsigned int at = file->first; at < file->first + file->count; at += 1u)
    {
        const unsigned int outcome = EMBED_ATOMIC_LOAD(&tests->test[at].outcome);
        row->passed += (outcome == CLI_TESTS_PASSED) ? 1u : 0u;
        row->failed += (outcome == CLI_TESTS_FAILED) ? 1u : 0u;
        row->skipped += (outcome == CLI_TESTS_SKIPPED) ? 1u : 0u;
        worst = ((outcome != CLI_TESTS_NONE) && (outcome < worst)) ? outcome : worst;
    }
    row->outcome = (worst <= CLI_TESTS_SKIPPED) ? worst : CLI_TESTS_NONE;
}

// the file whose rows the row `at` stands in, into `file_out`, and the row's place under the file's own row into `at`:
// 1, or 0 where the row stands past the files
EMBED_INLINE int tests_file_of(const TestsPaneCtx *args)
{
    unsigned int at = args->at;
    for (unsigned int file = 0u; file < s_tests->files; file += 1u)
    {
        const unsigned int rows = EMBED_CALL(tests_file_rows, TestsPaneCtx, .file = file);
        if (at < rows)
        {
            *args->file_out = file;
            *(unsigned int *)args->text = at;
            return 1;
        }
        at -= rows;
    }
    *(unsigned int *)args->text = at;
    return 0;
}

int cli_tests_pane_row(const CliTestsPaneCfg *args)
{
    const Tests *const tests = s_tests;
    CliTestsRow *const row = args->row;
    if (tests == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    unsigned int file_index = 0u;
    unsigned int under = 0u;
    if (EMBED_CALL(tests_file_of, TestsPaneCtx, .at = args->at, .file_out = &file_index, .text = (char *)&under))
    {
        const TestsFile *const file = &tests->file[file_index];
        row->path = tests->text + file->path;
        if (under == 0u)
        {
            row->kind = CLI_TESTS_ROW_FILE;
            row->name = row->path + ((file->folder_length != 0ULL) ? file->folder_length + 1ULL : 0ULL);
            row->name_length = cellul.len(&(CatenaFinitaCfg){.src = row->name, .cap = CLI_PATH_MAX_LEN});
            row->folder_length = file->folder_length;
            row->open = !file->closed;
            EMBED_CALL(tests_file_row, TestsPaneCtx, .file = file_index, .text = (const char *)row);
            return 1;
        }
        // the test rows and the message rows under them, in the order of the tests
        under -= 1u;
        for (unsigned int at = file->first; at < file->first + file->count; at += 1u)
        {
            const TestsTest *const test = &tests->test[at];
            const unsigned int outcome = EMBED_ATOMIC_LOAD(&test->outcome);
            if (under == 0u)
            {
                row->kind = CLI_TESTS_ROW_TEST;
                row->name = tests->text + test->label;
                row->name_length = test->label_length;
                row->outcome = outcome;
                row->milliseconds = test->milliseconds;
                const int failed_placed = (outcome == CLI_TESTS_FAILED) && test->fail_placed;
                row->line = failed_placed ? test->fail_line : test->line;
                return 1;
            }
            under -= 1u;
            if ((outcome == CLI_TESTS_FAILED) && (test->message_length != 0ULL))
            {
                if (under == 0u)
                {
                    row->kind = CLI_TESTS_ROW_MESSAGE;
                    row->name = tests->messages + test->message;
                    row->name_length = test->message_length;
                    row->outcome = outcome;
                    return 1;
                }
                under -= 1u;
            }
        }
        return 0;
    }
    const unsigned int covers = EMBED_ATOMIC_LOAD(&tests->covers);
    if ((covers == 0u) || (under > covers))
    {
        return 0;
    }
    if (under == 0u)
    {
        row->kind = CLI_TESTS_ROW_COVER_HEAD;
        for (unsigned int at = 0u; at < covers; at += 1u)
        {
            row->passed += tests->cover[at].ran;
            row->failed += tests->cover[at].missed;
        }
        return 1;
    }
    const TestsCover *const cover = &tests->cover[under - 1u];
    row->kind = CLI_TESTS_ROW_COVER;
    row->path = tests->text + cover->path;
    row->name = row->path;
    row->name_length = cellul.len(&(CatenaFinitaCfg){.src = row->name, .cap = CLI_PATH_MAX_LEN});
    row->passed = cover->ran;
    row->failed = cover->missed;
    row->line = cover->first_missed;
    return 1;
}

void cli_tests_pane_toggle(const CliTestsPaneCfg *args)
{
    unsigned int file = 0u;
    unsigned int under = 0u;
    if ((s_tests != NULL) &&
        EMBED_CALL(tests_file_of, TestsPaneCtx, .at = args->at, .file_out = &file, .text = (char *)&under) &&
        (under == 0u))
    {
        s_tests->file[file].closed = !s_tests->file[file].closed;
    }
}

int cli_tests_pane_run(const CliTestsPaneCfg *args)
{
    Tests *tests = s_tests;
    cli_test_runs_stop();
    EMBED_ATOMIC_STORE(&s_running, 0u);
    if (!args->failed)
    {
        // the tree's tests are read again first: a file of tests written since the last reading runs too
        cli_tests_pane_read(args);
        tests = s_tests;
    }
    if (tests == NULL)
    {
        return 0;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < tests->tests; at += 1u)
    {
        tests->test[at].given = 0;
    }
    if (args->failed)
    {
        for (unsigned int at = 0u; (at < tests->tests) && (count < TESTS_SUPPLIED); at += 1u)
        {
            if (EMBED_ATOMIC_LOAD(&tests->test[at].outcome) == CLI_TESTS_FAILED)
            {
                s_supplied[count] = tests->text + tests->test[at].id;
                tests->test[at].given = 1;
                count += 1u;
            }
        }
        if (count == 0u)
        {
            cli_notice_post("No test failed in the last run.", 0);
            return 0;
        }
    }
    else
    {
        for (unsigned int file = 0u; (file < tests->files) && (count < TESTS_SUPPLIED); file += 1u)
        {
            const char *const path = tests->text + tests->file[file].path;
            // a run with coverage runs the Python files alone, cargo test recording none
            if (args->cover && EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = path))
            {
                continue;
            }
            s_supplied[count] = path;
            count += 1u;
            for (unsigned int at = tests->file[file].first; at < tests->file[file].first + tests->file[file].count;
                 at += 1u)
            {
                tests->test[at].given = 1;
            }
        }
        if (count == 0u)
        {
            cli_notice_post("No test is found to run: Run, Find Tests says what is taken as one.", 1);
            return 0;
        }
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    unsigned long long number = 0ULL;
    const CliJsonSink sink = {cli_tests_pane_event, NULL};
    EMBED_ATOMIC_STORE(&s_ended, 0u);
    EMBED_ATOMIC_STORE(&s_running, 1u);
    if (!EMBED_CALL(cli_test_runs_root.start, CliTestRunsRootCfg, .root = args->root, .supplied = s_supplied,
                    .count = count, .parallel = s_parallel, .cover = args->cover, .sink = sink, .number = &number,
                    .err_msg = &err_msg))
    {
        EMBED_ATOMIC_STORE(&s_running, 0u);
        cli_notice_post((mmgr_line_finish(&err_msg) != 0ULL) ? why : "The tests could not be run.", 1);
        return 0;
    }
    return 1;
}

void cli_tests_pane_stop(const CliTestsPaneCfg *args)
{
    (void)args;
    cli_test_runs_stop();
}

int cli_tests_pane_running(const CliTestsPaneCfg *args)
{
    (void)args;
    return EMBED_ATOMIC_LOAD(&s_running) != 0u;
}

int cli_tests_pane_any_failed(const CliTestsPaneCfg *args)
{
    (void)args;
    const Tests *const tests = s_tests;
    for (unsigned int at = 0u; (tests != NULL) && (at < tests->tests); at += 1u)
    {
        if (EMBED_ATOMIC_LOAD(&tests->test[at].outcome) == CLI_TESTS_FAILED)
        {
            return 1;
        }
    }
    return 0;
}

// a whole number member of `node` in `json`, 0 where it has none
EMBED_INLINE unsigned long long tests_integer(const TestsPaneCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->node, .name = args->name);
    const char *const digits = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = member, .kind = CFG_JSON_NUMBER)
                                   ? json->text + json->node[member].text
                                   : "";
    return EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = digits, .at = 0ULL,
                      .size = cellul.len(&(CatenaFinitaCfg){.src = digits, .cap = CLI_CELLS_POOL}), .end = NULL);
}

// a number member of `node` in `json`, written in seconds with a fraction, in milliseconds; 0 where it has none or
// is written with an exponent, which the run writes for a time under a microsecond
EMBED_INLINE unsigned int tests_milliseconds(const TestsPaneCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->node, .name = args->name);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = member,
                    .kind = CFG_JSON_NUMBER))
    {
        return 0u;
    }
    const char *const written = json->text + json->node[member].text;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = written, .cap = CLI_CELLS_POOL});
    unsigned long long at = 0ULL;
    unsigned long long milliseconds = 1000ULL * EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = written,
                                                           .at = 0ULL, .size = size, .end = &at);
    unsigned long long place = 100ULL;
    for (unsigned long long each = at + 1ULL; (at < size) && (written[at] == '.') && (each < size) &&
                                              (written[each] >= '0') && (written[each] <= '9') && (place != 0ULL);
         each += 1ULL)
    {
        milliseconds += (unsigned long long)(written[each] - '0') * place;
        place /= 10ULL;
    }
    for (unsigned long long each = 0ULL; each < size; each += 1ULL)
    {
        milliseconds = ((written[each] == 'e') || (written[each] == 'E')) ? 0ULL : milliseconds;
    }
    // a test's time in milliseconds is far inside an unsigned int
    return (unsigned int)milliseconds;
}

// a string member of `node` in `json`, "" where it has none, its bytes into `length_pointer`
EMBED_INLINE const char *tests_string(const TestsPaneCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->node, .name = args->name),
                   .length_pointer = &length);
    *(unsigned long long *)args->file_out = length;
    return (text != NULL) ? text : "";
}

// the test whose name is `text`: its place, or TESTS_TESTS where none has it
EMBED_INLINE unsigned int tests_by_id(const TestsPaneCtx *args)
{
    const Tests *const tests = s_tests;
    for (unsigned int at = 0u; at < tests->tests; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = tests->text + tests->test[at].id, .other = args->text, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    return TESTS_TESTS;
}

// the file the test `at` stands in
EMBED_INLINE TestsFile *tests_file_holding(const TestsPaneCtx *args)
{
    Tests *const tests = s_tests;
    for (unsigned int file = 0u; file < tests->files; file += 1u)
    {
        if ((args->at >= tests->file[file].first) && (args->at < tests->file[file].first + tests->file[file].count))
        {
            return &tests->file[file];
        }
    }
    return &tests->file[0];
}

// a run started: the tests handed to it running
EMBED_INLINE void tests_started(const TestsPaneCtx *args)
{
    Tests *const tests = s_tests;
    unsigned long long length = 0ULL;
    const char *const runner = EMBED_CALL(tests_string, TestsPaneCtx, .json = args->json, .node = args->node,
                                          .name = "runner", .file_out = (unsigned int *)&length);
    VerbaLinea line = {tests->runner, sizeof(tests->runner), 0ULL};
    mmgr_line_put_n(&line, runner, length);
    (void)mmgr_line_finish(&line);
    // a run's processes are few
    tests->workers = (unsigned int)EMBED_CALL(tests_integer, TestsPaneCtx, .json = args->json, .node = args->node,
                                              .name = "workers");
    for (unsigned int at = 0u; at < tests->tests; at += 1u)
    {
        if (tests->test[at].given)
        {
            EMBED_ATOMIC_STORE(&tests->test[at].outcome, CLI_TESTS_RUNNING);
        }
    }
}

// a result: its test's time, message and failing place set, then its outcome
EMBED_INLINE void tests_result(const TestsPaneCtx *args)
{
    Tests *const tests = s_tests;
    const CliJson *json = args->json;
    const unsigned int outcome = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->node, .name = "outcome");
    unsigned long long length = 0ULL;
    const char *const id = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = outcome, .name = "id",
                                      .file_out = (unsigned int *)&length);
    const unsigned int at = EMBED_CALL(tests_by_id, TestsPaneCtx, .text = id);
    if (at == TESTS_TESTS)
    {
        return;
    }
    TestsTest *const test = &tests->test[at];
    const char *const how = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = outcome, .name = "outcome",
                                       .file_out = (unsigned int *)&length);
    unsigned int result = CLI_TESTS_NONE;
    if (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "passed", .cap = 16ULL}))
    {
        result = CLI_TESTS_PASSED;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "failed", .cap = 16ULL}))
    {
        result = CLI_TESTS_FAILED;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "skipped", .cap = 16ULL}))
    {
        result = CLI_TESTS_SKIPPED;
    }
    test->milliseconds = EMBED_CALL(tests_milliseconds, TestsPaneCtx, .json = json, .node = outcome, .name = "seconds");
    const unsigned int line_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = outcome, .name = "line");
    test->fail_placed = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                   .node = line_node, .kind = CFG_JSON_NUMBER);
    test->fail_line = EMBED_CALL(tests_integer, TestsPaneCtx, .json = json, .node = outcome, .name = "line");
    // a failure's message keeps what the result's said where the details come after it with none
    unsigned long long message_length = 0ULL;
    const char *const message = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = outcome, .name = "message",
                                           .file_out = (unsigned int *)&message_length);
    if ((result == CLI_TESTS_FAILED) && (message_length != 0ULL) && (test->message_length == 0ULL))
    {
        unsigned long long first = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = message,
                                              .at = 0ULL, .size = message_length, .set = "\n");
        first -= ((first > 0ULL) && (message[first - 1ULL] == '\r')) ? 1ULL : 0ULL;
        if (first + 1ULL <= (unsigned long long)(TESTS_MESSAGES - tests->messages_used))
        {
            test->message = tests->messages_used;
            memor.cpy(&(MemoriaCfg){.dst = tests->messages + test->message, .src = message, .bytes = first});
            tests->messages[test->message + first] = '\0';
            // a message that fits the bytes is counted by an unsigned int
            tests->messages_used += (unsigned int)first + 1u;
            test->message_length = first;
            TestsFile *const file = EMBED_CALL(tests_file_holding, TestsPaneCtx, .at = at);
            EMBED_ATOMIC_STORE(&file->messages, EMBED_ATOMIC_LOAD(&file->messages) + 1u);
        }
    }
    EMBED_ATOMIC_STORE(&test->outcome, result);
}

// the coverage: the files' lines that ran and were missed, published through their count
EMBED_INLINE void tests_coverage(const TestsPaneCtx *args)
{
    Tests *const tests = s_tests;
    const CliJson *json = args->json;
    EMBED_ATOMIC_STORE(&tests->covers, 0u);
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->node, .name = "files");
    unsigned int count = 0u;
    for (unsigned int file = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = files, .kind = CFG_JSON_ARRAY)
                                 ? json->node[files].first
                                 : CLI_JSON_NONE;
         (file != CLI_JSON_NONE) && (count < TESTS_COVERS); file = json->node[file].next)
    {
        unsigned long long length = 0ULL;
        const char *const name = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = file, .name = "file",
                                            .file_out = (unsigned int *)&length);
        const unsigned int path = EMBED_CALL(tests_text_append, TestsPaneCtx, .text = name, .length = length);
        if (path == TESTS_TEXT)
        {
            break;
        }
        const unsigned int ran = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = file, .name = "ran");
        const unsigned int missed = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = file, .name = "missed");
        TestsCover *const cover = &tests->cover[count];
        *cover = (TestsCover){path, 0u, 0u, 0ULL};
        cover->ran = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = ran,
                                .kind = CFG_JSON_ARRAY)
                         ? json->node[ran].count
                         : 0u;
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = missed,
                       .kind = CFG_JSON_ARRAY))
        {
            cover->missed = json->node[missed].count;
            unsigned long long first = ~0ULL;
            for (unsigned int item = json->node[missed].first; item != CLI_JSON_NONE; item = json->node[item].next)
            {
                const char *const digits = json->text + json->node[item].text;
                const unsigned long long line = EMBED_CALL(
                    cli_text_at_size.number, CliTextAtSizeCfg, .text = digits, .at = 0ULL,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = digits, .cap = CLI_CELLS_POOL}), .end = NULL);
                first = (line < first) ? line : first;
            }
            cover->first_missed = (first != ~0ULL) ? first : 0ULL;
        }
        count += 1u;
    }
    EMBED_ATOMIC_STORE(&tests->covers, count);
}

// a run's end: its counts, its seconds in tenths and its message, no test left running
EMBED_INLINE void tests_done(const TestsPaneCtx *args)
{
    Tests *const tests = s_tests;
    const CliJson *json = args->json;
    tests->passed = EMBED_CALL(tests_integer, TestsPaneCtx, .json = json, .node = args->node, .name = "passed");
    tests->failed = EMBED_CALL(tests_integer, TestsPaneCtx, .json = json, .node = args->node, .name = "failed");
    tests->skipped = EMBED_CALL(tests_integer, TestsPaneCtx, .json = json, .node = args->node, .name = "skipped");
    const unsigned int milliseconds =
        EMBED_CALL(tests_milliseconds, TestsPaneCtx, .json = json, .node = args->node, .name = "seconds");
    tests->tenths = (milliseconds + 50u) / 100u;
    unsigned long long length = 0ULL;
    const char *const message = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = args->node,
                                           .name = "message", .file_out = (unsigned int *)&length);
    VerbaLinea line = {tests->message, sizeof(tests->message), 0ULL};
    mmgr_line_put_n(&line, message, length);
    (void)mmgr_line_finish(&line);
    for (unsigned int at = 0u; at < tests->tests; at += 1u)
    {
        if (EMBED_ATOMIC_LOAD(&tests->test[at].outcome) == CLI_TESTS_RUNNING)
        {
            EMBED_ATOMIC_STORE(&tests->test[at].outcome, CLI_TESTS_NONE);
        }
    }
    EMBED_ATOMIC_STORE(&s_ended, 1u);
    EMBED_ATOMIC_STORE(&s_running, 0u);
}

void cli_tests_pane_event(const CliEachJsonSizeCfg *args)
{
    if (s_tests == NULL)
    {
        return;
    }
    CliJson tree;
    CliJson *const json = &tree;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ULL};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = json, .text = args->json, .size = args->size, .nodes_more = 0u,
                                         .text_more = 0ULL, .err_msg = &ignored, .owner = &g_cli_tests_pane_cells);
    unsigned long long length = 0ULL;
    const char *const kind = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = root, .name = "kind",
                                        .file_out = (unsigned int *)&length);
    if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "started", .cap = 16ULL}))
    {
        EMBED_CALL(tests_started, TestsPaneCtx, .json = json, .node = root);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "result", .cap = 16ULL}))
    {
        EMBED_CALL(tests_result, TestsPaneCtx, .json = json, .node = root);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "coverage", .cap = 16ULL}))
    {
        EMBED_CALL(tests_coverage, TestsPaneCtx, .json = json, .node = root);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "done", .cap = 16ULL}))
    {
        EMBED_CALL(tests_done, TestsPaneCtx, .json = json, .node = root);
    }
    g_cli_cells->persistent_buf_release(json->node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json->node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    EMBED_ATOMIC_STORE(&s_changed, 1u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

int cli_tests_pane_changed(const CliTestsPaneCfg *args)
{
    (void)args;
    return EMBED_ATOMIC_EXCHANGE_32(&s_changed, 0u) != 0u;
}

int cli_tests_pane_said(const CliTestsPaneCfg *args)
{
    (void)args;
    return (EMBED_ATOMIC_LOAD(&s_running) != 0u) || (EMBED_ATOMIC_LOAD(&s_ended) != 0u);
}

unsigned long long cli_tests_pane_summary(const CliTestsPaneCfg *args)
{
    const Tests *const tests = s_tests;
    VerbaLinea line = {args->text, CLI_TESTS_SUMMARY, 0ULL};
    *args->failed_out = 0;
    if (tests == NULL)
    {
        return 0ULL;
    }
    if (EMBED_ATOMIC_LOAD(&s_running) != 0u)
    {
        unsigned int done = 0u;
        for (unsigned int at = 0u; at < tests->tests; at += 1u)
        {
            const unsigned int outcome = EMBED_ATOMIC_LOAD(&tests->test[at].outcome);
            done += (tests->test[at].given && (outcome != CLI_TESTS_RUNNING) && (outcome != CLI_TESTS_NONE)) ? 1u : 0u;
        }
        mmgr_line_put(&line, "Running with ");
        mmgr_line_put(&line, tests->runner);
        mmgr_line_put(&line, " over ");
        mmgr_line_decimal(&line, tests->workers, 1u);
        mmgr_line_put(&line, (tests->workers == 1u) ? " process: " : " processes: ");
        mmgr_line_decimal(&line, done, 1u);
        mmgr_line_put(&line, " done.");
    }
    else if (EMBED_ATOMIC_LOAD(&s_ended) != 0u)
    {
        *args->failed_out = tests->failed > 0ULL;
        if ((tests->message[0] != '\0') && (tests->passed == 0ULL) && (tests->failed == 0ULL))
        {
            mmgr_line_put(&line, "No test ran: ");
            mmgr_line_put(&line, tests->message);
        }
        else
        {
            mmgr_line_decimal(&line, tests->passed, 1u);
            mmgr_line_put(&line, " passed, ");
            mmgr_line_decimal(&line, tests->failed, 1u);
            mmgr_line_put(&line, " failed");
            if (tests->skipped != 0ULL)
            {
                mmgr_line_put(&line, ", ");
                mmgr_line_decimal(&line, tests->skipped, 1u);
                mmgr_line_put(&line, " skipped");
            }
            mmgr_line_put(&line, " in ");
            mmgr_line_decimal(&line, tests->tenths / 10ULL, 1u);
            mmgr_line_ch(&line, '.');
            mmgr_line_decimal(&line, tests->tenths % 10ULL, 1u);
            mmgr_line_put(&line, " s");
        }
    }
    else
    {
        return 0ULL;
    }
    return mmgr_line_finish(&line) ? line.at : 0ULL;
}

int cli_tests_pane_parallel(const CliTestsPaneCfg *args)
{
    (void)args;
    return s_parallel;
}

void cli_tests_pane_parallel_set(const CliTestsPaneCfg *args)
{
    s_parallel = args->on;
    char folder[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(&(CliStateValueCfg){
            .folder = folder, .key = TESTS_PARALLEL_KEY, .value = args->on ? "true" : "false", .err_msg = &err_msg});
    }
}
