// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_test.c: the language servers' check: a path and its URI name each other, a server that ends is known to have
// ended with what it had open and why, a request the server sends is answered later while it is received on meanwhile,
// a request's answer is its message handed over, and a message is read by its length into a cell of the owner named
#include "../../../../../ui/cli/src/command_line/cells.h"
#include "../../../../../ui/cli/src/servers/lsp.h"
#include "../../../../../ui/cli/src/servers/lsp_internal.h"
#include "../../../../../ui/cli/src/tests/test_runs.h"
#include "cli_check.h"

// A language server in Python that answers initialize, writes on its errors each file it is given, and ends as
// orior/end requests
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
    "        send({\"jsonrpc\": \"2.0\", \"id\": message[\"id\"], \"result\": {\"capabilities\": {}}})\n"
    "    elif method == \"textDocument/didOpen\":\n"
    "        sys.stderr.write(\"given \" + message[\"params\"][\"textDocument\"][\"uri\"] + \"\\n\")\n"
    "        sys.stderr.flush()\n"
    "    elif method == \"orior/query\":\n"
    "        send({\"jsonrpc\": \"2.0\", \"id\": 7, \"method\": \"orior/question\", \"params\": {}})\n"
    "        send({\"jsonrpc\": \"2.0\", \"method\": \"orior/meanwhile\", \"params\": {}})\n"
    "    elif method is None and message.get(\"id\") == 7:\n"
    "        sys.stderr.write(\"answered \" + json.dumps(message.get(\"result\")) + \"\\n\")\n"
    "        sys.stderr.flush()\n"
    "    elif method == \"orior/echo\":\n"
    "        send({\"jsonrpc\": \"2.0\", \"id\": message[\"id\"], \"result\": message[\"params\"]})\n"
    "    elif method == \"orior/refuse\":\n"
    "        send({\"jsonrpc\": \"2.0\", \"id\": message[\"id\"], \"error\": {\"code\": 1, \"message\": "
    "\"refused\"}})\n"
    "    elif method == \"orior/end\":\n"
    "        sys.stderr.write(\"ending as requested\\n\")\n"
    "        sys.stderr.flush()\n"
    "        sys.exit(3)\n";

// a path and its URI name each other
static void lsp_uri(void)
{
#if defined(_WIN32)
    static const char s_path[] = "D:\\a b\\c#.cu";
#else
    static const char s_path[] = "/a b/c#.cu";
#endif
    char uri[CLI_PATH_MAX_LEN * 3u];
    cli_lsp_uri_of(s_path, uri, sizeof(uri));
    cli_check(
        (cellul.starts(&(CatenaFinitaCfg){.src = uri, .other = "file:///", .cap = CLI_CELLS_POOL})) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = uri, .cap = CLI_CELLS_POOL, .other = "a%20b/c%23.cu", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a path is written as a file URI, its letters escaped");
    char back[CLI_PATH_MAX_LEN];
    cli_check(cli_lsp_path_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), back),
              "the URI names a path");
    for (char *at = back; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    char wanted[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = wanted,
                            .src = s_path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_path, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = wanted; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = back, .other = wanted, .cap = CLI_CELLS_POOL}),
              "and the path is the one it was written from");
}

// what a server sends on its own let go
static void lsp_request(void *context, const char *method, const char *params, unsigned long long size,
                        VerbaLinea *answer)
{
    (void)context;
    (void)method;
    (void)params;
    (void)size;
    mmgr_line_put(answer, "null");
}

// the files a server had open, `path` and its text, each after the last
static void lsp_open_capture(void *context, const char *path, const char *text, unsigned long long length)
{
    VerbaLinea *const out = context;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        mmgr_line_ch(out, (at[0] == '\\') ? '/' : at[0]);
    }
    mmgr_line_ch(out, '|');
    mmgr_line_put_n(out, text, length);
}

