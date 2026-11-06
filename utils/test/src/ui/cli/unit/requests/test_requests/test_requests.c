// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_requests.c: a file's requests are read with their names, lines, headers and bodies; variables come from the file and the environment chosen,
// the private values over the shared; the private file is made beside the file and named in the .gitignore; a curl line becomes a request and a request
// a curl line; and each request stores its last answers, the last first
#include "ui/cli/src/requests/requests_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_file[] = "@base = http://{{host}}/v1\n\n### List them\nGET {{base}}/items\n    ?page=2\nAccept: application/json\n# a "
                             "comment\n\n###\n# @name make\n# @no-redirect\nPOST {{base}}/items\nContent-Type: application/json\n\n{\"name\": "
                             "\"{{who}}\"}\n\n> {% client.test(); %}\n\n### From a file\nhttp://example.com/upload\n\n< ./body.json\n";

static CliRequestsParsed s_parsed;
static CliRequestsValues s_values;
static CliRequestsSent s_sent;
static CliRequestsAnswer s_answer;
static char s_body[1u << 16u];
static char s_out[16u << 20u];
static char s_big[CLI_REQUESTS_HISTORY_BODY_MAX + 10u];
static char s_root[CLI_PATH_MAX_LEN];
static char s_error[1024];
static int s_home_set;
static int s_parsed_live;
static const CliJson *s_json;

// a text of the parsed file's pool
static const char *parsed_text(CliRequestsText text)
{
    return EMBED_CALL(cli_requests_pool_internal.text, CliRequestsPoolCtx, .pool = s_parsed.text, .text_requests_text = text);
}

// the key of a request of the parsed file, in a static text
static const char *request_key(const CliRequest *request)
{
    static char s_key[256];
    VerbaLinea line = {s_key, sizeof(s_key), 0ull};
    EMBED_CALL(cli_requests_parsed_internal.key, CliRequestsParsedCtx, .parsed_read = &s_parsed, .request = request, .out = &line);
    (void)mmgr_line_finish(&line);
    return s_key;
}

// `text` parsed into s_parsed
static void text_parsed(const char *text, unsigned long long size)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_text_size_parsed_internal.parse, CliRequestsTextSizeParsedCtx, .text = text, .size = size, .parsed = &s_parsed));
    s_parsed_live = 1;
}

// the folder `name` of the run's, its api/v1 made, in s_root
static void root_created(const char *name)
{
    char folder[CLI_PATH_MAX_LEN];
    cli_unity_folder(name, s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "api", .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "api/v1", .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
}

// the shared and the private environment files written
static void environments_written(void)
{
    root_created("requests_values");
    cli_unity_file(s_root, "api/" CLI_REQUESTS_PUBLIC, "{\"$shared\": {\"who\": \"all\"}, \"dev\": {\"host\": \"localhost:1\", \"token\": \"none\"}, "
                                                       "\"prod\": {\"host\": \"example.com\"}}");
    cli_unity_file(s_root, CLI_REQUESTS_PRIVATE, "{\"dev\": {\"token\": \"secret\"}}");
}

// the value `name` of s_values, empty where it has none
static const char *value_of(const char *name)
{
    const CliRequestsText *const value = EMBED_CALL(cli_requests_size_name_internal.value, CliRequestsSizeNameCtx, .values_read = &s_values, .name = name, .size = CLI_UNITY_LENGTH(name));
    return (value != NULL) ? EMBED_CALL(cli_requests_pool_internal.text, CliRequestsPoolCtx, .pool = s_values.text, .text_requests_text = *value) : "";
}

// a tree with a shared environment file and a .gitignore, and the private file made
static void private_created(VerbaLinea *out)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    root_created("requests_private");
    cli_unity_file(s_root, "api/" CLI_REQUESTS_PUBLIC, "{\"local\": {}, \"stage\": {}}");
    cli_unity_file(s_root, ".gitignore", "target");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_text_table.private_init, CliRequestsTextCfg, .root = s_root, .path = "api/calls.http", .out = out, .err_msg = &err_msg));
}

// the file `name` under s_root read into s_body: its size
static unsigned long long body_read(const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = name, .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_body, sizeof(s_body));
    return (size != QRY_MEMBER_UNREAD) ? size : 0ull;
}

