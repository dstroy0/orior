// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_lsp.c: a path and its URI name each other, a server that ends is known to have ended with what it had open and why, a request the server sends
// is answered later while it is received on meanwhile, a request's answer is its message returned, and a message is read by its length into a cell of
// the owner named. The cases on a live server are ignored on a machine with no Python
#include "ui/cli/src/command_line/pool.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/servers/lsp.h"
#include "ui/cli/src/servers/lsp_internal.h"
#include "ui/cli/src/tests/test_runs.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// A language server in Python that answers initialize, writes on its errors each file it is sent, and ends as orior/end requests
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

// The request a server sent that is answered later, as its id's JSON, and the notifications received after it
typedef struct
{
    char id[64];
    int deferred;
    embed_atomic_usize meanwhile;
} LspLater;

// A text a reader reads from, a piece at a time
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
} LspSource;

static LspLater s_later;
static int s_ended_run_result;
static int s_ended_started;
static int s_ended_alive_after;
static char s_open_text[CLI_PATH_MAX_LEN + 64u];
static unsigned long long s_open_size;
static char s_open_requested[CLI_PATH_MAX_LEN + 64u];
static char s_ended_complaint[CLI_LSP_COMPLAINT];
static unsigned long long s_ended_complaint_size;
static unsigned long long s_in_use_closed;
static unsigned long long s_in_use_open;
static unsigned long long s_in_use_released;

// what a server sends on its own answered null
static void request_answered_null(const CliLspRecvCfg *args)
{
    mmgr_line_put(args->answer, "null");
}

// the files a server had open, `path` and its text, each after the last
static void open_recorded(const CliEachPathTextLengthCfg *args)
{
    VerbaLinea *const out = args->context;
    for (const char *at = args->path; at[0] != '\0'; at += 1)
    {
        mmgr_line_ch(out, (at[0] == '\\') ? '/' : at[0]);
    }
    mmgr_line_ch(out, '|');
    mmgr_line_put_n(out, args->text, args->length);
}

// the stand-in server started in a folder of its own, its request callback `on_request`: NULL where it did not start; the case ignored with no Python
static CliLsp *server_started(const char *name, void (*on_request)(const CliLspRecvCfg *args), void *context, char *dir)
{
    char python[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        TEST_IGNORE_MESSAGE("no Python on this machine");
    }
    cli_unity_folder(name, dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "stand_in.py", .joined = script);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = script, .bytes = s_stand_in, .size = sizeof(s_stand_in) - 1u));
    const char *const args[1] = {script};
    return EMBED_CALL(cli_lsp_err_msg.start, CliLspErrMsgCfg, .program = python, .request = args, .count = 1u, .root = dir, .options = "null", .length = 4ull, .settings = NULL, .setting_count = 0u, .recv = on_request, .context = context, .err_msg = &err_msg);
}

// a server opened a file and was requested to end, and what it left recorded, once for the process
static void ended_run_result(void)
{
    char dir[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (s_ended_run_result)
    {
        return;
    }
    s_ended_run_result = 1;
    CliLsp *const server = server_started("lsp_stand_in", request_answered_null, NULL, dir);
    s_ended_started = (server != NULL) && EMBED_CALL(cli_lsp_context.alive, CliLspContextCfg, .lsp = server);
    if (server == NULL)
    {
        return;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "a.txt", .joined = file);
    s_in_use_closed = mmgr_owner_in_use(g_cli_lsp_files_owner);
    (void)EMBED_CALL(cli_lsp_err_msg_text.open, CliLspErrMsgTextCfg, .lsp = server, .path = file, .language_id = "plaintext", .text = "one\n", .length = 4ull, .err_msg = &err_msg);
    s_in_use_open = mmgr_owner_in_use(g_cli_lsp_files_owner);
    (void)EMBED_CALL(cli_lsp_err_msg_length.notify, CliLspErrMsgLengthCfg, .lsp = server, .method = "orior/end", .params = "null", .length = 4ull, .err_msg = &err_msg);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (EMBED_CALL(cli_lsp_context.alive, CliLspContextCfg, .lsp = server) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 50u);
    }
    s_ended_alive_after = EMBED_CALL(cli_lsp_context.alive, CliLspContextCfg, .lsp = server);
    VerbaLinea open_line = {s_open_text, sizeof(s_open_text), 0ull};
    EMBED_CALL(cli_lsp_context.open_each, CliLspContextCfg, .lsp = server, .each = open_recorded, .context = &open_line);
    s_open_size = open_line.at;
    VerbaLinea required = {s_open_requested, sizeof(s_open_requested), 0ull};
    EMBED_CALL(open_recorded, CliEachPathTextLengthCfg, .context = &required, .path = file, .text = "one\n", .length = 4ull);
    (void)mmgr_line_finish(&required);
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 200u);
    VerbaLinea complaint = {s_ended_complaint, sizeof(s_ended_complaint), 0ull};
    EMBED_CALL(cli_lsp_path_out.complaint, CliLspPathOutCfg, .lsp = server, .out = &complaint);
    s_ended_complaint_size = complaint.at;
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    s_in_use_released = mmgr_owner_in_use(g_cli_lsp_files_owner);
}

