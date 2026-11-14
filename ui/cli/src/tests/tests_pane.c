// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tests_pane.c: the tests pane's rows. The files, tests, covered files and their texts are read on the window's thread
// and are in one movable cell taken as the first read comes, read and written a record or a text at a time; a run's
// reports come on the run's thread and write each test's record, then its outcome through an atomic word in a fixed
// cell, the message's text and the file's message count before it, so the window reads a whole result or none. The
// coverage is published through its count the same way. A read is refused while a run goes, so the run's thread never
// meets rows being rebuilt
#include "testing.h"

#include "../command_line/command_line.h"
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "cargo_tests.h"
#include "embed_atomic.h"
#include "test_runs.h"

// the max files, tests and covered files, the bytes their texts take, the bytes the failures' messages take, the
// bytes of a runner's name and of a run's end message, and the max tests or files passed to a run
#define TESTS_FILES 1024u
#define TESTS_TESTS 16384u
#define TESTS_COVERS 1024u
#define TESTS_TEXT (2u << 20u)
#define TESTS_MESSAGES (512u * 1024u)
#define TESTS_RUNNER 32u
#define TESTS_MESSAGE 256u
#define TESTS_SUPPLIED 16384u

// A file of tests: its path's place in the text, the bytes of its folder before its name, its first test and how many,
// and whether its rows are closed
typedef struct
{
    unsigned int path;
    unsigned long long folder_length;
    unsigned int first;
    unsigned int count;
    int closed;
} TestsFile;

// A test: its name's and its label's places in the text and the label's bytes, the line its name is on, whether it
// was passed to the run going, its time, its message's place in the messages and its bytes, and where it failed, the
// line where it is placed
typedef struct
{
    unsigned int id;
    unsigned int label;
    unsigned long long label_length;
    unsigned long long line;
    int given;
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

// the places in the movable cell: the files' records, the tests' records, the covered files' records, the text and the
// messages, each at its max
#define TESTS_FILE_AT(at_) ((unsigned long long)(at_) * sizeof(TestsFile))
#define TESTS_TEST_AT(at_) (TESTS_FILE_AT(TESTS_FILES) + ((unsigned long long)(at_) * sizeof(TestsTest)))
#define TESTS_COVER_AT(at_) (TESTS_TEST_AT(TESTS_TESTS) + ((unsigned long long)(at_) * sizeof(TestsCover)))
#define TESTS_TEXT_AT(at_) (TESTS_COVER_AT(TESTS_COVERS) + (unsigned long long)(at_))
#define TESTS_MESSAGE_AT(at_) (TESTS_TEXT_AT(TESTS_TEXT) + (unsigned long long)(at_))
#define TESTS_CELL_BYTES TESTS_MESSAGE_AT(TESTS_MESSAGES)

// The pane's data: the movable cell the records and texts are in; the fixed cell of the words the run's thread
// publishes through, each test's outcome and each file's count of tests failed with a message; the files and tests and
// how many, the covered files and how many, how much of the text and of the messages is used, the run going's runner
// and its processes, and the last run's end: its counts, its seconds in tenths and its message
typedef struct
{
    const CarcerMovableEntry *cell;
    embed_atomic_u32 *words;
    unsigned int files;
    unsigned int tests;
    embed_atomic_u32 covers;
    unsigned int used;
    unsigned int messages_used;
    char runner[TESTS_RUNNER];
    unsigned int workers;
    unsigned long long passed;
    unsigned long long failed;
    unsigned long long skipped;
    unsigned long long tenths;
    char message[TESTS_MESSAGE];
} Tests;

// the words: a test's outcome at its place, then a file's message count at its place
#define TESTS_OUTCOME(at_) (&s_tests.words[(at_)])
#define TESTS_FILE_MESSAGES(file_) (&s_tests.words[TESTS_TESTS + (file_)])
#define TESTS_WORDS ((unsigned long long)(TESTS_TESTS + TESTS_FILES) * sizeof(embed_atomic_u32))

static Tests s_tests;
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
    if (args->length + 1ULL > (unsigned long long)(TESTS_TEXT - s_tests.used))
    {
        return TESTS_TEXT;
    }
    const unsigned int at = s_tests.used;
    const char end = '\0';
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_tests.cell, .offset = TESTS_TEXT_AT(at), .src = args->text, .bytes = args->length});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_tests.cell, .offset = TESTS_TEXT_AT(at + args->length), .src = &end, .bytes = 1ULL});
    // a text that fits the bytes is counted by an unsigned int
    s_tests.used = at + (unsigned int)args->length + 1u;
    return at;
}