// the request `line` sends, written as a request of a file, into s_out: 1, or 0 where it is no curl line
static int curl_read(const char *line)
{
    char written[4096];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {written, sizeof(written), 0ull};
    if (!EMBED_CALL(cli_requests_table.from_curl, CliRequestsCfg, .line_text = line, .size = CLI_UNITY_LENGTH(line), .out = &out, .err_msg = &err_msg))
    {
        return 0;
    }
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(written, out.at, &read);
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = read, .length_pointer = NULL);
    const char *const copied = (text != NULL) ? text : "";
    memor.cpy(&(MemoriaCfg){.dst = s_out, .src = copied, .bytes = CLI_UNITY_LENGTH(copied) + 1u});
    return text != NULL;
}

// a APPEND of a header and a body set in s_sent and written as a curl line into `back`, which is sized for 4096
static void curl_write(char *back)
{
    char written[4096];
    memor.set(&(MemoriaCfg){.dst = &s_sent, .val = 0, .bytes = sizeof(s_sent)});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.method, .src = "PUT", .bytes = 4ull});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.url, .src = "http://h/x", .bytes = 11ull});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.header_name[0], .src = "A", .bytes = 2ull});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.header_value[0], .src = "it's", .bytes = 5ull});
    s_sent.headers = 1u;
    s_sent.body = "{}";
    s_sent.body_size = 2ull;
    VerbaLinea out = {written, sizeof(written), 0ull};
    EMBED_CALL(cli_requests_out_internal.curl_write, CliRequestsOutCtx, .sent = &s_sent, .out = &out);
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(written, out.at, &read);
    const char *const curl = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = read, .length_pointer = NULL);
    const char *const copied = (curl != NULL) ? curl : "";
    memor.cpy(&(MemoriaCfg){.dst = back, .src = copied, .bytes = CLI_UNITY_LENGTH(copied) + 1u});
}

// the answers stored for the request at `line` of the history's file, read into the shared tree: the array of them
static unsigned int answers_read(unsigned long long line)
{
    static const char s_calls[] = "### make\nGET http://h/a\n\n### other\nGET http://h/b\n\n### none\nGET "
                                  "http://h/c\n\n### big\nGET http://h/d\n";
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_requests_text_table.answers, CliRequestsTextCfg, .root = s_root, .path = "calls.http", .text = s_calls, .size = sizeof(s_calls) - 1u, .line = line, .out = &out, .err_msg = &err_msg);
    unsigned int read = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &read);
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = read, .name = "answers");
}

// an answer at `at` with `size` bytes of `body` stored under `key`: 1, or 0
static int answer_stored(const char *key, unsigned long long at, const char *body, unsigned long long size)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    s_answer.at = at;
    s_answer.status = 200u;
    s_answer.body = (const unsigned char *)body;
    s_answer.size = size;
    return EMBED_CALL(cli_requests_err_msg_internal.history_save, CliRequestsErrMsgCtx, .root = s_root, .path = "calls.http", .key_text = key, .key_size = CLI_UNITY_LENGTH(key), .answer = &s_answer, .sent = &s_sent, .err_msg = &err_msg);
}

// the history's folder made, a GET set in s_sent
static void history_begun(void)
{
    cli_unity_folder("requests_stored", s_root);
    memor.set(&(MemoriaCfg){.dst = &s_sent, .val = 0, .bytes = sizeof(s_sent)});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.method, .src = "GET", .bytes = 4ull});
    s_sent.body = "";
}

// more answers stored under `make` than a request stores, the first of them large, and one under `other`, once for the process,
// since the answers are stored under the home the process's tests share
static void answers_stored(void)
{
    static int s_stored = 0;
    if (s_stored)
    {
        return;
    }
    s_stored = 1;
    history_begun();
    memor.set(&(MemoriaCfg){.dst = s_big, .val = 'x', .bytes = sizeof(s_big)});
    for (unsigned int at = 0u; at < (CLI_REQUESTS_HISTORY_MAX + 2u); at += 1u)
    {
        TEST_ASSERT_TRUE(answer_stored("make", at, s_big, (at == 0u) ? sizeof(s_big) : 0ull));
    }
    TEST_ASSERT_TRUE(answer_stored("other", 99ull, "", 0ull));
}

