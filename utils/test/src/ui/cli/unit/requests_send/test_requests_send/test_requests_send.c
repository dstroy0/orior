// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_requests_send.c: a request is sent and its answer read whole, chunked and gzipped; a redirect is followed, a 303 as a GET and not where the
// request specifies; an answer without a length is read to its end and one that is not text stored as its bytes; and https goes through the system's
// TLS, a certificate no root signs refused
#include "ui/cli/src/comms/tls.h"
#include "ui/cli/src/editor/archive.h"
#include "ui/cli/src/requests/requests_internal.h"
#include "ui/cli/src/text/text.h"
#include "ui/cli/src/toolchains/toolchains.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the most answers a server sends and the bytes of each request it receives
#define REQUESTS_SERVED 4u
#define REQUESTS_HEAD_MAX 4096u

// A server on the loopback that answers each connection with the next of its answers, and records what it received
typedef struct
{
    CliSocket listener;
    const char *answer[REQUESTS_SERVED];
    unsigned long long size[REQUESTS_SERVED];
    unsigned int count;
    char heads[REQUESTS_SERVED][REQUESTS_HEAD_MAX];
    unsigned int last_served;
    int done;
} RequestsServer;

static const char s_ok[] = "{\"ok\": true}";
static const char s_see[] = "HTTP/1.1 303 See Other\r\nLocation: /done\r\nContent-Length: 0\r\n\r\n";
static const char s_done[] = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\ndone";

static RequestsServer s_server = {{0ull}, {NULL}, {0ull}, 0u, {{0}}, 0u, 1};
static CliRequestsSent s_sent;
static char s_chunked[4096];
static char s_url[256];

static int s_tls_ran;
static int s_openssl_found;
static int s_certificate_made;
static int s_openssl_started;
static int s_unchecked_failed;
static unsigned int s_unchecked_status;
static int s_unchecked_body_named;
static double s_tls_time;
static unsigned long long s_tls_in_use;
static unsigned long long s_tls_high_water;
static int s_checked_failed;
static char s_checked_why[256];

// the server's connections accepted, one for each answer, each request read whole and recorded before it is answered
static void connections_served(void *context)
{
    (void)context;
    for (unsigned int index = 0u; index < s_server.count; index += 1u)
    {
        CliSocket peer;
        if (!EMBED_CALL(cli_listener_internal.socket_accept, CliListenerCtx, .listener_read = &s_server.listener, .peer = &peer))
        {
            break;
        }
        char head[REQUESTS_HEAD_MAX];
        unsigned long long size = 0ull;
        while ((size < (sizeof(head) - 1u)) && EMBED_CALL(cli_size_bytes_internal.socket_read, CliSizeBytesCtx, .sock = &peer, .bytes_out = head + size, .size = 1ull))
        {
            size += 1ull;
            if ((size >= 4ull) && (memor.cmp(&(MemoriaCfg){.src = head + size - 4u, .other = "\r\n\r\n", .bytes = 4u}) == 0))
            {
                break;
            }
        }
        unsigned long long length = 0ull;
        for (unsigned long long at = 0ull; at < size; at += 1ull)
        {
            static const char s_named[] = "\ncontent-length:";
            if (((at + sizeof(s_named) - 1u) <= size) && EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx, .text = head + at, .size = sizeof(s_named) - 1u, .needle = s_named))
            {
                unsigned long long end = at + sizeof(s_named) - 1u;
                while ((end < size) && (head[end] == ' '))
                {
                    end += 1ull;
                }
                unsigned long long digits = end;
                while ((digits < size) && (head[digits] >= '0') && (head[digits] <= '9'))
                {
                    digits += 1ull;
                }
                (void)EMBED_CALL(cli_requests_text_size_base_internal.number, CliRequestsTextSizeBaseCtx, .text = head + end, .size = digits - end, .base = 10u, .most = REQUESTS_HEAD_MAX, .value_u64s = &length);
            }
        }
        const unsigned long long room = sizeof(head) - 1u - size;
        const unsigned long long body = (length < room) ? length : room;
        if ((body > 0ull) && EMBED_CALL(cli_size_bytes_internal.socket_read, CliSizeBytesCtx, .sock = &peer, .bytes_out = head + size, .size = body))
        {
            size += body;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_server.heads[index], .src = head, .bytes = size});
        s_server.heads[index][size] = '\0';
        s_server.last_served = index + 1u;
        (void)EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &peer, .bytes = s_server.answer[index], .size = s_server.size[index]);
        EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &peer);
    }
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &s_server.listener);
    s_server.done = 1;
}

