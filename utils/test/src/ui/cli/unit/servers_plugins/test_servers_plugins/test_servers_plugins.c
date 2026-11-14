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
static const char s_are_in[] = "import json, sys\n"
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
                                 "        runs = {\"executeCommandProvider\": {\"commands\": [\"are.run\"]}}\n"
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
                                 "        ran = {\"title\": \"Run on the stand-in\", \"command\": \"are.run\", \"arguments\": [seen]}\n"
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
static char s_error[1024];
static VerbaLinea s_err_msg;
static char s_tree[CLI_PATH_MAX_LEN];
static char s_file[CLI_PATH_MAX_LEN];
static char s_actions[8192];
static int s_started;
static int s_opened;

void setUp(void)
{
    cli_unity_begun();
    s_event_count = 0u;
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
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
    EMBED_CALL(cli_servers_text_internal.plugin_folder_append, CliServersTextCtx, .written = &escaped, .text = s_text, .length = sizeof(s_text) - 1u, .folder = "C:\\a \"b\"", .escaped = 1);
    (void)mmgr_line_finish(&escaped);
    TEST_ASSERT_EQUAL_STRING("{\"at\":\"C:\\\\a \\\"b\\\"/x\"}", out);
}

void test_the_folder_mark_in_an_argument_is_the_folder_unchanged(void)
{
    char word[256];
    VerbaLinea plain = {word, sizeof(word), 0ull};
    EMBED_CALL(cli_servers_text_internal.plugin_folder_append, CliServersTextCtx, .written = &plain, .text = "{folder}/x", .length = 10ull, .folder = "C:\\a", .escaped = 0);
    (void)mmgr_line_finish(&plain);
    TEST_ASSERT_EQUAL_STRING("C:\\a/x", word);
}

