// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_servers_plugins.c: a plugin's language server is started as its plugin.json names it, sent a file of its language as the editor
// opens it, its diagnostics join the file's, a request it sends goes to the window and the answer returns, and it stops
#include "ui/cli/src/servers/servers_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/tests/test_runs.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// A language server in Python that answers initialize; for each file it is sent, reports one thing wrong in it and asks the editor a
// question; and reports the answer it receives
static const char s_stand_in[] = "import json, sys\n"
                                 "def read():\n"
                                 "    length = 0\n"
                                 "    while True:\n"
                                 "        line = sys.stdin.buffer.readline()\n"
                                 "        if not line:\n"
                                 "            return None\n"
                                 "        line = line.strip()\n"
                                 "        if not line:\n"
                                 "            break\n"
                                 "        name, _, value = line.decode().partition(\":\")\n"
                                 "        if name.lower() == \"content-length\":\n"
                                 "            length = int(value)\n"
                                 "    return json.loads(sys.stdin.buffer.read(length))\n"
                                 "def send(message):\n"
                                 "    data = json.dumps(message).encode()\n"
                                 "    sys.stdout.buffer.write(b\"Content-Length: %d\\r\\n\\r\\n\" % len(data) + data)\n"
                                 "    sys.stdout.buffer.flush()\n"
                                 "while True:\n"
                                 "    message = read()\n"
                                 "    if message is None:\n"
                                 "        break\n"
                                 "    method = message.get(\"method\")\n"
                                 "    if method == \"initialize\":\n"
                                 "        runs = {\"executeCommandProvider\": {\"commands\": [\"stand.run\"]}}\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"id\": message[\"id\"], \"result\": {\"capabilities\": runs}})\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"method\": \"orior/options\",\n"
                                 "              \"params\": {\"given\": message[\"params\"][\"initializationOptions\"],\n"
                                 "                         \"unwritten\": sys.flags.dont_write_bytecode}})\n"
                                 "    elif method == \"textDocument/didOpen\":\n"
                                 "        uri = message[\"params\"][\"textDocument\"][\"uri\"]\n"
                                 "        place = {\"start\": {\"line\": 0, \"character\": 0}, \"end\": {\"line\": 0, \"character\": 3}}\n"
                                 "        found = {\"range\": place, \"severity\": 2, \"message\": \"the stand-in finds this\"}\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"method\": \"textDocument/publishDiagnostics\",\n"
                                 "              \"params\": {\"uri\": uri, \"diagnostics\": [found]}})\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"id\": 9, \"method\": \"orior/question\", \"params\": {\"uri\": uri}})\n"
                                 "    elif method == \"textDocument/codeAction\":\n"
                                 "        seen = len(message[\"params\"][\"context\"][\"diagnostics\"])\n"
                                 "        ran = {\"title\": \"Run on the stand-in\", \"command\": \"stand.run\", \"arguments\": [seen]}\n"
                                 "        notify = {\"title\": \"Notify the plugin\", \"command\": {\"title\": \"t\", \"command\": \"plugin.do\"}}\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"id\": message[\"id\"], \"result\": [ran, notify]})\n"
                                 "    elif method == \"workspace/executeCommand\":\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"id\": message[\"id\"], \"result\": None})\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"method\": \"orior/ran\", \"params\": message[\"params\"]})\n"
                                 "    elif method is None and message.get(\"id\") == 9:\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"method\": \"orior/answered\", \"params\": {\"result\": "
                                 "message.get(\"result\")}})\n";

// the events received, each `event json`, and how many
static char s_events[32][4096];
static unsigned int s_event_count;
static CliLock s_events_lock = CLI_LOCK_INIT;
static char s_why[1024];
static VerbaLinea s_err_msg;
static char s_tree[CLI_PATH_MAX_LEN];
static char s_file[CLI_PATH_MAX_LEN];
static char s_actions[8192];
static int s_started;
static int s_opened;

// an event received recorded as `event json`
static void event_recorded(const CliEachEventJsonSizeCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_events_lock);
    if (s_event_count < 32u)
    {
        VerbaLinea line = {s_events[s_event_count], sizeof(s_events[0]), 0ull};
        mmgr_line_put(&line, args->event);
        mmgr_line_ch(&line, ' ');
        mmgr_line_put_n(&line, args->json, args->size);
        (void)mmgr_line_finish(&line);
        s_event_count += 1u;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_events_lock);
}