// a server started that sends `count` answers, the last server waited for first: its port
static unsigned int server_started(const char *const *answers, const unsigned long long *sizes, unsigned int count)
{
    static CliThread s_thread = {connections_served, NULL};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    int done = 0;
    for (unsigned int tries = 0u; !done && (tries < 500u); tries += 1u)
    {
        done = s_server.done;
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = done ? 0u : 10u);
    }
    unsigned int port = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_server.answer[at] = answers[at];
        s_server.size[at] = sizes[at];
    }
    s_server.count = count;
    s_server.last_served = 0u;
    s_server.done = 0;
    const int listening = EMBED_CALL(cli_err_msg_port_internal.socket_listening, CliErrMsgPortCtx, .listener = &s_server.listener, .port_pointer = &port, .err_msg = &err_msg);
    TEST_ASSERT_TRUE(listening);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread));
    return port;
}

// the request s_sent made: `method` to `url`, with the header `name` where it is not NULL, `body`, and whether it follows a redirect
static const CliRequestsSent *request_made(const char *method, const char *url, const char *name, const char *value, const char *body, int redirect)
{
    memor.set(&(MemoriaCfg){.dst = &s_sent, .val = 0, .bytes = sizeof(s_sent)});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.method, .src = method, .bytes = CLI_UNITY_LENGTH(method) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.url, .src = url, .bytes = CLI_UNITY_LENGTH(url) + 1u});
    if (name != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_sent.header_name[0], .src = name, .bytes = CLI_UNITY_LENGTH(name) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = s_sent.header_value[0], .src = value, .bytes = CLI_UNITY_LENGTH(value) + 1u});
        s_sent.headers = 1u;
    }
    s_sent.body = body;
    s_sent.body_size = CLI_UNITY_LENGTH(body);
    s_sent.redirect = redirect;
    return &s_sent;
}

// the loopback address of `port` with `path` after it, in s_url
static const char *loopback_url(unsigned int port, const char *path)
{
    VerbaLinea line = {s_url, sizeof(s_url), 0ull};
    mmgr_line_put(&line, "http://127.0.0.1:");
    mmgr_line_decimal(&line, port, 1u);
    mmgr_line_put(&line, path);
    (void)mmgr_line_finish(&line);
    return s_url;
}

// a POST sent to a server that answers it chunked and gzipped: the room with its answer
static CliRequestsRoom *chunked_sent(void)
{
    static const char s_hex[] = "0123456789abcdef";
    unsigned char json[256];
    const unsigned long long zipped = EMBED_CALL(cli_archive_size.gzip, CliArchiveSizeCfg, .data = (const unsigned char *)s_ok, .size = sizeof(s_ok) - 1u, .out = json, .room = sizeof(json));
    VerbaLinea line = {s_chunked, sizeof(s_chunked), 0ull};
    mmgr_line_put(&line, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Encoding: "
                         "gzip\r\nTransfer-Encoding: chunked\r\n\r\n");
    const unsigned long long parts[3] = {0ull, 7ull, zipped};
    for (unsigned int part = 0u; part < 2u; part += 1u)
    {
        const unsigned long long size = parts[part + 1u] - parts[part];
        if (size >= 16ull)
        {
            mmgr_line_ch(&line, s_hex[(size >> 4u) & 15u]);
        }
        mmgr_line_ch(&line, s_hex[size & 15u]);
        mmgr_line_put(&line, ";x=y\r\n");
        mmgr_line_put_n(&line, (const char *)json + parts[part], size);
        mmgr_line_put(&line, "\r\n");
    }
    mmgr_line_put(&line, "0\r\nTrailer: 1\r\n\r\n");
    const char *const answers[1] = {s_chunked};
    const unsigned long long sizes[1] = {line.at};
    const unsigned int port = server_started(answers, sizes, 1u);
    CliRequestsRoom *const room = cli_requests_room_take();
    EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = request_made("POST", loopback_url(port, "/items?x=1"), "X-One", "1", "hello", 1), .verify = 1, .room = room);
    return room;
}

