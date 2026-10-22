// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_test.c: the language servers' check: a path and its URI name each other, a server that ends is known to have
// ended with what it held and why, a request the server asks is answered later while it is heard on meanwhile, and a
// message is read by its length
#include "../../../../../ui/cli/src/servers/lsp.h"
#include "../../../../../ui/cli/src/servers/lsp_internal.h"
#include "../../../../../ui/cli/src/tests/test_runs.h"
#include "cli_check.h"

// A language server in Python that answers initialize, says on its errors each file it is given, and ends as
// orior/end asks
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
    "    elif method == \"orior/ask\":\n"
    "        send({\"jsonrpc\": \"2.0\", \"id\": 7, \"method\": \"orior/question\", \"params\": {}})\n"
    "        send({\"jsonrpc\": \"2.0\", \"method\": \"orior/meanwhile\", \"params\": {}})\n"
    "    elif method is None and message.get(\"id\") == 7:\n"
    "        sys.stderr.write(\"answered \" + json.dumps(message.get(\"result\")) + \"\\n\")\n"
    "        sys.stderr.flush()\n"
    "    elif method == \"orior/end\":\n"
    "        sys.stderr.write(\"ending as asked\\n\")\n"
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
    char uri[CLI_PATH_LONGEST * 3u];
    cli_lsp_uri_of(s_path, uri, sizeof(uri));
    cli_check(
        (cellul.starts(&(CatenaFinitaCfg){.src = uri, .other = "file:///", .cap = CLI_CELLS_POOL})) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = uri, .cap = CLI_CELLS_POOL, .other = "a%20b/c%23.cu", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a path is written as a file URI, its letters escaped");
    char back[CLI_PATH_LONGEST];
    cli_check(cli_lsp_path_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), back),
              "the URI names a path");
    for (char *at = back; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    char wanted[CLI_PATH_LONGEST];
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

// what a server says on its own let go
static void lsp_heard(void *context, const char *method, const char *params, unsigned long long size, CliLine *answer)
{
    (void)context;
    (void)method;
    (void)params;
    (void)size;
    cli_line_text(answer, "null");
}

// the files a server kept, `path` and its text, each after the last
static void lsp_held(void *context, const char *path, const char *text, unsigned long long length)
{
    CliLine *const held = context;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        cli_line_character(held, (at[0] == '\\') ? '/' : at[0]);
    }
    cli_line_character(held, '|');
    cli_line_counted(held, text, length);
}