// a server that ends is known to have ended with what it had open and why
static void lsp_ended(void)
{
    char python[CLI_PATH_MAX_LEN];
    if (!cli_test_python(python))
    {
        cli_check(1, "the machine has no Python, and no server runs");
        return;
    }
    char dir[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check_folder("lsp_stand_in", dir);
    cli_full(dir, "stand_in.py", script);
    (void)query_file_written(script, s_stand_in, sizeof(s_stand_in) - 1u);
    const char *const args[1] = {script};
    CliLsp *const server = cli_lsp_started(python, args, 1u, dir, "null", 4ull, NULL, 0u, lsp_request, NULL, &err_msg);
    cli_check((server != NULL) && cli_lsp_alive(server), "the server starts");
    if (server == NULL)
    {
        return;
    }
    cli_full(dir, "a.txt", file);
    const unsigned long long in_use_closed = embed_atomic_usize_load(&g_cli_lsp_files_cells.in_use);
    (void)cli_lsp_opened(server, file, "plaintext", "one\n", 4ull, &err_msg);
    const unsigned long long in_use_open = embed_atomic_usize_load(&g_cli_lsp_files_cells.in_use);
    (void)cli_lsp_notified(server, "orior/end", "null", 4ull, &err_msg);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (cli_lsp_alive(server) && (cli_clock_milliseconds() < until))
    {
        query_paused(50u);
    }
    cli_check(!cli_lsp_alive(server), "the server's end is known");
    char open_text[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea open_line = {open_text, sizeof(open_text), 0ull};
    cli_lsp_open_each(server, lsp_open_capture, &open_line);
    char wanted_text[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea wanted = {wanted_text, sizeof(wanted_text), 0ull};
    lsp_open_capture(&wanted, file, "one\n", 4ull);
    cli_check_text(open_text, open_line.at, (mmgr_line_finish(&wanted) > 0ull) ? wanted_text : "",
                   "what it had open is recorded");
    query_paused(200u);
    char complaint[CLI_LSP_COMPLAINT];
    VerbaLinea out = {complaint, sizeof(complaint), 0ull};
    cli_lsp_complaint(server, &out);
    cli_check_text(complaint, out.at, "ending as requested", "and the last thing it wrote");
    cli_lsp_unref(server);
    cli_check((in_use_open > in_use_closed) && (embed_atomic_usize_load(&g_cli_lsp_files_cells.in_use) < in_use_open),
              "a file open stores its URI in a cell, given back as the server is let go");
}

// The request a server sent that is answered later, as its id's JSON, and the notifications received after it
typedef struct
{
    char id[64];
    int deferred;
    embed_atomic_usize meanwhile;
} LspLater;

// the server's question recorded to be answered later, and what it sends after it counted
static void lsp_request_later(void *context, const char *method, const char *params, unsigned long long size,
                              VerbaLinea *answer)
{
    (void)params;
    (void)size;
    (void)answer;
    LspLater *const later = context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "orior/question", .cap = CLI_CELLS_POOL}))
    {
        later->deferred = cli_lsp_answer_later(later->id, sizeof(later->id));
    }
    (void)embed_atomic_usize_fetch_add(
        &later->meanwhile,
        (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "orior/meanwhile", .cap = CLI_CELLS_POOL})) ? 1u : 0u);
}