void setUp(void)
{
    char home[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    if (!s_home_set)
    {
        s_home_set = 1;
        cli_unity_folder("requests_home", home);
        (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    }
}

void tearDown(void)
{
    if (s_parsed_live)
    {
        s_parsed_live = 0;
        g_cli_guard->persistent_buf_release(s_parsed.requests, g_cli_requests_owner);
        g_cli_guard->persistent_buf_release(s_parsed.text, g_cli_requests_owner);
        s_parsed.requests = NULL;
        s_parsed.requests_capacity = 0u;
        s_parsed.text = NULL;
        s_parsed.text_capacity = 0ULL;
        s_parsed.count = 0u;
        s_parsed.variables = 0u;
        s_parsed.used = 0ULL;
        s_parsed.full = 0;
    }
}

void test_a_files_three_requests_and_its_variable_are_read(void)
{
    text_parsed(s_file, sizeof(s_file) - 1u);
    TEST_ASSERT_EQUAL_UINT(3u, s_parsed.count);
    TEST_ASSERT_EQUAL_UINT(1u, s_parsed.variables);
    TEST_ASSERT_EQUAL_STRING("base", parsed_text(s_parsed.variable_name[0]));
    TEST_ASSERT_EQUAL_STRING("http://{{host}}/v1", parsed_text(s_parsed.variable_value[0]));
}

void test_a_requests_name_method_continued_address_and_line_are_read(void)
{
    text_parsed(s_file, sizeof(s_file) - 1u);
    const CliRequest *const list = &s_parsed.requests[0];
    TEST_ASSERT_EQUAL_STRING("List them", parsed_text(list->name));
    TEST_ASSERT_EQUAL_STRING("GET", parsed_text(list->method));
    TEST_ASSERT_EQUAL_STRING("{{base}}/items?page=2", parsed_text(list->url));
    TEST_ASSERT_EQUAL_UINT64(3ull, list->line);
}

void test_a_requests_headers_are_read_and_it_has_no_body(void)
{
    text_parsed(s_file, sizeof(s_file) - 1u);
    const CliRequest *const list = &s_parsed.requests[0];
    TEST_ASSERT_EQUAL_UINT(1u, list->headers);
    TEST_ASSERT_EQUAL_STRING("Accept", parsed_text(list->header_name[0]));
    TEST_ASSERT_EQUAL_STRING("application/json", parsed_text(list->header_value[0]));
    TEST_ASSERT_FALSE(list->has_body);
}

void test_a_request_named_with_no_redirect_and_its_body_is_read(void)
{
    text_parsed(s_file, sizeof(s_file) - 1u);
    const CliRequest *const make = &s_parsed.requests[1];
    TEST_ASSERT_EQUAL_STRING("make", parsed_text(make->name));
    TEST_ASSERT_FALSE(make->redirect);
    TEST_ASSERT_TRUE(make->has_body);
    TEST_ASSERT_EQUAL_STRING("{\"name\": \"{{who}}\"}", parsed_text(make->body));
}

void test_a_body_read_from_a_file_names_it(void)
{
    text_parsed(s_file, sizeof(s_file) - 1u);
    const CliRequest *const upload = &s_parsed.requests[2];
    TEST_ASSERT_EQUAL_STRING("GET", parsed_text(upload->method));
    TEST_ASSERT_TRUE(upload->has_body_file);
    TEST_ASSERT_EQUAL_STRING("./body.json", parsed_text(upload->body_file));
}

void test_a_line_names_the_request_it_is_in(void)
{
    text_parsed(s_file, sizeof(s_file) - 1u);
    TEST_ASSERT_EQUAL_PTR(&s_parsed.requests[1], EMBED_CALL(cli_requests_internal.at, CliRequestsCtx, .parsed_read = &s_parsed, .line_u64 = 14ull));
}

void test_a_line_above_the_first_names_the_first(void)
{
    text_parsed(s_file, sizeof(s_file) - 1u);
    TEST_ASSERT_EQUAL_PTR(&s_parsed.requests[0], EMBED_CALL(cli_requests_internal.at, CliRequestsCtx, .parsed_read = &s_parsed, .line_u64 = 0ull));
}

void test_a_requests_key_is_its_name_from_its_at_name_or_its_hash_line(void)
{
    text_parsed(s_file, sizeof(s_file) - 1u);
    TEST_ASSERT_EQUAL_STRING("make", request_key(&s_parsed.requests[1]));
    TEST_ASSERT_EQUAL_STRING("From a file", request_key(&s_parsed.requests[2]));
}

void test_a_request_with_no_name_is_keyed_by_its_method_and_address(void)
{
    static const char s_bare[] = "http://example.com/upload";
    text_parsed(s_bare, sizeof(s_bare) - 1u);
    TEST_ASSERT_EQUAL_STRING("GET http://example.com/upload", request_key(&s_parsed.requests[0]));
}

void test_a_files_read_is_sized_to_its_parts_and_twice_its_bytes(void)
{
    static const char s_bare[] = "http://example.com/upload";
    text_parsed(s_bare, sizeof(s_bare) - 1u);
    TEST_ASSERT_EQUAL_UINT(1u, s_parsed.requests_capacity);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64((2u * sizeof(s_bare)) + 64u, s_parsed.text_capacity);
}

void test_the_environments_names_and_files_are_read(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    environments_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_text_table.environments, CliRequestsTextCfg, .root = s_root, .path = "api/v1/calls.http", .out = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT("{\"names\":[\"dev\",\"prod\"],\"shared\":\"api/" CLI_REQUESTS_PUBLIC "\",\"private\":\"" CLI_REQUESTS_PRIVATE "\"}", s_out, out.at);
}

void test_the_private_values_are_over_the_shared_and_shared_reaches_every_environment(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    environments_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_err_msg_root_internal.values, CliRequestsErrMsgRootCtx, .root = s_root, .path = "api/v1/calls.http", .env = "dev", .values = &s_values, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING("localhost:1", value_of("host"));
    TEST_ASSERT_EQUAL_STRING("secret", value_of("token"));
    TEST_ASSERT_EQUAL_STRING("all", value_of("who"));
    g_cli_guard->persistent_buf_release(s_values.text, g_cli_requests_owner);
    s_values.text = NULL;
    s_values.capacity = 0ULL;
    s_values.used = 0ULL;
    s_values.count = 0u;
}

void test_the_environments_and_their_values_are_read_in_cells_each_released(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    environments_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_err_msg_root_internal.values, CliRequestsErrMsgRootCtx, .root = s_root, .path = "api/v1/calls.http", .env = "dev", .values = &s_values, .err_msg = &err_msg));
    g_cli_guard->persistent_buf_release(s_values.text, g_cli_requests_owner);
    s_values.text = NULL;
    s_values.capacity = 0ULL;
    s_values.used = 0ULL;
    s_values.count = 0u;
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_requests_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_requests_owner));
}

