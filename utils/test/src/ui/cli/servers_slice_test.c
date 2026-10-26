// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_slice_test.c: a file the tree's check gives its server in slices: cut in one walk into windows of a view's
// height, the first given as the file opened and each after as its next version, each stored with its map; what the
// server publishes of a window moved to the file's own lines, and what it publishes of a version given before the
// slice stored dropped
#include "../../../../../ui/cli/src/code/symbols.h"
#include "../../../../../ui/cli/src/servers/servers_internal.h"
#include "../../../../../ui/cli/src/tests/test_runs.h"
#include "cli_check.h"

// A language server in Python that answers initialize and shutdown and, for each text it is given, publishes as that
// version's diagnostics one problem on each line that names checked_missing
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
    "    elif method in (\"textDocument/didOpen\", \"textDocument/didChange\"):\n"
    "        params = message[\"params\"]\n"
    "        document = params[\"textDocument\"]\n"
    "        text = document[\"text\"] if \"text\" in document else params[\"contentChanges\"][0][\"text\"]\n"
    "        found = []\n"
    "        for at, line in enumerate(text.split(\"\\n\")):\n"
    "            place = line.find(\"checked_missing\")\n"
    "            if place >= 0:\n"
    "                found.append({\"range\": {\"start\": {\"line\": at, \"character\": place}, \"end\": {\"line\": at, "
    "\"character\": place + 15}}, \"severity\": 1, \"message\": \"call to undeclared function\"})\n"
    "        send({\"jsonrpc\": \"2.0\", \"method\": \"textDocument/publishDiagnostics\", \"params\": {\"uri\": "
    "document[\"uri\"], \"version\": document[\"version\"], \"diagnostics\": found}})\n"
    "    elif method == \"shutdown\":\n"
    "        send({\"jsonrpc\": \"2.0\", \"id\": message[\"id\"], \"result\": None})\n"
    "    elif method == \"exit\":\n"
    "        break\n";

// What the server published of the file, each moved to its lines: how many, the last, `dropped` where it was of an
// older slice, and its size, under its lock; and the file's path
typedef struct
{
    CliLock lock;
    unsigned int published;
    char last[4096];
    unsigned long long size;
    char path[CLI_PATH_MAX_LEN];
} SliceHeard;

static SliceHeard s_heard = {CLI_LOCK_INIT, 0u, "", 0ull, ""};

// what the server sends on its own: its diagnostics moved to the file's lines as the thread that reads it receives
// them
static void slice_received(void *context, const char *method, const char *params, unsigned long long size,
                           VerbaLinea *answer)
{
    (void)context;
    (void)answer;
    unsigned long long all_at = 0ull;
    unsigned long long all_length = 0ull;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "textDocument/publishDiagnostics", .cap = CLI_CELLS_POOL}) ||
        !cli_lsp_member(params, size, "diagnostics", &all_at, &all_length))
    {
        return;
    }
    unsigned long long mapped_size = 0ull;
    char *const mapped =
        cli_servers_slice_diagnostics(s_heard.path, params, size, params + all_at, all_length, &mapped_size);
    cli_lock_take(&s_heard.lock);
    VerbaLinea last = {s_heard.last, sizeof(s_heard.last), 0ull};
    // the diagnostics of one window are below 4 KiB here
    mmgr_line_put_n(&last, (mapped != NULL) ? mapped : "dropped", (mapped != NULL) ? (size_t)mapped_size : 7u);
    s_heard.size = last.at;
    s_heard.published += 1u;
    cli_lock_give(&s_heard.lock);
    g_cli_cells->persistent_buf_release(mapped, NULL);
}

// The windows given: the server, how many, and each window's lines and what the server published of it, one a line
typedef struct
{
    CliLsp *server;
    unsigned int windows;
    VerbaLinea *out;
} SliceGiven;

// a window given the server, the first as the file opened and each after as its next version, and what it publishes
// of it waited on and written as `first-last:diagnostics`
static int slice_window(void *context, const CliServersCut *cut, unsigned int first, unsigned int last)
{
    SliceGiven *const given = (SliceGiven *)context;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int sent = cli_servers_cut_sent(given->server, (given->windows == 0u) ? "c" : NULL, cut, &err_msg);
    given->windows += 1u;
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    int heard = 0;
    while (sent && !heard && (cli_clock_milliseconds() < until))
    {
        cli_lock_take(&s_heard.lock);
        heard = s_heard.published == given->windows;
        cli_lock_give(&s_heard.lock);
        query_paused(heard ? 0u : 10u);
    }
    mmgr_line_decimal(given->out, first, 1u);
    mmgr_line_ch(given->out, '-');
    mmgr_line_decimal(given->out, last, 1u);
    mmgr_line_ch(given->out, ':');
    cli_lock_take(&s_heard.lock);
    mmgr_line_put_n(given->out, heard ? s_heard.last : "none", heard ? (size_t)s_heard.size : 4u);
    cli_lock_give(&s_heard.lock);
    mmgr_line_ch(given->out, '\n');
    return sent && heard;
}