// a server that ends is known to have ended with what it kept and why
static void lsp_ended(void)
{
    char python[CLI_PATH_LONGEST];
    if (!cli_test_python(python))
    {
        cli_check(1, "the machine has no Python, and no server runs");
        return;
    }
    char dir[CLI_PATH_LONGEST];
    char script[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("lsp_stand_in", dir);
    cli_full(dir, "stand_in.py", script);
    (void)query_file_written(script, s_stand_in, sizeof(s_stand_in) - 1u);
    const char *const args[1] = {script};
    CliLsp *const server = cli_lsp_started(python, args, 1u, dir, "null", 4ull, NULL, 0u, lsp_heard, NULL, &said);
    cli_check((server != NULL) && cli_lsp_alive(server), "the server starts");
    if (server == NULL)
    {
        return;
    }
    cli_full(dir, "a.txt", file);
    const unsigned long long closed_held = g_cli_lsp_files_cells.holds;
    (void)cli_lsp_opened(server, file, "plaintext", "one\n", 4ull, &said);
    const unsigned long long open_held = g_cli_lsp_files_cells.holds;
    (void)cli_lsp_notified(server, "orior/end", "null", 4ull, &said);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (cli_lsp_alive(server) && (cli_clock_milliseconds() < until))
    {
        query_paused(50u);
    }
    cli_check(!cli_lsp_alive(server), "the server's end is known");
    char held_text[CLI_PATH_LONGEST + 64u];
    CliLine held = {held_text, sizeof(held_text), 0ull};
    cli_lsp_held_each(server, lsp_held, &held);
    char wanted_text[CLI_PATH_LONGEST + 64u];
    CliLine wanted = {wanted_text, sizeof(wanted_text), 0ull};
    lsp_held(&wanted, file, "one\n", 4ull);
    cli_check_text(held_text, held.at, (cli_line_closed(&wanted) > 0ull) ? wanted_text : "", "what it held is kept");
    query_paused(200u);
    char complaint[CLI_LSP_COMPLAINT];
    CliLine out = {complaint, sizeof(complaint), 0ull};
    cli_lsp_complaint(server, &out);
    cli_check_text(complaint, out.at, "ending as asked", "and the last thing it said");
    cli_lsp_given(server);
    cli_check((open_held > closed_held) && (g_cli_lsp_files_cells.holds < open_held),
              "a file open holds its URI in a cell, given back as the server is let go");
}

// The request a server asked that is answered later, as its id's JSON, and the notifications heard after it
typedef struct
{
    char id[64];
    int held;
    int meanwhile;
} LspLater;

// the server's question kept to be answered later, and what it says after it counted
static void lsp_heard_later(void *context, const char *method, const char *params, unsigned long long size,
                            CliLine *answer)
{
    (void)params;
    (void)size;
    (void)answer;
    LspLater *const later = context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "orior/question", .cap = CLI_CELLS_POOL}))
    {
        later->held = cli_lsp_answer_later(later->id, sizeof(later->id));
    }
    later->meanwhile +=
        (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "orior/meanwhile", .cap = CLI_CELLS_POOL})) ? 1 : 0;
}

// a request the server asks is kept, the server heard on meanwhile, and answered later
static void lsp_answered_later(void)
{
    char python[CLI_PATH_LONGEST];
    if (!cli_test_python(python))
    {
        cli_check(1, "the machine has no Python, and no server runs");
        return;
    }
    char dir[CLI_PATH_LONGEST];
    char script[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("lsp_later", dir);
    cli_full(dir, "stand_in.py", script);
    (void)query_file_written(script, s_stand_in, sizeof(s_stand_in) - 1u);
    const char *const args[1] = {script};
    static LspLater s_later;
    CliLsp *const server =
        cli_lsp_started(python, args, 1u, dir, "null", 4ull, NULL, 0u, lsp_heard_later, &s_later, &said);
    if (server == NULL)
    {
        cli_check(0, "the server starts");
        return;
    }
    (void)cli_lsp_notified(server, "orior/ask", "null", 4ull, &said);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while ((s_later.meanwhile == 0) && (cli_clock_milliseconds() < until))
    {
        query_paused(20u);
    }
    cli_check(s_later.held && (s_later.meanwhile == 1),
              "a request the server asks is held unanswered, and what the server says after it is heard");
    cli_check(cli_lsp_answer(server, s_later.id, "\"later\"", 7ull, &said), "the request is answered later");
    char complaint[CLI_LSP_COMPLAINT];
    CliLine out = {complaint, sizeof(complaint), 0ull};
    while ((out.at == 0ull) && (cli_clock_milliseconds() < until + 5000ull))
    {
        query_paused(20u);
        cli_lsp_complaint(server, &out);
    }
    cli_check_text(complaint, out.at, "answered \"later\"", "and the server is given the answer");
    cli_lsp_stopped(server);
    cli_lsp_given(server);
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
              "its body is as long as it says");
    cli_check(cli_lsp_read_message(&s_reader, body, sizeof(body), &size), "the next is read after it");
    cli_check_text(body, size, "{}", "whole");
    cli_check(!cli_lsp_read_message(&s_reader, body, sizeof(body), &size), "and none past the end");
}

int main(void)
{
    lsp_uri();
    lsp_ended();
    lsp_answered_later();
    lsp_framed();
    return cli_checked();
}