// the next piece of the text
static unsigned long long source_read(const CliSourcePieceCapacityCfg *args)
{
    LspSource *const from = args->source;
    const unsigned long long left = from->size - from->at;
    const unsigned long long copied = (left < args->capacity) ? left : args->capacity;
    memor.cpy(&(MemoriaCfg){.dst = args->piece, .src = from->text + from->at, .bytes = copied});
    from->at += copied;
    return copied;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_path_is_written_as_a_file_uri_its_letters_escaped(void)
{
#if defined(_WIN32)
    static const char s_path[] = "D:\\a b\\c#.cu";
#else
    static const char s_path[] = "/a b/c#.cu";
#endif
    char uri[CLI_PATH_MAX_LEN * 3u];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = s_path, .uri_out = uri, .capacity = sizeof(uri));
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = uri, .other = "file:///", .cap = CLI_POOL_BYTES}));
    CLI_TEST_ASSERT_CONTAINS("a%20b/c%23.cu", uri);
}

void test_a_uri_names_the_path_it_was_written_from(void)
{
#if defined(_WIN32)
    static const char s_path[] = "D:\\a b\\c#.cu";
    static const char s_requested[] = "D:/a b/c#.cu";
#else
    static const char s_path[] = "/a b/c#.cu";
    static const char s_requested[] = "/a b/c#.cu";
#endif
    char uri[CLI_PATH_MAX_LEN * 3u];
    char back[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = s_path, .uri_out = uri, .capacity = sizeof(uri));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri, .length = CLI_UNITY_LENGTH(uri), .path_out = back));
    for (char *at = back; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    TEST_ASSERT_EQUAL_STRING(s_requested, back);
}

void test_a_servers_end_is_known(void)
{
    ended_run_result();
    TEST_ASSERT_TRUE(s_ended_started);
    TEST_ASSERT_FALSE(s_ended_alive_after);
}

void test_what_an_ended_server_had_open_is_recorded(void)
{
    ended_run_result();
    CLI_TEST_ASSERT_TEXT(s_open_requested, s_open_text, s_open_size);
}

void test_the_last_thing_an_ended_server_wrote_is_recorded(void)
{
    ended_run_result();
    CLI_TEST_ASSERT_TEXT("ending as requested", s_ended_complaint, s_ended_complaint_size);
}

void test_a_file_open_stores_its_uri_in_a_cell_released_as_the_server_is(void)
{
    ended_run_result();
    TEST_ASSERT_GREATER_THAN_UINT64(s_in_use_closed, s_in_use_open);
    TEST_ASSERT_LESS_THAN_UINT64(s_in_use_open, s_in_use_released);
}

void test_a_requests_answer_is_the_message_returned_its_result_ended_by_a_0(void)
{
    char dir[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliLsp *const server = server_started("lsp_handed", request_answered_null, NULL, dir);
    TEST_ASSERT_NOT_NULL(server);
    CliLspAnswer answer;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = server, .method = "orior/echo", .params = "[1,2]", .length = 5ull, .wait = 5000ull, .answer = &answer, .err_msg = &err_msg));
    TEST_ASSERT_NOT_NULL(answer.message);
    TEST_ASSERT_TRUE(answer.text > answer.message);
    CLI_TEST_ASSERT_TEXT("[1, 2]", answer.text, answer.size);
    TEST_ASSERT_EQUAL_CHAR('\0', answer.text[answer.size]);
    g_cli_guard->persistent_buf_release(answer.message, g_cli_servers_owner);
    EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = server);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
}