// a test the finder found: a file row where its file is not the last file's, and the test under it with its label,
// its class and its name joined as its language joins them
static void tests_found(const CliTestEachCfg *args)
{
    const CliTest *const test = args->test;
    const unsigned long long file_length = cellul.len(&(CatenaFinitaCfg){.src = test->file, .cap = CLI_PATH_MAX_LEN});
    TestsFile last = {0u, 0ULL, 0u, 0u, 0};
    if (s_tests.files != 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_tests.cell, .offset = TESTS_FILE_AT(s_tests.files - 1u), .dst = &last, .bytes = sizeof(last)});
    }
    if ((s_tests.files == 0u) ||
        (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = s_tests.cell,
                                                              .offset = TESTS_TEXT_AT(last.path),
                                                              .src = test->file,
                                                              .bytes = file_length + 1ULL}) != 0))
    {
        if (s_tests.files >= TESTS_FILES)
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
        last = (TestsFile){path, cut, s_tests.tests, 0u, 0};
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = s_tests.cell, .offset = TESTS_FILE_AT(s_tests.files), .src = &last, .bytes = sizeof(last)});
        EMBED_ATOMIC_STORE(TESTS_FILE_MESSAGES(s_tests.files), 0u);
        s_tests.files += 1u;
    }
    if (s_tests.tests >= TESTS_TESTS)
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
                   .length = cellul.len(&(CatenaFinitaCfg){.src = test->id, .cap = CLI_POOL_BYTES}));
    const unsigned int label_at = EMBED_CALL(tests_text_append, TestsPaneCtx, .text = label, .length = line.at);
    if ((id == TESTS_TEXT) || (label_at == TESTS_TEXT))
    {
        return;
    }
    const TestsTest found = {id, label_at, line.at, test->line, 0, 0u, 0u, 0ULL, 0ULL, 0};
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_tests.cell, .offset = TESTS_TEST_AT(s_tests.tests), .src = &found, .bytes = sizeof(found)});
    EMBED_ATOMIC_STORE(TESTS_OUTCOME(s_tests.tests), CLI_TESTS_NONE);
    s_tests.tests += 1u;
    last.count += 1u;
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_tests.cell, .offset = TESTS_FILE_AT(s_tests.files - 1u), .src = &last, .bytes = sizeof(last)});
}

// the key the Parallel choice is saved under in orior's folder
#define TESTS_PARALLEL_KEY "tests.parallel"

void cli_tests_pane_begin(const CliTestsPaneCfg *args)
{
    s_woken = args->woken;
    char saved_text[16];
    const char *const saved = cli_state_entry_value(args->entries, TESTS_PARALLEL_KEY, saved_text, sizeof(saved_text));
    s_parallel = (saved == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = saved, .other = "false", .cap = 8ULL});
}

void cli_tests_pane_read(const CliTestsPaneCfg *args)
{
    if (EMBED_ATOMIC_LOAD(&s_running) != 0u)
    {
        return;
    }
    if (s_tests.cell == NULL)
    {
        // the outcome and message words are read and written by the runs' threads as atomics, so they are fixed
        // outside the pool, where they punch no hole in the persistent tier
        static embed_atomic_u32 s_words[TESTS_TESTS + TESTS_FILES];
        s_tests.cell = g_cli_guard->movable_buf_alloc(TESTS_CELL_BYTES, g_cli_tests_pane_owner);
        if (s_tests.cell == NULL)
        {
            return;
        }
        s_tests.words = s_words;
        memor.set(&(MemoriaCfg){.dst = s_tests.words, .val = 0, .bytes = (embed_word)TESTS_WORDS});
    }
    s_tests.files = 0u;
    s_tests.tests = 0u;
    s_tests.used = 0u;
    s_tests.messages_used = 0u;
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
    TestsFile file;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_tests.cell, .offset = TESTS_FILE_AT(args->file), .dst = &file, .bytes = sizeof(file)});
    return 1u + (file.closed ? 0u : file.count + EMBED_ATOMIC_LOAD(TESTS_FILE_MESSAGES(args->file)));
}