// a POST sent to a server that answers it with a 303, then the place it names: the room with its answer
static CliRequestsRoom *see_other_followed(void)
{
    const char *const answers[2] = {s_see, s_done};
    const unsigned long long sizes[2] = {sizeof(s_see) - 1u, sizeof(s_done) - 1u};
    const unsigned int port = server_started(answers, sizes, 2u);
    CliRequestsRoom *const room = cli_requests_room_take();
    EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = request_made("POST", loopback_url(port, "/start"), "Content-Type", "text/plain", "x", 1), .verify = 1, .room = room);
    return room;
}

// the program openssl into `path`, where the PATH or Git's own tools have it: 1, or 0
static int openssl_found(char *path)
{
    static const char *const s_names[1] = {"openssl"};
    static const char *const s_git[2] = {"C:\\Program Files\\Git\\usr\\bin", "C:\\Program Files\\Git\\mingw64\\bin"};
    return EMBED_CALL(cli_toolchains_names.on_path, CliToolchainsNamesCfg, .names = s_names, .count = 1u, .program = path) || EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = s_git[0], .names = s_names, .count = 1u, .path = path) || EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = s_git[1], .names = s_names, .count = 1u, .path = path);
}

// a TLS server of a certificate of its own made and sent a request unchecked and checked, what each answered recorded, once for the process
static void tls_ran(void)
{
    static const char s_conf[] = "[req]\ndistinguished_name = name\n[name]\n";
    char openssl[CLI_PATH_MAX_LEN];
    char dir[CLI_PATH_MAX_LEN];
    char conf[CLI_PATH_MAX_LEN];
    char why[1024];
    char accept[16];
    char url[256];
    if (s_tls_ran)
    {
        return;
    }
    s_tls_ran = 1;
    s_openssl_found = openssl_found(openssl);
    if (!s_openssl_found)
    {
        return;
    }
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_folder("requests_tls", dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "openssl.cnf", .joined = conf);
    (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = conf, .bytes = s_conf, .size = sizeof(s_conf) - 1u);
    const CliSetting setting = {"OPENSSL_CONF", conf};
    const char *const made[17] = {openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2", "-subj", "/CN=localhost", "-keyout", "key.pem", "-out", "cert.pem", "-config", "openssl.cnf", NULL};
    CliRan *const ran = cli_ran_take();
    s_certificate_made = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = made, .folder = dir, .settings = &setting, .count = 1u, .ran = ran, .err_msg = &err_msg) && (ran->code == 0ll);
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    unsigned int port = 0u;
    (void)EMBED_CALL(cli_err_msg_port_internal.port_free, CliErrMsgPortCtx, .port_pointer = &port, .err_msg = &err_msg);
    VerbaLinea port_line = {accept, sizeof(accept), 0ull};
    mmgr_line_decimal(&port_line, port, 1u);
    (void)mmgr_line_finish(&port_line);
    const char *const serve[11] = {openssl, "s_server", "-quiet", "-www", "-cert", "cert.pem", "-key", "key.pem", "-accept", accept, NULL};
    CliSpawned server;
    err_msg.at = 0ull;
    s_openssl_started = EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &server, .words = serve, .folder = dir, .settings = &setting, .count = 1u, .detached = 1, .err_msg = &err_msg);
    int up = 0;
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    while (s_openssl_started && !up && (cli_clock_milliseconds() < until))
    {
        CliSocket probe;
        err_msg.at = 0ull;
        up = EMBED_CALL(cli_err_msg_internal.socket_connected_within, CliErrMsgCtx, .sock_out = &probe, .host = "127.0.0.1", .port = port, .milliseconds = 200u, .err_msg = &err_msg);
        EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &probe);
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = up ? 0u : 50u);
    }
    VerbaLinea url_line = {url, sizeof(url), 0ull};
    mmgr_line_put(&url_line, "https://localhost:");
    mmgr_line_decimal(&url_line, port, 1u);
    mmgr_line_ch(&url_line, '/');
    (void)mmgr_line_finish(&url_line);
    CliRequestsRoom *const room = cli_requests_room_take();
    EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = request_made("GET", url, NULL, NULL, "", 1), .verify = 0, .room = room);
    const CliRequestsAnswer *const answer = &room->answer;
    s_unchecked_failed = answer->has_failed;
    s_unchecked_status = answer->status;
    s_unchecked_body_named = EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx, .text = (const char *)answer->body, .size = answer->size, .needle = "s_server") || EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx, .text = (const char *)answer->body, .size = answer->size, .needle = "ciphers");
    s_tls_time = answer->times.tls;
    s_tls_in_use = embed_atomic_usize_load(&g_cli_tls_cells.in_use);
    s_tls_high_water = embed_atomic_usize_load(&g_cli_tls_cells.hw);
    EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = request_made("GET", url, NULL, NULL, "", 1), .verify = 1, .room = room);
    s_checked_failed = answer->has_failed;
    VerbaLinea checked_why = {s_checked_why, sizeof(s_checked_why), 0ull};
    mmgr_line_put(&checked_why, answer->failed);
    (void)mmgr_line_finish(&checked_why);
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
    if (s_openssl_started)
    {
        EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = server.id);
        (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &server);
        EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &server);
    }
}

