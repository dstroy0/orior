// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_plugins_test.c: a plugin's language server: started as its plugin.json names it, handed a file of its
// language as the editor opens it, its diagnostics joining the file's, a request it sends passed to the window and its
// answer given back, and the server stopped
#include "../../../../../ui/cli/src/servers/servers_internal.h"
#include "../../../../../ui/cli/src/tests/test_runs.h"
#include "cli_check.h"

// A language server in Python that answers initialize; for each file it is given, reports one thing is wrong in it and
// poses the editor a question; and reports the answer it is given back
static const char s_stand_in[] =
    "import json, sys\n"
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

// an event received recorded as `event json`
static void events_capture(void *context, const char *event, const char *json, unsigned long long size)
{
    (void)context;
    cli_lock_take(&s_events_lock);
    if (s_event_count < 32u)
    {
        VerbaLinea line = {s_events[s_event_count], sizeof(s_events[0]), 0ull};
        mmgr_line_put(&line, event);
        mmgr_line_ch(&line, ' ');
        mmgr_line_put_n(&line, json, size);
        (void)mmgr_line_finish(&line);
        s_event_count += 1u;
    }
    cli_lock_give(&s_events_lock);
}

// whether an event received contains both `one` and `other`, waited for up to five seconds
static int event_seen(const char *one, const char *other)
{
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (cli_clock_milliseconds() < until)
    {
        cli_lock_take(&s_events_lock);
        int found = 0;
        for (unsigned int at = 0u; (at < s_event_count) && !found; at += 1u)
        {
            found =
                (cellul.find(&(CatenaFinitaCfg){
                     .src = s_events[at], .cap = CLI_CELLS_POOL, .other = one, .other_cap = CLI_CELLS_POOL}) != NULL) &&
                (cellul.find(&(CatenaFinitaCfg){
                     .src = s_events[at], .cap = CLI_CELLS_POOL, .other = other, .other_cap = CLI_CELLS_POOL}) != NULL);
        }
        cli_lock_give(&s_events_lock);
        if (found)
        {
            return 1;
        }
        query_paused(20u);
    }
    return 0;
}

// whether `text` contains `part`
static int plugins_text_has(const char *text, const char *part)
{
    return cellul.find(&(CatenaFinitaCfg){
               .src = text, .cap = CLI_CELLS_POOL, .other = part, .other_cap = CLI_CELLS_POOL}) != NULL;
}

// the actions the plugin's server offers for the file at `file` of `tree`, given its diagnostics there, among the
// file's; one whose command the server runs run by it, and one whose command it does not passed to the plugin
static void plugins_actions(const char *tree, const char *file)
{
    static char s_actions[8192];
    VerbaLinea out = {s_actions, sizeof(s_actions), 0ull};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int listed = cli_servers_actions(file, (CliTextPlace){0u, 0u}, (CliTextPlace){0u, 1u}, &out, &err_msg);
    (void)mmgr_line_finish(&out);
    cli_check(listed && plugins_text_has(s_actions, "\"orior_plugin\":\"standin\"") &&
                  plugins_text_has(s_actions, "Run on the stand-in") &&
                  plugins_text_has(s_actions, "Notify the plugin"),
              "the actions a plugin's server offers are among the file's, each carrying the plugin's id");
    cli_check(plugins_text_has(s_actions, "\"arguments\":[1]"), "and it is queried with the diagnostics it gave there");
    static const char s_run[] = "{\"orior_plugin\":\"standin\",\"action\":{\"title\":\"Run on the stand-in\","
                                "\"command\":\"stand.run\",\"arguments\":[1]}}";
    char edits[256];
    VerbaLinea done = {edits, sizeof(edits), 0ull};
    cli_check(cli_servers_act(tree, file, s_run, sizeof(s_run) - 1u, &done, &err_msg) &&
                  event_seen("orior/ran", "stand.run"),
              "an action whose command the plugin's server runs is run by it");
    static const char s_notify_action[] = "{\"orior_plugin\":\"standin\",\"action\":{\"title\":\"Notify the plugin\","
                                          "\"command\":{\"title\":\"t\",\"command\":\"plugin.do\"}}}";
    done.at = 0ull;
    cli_check(cli_servers_act(tree, file, s_notify_action, sizeof(s_notify_action) - 1u, &done, &err_msg) &&
                  event_seen("\"method\":\"orior/command\"", "plugin.do"),
              "and one whose command it does not run is passed to the plugin");
}