unsigned int cli_tests_pane_rows(const CliTestsPaneCfg *args)
{
    (void)args;
    if (s_tests.cell == NULL)
    {
        return 0u;
    }
    unsigned int count = 0u;
    for (unsigned int file = 0u; file < s_tests.files; file += 1u)
    {
        count += EMBED_CALL(tests_file_rows, TestsPaneCtx, .file = file);
    }
    const unsigned int covers = EMBED_ATOMIC_LOAD(&s_tests.covers);
    return count + ((covers != 0u) ? 1u + covers : 0u);
}

// the worst outcome among the file's tests, and its counts
EMBED_INLINE void tests_file_row(const TestsPaneCtx *args)
{
    TestsFile file;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_tests.cell, .offset = TESTS_FILE_AT(args->file), .dst = &file, .bytes = sizeof(file)});
    CliTestsRow *const row = (CliTestsRow *)args->text;
    unsigned int worst = CLI_TESTS_SKIPPED + 1u;
    for (unsigned int at = file.first; at < file.first + file.count; at += 1u)
    {
        const unsigned int outcome = EMBED_ATOMIC_LOAD(TESTS_OUTCOME(at));
        row->passed += (outcome == CLI_TESTS_PASSED) ? 1u : 0u;
        row->failed += (outcome == CLI_TESTS_FAILED) ? 1u : 0u;
        row->skipped += (outcome == CLI_TESTS_SKIPPED) ? 1u : 0u;
        worst = ((outcome != CLI_TESTS_NONE) && (outcome < worst)) ? outcome : worst;
    }
    row->outcome = (worst <= CLI_TESTS_SKIPPED) ? worst : CLI_TESTS_NONE;
}

// the file whose rows the row `at` is in, into `file_out`, and the row's place under the file's own row into `at`:
// 1, or 0 where the row is past the files
EMBED_INLINE int tests_file_of(const TestsPaneCtx *args)
{
    unsigned int at = args->at;
    for (unsigned int file = 0u; file < s_tests.files; file += 1u)
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

// the text at `at` copied into the row's text `text` of `length` bytes, cut to the row's size, with its end; the row's
// pointer `file_out` set to it
EMBED_INLINE void tests_row_text(const TestsPaneCtx *args)
{
    char *const copied = (char *)args->text;
    const unsigned long long length = (args->length < CLI_TESTS_ROW_TEXT) ? args->length : (CLI_TESTS_ROW_TEXT - 1ULL);
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_tests.cell, .offset = args->at, .dst = copied, .bytes = length});
    copied[length] = '\0';
    *(const char **)args->file_out = copied;
}

