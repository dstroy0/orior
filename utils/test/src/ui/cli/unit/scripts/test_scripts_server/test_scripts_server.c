// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_scripts_server.c: the language server of a script plugin, with a plugin and a server of the test's own in a
// fresh ORIOR_HOME. The server is a short Node program. Its echo/params request has its params as its result. Its
// echo/request_client notification makes it send the client the request echo/client_request, and it then sends the
// result that it got in the notification echo/result. The plugin starts the server and sends it a request, and the
// request's result comes back. The plugin's handler gives echo/client_request its result, and the plugin keeps what
// echo/result carries
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/plugins/plugins.h"
#include "ui/cli/src/programs/executables.h"
#include "ui/cli/src/scripts/scripts.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the server: it reads the protocol's frames from its input, and it sends each response and notification that the head
// of this file gives
static const char s_server[] =
    "let held = Buffer.alloc(0); let next = 1000;\n"
    "const send = (m) => { const b = Buffer.from(JSON.stringify(m));\n"
    "  process.stdout.write(`Content-Length: ${b.length}\\r\\n\\r\\n`); process.stdout.write(b); };\n"
    "const got = (m) => {\n"
    "  if (m.method === 'initialize') { send({ jsonrpc: '2.0', id: m.id, result: { capabilities: {} } }); }\n"
    "  else if (m.method === 'shutdown') { send({ jsonrpc: '2.0', id: m.id, result: null }); }\n"
    "  else if (m.method === 'exit') { process.exit(0); }\n"
    "  else if (m.method === 'echo/params') { send({ jsonrpc: '2.0', id: m.id, result: m.params }); }\n"
    "  else if (m.method === 'echo/request_client') {\n"
    "    send({ jsonrpc: '2.0', id: next++, method: 'echo/client_request', params: { value: m.params } }); }\n"
    "  else if ((m.id !== undefined) && (m.method === undefined)) {\n"
    "    send({ jsonrpc: '2.0', method: 'echo/result', params: m.result }); }\n"
    "};\n"
    "process.stdin.on('data', (piece) => { held = Buffer.concat([held, piece]);\n"
    "  for (;;) { const head = held.indexOf('\\r\\n\\r\\n'); if (head < 0) { return; }\n"
    "    const length = Number(/Content-Length: (\\d+)/i.exec(held.slice(0, head).toString())[1]);\n"
    "    if (held.length < head + 4 + length) { return; }\n"
    "    const body = held.slice(head + 4, head + 4 + length).toString(); held = held.slice(head + 4 + length);\n"
    "    got(JSON.parse(body)); } });\n";
// the plugin: its server started, a request, a notification, the result that echo/result carried, and the server
// stopped
static const char s_code[] =
    "let result = null;\n"
    "orior.server.on('echo/client_request', (params) => ({ value: params.value }));\n"
    "orior.server.on('echo/result', (params) => { result = params; return null; });\n"
    "orior.commands.on('start', async () => { await orior.server.start({ option: 1 }, []); return 'started'; });\n"
    "orior.commands.on('params', (value) => orior.server.request('echo/params', value));\n"
    "orior.commands.on('request_client', async (value) => {\n"
    "  await orior.server.notify('echo/request_client', value); return 'notified'; });\n"
    "orior.commands.on('result', () => result);\n"
    "orior.commands.on('stop', async () => { await orior.server.stop(); return 'stopped'; });\n";
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