// whether an event received contains both `one` and `other`, waited for up to five seconds
static int event_seen(const char *one, const char *other)
{
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (cli_clock_milliseconds() < until)
    {
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_events_lock);
        int found = 0;
        for (unsigned int at = 0u; (at < s_event_count) && !found; at += 1u)
        {
            found = (cellul.find(&(CatenaFinitaCfg){.src = s_events[at], .cap = CLI_CELLS_POOL, .other = one, .other_cap = CLI_CELLS_POOL}) != NULL) &&
                    (cellul.find(&(CatenaFinitaCfg){.src = s_events[at], .cap = CLI_CELLS_POOL, .other = other, .other_cap = CLI_CELLS_POOL}) != NULL);
        }
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_events_lock);
        if (found)
        {
            return 1;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    }
    return 0;
}

// the plugin's server started in a home of its own, the test ignored where the machine has no Python
static void server_started(void)
{
    static const char s_options[] = "{\"at\":\"{folder}/server\"}";
    // Python's own -B before the stand-in's file, which it echoes back as its flag that no bytecode is written
    static const char *const s_first[1] = {"-B"};
    char python[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        TEST_IGNORE_MESSAGE("the machine has no Python");
    }
    char home[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char manifest[CLI_PATH_MAX_LEN];
    cli_unity_folder("servers_plugins_home", home);
    cli_unity_folder("servers_plugins_tree", s_tree);
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home));
    cli_unity_file(home, "stand_in.py", s_stand_in);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "stand_in.py", .joined = script);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "plugins/standin", .joined = folder);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "plugin.json", .joined = manifest);
    char text[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea plugin = {text, sizeof(text), 0ull};
    mmgr_line_put(&plugin, "{\"id\": \"standin\", \"name\": \"Stand-in\", \"version\": \"1.0.0\", \"kind\": \"script\", \"server\": {\"program\": ");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &plugin, .text = python, .length = CLI_UNITY_LENGTH(python));
    mmgr_line_put(&plugin, ", \"args\": [");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &plugin, .text = script, .length = CLI_UNITY_LENGTH(script));
    mmgr_line_put(&plugin, "], \"languages\": [\"python\", \"*\"]}}");
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = manifest, .bytes = text, .size = plugin.at));
    const CliServersSink sink = {event_recorded, NULL, ""};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_root_sink.plugin_start, CliServersRootSinkCfg, .root = s_tree, .id = "standin", .options = s_options, .length = sizeof(s_options) - 1u, .first_letters = s_first, .firsts = 1u, .sink = &sink, .err_msg = &s_err_msg));
    s_started = 1;
}

// a plain file opened with the server running
static void file_open(void)
{
    char out[64];
    server_started();
    cli_unity_file(s_tree, "a.txt", "one\n");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "a.txt", .joined = s_file);
    VerbaLinea opened = {out, sizeof(out), 0ull};
    const CliServersSink sink = {event_recorded, NULL, ""};
    (void)EMBED_CALL(cli_servers_root.open, CliServersRootCfg, .root = s_tree, .path = s_file, .language = "plaintext", .text = "one\n", .size = 4ull, .sink = &sink, .out = &opened, .err_msg = &s_err_msg);
    s_opened = 1;
}

// the actions offered on the opened file, its diagnostics received first, into s_actions
static void actions_listed(void)
{
    file_open();
    TEST_ASSERT_TRUE(event_seen("lsp-diagnostics", "the stand-in finds this"));
    VerbaLinea out = {s_actions, sizeof(s_actions), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_path_out_err_msg_from.actions, CliServersPathOutErrMsgFromCfg, .path = s_file, .from = (CliTextPlace){0u, 0u}, .to = (CliTextPlace){0u, 1u}, .out = &out, .err_msg = &s_err_msg));
    (void)mmgr_line_finish(&out);
}

void setUp(void)
{
    cli_unity_begun();
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_events_lock);
    s_event_count = 0u;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_events_lock);
    s_err_msg = (VerbaLinea){s_why, sizeof(s_why), 0ull};
    s_started = 0;
    s_opened = 0;
}

void tearDown(void)
{
    if (s_opened)
    {
        char closed[64];
        VerbaLinea closing = {closed, sizeof(closed), 0ull};
        (void)EMBED_CALL(cli_servers_path_out_err_msg_from_to.close, CliServersPathOutErrMsgFromToCfg, .path = s_file, .err_msg = &closing);
    }
    if (s_started)
    {
        EMBED_CALL(cli_servers_root_sink.plugin_stop, CliServersRootSinkCfg, .id = "standin");
    }
}