int cli_tests_pane_row(const CliTestsPaneCfg *args)
{
    CliTestsRow *const row = args->row;
    if (s_tests.cell == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    unsigned int file_index = 0u;
    unsigned int under = 0u;
    if (EMBED_CALL(tests_file_of, TestsPaneCtx, .at = args->at, .file_out = &file_index, .text = (char *)&under))
    {
        TestsFile file;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_tests.cell, .offset = TESTS_FILE_AT(file_index), .dst = &file, .bytes = sizeof(file)});
        const unsigned long long path_length = g_cli_guard->movable_buf_measure(
            &(CarcerMovableCfg){.cell = s_tests.cell, .offset = TESTS_TEXT_AT(file.path)});
        EMBED_CALL(tests_row_text, TestsPaneCtx, .at = (unsigned int)TESTS_TEXT_AT(file.path), .length = path_length,
                   .text = row->path_copied, .file_out = (unsigned int *)&row->path);
        if (under == 0u)
        {
            row->type = CLI_TESTS_ROW_FILE;
            row->name = row->path + ((file.folder_length != 0ULL) ? file.folder_length + 1ULL : 0ULL);
            row->name_length = cellul.len(&(CatenaFinitaCfg){.src = row->name, .cap = CLI_PATH_MAX_LEN});
            row->folder_length = file.folder_length;
            row->open = !file.closed;
            EMBED_CALL(tests_file_row, TestsPaneCtx, .file = file_index, .text = (const char *)row);
            return 1;
        }
        // the test rows and the message rows under them, in the order of the tests
        under -= 1u;
        for (unsigned int at = file.first; at < file.first + file.count; at += 1u)
        {
            TestsTest test;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_tests.cell, .offset = TESTS_TEST_AT(at), .dst = &test, .bytes = sizeof(test)});
            const unsigned int outcome = EMBED_ATOMIC_LOAD(TESTS_OUTCOME(at));
            if (under == 0u)
            {
                row->type = CLI_TESTS_ROW_TEST;
                EMBED_CALL(tests_row_text, TestsPaneCtx, .at = (unsigned int)TESTS_TEXT_AT(test.label),
                           .length = test.label_length, .text = row->name_copied,
                           .file_out = (unsigned int *)&row->name);
                row->name_length = cellul.len(&(CatenaFinitaCfg){.src = row->name, .cap = CLI_TESTS_ROW_TEXT});
                row->outcome = outcome;
                row->milliseconds = test.milliseconds;
                const int failed_placed = (outcome == CLI_TESTS_FAILED) && test.fail_placed;
                row->line = failed_placed ? test.fail_line : test.line;
                return 1;
            }
            under -= 1u;
            if ((outcome == CLI_TESTS_FAILED) && (test.message_length != 0ULL))
            {
                if (under == 0u)
                {
                    row->type = CLI_TESTS_ROW_MESSAGE;
                    EMBED_CALL(tests_row_text, TestsPaneCtx, .at = (unsigned int)TESTS_MESSAGE_AT(test.message),
                               .length = test.message_length, .text = row->name_copied,
                               .file_out = (unsigned int *)&row->name);
                    row->name_length = cellul.len(&(CatenaFinitaCfg){.src = row->name, .cap = CLI_TESTS_ROW_TEXT});
                    row->outcome = outcome;
                    return 1;
                }
                under -= 1u;
            }
        }
        return 0;
    }
    const unsigned int covers = EMBED_ATOMIC_LOAD(&s_tests.covers);
    if ((covers == 0u) || (under > covers))
    {
        return 0;
    }
    if (under == 0u)
    {
        row->type = CLI_TESTS_ROW_COVER_HEAD;
        for (unsigned int at = 0u; at < covers; at += 1u)
        {
            TestsCover cover;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_tests.cell, .offset = TESTS_COVER_AT(at), .dst = &cover, .bytes = sizeof(cover)});
            row->passed += cover.ran;
            row->failed += cover.missed;
        }
        return 1;
    }
    TestsCover cover;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_tests.cell, .offset = TESTS_COVER_AT(under - 1u), .dst = &cover, .bytes = sizeof(cover)});
    row->type = CLI_TESTS_ROW_COVER;
    const unsigned long long path_length = g_cli_guard->movable_buf_measure(
        &(CarcerMovableCfg){.cell = s_tests.cell, .offset = TESTS_TEXT_AT(cover.path)});
    EMBED_CALL(tests_row_text, TestsPaneCtx, .at = (unsigned int)TESTS_TEXT_AT(cover.path), .length = path_length,
               .text = row->path_copied, .file_out = (unsigned int *)&row->path);
    row->name = row->path;
    row->name_length = cellul.len(&(CatenaFinitaCfg){.src = row->name, .cap = CLI_PATH_MAX_LEN});
    row->passed = cover.ran;
    row->failed = cover.missed;
    row->line = cover.first_missed;
    return 1;
}

void cli_tests_pane_toggle(const CliTestsPaneCfg *args)
{
    unsigned int file_index = 0u;
    unsigned int under = 0u;
    if ((s_tests.cell != NULL) &&
        EMBED_CALL(tests_file_of, TestsPaneCtx, .at = args->at, .file_out = &file_index, .text = (char *)&under) &&
        (under == 0u))
    {
        TestsFile file;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_tests.cell, .offset = TESTS_FILE_AT(file_index), .dst = &file, .bytes = sizeof(file)});
        file.closed = !file.closed;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = s_tests.cell, .offset = TESTS_FILE_AT(file_index), .src = &file, .bytes = sizeof(file)});
    }
}

