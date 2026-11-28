// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_scripts.c: the hosts of the script plugins, with a plugin of the test's own in a fresh ORIOR_HOME. An ask starts
// the host and answers. The state that the plugin keeps comes back after a stop, and an unknown command fails. A read
// of a file outside reads, or of a right that the reader refused, fails. A read or a write that the reader allowed
// reads or writes the file. A read of a right with no answer waits on the question. The tree's files are those that
// reads names, and a refused reads refuses them. A whole path in the tree gives its path from the top, and a path
// outside gives null. A secret that the plugin keeps reads back, a secret that it forgets reads null, and a key that
// secrets does not list fails. A host past its memory ends, and the next ask starts it again. A quiet stops the host
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/plugins/plugins.h"
#include "ui/cli/src/scripts/scripts.h"
#include "ui/cli/src/text/json_tree.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// The plugin's code: a count that it keeps, and a growth past its memory. A read of a file that reads lists, and a
// read of a file that it does not list. A write of a file that writes lists. The tree's files in order, and the path
// in the tree of a whole path. A secret kept and then read, a secret forgotten and then read, and a secret of a key
// that secrets does not list. Each call answers what it got, or 'refused'
static const char s_code[] =
    "let count = orior.state?.count ?? 0;\n"
    "orior.commands.on('count', (value) => { count += Number(value ?? 1); return { count }; });\n"
    "const tried = async (call) => { try { return await call(); } catch (error) { return 'refused'; } };\n"
    "orior.commands.on('read', () => tried(() => orior.files.read('a.txt')));\n"
    "orior.commands.on('readUnlisted', () => tried(() => orior.files.read('b.txt')));\n"
    "orior.commands.on('write', () => tried(async () => { await orior.files.write('out/c.txt', 'made'); "
    "return 'written'; }));\n"
    "orior.commands.on('files', async () => (await orior.tree.files()).sort());\n"
    "orior.commands.on('relative', (path) => orior.tree.relative(path));\n"
    "orior.commands.on('keep', (value) => tried(async () => { await orior.secrets.write('token', value); "
    "return orior.secrets.read('token'); }));\n"
    "orior.commands.on('forget', () => tried(async () => { await orior.secrets.write('token', null); "
    "return orior.secrets.read('token'); }));\n"
    "orior.commands.on('keepUnlisted', () => tried(() => orior.secrets.write('other', 'kept')));\n"
    "orior.commands.on('grow', () => { const all = []; for (;;) { all.push(new Array(1 << 20).fill(all.length)); } "
    "});\n"
    "orior.keep(() => ({ count }));\n";
static const char s_manifest[] =
    "{\"id\": \"counter\", \"name\": \"Counter\", \"version\": \"1.0.0\", \"type\": \"script\", "
    "\"api\": 5, \"main\": \"plugin.js\", \"resources\": {\"memoryMiB\": 64}, \"reads\": [\"a.txt\"], "
    "\"writes\": [\"out/*.txt\"], \"secrets\": [\"token\"]}";
static char s_tree[CLI_PATH_MAX_LEN];
static int s_written;
static char s_answer[4096];
static char s_why[CLI_ERR_MSG_MAX_LEN];

static void file_written(const char *folder, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = name, .joined = path);
    for (char *at = path + CLI_UNITY_LENGTH(folder) + 1u; at[0] != '\0'; at += 1)
    {
        if (at[0] == '/')
        {
            at[0] = '\0';
            EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
            at[0] = '/';
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                                        .bytes = text, .size = CLI_UNITY_LENGTH(text)),
                             name);
}

// the plugin written once in a fresh ORIOR_HOME
static void counter_written(void)
{
    if (s_written)
    {
        return;
    }
    char home[CLI_PATH_MAX_LEN];
    cli_unity_folder("scripts_home", home);
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    file_written(home, "plugins/counter/plugin.json", s_manifest);
    file_written(home, "plugins/counter/plugin.js", s_code);
    // the test tells the plugins module of the change, as the folder watch tells it in the window
    cli_plugins_languages_changed();
    // the tree that the plugin's files calls read and write in
    cli_unity_folder("scripts_tree", s_tree);
    file_written(s_tree, "a.txt", "hello");
    file_written(s_tree, "b.txt", "unlisted");
    file_written(s_tree, "out/keep.txt", "");
    EMBED_CALL(cli_scripts_tree.set_tree, CliScriptsTreeCfg, .root = s_tree);
    s_written = 1;
}