void test_an_answers_cell_stays_allocated_until_the_caller_releases_it(void)
{
    char dir[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliLsp *const server = server_started("lsp_handed", request_answered_null, NULL, dir);
    TEST_ASSERT_NOT_NULL(server);
    const unsigned long long before = mmgr_owner_in_use(g_cli_servers_owner);
    CliLspAnswer answer;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = server, .method = "orior/echo", .params = "[1,2]", .length = 5ull, .wait = 5000ull, .answer = &answer, .err_msg = &err_msg));
    TEST_ASSERT_GREATER_THAN_UINT64(before, mmgr_owner_in_use(g_cli_servers_owner));
    g_cli_guard->persistent_buf_release(answer.message, g_cli_servers_owner);
    TEST_ASSERT_EQUAL_UINT64(before, mmgr_owner_in_use(g_cli_servers_owner));
    EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = server);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
}

void test_a_request_the_server_refuses_returns_nothing_and_records_its_reason(void)
{
    char dir[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliLsp *const server = server_started("lsp_handed", request_answered_null, NULL, dir);
    TEST_ASSERT_NOT_NULL(server);
    const unsigned long long before = mmgr_owner_in_use(g_cli_servers_owner);
    CliLspAnswer answer;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = server, .method = "orior/refuse", .params = "null", .length = 4ull, .wait = 5000ull, .answer = &answer, .err_msg = &err_msg));
    TEST_ASSERT_NULL(answer.message);
    CLI_TEST_ASSERT_TEXT("refused", why, err_msg.at);
    TEST_ASSERT_EQUAL_UINT64(before, mmgr_owner_in_use(g_cli_servers_owner));
    EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = server);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
}

void test_a_message_is_read_by_its_length_and_the_next_after_it(void)
{
    static const char s_framed[] = "Content-Length: 41\r\nContent-Type: application/vscode-jsonrpc\r\n\r\n"
                                   "{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":{\"x\":1}}Content-Length: 2\r\n\r\n{}";
    static LspSource s_source = {s_framed, sizeof(s_framed) - 1u, 0ull};
    static CliLspReader s_reader;
    char body[256];
    unsigned long long size = 0ull;
    s_reader.read = source_read;
    s_reader.source = &s_source;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_lsp_reader.read_message, CliLspReaderCfg, .reader = &s_reader, .body_out = body, .capacity = sizeof(body), .size = &size));
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(body, size, &root);
    const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "result"), .name = "x");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = number, .type = CFG_JSON_NUMBER));
    TEST_ASSERT_EQUAL_CHAR('1', json->text[json->node[number].text]);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_lsp_reader.read_message, CliLspReaderCfg, .reader = &s_reader, .body_out = body, .capacity = sizeof(body), .size = &size));
    CLI_TEST_ASSERT_TEXT("{}", body, size);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_lsp_reader.read_message, CliLspReaderCfg, .reader = &s_reader, .body_out = body, .capacity = sizeof(body), .size = &size));
}

void test_a_message_read_into_a_cell_counts_to_the_owner_named_and_no_other(void)
{
    static const char s_framed[] = "Content-Length: 2\r\n\r\n{}";
    static LspSource s_source = {s_framed, sizeof(s_framed) - 1u, 0ull};
    static CliLspReader s_reader;
    s_reader.read = source_read;
    s_reader.source = &s_source;
    const unsigned long long debug_before = mmgr_owner_in_use(g_cli_debug_owner);
    const unsigned long long servers_before = mmgr_owner_in_use(g_cli_servers_owner);
    unsigned long long size = 0ull;
    char *const message = EMBED_CALL(cli_lsp_reader_size.message_alloc, CliLspReaderSizeCfg, .reader = &s_reader, .size = &size, .owner = g_cli_debug_owner);
    TEST_ASSERT_NOT_NULL(message);
    CLI_TEST_ASSERT_TEXT("{}", message, size);
    TEST_ASSERT_EQUAL_CHAR('\0', message[size]);
    TEST_ASSERT_GREATER_THAN_UINT64(debug_before, mmgr_owner_in_use(g_cli_debug_owner));
    TEST_ASSERT_EQUAL_UINT64(servers_before, mmgr_owner_in_use(g_cli_servers_owner));
    g_cli_guard->persistent_buf_release(message, g_cli_debug_owner);
    TEST_ASSERT_EQUAL_UINT64(debug_before, mmgr_owner_in_use(g_cli_debug_owner));
}