void test_the_folder_mark_in_options_is_the_folder_escaped_for_json(void)
{
    static const char s_text[] = "{\"at\":\"{folder}/x\"}";
    char out[256];
    VerbaLinea escaped = {out, sizeof(out), 0ull};
    EMBED_CALL(cli_servers_text_internal.plugin_folder_put, CliServersTextCtx, .written = &escaped, .text = s_text, .length = sizeof(s_text) - 1u, .folder = "C:\\a \"b\"", .escaped = 1);
    (void)mmgr_line_finish(&escaped);
    TEST_ASSERT_EQUAL_STRING("{\"at\":\"C:\\\\a \\\"b\\\"/x\"}", out);
}

void test_the_folder_mark_in_an_argument_is_the_folder_unchanged(void)
{
    char word[256];
    VerbaLinea plain = {word, sizeof(word), 0ull};
    EMBED_CALL(cli_servers_text_internal.plugin_folder_put, CliServersTextCtx, .written = &plain, .text = "{folder}/x", .length = 10ull, .folder = "C:\\a", .escaped = 0);
    (void)mmgr_line_finish(&plain);
    TEST_ASSERT_EQUAL_STRING("C:\\a/x", word);
}

void test_a_plugins_server_starts_with_the_options_its_plugin_gives(void)
{
    server_started();
    TEST_ASSERT_TRUE(event_seen("orior/options", "\"at\": \"/server\""));
}

void test_a_plugins_server_starts_with_the_arguments_its_plugin_gives(void)
{
    server_started();
    TEST_ASSERT_TRUE(event_seen("orior/options", "\"unwritten\": 1") || event_seen("orior/options", "\"unwritten\": true"));
}

void test_a_file_of_its_languages_is_sent_and_its_diagnostics_join_the_files(void)
{
    file_open();
    TEST_ASSERT_TRUE(event_seen("lsp-diagnostics", "the stand-in finds this"));
}

void test_a_request_to_the_editor_goes_to_the_window_pending(void)
{
    file_open();
    TEST_ASSERT_TRUE(event_seen("plugin-server", "\"method\":\"orior/question\""));
    TEST_ASSERT_TRUE(event_seen("plugin-server", "\"request\":9"));
}

void test_the_server_receives_the_plugins_answer(void)
{
    file_open();
    TEST_ASSERT_TRUE(event_seen("plugin-server", "\"request\":9"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_err_msg_id.plugin_answer, CliServersErrMsgIdCfg, .id = "standin", .request = "9", .result_text = "\"yes\"", .length = 5ull, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(event_seen("orior/answered", "\"result\": \"yes\""));
}

void test_the_servers_actions_are_among_the_files_each_with_the_plugins_id(void)
{
    actions_listed();
    CLI_TEST_ASSERT_CONTAINS("\"orior_plugin\":\"standin\"", s_actions);
    CLI_TEST_ASSERT_CONTAINS("Run on the stand-in", s_actions);
    CLI_TEST_ASSERT_CONTAINS("Notify the plugin", s_actions);
}

void test_the_server_is_queried_with_the_diagnostics_it_gave_there(void)
{
    actions_listed();
    CLI_TEST_ASSERT_CONTAINS("\"arguments\":[1]", s_actions);
}

void test_an_action_whose_command_the_server_runs_is_run_by_it(void)
{
    static const char s_run[] = "{\"orior_plugin\":\"standin\",\"action\":{\"title\":\"Run on the stand-in\",\"command\":\"stand.run\",\"arguments\":[1]}}";
    char edits[256];
    actions_listed();
    VerbaLinea done = {edits, sizeof(edits), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_path_size.act, CliServersPathSizeCfg, .tree = s_tree, .path = s_file, .raw = s_run, .size = sizeof(s_run) - 1u, .out = &done, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(event_seen("orior/ran", "stand.run"));
}

void test_an_action_whose_command_it_does_not_run_goes_to_the_plugin(void)
{
    static const char s_notify[] = "{\"orior_plugin\":\"standin\",\"action\":{\"title\":\"Notify the plugin\",\"command\":{\"title\":\"t\",\"command\":\"plugin.do\"}}}";
    char edits[256];
    actions_listed();
    VerbaLinea done = {edits, sizeof(edits), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_path_size.act, CliServersPathSizeCfg, .tree = s_tree, .path = s_file, .raw = s_notify, .size = sizeof(s_notify) - 1u, .out = &done, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(event_seen("\"method\":\"orior/command\"", "plugin.do"));
}

void test_a_stopped_plugin_server_is_sent_nothing(void)
{
    server_started();
    EMBED_CALL(cli_servers_root_sink.plugin_stop, CliServersRootSinkCfg, .id = "standin");
    s_started = 0;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_servers_err_msg.plugin_notify, CliServersErrMsgCfg, .id = "standin", .method = "orior/after", .params = "null", .length = 4ull, .err_msg = &s_err_msg));
}