// `text` written as the file `name` under `root`, the folders above it made
static void slice_written(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    cli_full(root, name, path);
    if (cli_path_parent(path, folder))
    {
        (void)query_folder_made(folder);
    }
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the tree waited on until it is read
static void slice_read_waited(void)
{
    static char s_found[1u << 16u];
    for (unsigned int tries = 0u; tries < 200u; tries += 1u)
    {
        VerbaLinea out = {s_found, sizeof(s_found), 0ull};
        cli_symbols_find("", 1u, &out);
        unsigned int found = CLI_JSON_NONE;
        const CliJson *const json = cli_check_parsed(s_found, out.at, &found);
        if (!cli_json_is(json, cli_json_member(json, found, "reading"), CFG_JSON_TRUE))
        {
            return;
        }
        query_paused(10u);
    }
}

// a file of 141 lines whose one problem is on its line 100 given a server a window at a time
static void slice_check(void)
{
    char python[CLI_PATH_MAX_LEN];
    if (!cli_test_python(python))
    {
        cli_check(1, "the machine has no Python, and no server runs");
        return;
    }
    static const char *const s_paths[2] = {"src/checked_shape.h", "src/checked_use.c"};
    char root[CLI_PATH_MAX_LEN];
    cli_check_folder("servers_slice", root);
    static char s_use[8192];
    VerbaLinea use = {s_use, sizeof(s_use), 0ull};
    mmgr_line_put(&use, "#include \"checked_shape.h\"\nint checked_total(int n)\n{\n    int total = 0;\n");
    for (unsigned int line = 4u; line < 100u; line += 1u)
    {
        mmgr_line_put(&use, "    total += checked_area(");
        mmgr_line_decimal(&use, line, 1u);
        mmgr_line_put(&use, ");\n");
    }
    mmgr_line_put(&use, "    total += checked_missing(n);\n");
    for (unsigned int line = 101u; line < 139u; line += 1u)
    {
        mmgr_line_put(&use, "    total += ");
        mmgr_line_decimal(&use, line, 1u);
        mmgr_line_put(&use, ";\n");
    }
    mmgr_line_put(&use, "    return total;\n}\n");
    (void)mmgr_line_finish(&use);
    slice_written(root, "src/checked_shape.h", "int checked_area(int w);\n");
    slice_written(root, "src/checked_use.c", s_use);
    cli_symbols_forget();
    cli_symbols_refresh(root, s_paths, 2u);
    slice_read_waited();
    cli_full(root, "src/checked_use.c", s_heard.path);
    char script[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_full(root, "stand_in.py", script);
    (void)query_file_written(script, s_stand_in, sizeof(s_stand_in) - 1u);
    const char *const args[1] = {script};
    CliLsp *const server =
        cli_lsp_started(python, args, 1u, root, "null", 4ull, NULL, 0u, slice_received, NULL, &err_msg);
    cli_check(server != NULL, "the server starts");
    if (server == NULL)
    {
        return;
    }
    static char s_windows[1u << 14u];
    VerbaLinea windows = {s_windows, sizeof(s_windows), 0ull};
    SliceGiven given = {server, 0u, &windows};
    unsigned long long size = 0ull;
    const unsigned char *const mapped = cli_file_read(s_heard.path, &size);
    const int cut = (mapped != NULL) && cli_servers_slices_cut(root, s_heard.path, (const char *)mapped, size,
                                                               slice_window, &given, &err_msg);
    if (mapped != NULL)
    {
        cli_file_let_go(mapped, size);
    }
    cli_check(cut && (cli_lsp_version(server, s_heard.path) == 3ull),
              "the file is given its server a window at a time, opened with the first and changed with each after");
    cli_check_text(s_windows, windows.at,
                   "0-63:[]\n64-127:[{\"range\":{\"start\":{\"line\":100,\"character\":13},\"end\":{\"line\":100,"
                   "\"character\":28}},\"severity\":1,\"message\":\"call to undeclared function\"}]\n128-191:[]\n",
                   "and what the server publishes of each window stands on the file's own lines");
    static const char s_none[] = "[]";
    unsigned long long older_size = 0ull;
    unsigned long long newer_size = 0ull;
    char *const older = cli_servers_slice_diagnostics(s_heard.path, "{\"version\":2}", 13ull, s_none,
                                                      sizeof(s_none) - 1u, &older_size);
    char *const newer = cli_servers_slice_diagnostics(s_heard.path, "{\"version\":4}", 13ull, s_none,
                                                      sizeof(s_none) - 1u, &newer_size);
    cli_check((older == NULL) && (newer != NULL),
              "what it publishes of a version given before the slice stored is dropped, and of the same text given "
              "again moved by it");
    g_cli_cells->persistent_buf_release(older, NULL);
    g_cli_cells->persistent_buf_release(newer, NULL);
    (void)cli_lsp_closed(server, s_heard.path, &err_msg);
    cli_servers_slice_closed(s_heard.path);
    cli_check(!cli_servers_sliced(s_heard.path), "and the file's slice is let go with it");
    cli_lsp_stopped(server);
    cli_lsp_unref(server);
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    slice_check();
    return cli_checked();
}
