// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_scripts.c: the hosts of the script plugins, with a plugin of the test's own in a fresh ORIOR_HOME. An ask starts
// the host and answers. The state that the plugin keeps comes back after a stop. A call of `orior.*` that the window
// does not answer is refused, and an unknown command fails. A host past its memory ends, and the next ask starts it
// again. A quiet stops the host
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/plugins/plugins.h"
#include "ui/cli/src/scripts/scripts.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the plugin's code: a count that it keeps, a read that the window refuses, and a growth past its memory
static const char s_code[] =
    "let count = orior.state?.count ?? 0;\n"
    "orior.commands.on('count', (value) => { count += Number(value ?? 1); return { count }; });\n"
    "orior.commands.on('read', async () => { try { await orior.files.read('a.txt'); return 'read'; }\n"
    "  catch (error) { return 'refused'; } });\n"
    "orior.commands.on('grow', () => { const all = []; for (;;) { all.push(new Array(1 << 20).fill(all.length)); } "
    "});\n"
    "orior.keep(() => ({ count }));\n";
static const char s_manifest[] =
    "{\"id\": \"counter\", \"name\": \"Counter\", \"version\": \"1.0.0\", \"type\": \"script\", "
    "\"api\": 5, \"main\": \"plugin.js\", \"resources\": {\"memoryMiB\": 64}}";
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
    s_written = 1;
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

void test_a_call_the_window_does_not_answer_is_refused(void)
{
    TEST_ASSERT_TRUE_MESSAGE(counter_asked("read", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"refused\"", s_answer);
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