// The plugin and its server written once in a fresh ORIOR_HOME. Its plugin.json names Node by its whole path, and the
// server's program by its whole path in the tree
static void echoer_written(void)
{
    if (s_written)
    {
        return;
    }
    char home[CLI_PATH_MAX_LEN];
    char tree[CLI_PATH_MAX_LEN];
    char node[CLI_PATH_MAX_LEN];
    char server[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    cli_unity_folder("scripts_server_home", home);
    cli_unity_folder("scripts_server_tree", tree);
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "node",
                                        .name = "node", .path = node, .err_msg = &err_msg),
                             "node is found");
    file_written(tree, "echo_server.js", s_server);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = tree, .relative = "echo_server.js", .joined = server);
    static char s_manifest[4096];
    VerbaLinea manifest = {s_manifest, sizeof(s_manifest), 0ULL};
    mmgr_line_put(&manifest, "{\"id\": \"echoer\", \"name\": \"Echoer\", \"version\": \"1.0.0\", \"type\": \"script\", "
                             "\"api\": 5, \"main\": \"plugin.js\", \"server\": {\"program\": ");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &manifest,
               .text_bytes = (const unsigned char *)node, .length = CLI_UNITY_LENGTH(node));
    mmgr_line_put(&manifest, ", \"args\": [");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &manifest,
               .text_bytes = (const unsigned char *)server, .length = CLI_UNITY_LENGTH(server));
    mmgr_line_put(&manifest,
                  "], \"languages\": [\"plaintext\"], \"methods\": [\"echo/client_request\", \"echo/result\"]}}");
    TEST_ASSERT_TRUE(mmgr_line_finish(&manifest) != 0ULL);
    file_written(home, "plugins/echoer/plugin.json", s_manifest);
    file_written(home, "plugins/echoer/plugin.js", s_code);
    // the test calls the plugins module's change handler, as the folder watch calls it in the window
    cli_plugins_languages_changed();
    EMBED_CALL(cli_scripts_tree.set_tree, CliScriptsTreeCfg, .root = tree);
    s_written = 1;
}

// the plugin's command `name` run with the JSON `value`: 1 with its result in s_answer, or 0 with the error in s_why
static int echoer_command_run(const char *name, const char *value)
{
    echoer_written();
    VerbaLinea answer = {s_answer, sizeof(s_answer), 0ULL};
    VerbaLinea why = {s_why, sizeof(s_why), 0ULL};
    const int ran =
        EMBED_CALL(cli_scripts.ask, CliScriptsCfg, .id = "echoer", .source = CLI_SCRIPTS_COMMAND, .name = name,
                   .value = value, .length = CLI_UNITY_LENGTH(value), .answer = &answer, .err_msg = &why);
    (void)mmgr_line_finish(&answer);
    (void)mmgr_line_finish(&why);
    return ran;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_request_to_the_plugins_server_gets_a_response_with_its_result(void)
{
    TEST_ASSERT_TRUE_MESSAGE(echoer_command_run("start", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"started\"", s_answer);
    TEST_ASSERT_TRUE_MESSAGE(echoer_command_run("params", "{\"x\":1}"), s_why);
    TEST_ASSERT_EQUAL_STRING("{\"x\":1}", s_answer);
}

void test_the_plugin_sends_the_response_to_a_request_of_its_server(void)
{
    TEST_ASSERT_TRUE_MESSAGE(echoer_command_run("start", "null"), s_why);
    TEST_ASSERT_TRUE_MESSAGE(echoer_command_run("request_client", "7"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"notified\"", s_answer);
    unsigned int tries = 0u;
    int received = 0;
    while (!received && (tries < 100u))
    {
        TEST_ASSERT_TRUE_MESSAGE(echoer_command_run("result", "null"), s_why);
        received = cellul.eq(&(CatenaFinitaCfg){.src = s_answer, .other = "{\"value\":7}", .cap = sizeof(s_answer)});
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = received ? 0u : 50u);
        tries += 1u;
    }
    TEST_ASSERT_EQUAL_STRING("{\"value\":7}", s_answer);
}

void test_a_stop_ends_the_plugins_server(void)
{
    TEST_ASSERT_TRUE_MESSAGE(echoer_command_run("start", "null"), s_why);
    TEST_ASSERT_TRUE_MESSAGE(echoer_command_run("stop", "null"), s_why);
    TEST_ASSERT_EQUAL_STRING("\"stopped\"", s_answer);
    TEST_ASSERT_FALSE(echoer_command_run("params", "{\"x\":2}"));
}