// the test `at` marked given to the run going, or not
EMBED_INLINE void tests_given_set(const TestsPaneCtx *args)
{
    TestsTest test;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_tests.cell, .offset = TESTS_TEST_AT(args->at), .dst = &test, .bytes = sizeof(test)});
    test.given = (int)args->file;
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_tests.cell, .offset = TESTS_TEST_AT(args->at), .src = &test, .bytes = sizeof(test)});
}

int cli_tests_pane_run(const CliTestsPaneCfg *args)
{
    cli_test_runs_stop();
    EMBED_ATOMIC_STORE(&s_running, 0u);
    if (!args->failed)
    {
        // the tree's tests are read again first: a file of tests written since the last reading runs too
        cli_tests_pane_read(args);
    }
    if (s_tests.cell == NULL)
    {
        return 0;
    }
    // the names passed to the run are in a copy of the text that lasts the start
    char *const text =
        g_cli_guard->temporary_buf_alloc((unsigned long long)s_tests.used + 1ULL, g_cli_tests_pane_owner);
    if (text == NULL)
    {
        return 0;
    }
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_tests.cell, .offset = TESTS_TEXT_AT(0u), .dst = text, .bytes = s_tests.used});
    text[s_tests.used] = '\0';
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < s_tests.tests; at += 1u)
    {
        EMBED_CALL(tests_given_set, TestsPaneCtx, .at = at, .file = 0u);
    }
    if (args->failed)
    {
        for (unsigned int at = 0u; (at < s_tests.tests) && (count < TESTS_SUPPLIED); at += 1u)
        {
            if (EMBED_ATOMIC_LOAD(TESTS_OUTCOME(at)) == CLI_TESTS_FAILED)
            {
                TestsTest test;
                (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                    .cell = s_tests.cell, .offset = TESTS_TEST_AT(at), .dst = &test, .bytes = sizeof(test)});
                s_supplied[count] = text + test.id;
                EMBED_CALL(tests_given_set, TestsPaneCtx, .at = at, .file = 1u);
                count += 1u;
            }
        }
        if (count == 0u)
        {
            g_cli_guard->persistent_buf_release(text, g_cli_tests_pane_owner);
            cli_notice_post("No test failed in the last run.", 0);
            return 0;
        }
    }
    else
    {
        for (unsigned int file_index = 0u; (file_index < s_tests.files) && (count < TESTS_SUPPLIED); file_index += 1u)
        {
            TestsFile file;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_tests.cell, .offset = TESTS_FILE_AT(file_index), .dst = &file, .bytes = sizeof(file)});
            const char *const path = text + file.path;
            // a run with coverage runs the Python files alone, cargo test recording none
            if (args->cover && EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = path))
            {
                continue;
            }
            s_supplied[count] = path;
            count += 1u;
            for (unsigned int at = file.first; at < file.first + file.count; at += 1u)
            {
                EMBED_CALL(tests_given_set, TestsPaneCtx, .at = at, .file = 1u);
            }
        }
        if (count == 0u)
        {
            g_cli_guard->persistent_buf_release(text, g_cli_tests_pane_owner);
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
    const int started = EMBED_CALL(cli_test_runs_root.start, CliTestRunsRootCfg, .root = args->root,
                                   .supplied = s_supplied, .count = count, .parallel = s_parallel, .cover = args->cover,
                                   .sink = sink, .number = &number, .err_msg = &err_msg);
    g_cli_guard->persistent_buf_release(text, g_cli_tests_pane_owner);
    if (!started)
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
    for (unsigned int at = 0u; (s_tests.cell != NULL) && (at < s_tests.tests); at += 1u)
    {
        if (EMBED_ATOMIC_LOAD(TESTS_OUTCOME(at)) == CLI_TESTS_FAILED)
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
    const char *const digits = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                          .node = member, .type = CFG_JSON_NUMBER)
                                   ? json->text + json->node[member].text
                                   : "";
    return EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = digits, .at = 0ULL,
                      .size = cellul.len(&(CatenaFinitaCfg){.src = digits, .cap = CLI_POOL_BYTES}), .end = NULL);
}