// the reader's answer `answer` for the right `right` of the plugin, put in the store
static void counter_answered(unsigned int right, unsigned int answer)
{
    counter_written();
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_scripts_rights.answer_set, CliScriptsRightsCfg, .id = "counter", .right = right,
                                .answer = answer, .err_msg = &err_msg));
}

// The reader in the window, on a thread of its own: it waits for the question, then allows the right. The question's
// title goes into s_question
static char s_question[256];
static void reader_allows(void *context)
{
    (void)context;
    int asked = 0;
    unsigned int tries = 0u;
    while (!asked && (tries < 3000u))
    {
        VerbaLinea title = {s_question, sizeof(s_question), 0ULL};
        char body[512];
        VerbaLinea bodied = {body, sizeof(body), 0ULL};
        asked =
            EMBED_CALL(cli_scripts_question_table.question, CliScriptsQuestionCfg, .title = &title, .body = &bodied);
        (void)mmgr_line_finish(&title);
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = asked ? 0u : 10u);
        tries += 1u;
    }
    EMBED_CALL(cli_scripts_question_table.answered, CliScriptsQuestionCfg, .answer = CLI_SCRIPTS_ALLOWED);
}

// the command `name` asked of the plugin with the JSON `value`: 1 with its answer in s_answer, or 0 with why in s_why
static int counter_asked(const char *name, const char *value)
{
    counter_written();
    VerbaLinea answer = {s_answer, sizeof(s_answer), 0ULL};
    VerbaLinea why = {s_why, sizeof(s_why), 0ULL};
    const int asked =
        EMBED_CALL(cli_scripts.ask, CliScriptsCfg, .id = "counter", .source = CLI_SCRIPTS_COMMAND, .name = name,
                   .value = value, .length = CLI_UNITY_LENGTH(value), .answer = &answer, .err_msg = &why);
    (void)mmgr_line_finish(&answer);
    (void)mmgr_line_finish(&why);
    return asked;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_an_ask_starts_the_host_and_answers(void)
{
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("count", "3"), s_why);
    TEST_ASSERT_EQUAL_STRING("{\"count\":3}", s_answer);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_scripts.running, CliScriptsCfg, .id = "counter"));
}

void test_the_kept_state_comes_back_after_a_stop(void)
{
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("count", "1"), s_why);
    EMBED_CALL(cli_scripts.stop, CliScriptsCfg, .id = "counter");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_scripts.running, CliScriptsCfg, .id = "counter"));
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("count", "2"), s_why);
    TEST_ASSERT_EQUAL_STRING("{\"count\":6}", s_answer);
}

void test_a_read_of_a_file_that_reads_does_not_list_is_refused(void)
{
    counter_answered(CLI_SCRIPTS_READS, CLI_SCRIPTS_ALLOWED);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("readUnlisted", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"refused\"", s_answer);
}

void test_a_read_that_the_reader_refused_is_refused(void)
{
    counter_answered(CLI_SCRIPTS_READS, CLI_SCRIPTS_REFUSED);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("read", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"refused\"", s_answer);
}

void test_a_read_that_the_reader_allowed_reads_the_file(void)
{
    counter_answered(CLI_SCRIPTS_READS, CLI_SCRIPTS_ALLOWED);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("read", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"hello\"", s_answer);
}

void test_a_read_waits_on_the_question_and_gets_the_file_once_the_reader_allows_it(void)
{
    counter_answered(CLI_SCRIPTS_READS, CLI_SCRIPTS_UNASKED);
    CliThread reader = {reader_allows, NULL, 0u};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &reader));
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("read", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"hello\"", s_answer);
    TEST_ASSERT_EQUAL_STRING("counter reads files of the tree", s_question);
    TEST_ASSERT_EQUAL_UINT(CLI_SCRIPTS_ALLOWED, EMBED_CALL(cli_scripts_rights.answer, CliScriptsRightsCfg,
                                                           .id = "counter", .right = CLI_SCRIPTS_READS));
}