void test_a_certificate_is_checked_unless_the_environment_specifies_not_to(void)
{
    environments_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_err_msg_root_internal.verifies, CliRequestsErrMsgRootCtx, .root = s_root, .path = "api/v1/calls.http", .env = "dev"));
    cli_unity_file(s_root, CLI_REQUESTS_PRIVATE, "{\"dev\": {\"token\": \"secret\", \"SSLConfiguration\": {\"verifyHostCertificate\": false}}}");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_requests_err_msg_root_internal.verifies, CliRequestsErrMsgRootCtx, .root = s_root, .path = "api/v1/calls.http", .env = "dev"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_err_msg_root_internal.verifies, CliRequestsErrMsgRootCtx, .root = s_root, .path = "api/v1/calls.http", .env = "prod"));
}

void test_a_request_created_ready_has_its_key_address_and_body_filled_in(void)
{
    char key[256];
    VerbaLinea key_line = {key, sizeof(key), 0ull};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    environments_written();
    char *const body_cell = g_cli_guard->temporary_buf_alloc(4096ull, g_checks_owner);
    VerbaLinea body = {body_cell, 4096ull, 0ull};
    const int ready = EMBED_CALL(cli_requests_text_internal.ready, CliRequestsTextCtx, .root = s_root, .path = "api/v1/calls.http", .text = s_file, .size = sizeof(s_file) - 1u, .line_u64 = 14ull, .env = "dev", .sent_out = &s_sent, .body = &body, .key = &key_line, .err_msg = &err_msg);
    TEST_ASSERT_TRUE(ready);
    CLI_TEST_ASSERT_TEXT("make", key, key_line.at);
    TEST_ASSERT_EQUAL_STRING("http://localhost:1/v1/items", s_sent.url);
    CLI_TEST_ASSERT_TEXT("{\"name\": \"all\"}", s_sent.body, s_sent.body_size);
    g_cli_guard->persistent_buf_release(body.out, g_checks_owner);
}