// a number member of `node` in `json`, written in seconds with a fraction, in milliseconds; 0 where it has none or
// is written with an exponent, which the run writes for a time under a microsecond
EMBED_INLINE unsigned int tests_milliseconds(const TestsPaneCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->node, .name = args->name);
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = member,
                    .type = CFG_JSON_NUMBER))
    {
        return 0u;
    }
    const char *const written = json->text + json->node[member].text;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = written, .cap = CLI_POOL_BYTES});
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

// the test whose name is `text` of `length` bytes: its place, or TESTS_TESTS where none has it
EMBED_INLINE unsigned int tests_by_id(const TestsPaneCtx *args)
{
    for (unsigned int at = 0u; at < s_tests.tests; at += 1u)
    {
        TestsTest test;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_tests.cell, .offset = TESTS_TEST_AT(at), .dst = &test, .bytes = sizeof(test)});
        if (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = s_tests.cell,
                                                                 .offset = TESTS_TEXT_AT(test.id),
                                                                 .src = args->text,
                                                                 .bytes = args->length + 1ULL}) == 0)
        {
            return at;
        }
    }
    return TESTS_TESTS;
}

// the file the test `at` is in: its place, 0 where none holds it
EMBED_INLINE unsigned int tests_file_holding(const TestsPaneCtx *args)
{
    for (unsigned int file_index = 0u; file_index < s_tests.files; file_index += 1u)
    {
        TestsFile file;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_tests.cell, .offset = TESTS_FILE_AT(file_index), .dst = &file, .bytes = sizeof(file)});
        if ((args->at >= file.first) && (args->at < file.first + file.count))
        {
            return file_index;
        }
    }
    return 0u;
}

// a run started: the tests passed to it running
EMBED_INLINE void tests_started(const TestsPaneCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const runner = EMBED_CALL(tests_string, TestsPaneCtx, .json = args->json, .node = args->node,
                                          .name = "runner", .file_out = (unsigned int *)&length);
    VerbaLinea line = {s_tests.runner, sizeof(s_tests.runner), 0ULL};
    mmgr_line_put_n(&line, runner, length);
    (void)mmgr_line_finish(&line);
    // a run's processes are few
    s_tests.workers = (unsigned int)EMBED_CALL(tests_integer, TestsPaneCtx, .json = args->json, .node = args->node,
                                               .name = "workers");
    for (unsigned int at = 0u; at < s_tests.tests; at += 1u)
    {
        TestsTest test;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_tests.cell, .offset = TESTS_TEST_AT(at), .dst = &test, .bytes = sizeof(test)});
        if (test.given)
        {
            EMBED_ATOMIC_STORE(TESTS_OUTCOME(at), CLI_TESTS_RUNNING);
        }
    }
}