// the TLS sequence run, the case ignored with no openssl on this machine
static void tls_ready(void)
{
    tls_ran();
    if (!s_openssl_found)
    {
        TEST_IGNORE_MESSAGE("no openssl on this machine to serve TLS");
    }
    TEST_ASSERT_TRUE(s_certificate_made);
    TEST_ASSERT_TRUE(s_openssl_started);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_request_is_sent_and_answered(void)
{
    CliRequestsRoom *const room = chunked_sent();
    const CliRequestsAnswer *const answer = &room->answer;
    TEST_ASSERT_FALSE(answer->has_failed);
    TEST_ASSERT_EQUAL_UINT(200u, answer->status);
    CLI_TEST_ASSERT_TEXT("OK", answer->head + answer->reason.at, answer->reason.size);
    TEST_ASSERT_FALSE(answer->binary);
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
}

void test_an_answer_is_read_whole_chunked_and_gzipped(void)
{
    CliRequestsRoom *const room = chunked_sent();
    const CliRequestsAnswer *const answer = &room->answer;
    CLI_TEST_ASSERT_TEXT(s_ok, (const char *)answer->body, answer->size);
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
}

void test_an_answer_is_timed(void)
{
    CliRequestsRoom *const room = chunked_sent();
    const CliRequestsAnswer *const answer = &room->answer;
    TEST_ASSERT_TRUE(answer->times.total >= answer->times.wait);
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
}

void test_the_request_is_sent_with_its_start_header_length_and_body(void)
{
    char wanted[64];
    CliRequestsRoom *const room = chunked_sent();
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
    const char *const heads = s_server.heads[0];
    VerbaLinea start = {wanted, sizeof(wanted), 0ull};
    mmgr_line_put(&start, "POST /items?x=1 HTTP/1.1\r\nHost: 127.0.0.1:");
    const unsigned long long length = CLI_UNITY_LENGTH(heads);
    const int starts = EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = heads, .other_pointer = wanted, .size = start.at);
    TEST_ASSERT_TRUE(starts);
    CLI_TEST_ASSERT_CONTAINS("X-One: 1\r\n", heads);
    CLI_TEST_ASSERT_CONTAINS("Content-Length: 5\r\n", heads);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(9ull, length);
    TEST_ASSERT_EQUAL_STRING("\r\n\r\nhello", heads + length - 9u);
}

void test_a_redirect_is_followed(void)
{
    CliRequestsRoom *const room = see_other_followed();
    TEST_ASSERT_EQUAL_UINT(200u, room->answer.status);
    CLI_TEST_ASSERT_TEXT("done", (const char *)room->answer.body, room->answer.size);
    TEST_ASSERT_EQUAL_UINT(1u, room->answer.redirects);
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
}

void test_a_303_is_followed_as_a_get(void)
{
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = see_other_followed());
    const int as_get = cellul.starts(&(CatenaFinitaCfg){.src = s_server.heads[1], .other = "GET /done HTTP/1.1", .cap = CLI_CELLS_POOL});
    TEST_ASSERT_TRUE(as_get);
}