// a request the server sends is recorded, the server received on meanwhile, and answered later
static void lsp_answered_later(void)
{
    char python[CLI_PATH_MAX_LEN];
    if (!cli_test_python(python))
    {
        cli_check(1, "the machine has no Python, and no server runs");
        return;
    }
    char dir[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check_folder("lsp_later", dir);
    cli_full(dir, "stand_in.py", script);
    (void)query_file_written(script, s_stand_in, sizeof(s_stand_in) - 1u);
    const char *const args[1] = {script};
    static LspLater s_later;
    CliLsp *const server =
        cli_lsp_started(python, args, 1u, dir, "null", 4ull, NULL, 0u, lsp_request_later, &s_later, &err_msg);
    if (server == NULL)
    {
        cli_check(0, "the server starts");
        return;
    }
    (void)cli_lsp_notified(server, "orior/query", "null", 4ull, &err_msg);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while ((embed_atomic_usize_load(&s_later.meanwhile) == 0u) && (cli_clock_milliseconds() < until))
    {
        query_paused(20u);
    }
    cli_check(s_later.deferred && (embed_atomic_usize_load(&s_later.meanwhile) == 1u),
              "a request the server sends is left unanswered, and what the server sends after it is received");
    cli_check(cli_lsp_answer(server, s_later.id, "\"later\"", 7ull, &err_msg), "the request is answered later");
    char complaint[CLI_LSP_COMPLAINT];
    VerbaLinea out = {complaint, sizeof(complaint), 0ull};
    while ((out.at == 0ull) && (cli_clock_milliseconds() < until + 5000ull))
    {
        query_paused(20u);
        cli_lsp_complaint(server, &out);
    }
    cli_check_text(complaint, out.at, "answered \"later\"", "and the server is given the answer");
    cli_lsp_stopped(server);
    cli_lsp_unref(server);
}

// a request's answer is the message it came in handed over, its result ended by a 0 where it stands, and given back by
// the caller; a request refused hands nothing over
static void lsp_answer_handed(void)
{
    char python[CLI_PATH_MAX_LEN];
    if (!cli_test_python(python))
    {
        cli_check(1, "the machine has no Python, and no server runs");
        return;
    }
    char dir[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check_folder("lsp_handed", dir);
    cli_full(dir, "stand_in.py", script);
    (void)query_file_written(script, s_stand_in, sizeof(s_stand_in) - 1u);
    const char *const args[1] = {script};
    CliLsp *const server = cli_lsp_started(python, args, 1u, dir, "null", 4ull, NULL, 0u, lsp_request, NULL, &err_msg);
    if (server == NULL)
    {
        cli_check(0, "the server starts");
        return;
    }
    const unsigned long long before = embed_atomic_usize_load(&g_cli_servers_cells.in_use);
    CliLspAnswer answer;
    cli_check(cli_lsp_requested(server, "orior/echo", "[1,2]", 5ull, 5000ull, &answer, &err_msg),
              "a request is answered");
    cli_check((answer.message != NULL) && (answer.text > answer.message),
              "the result stands in the message handed over");
    cli_check_text(answer.text, answer.size, "[1, 2]", "and is the server's result");
    cli_check(answer.text[answer.size] == '\0', "ended by a 0");
    cli_check(embed_atomic_usize_load(&g_cli_servers_cells.in_use) > before,
              "the message's cell stays taken for the caller");
    g_cli_cells->persistent_buf_release(answer.message, &g_cli_servers_cells);
    cli_check(embed_atomic_usize_load(&g_cli_servers_cells.in_use) == before, "until the caller gives it back");
    cli_check(!cli_lsp_requested(server, "orior/refuse", "null", 4ull, 5000ull, &answer, &err_msg),
              "a request the server refuses is 0");
    cli_check(answer.message == NULL, "with nothing handed over");
    cli_check_text(why, err_msg.at, "refused", "and the server's reason given");
    cli_check(embed_atomic_usize_load(&g_cli_servers_cells.in_use) == before, "and the refusal's message given back");
    cli_lsp_stopped(server);
    cli_lsp_unref(server);
}

// A text a reader reads from, a piece at a time
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
} LspSource;

static unsigned long long lsp_read(void *source, char *piece, unsigned long long room)
{
    LspSource *const from = source;
    const unsigned long long left = from->size - from->at;
    const unsigned long long taken = (left < room) ? left : room;
    memor.cpy(&(MemoriaCfg){.dst = piece, .src = from->text + from->at, .bytes = taken});
    from->at += taken;
    return taken;
}

// a message is read by its length
static void lsp_framed(void)
{
    static const char s_framed[] = "Content-Length: 41\r\nContent-Type: application/vscode-jsonrpc\r\n\r\n"
                                   "{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":{\"x\":1}}Content-Length: 2\r\n\r\n{}";
    static LspSource s_source = {s_framed, sizeof(s_framed) - 1u, 0ull};
    static CliLspReader s_reader;
    s_reader.read = lsp_read;
    s_reader.source = &s_source;
    char body[256];
    unsigned long long size = 0ull;
    cli_check(cli_lsp_read_message(&s_reader, body, sizeof(body), &size), "a message is read");
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(body, size, &root);
    const unsigned int x = cli_json_member(json, cli_json_member(json, root, "result"), "x");
    cli_check(cli_json_is(json, x, CFG_JSON_NUMBER) && (json->text[json->node[x].text] == '1'),
              "its body is as long as it declares");
    cli_check(cli_lsp_read_message(&s_reader, body, sizeof(body), &size), "the next is read after it");
    cli_check_text(body, size, "{}", "whole");
    cli_check(!cli_lsp_read_message(&s_reader, body, sizeof(body), &size), "and none past the end");
}

// a message read into a cell counts to the owner it was read for, and to no other
static void lsp_message_owned(void)
{
    static const char s_framed[] = "Content-Length: 2\r\n\r\n{}";
    static LspSource s_source = {s_framed, sizeof(s_framed) - 1u, 0ull};
    static CliLspReader s_reader;
    s_reader.read = lsp_read;
    s_reader.source = &s_source;
    const unsigned long long debug_before = embed_atomic_usize_load(&g_cli_debug_cells.in_use);
    const unsigned long long servers_before = embed_atomic_usize_load(&g_cli_servers_cells.in_use);
    unsigned long long size = 0ull;
    char *const message = cli_lsp_message_taken(&s_reader, &size, &g_cli_debug_cells);
    cli_check(message != NULL, "a message is read into a cell");
    cli_check_text((message != NULL) ? message : "", size, "{}", "whole");
    cli_check((message != NULL) && (message[size] == '\0'), "with a 0 after it");
    cli_check(embed_atomic_usize_load(&g_cli_debug_cells.in_use) > debug_before, "the cell counts to the owner named");
    cli_check(embed_atomic_usize_load(&g_cli_servers_cells.in_use) == servers_before, "and to no other");
    g_cli_cells->persistent_buf_release(message, &g_cli_debug_cells);
    cli_check(embed_atomic_usize_load(&g_cli_debug_cells.in_use) == debug_before, "and is given back under that owner");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    lsp_uri();
    lsp_ended();
    lsp_answered_later();
    lsp_answer_handed();
    lsp_framed();
    lsp_message_owned();
    return cli_checked();
}