// a result: its test's time, message and failing place set, then its outcome
EMBED_INLINE void tests_result(const TestsPaneCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int outcome = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->node, .name = "outcome");
    unsigned long long length = 0ULL;
    const char *const id = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = outcome, .name = "id",
                                      .file_out = (unsigned int *)&length);
    const unsigned int at = EMBED_CALL(tests_by_id, TestsPaneCtx, .text = id, .length = length);
    if (at == TESTS_TESTS)
    {
        return;
    }
    TestsTest test;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_tests.cell, .offset = TESTS_TEST_AT(at), .dst = &test, .bytes = sizeof(test)});
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
    test.milliseconds = EMBED_CALL(tests_milliseconds, TestsPaneCtx, .json = json, .node = outcome, .name = "seconds");
    const unsigned int line_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = outcome, .name = "line");
    test.fail_placed = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                  .node = line_node, .type = CFG_JSON_NUMBER);
    test.fail_line = EMBED_CALL(tests_integer, TestsPaneCtx, .json = json, .node = outcome, .name = "line");
    // a failure's message retains what the result's said where the details come after it with none
    unsigned long long message_length = 0ULL;
    const char *const message = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = outcome, .name = "message",
                                           .file_out = (unsigned int *)&message_length);
    unsigned int file_index = TESTS_FILES;
    if ((result == CLI_TESTS_FAILED) && (message_length != 0ULL) && (test.message_length == 0ULL))
    {
        unsigned long long first = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = message,
                                              .at = 0ULL, .size = message_length, .set = "\n");
        first -= ((first > 0ULL) && (message[first - 1ULL] == '\r')) ? 1ULL : 0ULL;
        if (first + 1ULL <= (unsigned long long)(TESTS_MESSAGES - s_tests.messages_used))
        {
            const char end = '\0';
            test.message = s_tests.messages_used;
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = s_tests.cell, .offset = TESTS_MESSAGE_AT(test.message), .src = message, .bytes = first});
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = s_tests.cell, .offset = TESTS_MESSAGE_AT(test.message + first), .src = &end, .bytes = 1ULL});
            // a message that fits the bytes is counted by an unsigned int
            s_tests.messages_used += (unsigned int)first + 1u;
            test.message_length = first;
            file_index = EMBED_CALL(tests_file_holding, TestsPaneCtx, .at = at);
        }
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_tests.cell, .offset = TESTS_TEST_AT(at), .src = &test, .bytes = sizeof(test)});
    if (file_index != TESTS_FILES)
    {
        EMBED_ATOMIC_STORE(TESTS_FILE_MESSAGES(file_index), EMBED_ATOMIC_LOAD(TESTS_FILE_MESSAGES(file_index)) + 1u);
    }
    EMBED_ATOMIC_STORE(TESTS_OUTCOME(at), result);
}

// the coverage: the files' lines that ran and were missed, published through their count
EMBED_INLINE void tests_coverage(const TestsPaneCtx *args)
{
    const CliJson *json = args->json;
    EMBED_ATOMIC_STORE(&s_tests.covers, 0u);
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->node, .name = "files");
    unsigned int count = 0u;
    for (unsigned int file = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                        .node = files, .type = CFG_JSON_ARRAY)
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
        TestsCover cover = {path, 0u, 0u, 0ULL};
        cover.ran = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = ran,
                               .type = CFG_JSON_ARRAY)
                        ? json->node[ran].count
                        : 0u;
        if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = missed,
                       .type = CFG_JSON_ARRAY))
        {
            cover.missed = json->node[missed].count;
            unsigned long long first = ~0ULL;
            for (unsigned int item = json->node[missed].first; item != CLI_JSON_NONE; item = json->node[item].next)
            {
                const char *const digits = json->text + json->node[item].text;
                const unsigned long long line = EMBED_CALL(
                    cli_text_at_size.number, CliTextAtSizeCfg, .text = digits, .at = 0ULL,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = digits, .cap = CLI_POOL_BYTES}), .end = NULL);
                first = (line < first) ? line : first;
            }
            cover.first_missed = (first != ~0ULL) ? first : 0ULL;
        }
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = s_tests.cell, .offset = TESTS_COVER_AT(count), .src = &cover, .bytes = sizeof(cover)});
        count += 1u;
    }
    EMBED_ATOMIC_STORE(&s_tests.covers, count);
}

// a run's end: its counts, its seconds in tenths and its message, no test left running
EMBED_INLINE void tests_done(const TestsPaneCtx *args)
{
    const CliJson *json = args->json;
    s_tests.passed = EMBED_CALL(tests_integer, TestsPaneCtx, .json = json, .node = args->node, .name = "passed");
    s_tests.failed = EMBED_CALL(tests_integer, TestsPaneCtx, .json = json, .node = args->node, .name = "failed");
    s_tests.skipped = EMBED_CALL(tests_integer, TestsPaneCtx, .json = json, .node = args->node, .name = "skipped");
    const unsigned int milliseconds =
        EMBED_CALL(tests_milliseconds, TestsPaneCtx, .json = json, .node = args->node, .name = "seconds");
    s_tests.tenths = (milliseconds + 50u) / 100u;
    unsigned long long length = 0ULL;
    const char *const message = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = args->node,
                                           .name = "message", .file_out = (unsigned int *)&length);
    VerbaLinea line = {s_tests.message, sizeof(s_tests.message), 0ULL};
    mmgr_line_put_n(&line, message, length);
    (void)mmgr_line_finish(&line);
    for (unsigned int at = 0u; at < s_tests.tests; at += 1u)
    {
        if (EMBED_ATOMIC_LOAD(TESTS_OUTCOME(at)) == CLI_TESTS_RUNNING)
        {
            EMBED_ATOMIC_STORE(TESTS_OUTCOME(at), CLI_TESTS_NONE);
        }
    }
    EMBED_ATOMIC_STORE(&s_ended, 1u);
    EMBED_ATOMIC_STORE(&s_running, 0u);
}

