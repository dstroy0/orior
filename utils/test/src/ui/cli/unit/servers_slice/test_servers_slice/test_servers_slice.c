// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_servers_slice.c: a file the tree check sends its server in slices: cut in one walk into windows of a view's height, the first sent
// as the file opened and each after as its next version, each stored with its map; what the server publishes of a window moved to the file's
// own lines, and what it publishes of a version sent before the stored slice dropped
#include "ui/cli/src/code/symbols.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/servers/servers_internal.h"
#include "ui/cli/src/tests/test_runs.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// A language server in Python that answers initialize and shutdown and, for each text it receives, publishes as that version's diagnostics
// one problem on each line that names checked_missing
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
                                 "        send({\"jsonrpc\": \"2.0\", \"id\": message[\"id\"], \"result\": {\"capabilities\": {}}})\n"
                                 "    elif method in (\"textDocument/didOpen\", \"textDocument/didChange\"):\n"
                                 "        params = message[\"params\"]\n"
                                 "        document = params[\"textDocument\"]\n"
                                 "        text = document[\"text\"] if \"text\" in document else params[\"contentChanges\"][0][\"text\"]\n"
                                 "        found = []\n"
                                 "        for at, line in enumerate(text.split(\"\\n\")):\n"
                                 "            place = line.find(\"checked_missing\")\n"
                                 "            if place >= 0:\n"
                                 "                found.append({\"range\": {\"start\": {\"line\": at, \"character\": place}, \"end\": {\"line\": "
                                 "at, "
                                 "\"character\": place + 15}}, \"severity\": 1, \"message\": \"call to undeclared function\"})\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"method\": \"textDocument/publishDiagnostics\", \"params\": {\"uri\": "
                                 "document[\"uri\"], \"version\": document[\"version\"], \"diagnostics\": found}})\n"
                                 "    elif method == \"shutdown\":\n"
                                 "        send({\"jsonrpc\": \"2.0\", \"id\": message[\"id\"], \"result\": None})\n"
                                 "    elif method == \"exit\":\n"
                                 "        break\n";

// What the server published of the file, each moved to its lines: how many, the last, `dropped` where it was of an older slice, and its
// size, under its lock; and the file's path
typedef struct
{
    unsigned int published;
    char last[4096];
    unsigned long long size;
    char path[CLI_PATH_MAX_LEN];
} SliceReceived;

// The windows sent: the server, how many, and each window's lines and what the server published of it, one a line
typedef struct
{
    CliLsp *server;
    unsigned int windows;
    VerbaLinea *out;
} SliceSent;

// What the whole run showed, read once for the process
typedef struct
{
    int ran;
    int cut;
    unsigned long long version;
    unsigned long long windows_size;
    int older_dropped;
    int newer_moved;
    int released;
} SliceRun;

static SliceReceived s_received = {0u, "", 0ull, ""};
static char s_windows[1u << 14u];
static SliceRun s_run;

// what the server sends on its own: its diagnostics moved to the file's lines as the thread that reads it receives them
static void server_message_received(const CliLspRecvCfg *args)
{
    const char *params = args->params;
    unsigned long long size = args->size;
    unsigned long long all_at = 0ull;
    unsigned long long all_length = 0ull;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->method, .other = "textDocument/publishDiagnostics", .cap = CLI_CELLS_POOL}) || !EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = params, .size_value = size, .name = "diagnostics", .start = &all_at, .length_pointer = &all_length))
    {
        return;
    }
    unsigned long long mapped_size = 0ull;
    char *const mapped = EMBED_CALL(cli_servers_path_size_internal.slice_diagnostics, CliServersPathSizeCtx, .path = s_received.path, .params_text = params, .params_size = size, .all = params + all_at, .size = all_length, .mapped_size = &mapped_size, .owner = &g_checks_cells);
    VerbaLinea last = {s_received.last, sizeof(s_received.last), 0ull};
    // the diagnostics of one window are below 4 KiB here
    mmgr_line_put_n(&last, (mapped != NULL) ? mapped : "dropped", (mapped != NULL) ? (size_t)mapped_size : 7u);
    s_received.size = last.at;
    s_received.published += 1u;
    g_cli_cells->persistent_buf_release(mapped, &g_checks_cells);
}

// a window sent to the server, the first as the file opened and each after as its next version, and what it publishes of it waited on and
// written as `first-last:diagnostics`
static int window_sent(const CliServersWindowCfg *args)
{
    const CliServersCut *cut = args->cut;
    SliceSent *const sent = (SliceSent *)args->context;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int delivered = EMBED_CALL(cli_servers_err_msg_server_internal.cut_send, CliServersErrMsgServerCtx, .server = sent->server, .id = (sent->windows == 0u) ? "c" : NULL, .cut = cut, .err_msg = &err_msg);
    sent->windows += 1u;
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    int received = 0;
    while (delivered && !received && (cli_clock_milliseconds() < until))
    {
        received = s_received.published == sent->windows;
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = received ? 0u : 10u);
    }
    mmgr_line_decimal(sent->out, args->first, 1u);
    mmgr_line_ch(sent->out, '-');
    mmgr_line_decimal(sent->out, args->last, 1u);
    mmgr_line_ch(sent->out, ':');
    mmgr_line_put_n(sent->out, received ? s_received.last : "none", received ? (size_t)s_received.size : 4u);
    mmgr_line_ch(sent->out, '\n');
    return delivered && received;
}

// `text` written as the file `name` under `root`, the folders above it made
static void file_written(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder))
    {
        TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder));
    }
    cli_unity_file(root, name, text);
}