void test_no_environment_leaves_no_host_and_the_request_is_refused(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    environments_written();
    char *const body_cell = g_cli_guard->temporary_buf_alloc(4096ull, g_checks_owner);
    VerbaLinea body = {body_cell, 4096ull, 0ull};
    const int ready = EMBED_CALL(cli_requests_text_internal.ready, CliRequestsTextCtx, .root = s_root, .path = "api/v1/calls.http", .text = s_file, .size = sizeof(s_file) - 1u, .line_u64 = 14ull, .env = NULL, .sent_out = &s_sent, .body = &body, .key = NULL, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    g_cli_guard->persistent_buf_release(body.out, g_checks_owner);
    TEST_ASSERT_FALSE(ready);
    CLI_TEST_ASSERT_CONTAINS("{{host}}", s_error);
}

void test_a_uuid_is_of_version_4_adjacent_a_timestamp(void)
{
    static CliRequestsValues s_none;
    static const char s_dynamic[] = "{{$uuid}} {{$timestamp}}";
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_text_size_internal.fill, CliRequestsTextSizeCtx, .text = s_dynamic, .size = sizeof(s_dynamic) - 1u, .values_read = &s_none, .out = &out, .err_msg = &err_msg));
    TEST_ASSERT_GREATER_THAN_UINT64(40ull, out.at);
    TEST_ASSERT_EQUAL_CHAR('4', s_out[14]);
}

void test_time_zero_is_written_in_iso_form(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_requests_out_seconds_internal.iso, CliRequestsOutSecondsCtx, .seconds = 0ull, .out = &out);
    CLI_TEST_ASSERT_TEXT("1970-01-01T00:00:00Z", s_out, out.at);
}

void test_a_later_time_is_written_in_iso_form(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_requests_out_seconds_internal.iso, CliRequestsOutSecondsCtx, .seconds = 1760000000ull, .out = &out);
    CLI_TEST_ASSERT_TEXT("2025-10-09T08:53:20Z", s_out, out.at);
}

void test_the_private_file_is_created_adjacent_the_file(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    private_created(&out);
    CLI_TEST_ASSERT_TEXT("\"api/" CLI_REQUESTS_PRIVATE "\"", s_out, out.at);
}

void test_the_private_file_has_the_shared_files_environments(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    unsigned int read = CLI_JSON_NONE;
    private_created(&out);
    const unsigned long long size = body_read("api/" CLI_REQUESTS_PRIVATE);
    const CliJson *const json = cli_unity_parsed(s_body, size, &read);
    out.at = 0ull;
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = read, .out = &out, .indent = 0u, .sorted = 0);
    CLI_TEST_ASSERT_TEXT("{\"local\":{},\"stage\":{}}", s_out, out.at);
}

void test_the_private_file_is_named_in_the_gitignore(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    private_created(&out);
    const unsigned long long size = body_read(".gitignore");
    CLI_TEST_ASSERT_TEXT("target\n" CLI_REQUESTS_PRIVATE "\n", s_body, size);
}

void test_the_private_file_created_again_stores_the_one_there_and_is_named_once(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    private_created(&out);
    out.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_text_table.private_init, CliRequestsTextCfg, .root = s_root, .path = "api/calls.http", .out = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT("\"api/" CLI_REQUESTS_PRIVATE "\"", s_out, out.at);
    const unsigned long long size = body_read(".gitignore");
    CLI_TEST_ASSERT_TEXT("target\n" CLI_REQUESTS_PRIVATE "\n", s_body, size);
}

void test_a_curl_line_becomes_a_request(void)
{
    TEST_ASSERT_TRUE(curl_read("curl 'https://api.example.com/v1/items?x=1' \\\n  -H 'Accept: application/json' "
                               "\\\n  -H \"Authorization: Bearer abc\" \\\n  --data-raw $'{\"a\":\"it\\'s\"}' \\\n "
                               " --compressed"));
    TEST_ASSERT_EQUAL_STRING("POST https://api.example.com/v1/items?x=1\nAccept: application/json\nAuthorization: "
                             "Bearer abc\nContent-Type: application/x-www-form-urlencoded\n\n{\"a\":\"it's\"}\n",
                             s_out);
}

void test_curl_g_appends_the_data_in_the_address(void)
{
    TEST_ASSERT_TRUE(curl_read("curl -G https://h/s -d q=a -d n=2"));
    TEST_ASSERT_EQUAL_STRING("GET https://h/s?q=a&n=2\n", s_out);
}

void test_curl_u_is_basic_authorization(void)
{
    TEST_ASSERT_TRUE(curl_read("curl -u me:pw -X DELETE http://h/x"));
    TEST_ASSERT_EQUAL_STRING("DELETE http://h/x\nAuthorization: Basic bWU6cHc=\n", s_out);
}