void cli_tests_pane_event(const CliEachJsonSizeCfg *args)
{
    if (s_tests.cell == NULL)
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
                                         .text_more = 0ULL, .err_msg = &ignored, .owner = g_cli_tests_pane_owner);
    unsigned long long length = 0ULL;
    const char *const type = EMBED_CALL(tests_string, TestsPaneCtx, .json = json, .node = root, .name = "type",
                                        .file_out = (unsigned int *)&length);
    if (cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "started", .cap = 16ULL}))
    {
        EMBED_CALL(tests_started, TestsPaneCtx, .json = json, .node = root);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "result", .cap = 16ULL}))
    {
        EMBED_CALL(tests_result, TestsPaneCtx, .json = json, .node = root);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "coverage", .cap = 16ULL}))
    {
        EMBED_CALL(tests_coverage, TestsPaneCtx, .json = json, .node = root);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "done", .cap = 16ULL}))
    {
        EMBED_CALL(tests_done, TestsPaneCtx, .json = json, .node = root);
    }
    g_cli_guard->persistent_buf_release(json->node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json->node));
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
    VerbaLinea line = {args->text, CLI_TESTS_SUMMARY, 0ULL};
    *args->failed_out = 0;
    if (s_tests.cell == NULL)
    {
        return 0ULL;
    }
    if (EMBED_ATOMIC_LOAD(&s_running) != 0u)
    {
        unsigned int done = 0u;
        for (unsigned int at = 0u; at < s_tests.tests; at += 1u)
        {
            TestsTest test;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_tests.cell, .offset = TESTS_TEST_AT(at), .dst = &test, .bytes = sizeof(test)});
            const unsigned int outcome = EMBED_ATOMIC_LOAD(TESTS_OUTCOME(at));
            done += (test.given && (outcome != CLI_TESTS_RUNNING) && (outcome != CLI_TESTS_NONE)) ? 1u : 0u;
        }
        mmgr_line_put(&line, "Running with ");
        mmgr_line_put(&line, s_tests.runner);
        mmgr_line_put(&line, " over ");
        mmgr_line_decimal(&line, s_tests.workers, 1u);
        mmgr_line_put(&line, (s_tests.workers == 1u) ? " process: " : " processes: ");
        mmgr_line_decimal(&line, done, 1u);
        mmgr_line_put(&line, " done.");
    }
    else if (EMBED_ATOMIC_LOAD(&s_ended) != 0u)
    {
        *args->failed_out = s_tests.failed > 0ULL;
        if ((s_tests.message[0] != '\0') && (s_tests.passed == 0ULL) && (s_tests.failed == 0ULL))
        {
            mmgr_line_put(&line, "No test ran: ");
            mmgr_line_put(&line, s_tests.message);
        }
        else
        {
            mmgr_line_decimal(&line, s_tests.passed, 1u);
            mmgr_line_put(&line, " passed, ");
            mmgr_line_decimal(&line, s_tests.failed, 1u);
            mmgr_line_put(&line, " failed");
            if (s_tests.skipped != 0ULL)
            {
                mmgr_line_put(&line, ", ");
                mmgr_line_decimal(&line, s_tests.skipped, 1u);
                mmgr_line_put(&line, " skipped");
            }
            mmgr_line_put(&line, " in ");
            mmgr_line_decimal(&line, s_tests.tenths / 10ULL, 1u);
            mmgr_line_ch(&line, '.');
            mmgr_line_decimal(&line, s_tests.tenths % 10ULL, 1u);
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