void test_each_redirect_is_recorded_with_the_address_it_came_from(void)
{
    static const char s_found[] = "HTTP/1.1 302 Found\r\nContent-Type: text/plain\r\nLocation: /echo\r\nContent-Length: "
                                  "0\r\nConnection: close\r\n\r\n";
    static const char s_echo[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    const char *const answers[2] = {s_found, s_echo};
    const unsigned long long sizes[2] = {sizeof(s_found) - 1u, sizeof(s_echo) - 1u};
    const unsigned int port = server_started(answers, sizes, 2u);
    CliRequestsRoom *const room = cli_requests_room_take();
    EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = request_made("GET", loopback_url(port, "/moved"), NULL, NULL, "", 1), .verify = 1, .room = room);
    TEST_ASSERT_EQUAL_UINT(200u, room->answer.status);
    CLI_TEST_ASSERT_TEXT("ok", (const char *)room->answer.body, room->answer.size);
    TEST_ASSERT_EQUAL_UINT(1u, room->answer.redirects);
    TEST_ASSERT_EQUAL_UINT(302u, room->answer.redirect_status[0]);
    TEST_ASSERT_EQUAL_STRING(s_url, room->answer.redirect_url[0]);
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
}

void test_no_redirect_is_followed_where_the_request_specifies(void)
{
    static const char s_away[] = "HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:1/x\r\nContent-Length: 0\r\n\r\n";
    const char *const answers[1] = {s_away};
    const unsigned long long sizes[1] = {sizeof(s_away) - 1u};
    const unsigned int port = server_started(answers, sizes, 1u);
    CliRequestsRoom *const room = cli_requests_room_take();
    EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = request_made("GET", loopback_url(port, "/"), NULL, NULL, "", 0), .verify = 1, .room = room);
    TEST_ASSERT_EQUAL_UINT(302u, room->answer.status);
    TEST_ASSERT_EQUAL_UINT(0u, room->answer.redirects);
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
}

void test_a_location_is_read_from_where_it_was_sent(void)
{
    char joined[256];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    EMBED_CALL(cli_requests_out_base_internal.join, CliRequestsOutBaseCtx, .base_text = "http://h:1/a/b?c", .location = "d", .out = &line);
    CLI_TEST_ASSERT_TEXT("http://h:1/a/d", joined, line.at);
}

void test_a_location_with_its_own_host_is_read_as_it_stands(void)
{
    char joined[256];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    EMBED_CALL(cli_requests_out_base_internal.join, CliRequestsOutBaseCtx, .base_text = "https://h/a/b", .location = "//o/p", .out = &line);
    CLI_TEST_ASSERT_TEXT("https://o/p", joined, line.at);
}

void test_an_answer_without_a_length_is_read_to_its_end_as_its_bytes(void)
{
    static const char s_png[] = "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n\r\n\x89PNG\x00\x01";
    const char *const answers[1] = {s_png};
    const unsigned long long sizes[1] = {sizeof(s_png) - 1u};
    const unsigned int port = server_started(answers, sizes, 1u);
    CliRequestsRoom *const room = cli_requests_room_take();
    EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = request_made("GET", loopback_url(port, "/i.png"), NULL, NULL, "", 1), .verify = 1, .room = room);
    TEST_ASSERT_TRUE(room->answer.binary);
    TEST_ASSERT_EQUAL_UINT64(6ull, room->answer.size);
    TEST_ASSERT_EQUAL_MEMORY("\x89PNG\x00\x01", room->answer.body, 6u);
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
}

void test_a_request_nothing_answers_reports_what_failed(void)
{
    CliRequestsRoom *const room = cli_requests_room_take();
    EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = request_made("GET", "http://127.0.0.1:1/", NULL, NULL, "", 1), .verify = 1, .room = room);
    TEST_ASSERT_TRUE(room->answer.has_failed);
    TEST_ASSERT_EQUAL_UINT(0u, room->answer.status);
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
}

void test_https_unchecked_is_answered_by_the_server(void)
{
    tls_ready();
    TEST_ASSERT_FALSE(s_unchecked_failed);
    TEST_ASSERT_EQUAL_UINT(200u, s_unchecked_status);
    TEST_ASSERT_TRUE(s_unchecked_body_named);
}

void test_tls_is_timed(void)
{
    tls_ready();
    TEST_ASSERT_TRUE(s_tls_time > 0.0);
}

void test_windows_a_schannel_sessions_room_is_in_cells_released_as_it_closes(void)
{
#if defined(_WIN32)
    tls_ready();
    TEST_ASSERT_EQUAL_UINT64(0ull, s_tls_in_use);
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, s_tls_high_water);
#else
    TEST_IGNORE_MESSAGE("libssl stores its own");
#endif
}

void test_a_certificate_no_root_signs_is_refused(void)
{
    tls_ready();
    TEST_ASSERT_TRUE(s_checked_failed);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_checked_why, .other = "TLS:", .cap = CLI_CELLS_POOL}));
}