// up to two seconds until the tree is read
static void tree_read_awaited(void)
{
    static char s_found[1u << 16u];
    for (unsigned int tries = 0u; tries < 200u; tries += 1u)
    {
        VerbaLinea out = {s_found, sizeof(s_found), 0ull};
        EMBED_CALL(cli_symbols_query_most.tree_declarations_find, CliSymbolsQueryMostCfg, .query = "", .most = 1u, .out = &out);
        unsigned int found = CLI_JSON_NONE;
        const CliJson *const json = cli_unity_parsed(s_found, out.at, &found);
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = found, .name = "reading"), .kind = CFG_JSON_TRUE))
        {
            return;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 10u);
    }
}

// a file of 141 lines whose one problem is on its line 100 sent to a server a window at a time, once for the process; the test ignored where
// the machine has no Python
static void file_sliced(void)
{
    static char s_use[8192];
    char python[CLI_PATH_MAX_LEN];
    if (s_run.ran)
    {
        return;
    }
    if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        TEST_IGNORE_MESSAGE("the machine has no Python");
    }
    s_run.ran = 1;
    char root[CLI_PATH_MAX_LEN];
    cli_unity_folder("servers_slice", root);
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
    file_written(root, "src/checked_shape.h", "int checked_area(int w);\n");
    file_written(root, "src/checked_use.c", s_use);
    cli_tree_forgotten();
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = root);
    tree_read_awaited();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "src/checked_use.c", .joined = s_received.path);
    char script[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_file(root, "stand_in.py", s_stand_in);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "stand_in.py", .joined = script);
    const char *const args[1] = {script};
    CliLsp *const server = EMBED_CALL(cli_lsp_err_msg.start, CliLspErrMsgCfg, .program = python, .request = args, .count = 1u, .root = root, .options = "null", .length = 4ull, .settings = NULL, .setting_count = 0u, .recv = server_message_received, .context = NULL, .err_msg = &err_msg);
    TEST_ASSERT_NOT_NULL_MESSAGE(server, "the server starts");
    VerbaLinea windows = {s_windows, sizeof(s_windows), 0ull};
    SliceSent sent = {server, 0u, &windows};
    unsigned long long size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = s_received.path, .size_pointer = &size, .owner = &g_checks_cells);
    s_run.cut = (mapped != NULL) && EMBED_CALL(cli_servers_path_size_root_err_msg_internal.slices_cut, CliServersPathSizeRootErrMsgCtx, .root = root, .path = s_received.path, .text = (const char *)mapped, .size = size, .window = window_sent, .context = &sent, .err_msg = &err_msg);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_checks_cells);
    s_run.version = EMBED_CALL(cli_lsp_path_out.version, CliLspPathOutCfg, .lsp = server, .path = s_received.path);
    s_run.windows_size = windows.at;
    static const char s_none[] = "[]";
    unsigned long long older_size = 0ull;
    unsigned long long newer_size = 0ull;
    char *const older = EMBED_CALL(cli_servers_path_size_internal.slice_diagnostics, CliServersPathSizeCtx, .path = s_received.path, .params_text = "{\"version\":2}", .params_size = 13ull, .all = s_none, .size = sizeof(s_none) - 1u, .mapped_size = &older_size, .owner = &g_checks_cells);
    char *const newer = EMBED_CALL(cli_servers_path_size_internal.slice_diagnostics, CliServersPathSizeCtx, .path = s_received.path, .params_text = "{\"version\":4}", .params_size = 13ull, .all = s_none, .size = sizeof(s_none) - 1u, .mapped_size = &newer_size, .owner = &g_checks_cells);
    s_run.older_dropped = older == NULL;
    s_run.newer_moved = newer != NULL;
    g_cli_cells->persistent_buf_release(older, &g_checks_cells);
    g_cli_cells->persistent_buf_release(newer, &g_checks_cells);
    (void)EMBED_CALL(cli_lsp_err_msg_text.close, CliLspErrMsgTextCfg, .lsp = server, .path = s_received.path, .err_msg = &err_msg);
    EMBED_CALL(cli_servers_path_lsp_internal.slice_close, CliServersPathLspCtx, .path = s_received.path);
    s_run.released = !EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = s_received.path);
    EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = server);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_file_is_sent_a_window_at_a_time_opened_with_the_first_and_changed_with_each_after(void)
{
    file_sliced();
    TEST_ASSERT_TRUE(s_run.cut);
    TEST_ASSERT_EQUAL_UINT64(3ull, s_run.version);
}

void test_what_the_server_publishes_of_each_window_is_on_the_files_own_lines(void)
{
    file_sliced();
    CLI_TEST_ASSERT_TEXT("0-63:[]\n64-127:[{\"range\":{\"start\":{\"line\":100,\"character\":13},\"end\":{\"line\":100,\"character\":28}},\"severity\":1,\"message\":\"call to undeclared function\"}]\n128-191:[]\n", s_windows, s_run.windows_size);
}

void test_what_it_publishes_of_a_version_before_the_stored_slice_is_dropped(void)
{
    file_sliced();
    TEST_ASSERT_TRUE(s_run.older_dropped);
}

void test_what_it_publishes_of_the_same_text_sent_again_is_moved(void)
{
    file_sliced();
    TEST_ASSERT_TRUE(s_run.newer_moved);
}

void test_the_files_slice_is_released_as_it_closes(void)
{
    file_sliced();
    TEST_ASSERT_TRUE(s_run.released);
}