// a plugin's server started as its plugin.json names it, given a file of its language, its diagnostics joining the
// file's, its request passed to the window and answered, and the server stopped
static void plugins_server(void)
{
    char python[CLI_PATH_MAX_LEN];
    if (!cli_test_python(python))
    {
        cli_check(1, "the machine has no Python, and no server runs");
        return;
    }
    char home[CLI_PATH_MAX_LEN];
    char tree[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char manifest[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    cli_check_folder("servers_plugins_home", home);
    cli_check_folder("servers_plugins_tree", tree);
    (void)query_environment_set("ORIOR_HOME", home);
    cli_full(home, "stand_in.py", script);
    (void)query_file_written(script, s_stand_in, sizeof(s_stand_in) - 1u);
    cli_full(home, "plugins/standin", folder);
    query_folder_made(folder);
    cli_full(folder, "plugin.json", manifest);
    char text[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea plugin = {text, sizeof(text), 0ull};
    mmgr_line_put(&plugin,
                  "{\"id\": \"standin\", \"name\": \"Stand-in\", \"version\": \"1.0.0\", \"kind\": \"script\", "
                  "\"server\": {\"program\": ");
    cli_json_quoted(&plugin, python, cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&plugin, ", \"args\": [");
    cli_json_quoted(&plugin, script, cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&plugin, "], \"languages\": [\"python\", \"*\"]}}");
    (void)query_file_written(manifest, text, plugin.at);
    const CliServersSink sink = {events_capture, NULL, ""};
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    static const char s_options[] = "{\"at\":\"{folder}/server\"}";
    // Python's own -B before the stand-in's file, which it echoes back as its flag that no bytecode is written
    static const char *const s_first[1] = {"-B"};
    cli_check(
        cli_servers_plugin_start(tree, "standin", s_options, sizeof(s_options) - 1u, s_first, 1u, &sink, &err_msg),
        "a plugin's server starts as its plugin.json names it");
    cli_check(event_seen("orior/options", "\"at\": \"/server\""),
              "it starts with the options its plugin gives, `{folder}` the folder its program was found in");
    cli_check(event_seen("orior/options", "\"unwritten\": 1") || event_seen("orior/options", "\"unwritten\": true"),
              "and the arguments its plugin puts before its plugin.json's");
    cli_full(tree, "a.txt", file);
    (void)query_file_written(file, "one\n", 4ull);
    char out[64];
    VerbaLinea opened = {out, sizeof(out), 0ull};
    (void)cli_servers_open(tree, file, "plaintext", "one\n", 4ull, &sink, &opened, &err_msg);
    cli_check(
        event_seen("lsp-diagnostics", "the stand-in finds this"),
        "a file of its languages, `*` every one, is handed to it as it opens, and its diagnostics join the file's");
    cli_check(event_seen("plugin-server", "\"method\":\"orior/question\"") &&
                  event_seen("plugin-server", "\"request\":9"),
              "what it requests of the editor goes to the window, its request pending");
    cli_check(cli_servers_plugin_answer("standin", "9", "\"yes\"", 5ull, &err_msg), "the plugin answers it");
    cli_check(event_seen("orior/answered", "\"result\": \"yes\""), "and the server is given the answer");
    plugins_actions(tree, file);
    char closed[64];
    VerbaLinea closing = {closed, sizeof(closed), 0ull};
    (void)cli_servers_close(file, &closing);
    cli_servers_plugin_stop("standin");
    cli_check(!cli_servers_plugin_notify("standin", "orior/after", "null", 4ull, &err_msg),
              "a plugin's server stopped is sent nothing");
}

// `{folder}` in a text written as the folder, escaped where the text is JSON and as it is where not
static void plugins_folder(void)
{
    static const char s_text[] = "{\"at\":\"{folder}/x\"}";
    char out[256];
    VerbaLinea escaped = {out, sizeof(out), 0ull};
    cli_servers_plugin_folder_put(&escaped, s_text, sizeof(s_text) - 1u, "C:\\a \"b\"", 1);
    (void)mmgr_line_finish(&escaped);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = out, .other = "{\"at\":\"C:\\\\a \\\"b\\\"/x\"}", .cap = 256u}),
              "`{folder}` in options is the folder escaped for a JSON string");
    char word[256];
    VerbaLinea plain = {word, sizeof(word), 0ull};
    cli_servers_plugin_folder_put(&plain, "{folder}/x", 10ull, "C:\\a", 0);
    (void)mmgr_line_finish(&plain);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "C:\\a/x", .cap = 256u}),
              "and in an argument the folder as it is");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    plugins_folder();
    plugins_server();
    return cli_checked();
}