// The tree's checks come before the write, because the write adds a file to the tree
void test_the_files_of_the_tree_are_those_that_reads_names_from_its_top(void)
{
    counter_answered(CLI_SCRIPTS_READS, CLI_SCRIPTS_ALLOWED);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("files", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("[\"a.txt\"]", s_answer);
}

void test_the_files_of_the_tree_that_the_reader_refused_are_refused(void)
{
    counter_answered(CLI_SCRIPTS_READS, CLI_SCRIPTS_REFUSED);
    TEST_ASSERT_FALSE(counter_asked("files", "null"));
}

void test_a_whole_path_in_the_tree_gives_its_path_from_the_top(void)
{
    counter_written();
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "out/keep.txt", .joined = path);
    char value[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea quoted = {value, sizeof(value), 0ULL};
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &quoted,
               .text_bytes = (const unsigned char *)path, .length = CLI_UNITY_LENGTH(path));
    (void)mmgr_line_finish(&quoted);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("relative", value), s_why);
    TEST_ASSERT_EQUAL_STRING("\"out/keep.txt\"", s_answer);
}

void test_a_path_outside_the_tree_gives_null(void)
{
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("relative", "\"/elsewhere/x.txt\""), s_why);
    TEST_ASSERT_EQUAL_STRING("null", s_answer);
}

// The test ignores the check where the machine has no keychain. A secret that the test stores and forgets tells
static void keychain_required(void)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_err_msg_service_internal.keychain_store, CliErrMsgServiceCtx, .service = "plugin counter",
                    .account = "probe", .secret = "probe", .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no keychain on this machine");
    }
    (void)EMBED_CALL(cli_err_msg_service_internal.keychain_forget, CliErrMsgServiceCtx, .service = "plugin counter",
                     .account = "probe", .err_msg = &err_msg);
}

void test_a_secret_that_the_plugin_keeps_reads_back(void)
{
    keychain_required();
    counter_answered(CLI_SCRIPTS_SECRETS, CLI_SCRIPTS_ALLOWED);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("keep", "\"kept token\""), s_why);
    TEST_ASSERT_EQUAL_STRING("\"kept token\"", s_answer);
}

void test_a_secret_that_the_plugin_forgets_reads_null(void)
{
    keychain_required();
    counter_answered(CLI_SCRIPTS_SECRETS, CLI_SCRIPTS_ALLOWED);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("keep", "\"kept token\""), s_why);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("forget", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("null", s_answer);
}

void test_a_secret_of_a_key_that_secrets_does_not_list_is_refused(void)
{
    counter_answered(CLI_SCRIPTS_SECRETS, CLI_SCRIPTS_ALLOWED);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("keepUnlisted", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"refused\"", s_answer);
}

void test_a_write_that_the_reader_allowed_writes_the_file(void)
{
    counter_answered(CLI_SCRIPTS_WRITES, CLI_SCRIPTS_ALLOWED);
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("write", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"written\"", s_answer);
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "out/c.txt", .joined = path);
    unsigned long long size = 0ULL;
    const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                 .size_pointer = &size, .owner = g_cli_scripts_owner);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_EQUAL_UINT64(4ULL, size);
    TEST_ASSERT_EQUAL_MEMORY("made", text, 4u);
    g_platform_guard->persistent_buf_release((void *)(embed_word)text, g_cli_scripts_owner);
}

void test_an_unknown_command_fails(void)
{
    TEST_ASSERT_FALSE(counter_asked("none", "null"));
}

void test_a_host_past_its_memory_ends_and_the_next_ask_starts_it_again(void)
{
    TEST_ASSERT_FALSE(counter_asked("grow", "null"));
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("count", "1"), s_why);
    TEST_ASSERT_EQUAL_STRING("{\"count\":5}", s_answer);
}

void test_a_quiet_stops_the_host(void)
{
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("count", "0"), s_why);
    EMBED_CALL(cli_scripts.quiet, CliScriptsCfg, .now_ms = cli_clock_milliseconds() + CLI_SCRIPTS_QUIET_MS + 1ULL);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_scripts.running, CliScriptsCfg, .id = "counter"));
}