void test_curl_f_is_a_form_a_file_read_from_adjacent_it(void)
{
    TEST_ASSERT_TRUE(curl_read("curl -F name=me -F file=@a.png http://h/up"));
    CLI_TEST_ASSERT_CONTAINS("Content-Disposition: form-data; name=\"file\"; filename=\"a.png\"\n\n< a.png\n", s_out);
}

void test_a_curl_line_copied_for_cmd_is_read(void)
{
    TEST_ASSERT_TRUE(curl_read("curl ^\n  \"http://h/x\" ^\n  -H ^\"A: b^\""));
    TEST_ASSERT_EQUAL_STRING("GET http://h/x\nA: b\n", s_out);
}

void test_curl_json_agent_referer_cookie_and_urlencoded_data_are_read(void)
{
    TEST_ASSERT_TRUE(curl_read("curl --json {} -A ua -e ref -b c=1 --data-urlencode 'q=a b' -H nocolon -F plain --url=http://h/x"));
    TEST_ASSERT_EQUAL_STRING("POST http://h/x\nContent-Type: application/json\nAccept: application/json\nUser-Agent: ua\nReferer: ref\n"
                             "Cookie: c=1\n\n{}&q=a%20b\n",
                             s_out);
}

void test_curl_i_is_a_head_and_a_flag_with_no_value_is_refused(void)
{
    TEST_ASSERT_TRUE(curl_read("curl -I http://h/x"));
    TEST_ASSERT_EQUAL_STRING("HEAD http://h/x\n", s_out);
    TEST_ASSERT_FALSE(curl_read("curl http://h/x -H"));
}

void test_another_programs_line_is_not_read(void)
{
    TEST_ASSERT_FALSE(curl_read("wget http://h"));
}

void test_a_request_becomes_a_curl_line(void)
{
    char back[4096];
    curl_write(back);
    TEST_ASSERT_EQUAL_STRING("curl \\\n  -X PUT \\\n  'http://h/x' \\\n  -H 'A: it'\\''s' \\\n  --data-raw '{}'", back);
}

void test_a_curl_line_written_reads_back_as_the_request(void)
{
    char back[4096];
    curl_write(back);
    TEST_ASSERT_TRUE(curl_read(back));
    TEST_ASSERT_EQUAL_STRING("PUT http://h/x\nA: it's\nContent-Type: application/x-www-form-urlencoded\n\n{}\n", s_out);
}

void test_a_request_stores_its_last_answers_the_last_first(void)
{
    answers_stored();
    const unsigned int make = answers_read(1ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = make, .kind = CFG_JSON_ARRAY));
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_HISTORY_MAX, s_json->node[make].count);
    for (unsigned int at = 0u; at < CLI_REQUESTS_HISTORY_MAX; at += 1u)
    {
        char required[16];
        VerbaLinea line = {required, sizeof(required), 0ull};
        const unsigned int when = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = make, .index = at), .name = "at");
        mmgr_line_decimal(&line, CLI_REQUESTS_HISTORY_MAX + 1u - at, 1u);
        TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, when);
        CLI_TEST_ASSERT_TEXT(required, s_json->text + s_json->node[when].text, s_json->node[when].length);
    }
}

void test_each_request_stores_its_own_answers(void)
{
    answers_stored();
    const unsigned int other = answers_read(4ull);
    TEST_ASSERT_EQUAL_UINT(1u, s_json->node[other].count);
}

void test_a_request_with_no_answers_stored_lists_none(void)
{
    answers_stored();
    const unsigned int none = answers_read(7ull);
    TEST_ASSERT_EQUAL_UINT(0u, s_json->node[none].count);
}

void test_a_large_answers_body_is_stored_cut(void)
{
    history_begun();
    memor.set(&(MemoriaCfg){.dst = s_big, .val = 'y', .bytes = sizeof(s_big)});
    TEST_ASSERT_TRUE(answer_stored("big", 0ull, s_big, sizeof(s_big)));
    const unsigned int answers = answers_read(10ull);
    const unsigned int big = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = answers, .index = 0u);
    const unsigned int body = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = big, .name = "body");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = big, .name = "cut"), .kind = CFG_JSON_TRUE));
    TEST_ASSERT_EQUAL_UINT64(CLI_REQUESTS_HISTORY_BODY_MAX, s_json->node[body].length);
}
