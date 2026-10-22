// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_send_test.c: Requests sent: a request is sent and its answer read whole, chunked and gzipped; a redirect is
// followed, a 303 as a GET and not where the request says; an answer without a length is read to its end and one that
// is not text kept as its bytes; and https goes through the system's TLS, a certificate no root signs refused
#include "../../../../../ui/cli/src/comms/tls.h"
#include "../../../../../ui/cli/src/editor/archive.h"
#include "../../../../../ui/cli/src/requests/requests_internal.h"
#include "../../../../../ui/cli/src/toolchains/toolchains.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

// the most answers a server gives and the bytes of each request it is told
#define REQUESTS_SERVED 4u

#define REQUESTS_TOLD 4096u

// A server on the loopback that answers each connection with the next of its answers, and keeps what it was sent
typedef struct
{
    CliSocket listener;
    const char *answer[REQUESTS_SERVED];
    unsigned long long size[REQUESTS_SERVED];
    unsigned int count;
    char told[REQUESTS_SERVED][REQUESTS_TOLD];
    unsigned int heard;
    int done;
} RequestsServer;

static RequestsServer s_server = {{0ull}, {NULL}, {0ull}, 0u, {{0}}, 0u, 1};
static CliLock s_lock = CLI_LOCK_INIT;
static CliRequestsSent s_sent;
static char s_chunked[4096];

// the server's connections taken, one for each answer, each request read whole and told before it is answered
static void requests_serving(void *context)
{
    (void)context;
    for (unsigned int index = 0u; index < s_server.count; index += 1u)
    {
        CliSocket peer;
        if (!cli_socket_accepted(&s_server.listener, &peer))
        {
            break;
        }
        char head[REQUESTS_TOLD];
        unsigned long long size = 0ull;
        while ((size < (sizeof(head) - 1u)) && cli_socket_read(&peer, head + size, 1ull))
        {
            size += 1ull;
            if ((size >= 4ull) &&
                (memor.cmp(&(MemoriaCfg){.src = head + size - 4u, .other = "\r\n\r\n", .bytes = 4u}) == 0))
            {
                break;
            }
        }
        unsigned long long length = 0ull;
        for (unsigned long long at = 0ull; at < size; at += 1ull)
        {
            static const char s_named[] = "\ncontent-length:";
            if (((at + sizeof(s_named) - 1u) <= size) && cli_requests_holds(head + at, sizeof(s_named) - 1u, s_named))
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
                (void)cli_requests_number(head + end, digits - end, 10u, REQUESTS_TOLD, &length);
            }
        }
        const unsigned long long room = sizeof(head) - 1u - size;
        const unsigned long long body = (length < room) ? length : room;
        if ((body > 0ull) && cli_socket_read(&peer, head + size, body))
        {
            size += body;
        }
        cli_lock_take(&s_lock);
        memor.cpy(&(MemoriaCfg){.dst = s_server.told[index], .src = head, .bytes = size});
        s_server.told[index][size] = '\0';
        s_server.heard = index + 1u;
        cli_lock_give(&s_lock);
        (void)cli_socket_written(&peer, s_server.answer[index], s_server.size[index]);
        cli_socket_closed(&peer);
    }
    cli_lock_take(&s_lock);
    cli_socket_closed(&s_server.listener);
    s_server.done = 1;
    cli_lock_give(&s_lock);
}

// a server started that gives `count` answers, the last server waited for first: its port
static unsigned int requests_serve(const char *const *answers, const unsigned long long *sizes, unsigned int count)
{
    int done = 0;
    for (unsigned int tries = 0u; !done && (tries < 500u); tries += 1u)
    {
        cli_lock_take(&s_lock);
        done = s_server.done;
        cli_lock_give(&s_lock);
        query_paused(done ? 0u : 10u);
    }
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned int port = 0u;
    cli_lock_take(&s_lock);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_server.answer[at] = answers[at];
        s_server.size[at] = sizes[at];
    }
    s_server.count = count;
    s_server.heard = 0u;
    s_server.done = 0;
    const int listening = cli_socket_listening(&s_server.listener, &port, &said);
    cli_lock_give(&s_lock);
    static CliThread s_thread = {requests_serving, NULL};
    cli_check(listening && cli_thread_started(&s_thread), "a server listens");
    return port;
}

// the request s_sent made: `method` to `url`, with the header `name` where it is not NULL, `body`, and whether it
// follows a redirect
static const CliRequestsSent *requests_sent(const char *method, const char *url, const char *name, const char *value,
                                            const char *body, int redirect)
{
    memor.set(&(MemoriaCfg){.dst = &s_sent, .val = 0, .bytes = sizeof(s_sent)});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.method,
                            .src = method,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = method, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.url,
                            .src = url,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = url, .cap = CLI_CELLS_POOL}) + 1u});
    if (name != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_sent.header_name[0],
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = s_sent.header_value[0],
                                .src = value,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
        s_sent.headers = 1u;
    }
    s_sent.body = body;
    s_sent.body_size = cellul.len(&(CatenaFinitaCfg){.src = body, .cap = CLI_CELLS_POOL});
    s_sent.redirect = redirect;
    return &s_sent;
}

// `url` with the server's `port` after 127.0.0.1 and `path` after it, into `out`, which holds `room`
static const char *requests_url(unsigned int port, const char *path, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    cli_line_text(&line, "http://127.0.0.1:");
    cli_line_decimal(&line, port, 1u);
    cli_line_text(&line, path);
    (void)cli_line_closed(&line);
    return out;
}

// the answer's text from `at` of its head, `size` bytes, checked against `wanted`
static int requests_head_is(const CliRequestsAnswer *answer, CliRequestsText text, const char *wanted)
{
    return (text.size == cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = answer->head + text.at, .other = wanted, .bytes = text.size}) == 0);
}

// a request is sent and its answer read whole, chunked and gzipped
static void requests_chunked(void)
{
    unsigned char json[256];
    static const char s_ok[] = "{\"ok\": true}";
    const unsigned long long zipped = cli_gzip((const unsigned char *)s_ok, sizeof(s_ok) - 1u, json, sizeof(json));
    CliLine line = {s_chunked, sizeof(s_chunked), 0ull};
    cli_line_text(&line, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Encoding: "
                         "gzip\r\nTransfer-Encoding: chunked\r\n\r\n");
    const unsigned long long parts[3] = {0ull, 7ull, zipped};
    for (unsigned int part = 0u; part < 2u; part += 1u)
    {
        static const char s_hex[] = "0123456789abcdef";
        const unsigned long long size = parts[part + 1u] - parts[part];
        if (size >= 16ull)
        {
            cli_line_character(&line, s_hex[(size >> 4u) & 15u]);
        }
        cli_line_character(&line, s_hex[size & 15u]);
        cli_line_text(&line, ";x=y\r\n");
        cli_line_counted(&line, (const char *)json + parts[part], size);
        cli_line_text(&line, "\r\n");
    }
    cli_line_text(&line, "0\r\nTrailer: 1\r\n\r\n");
    const char *const answers[1] = {s_chunked};
    const unsigned long long sizes[1] = {line.at};
    char url[256];
    const unsigned int port = requests_serve(answers, sizes, 1u);
    CliRequestsRoom *const room = cli_requests_room_taken();
    cli_requests_send(
        requests_sent("POST", requests_url(port, "/items?x=1", url, sizeof(url)), "X-One", "1", "hello", 1), 1, room);
    const CliRequestsAnswer *const answer = &room->answer;
    cli_check(!answer->has_failed && (answer->status == 200u) && requests_head_is(answer, answer->reason, "OK") &&
                  !answer->binary,
              "a request is sent and answered");
    cli_check((answer->size == sizeof(s_ok) - 1u) &&
                  (memor.cmp(&(MemoriaCfg){.src = answer->body, .other = s_ok, .bytes = answer->size}) == 0),
              "its answer read whole, chunked and gzipped");
    cli_check(answer->times.total >= answer->times.wait, "and timed");
    cli_requests_room_given(room);
    cli_lock_take(&s_lock);
    const char *const told = s_server.told[0];
    char wanted[64];
    CliLine start = {wanted, sizeof(wanted), 0ull};
    cli_line_text(&start, "POST /items?x=1 HTTP/1.1\r\nHost: 127.0.0.1:");
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = told, .cap = CLI_CELLS_POOL});
    cli_check(
        cli_text_same(told, wanted, start.at) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = told, .cap = CLI_CELLS_POOL, .other = "X-One: 1\r\n", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = told, .cap = CLI_CELLS_POOL, .other = "Content-Length: 5\r\n", .other_cap = CLI_CELLS_POOL}) !=
             NULL) &&
            (length >= 9ull) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = told + length - 9u, .other = "\r\n\r\nhello", .cap = CLI_CELLS_POOL})),
        "the request as it went");
    cli_lock_give(&s_lock);
}

// a redirect is followed, a 303 as a GET and not where the request says
static void requests_redirects(void)
{
    static const char s_see[] = "HTTP/1.1 303 See Other\r\nLocation: /done\r\nContent-Length: 0\r\n\r\n";
    static const char s_done[] = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\ndone";
    static const char s_found[] =
        "HTTP/1.1 302 Found\r\nContent-Type: text/plain\r\nLocation: /echo\r\nContent-Length: "
        "0\r\nConnection: close\r\n\r\n";
    static const char s_echo[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    static const char s_away[] = "HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:1/x\r\nContent-Length: 0\r\n\r\n";
    const char *const first[2] = {s_see, s_done};
    const unsigned long long first_sizes[2] = {sizeof(s_see) - 1u, sizeof(s_done) - 1u};
    char url[256];
    unsigned int port = requests_serve(first, first_sizes, 2u);
    CliRequestsRoom *const room = cli_requests_room_taken();
    cli_requests_send(
        requests_sent("POST", requests_url(port, "/start", url, sizeof(url)), "Content-Type", "text/plain", "x", 1), 1,
        room);
    cli_check((room->answer.status == 200u) && (room->answer.size == 4ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = room->answer.body, .other = "done", .bytes = 4u}) == 0) &&
                  (room->answer.redirects == 1u),
              "a redirect is followed");
    cli_lock_take(&s_lock);
    cli_check(cellul.starts(
                  &(CatenaFinitaCfg){.src = s_server.told[1], .other = "GET /done HTTP/1.1", .cap = CLI_CELLS_POOL}),
              "a 303 as a GET");
    cli_lock_give(&s_lock);
    const char *const second[2] = {s_found, s_echo};
    const unsigned long long second_sizes[2] = {sizeof(s_found) - 1u, sizeof(s_echo) - 1u};
    port = requests_serve(second, second_sizes, 2u);
    cli_requests_send(requests_sent("GET", requests_url(port, "/moved", url, sizeof(url)), NULL, NULL, "", 1), 1, room);
    cli_check(
        (room->answer.status == 200u) && (room->answer.size == 2ull) &&
            (memor.cmp(&(MemoriaCfg){.src = room->answer.body, .other = "ok", .bytes = 2u}) == 0) &&
            (room->answer.redirects == 1u) && (room->answer.redirect_status[0] == 302u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = room->answer.redirect_url[0], .other = url, .cap = CLI_CELLS_POOL})),
        "each redirect kept with the address it came from");
    const char *const third[1] = {s_away};
    const unsigned long long third_sizes[1] = {sizeof(s_away) - 1u};
    port = requests_serve(third, third_sizes, 1u);
    cli_requests_send(requests_sent("GET", requests_url(port, "/", url, sizeof(url)), NULL, NULL, "", 0), 1, room);
    cli_check((room->answer.status == 302u) && (room->answer.redirects == 0u), "and none where the request says not");
    cli_requests_room_given(room);
    char joined[256];
    CliLine line = {joined, sizeof(joined), 0ull};
    cli_requests_joined("http://h:1/a/b?c", "d", &line);
    cli_check_text(joined, line.at, "http://h:1/a/d", "a location read from where it was given");
    line.at = 0ull;
    cli_requests_joined("https://h/a/b", "//o/p", &line);
    cli_check_text(joined, line.at, "https://o/p", "and one with its own host");
}

// an answer without a length is read to its end and one that is not text kept as its bytes
static void requests_unbounded(void)
{
    static const char s_png[] = "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n\r\n\x89PNG\x00\x01";
    const char *const answers[1] = {s_png};
    const unsigned long long sizes[1] = {sizeof(s_png) - 1u};
    char url[256];
    const unsigned int port = requests_serve(answers, sizes, 1u);
    CliRequestsRoom *const room = cli_requests_room_taken();
    cli_requests_send(requests_sent("GET", requests_url(port, "/i.png", url, sizeof(url)), NULL, NULL, "", 1), 1, room);
    cli_check(room->answer.binary && (room->answer.size == 6ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = room->answer.body, .other = "\x89PNG\x00\x01", .bytes = 6u}) == 0),
              "an answer without a length is read to its end, its bytes as they came");
    cli_requests_send(requests_sent("GET", "http://127.0.0.1:1/", NULL, NULL, "", 1), 1, room);
    cli_check(room->answer.has_failed && (room->answer.status == 0u), "a request nothing answers says what failed");
    cli_requests_room_given(room);
}

// the program openssl into `path`, where the PATH or Git's own tools give it: 1, or 0
static int requests_openssl(char *path)
{
    static const char *const s_names[1] = {"openssl"};
    static const char *const s_git[2] = {"C:\\Program Files\\Git\\usr\\bin", "C:\\Program Files\\Git\\mingw64\\bin"};
    return cli_toolchains_on_path(s_names, 1u, path) || cli_toolchains_program_in(s_git[0], s_names, 1u, path) ||
           cli_toolchains_program_in(s_git[1], s_names, 1u, path);
}

// https goes through the system's TLS, and a certificate no root signs is refused
static void requests_tls(void)
{
    char openssl[CLI_PATH_LONGEST];
    if (!requests_openssl(openssl))
    {
        cli_check(1, "openssl is not here to serve TLS: the test of TLS needs it");
        return;
    }
    char dir[CLI_PATH_LONGEST];
    char conf[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("requests_tls", dir);
    cli_full(dir, "openssl.cnf", conf);
    static const char s_conf[] = "[req]\ndistinguished_name = name\n[name]\n";
    (void)query_file_written(conf, s_conf, sizeof(s_conf) - 1u);
    const CliSetting setting = {"OPENSSL_CONF", conf};
    const char *const made[17] = {openssl, "req",      "-x509",   "-newkey",       "rsa:2048", "-nodes",
                                  "-days", "2",        "-subj",   "/CN=localhost", "-keyout",  "key.pem",
                                  "-out",  "cert.pem", "-config", "openssl.cnf",   NULL};
    CliRan *const ran = cli_ran_taken();
    cli_check(cli_ran(made, dir, &setting, 1u, ran, &said) && (ran->code == 0ll), "a certificate is made");
    cli_ran_given(ran);
    unsigned int port = 0u;
    (void)cli_port_free(&port, &said);
    char accept[16];
    CliLine port_line = {accept, sizeof(accept), 0ull};
    cli_line_decimal(&port_line, port, 1u);
    (void)cli_line_closed(&port_line);
    const char *const serve[11] = {openssl, "s_server", "-quiet",  "-www", "-cert", "cert.pem",
                                   "-key",  "key.pem",  "-accept", accept, NULL};
    CliSpawned server;
    said.at = 0ull;
    const int started = cli_spawn_fed(&server, serve, dir, &setting, 1u, 1, &said);
    cli_check(started, "openssl serves TLS");
    int up = 0;
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    while (started && !up && (cli_clock_milliseconds() < until))
    {
        CliSocket probe;
        said.at = 0ull;
        up = cli_socket_connected_within(&probe, "127.0.0.1", port, 200u, &said);
        cli_socket_closed(&probe);
        query_paused(up ? 0u : 50u);
    }
    char url[256];
    CliLine url_line = {url, sizeof(url), 0ull};
    cli_line_text(&url_line, "https://localhost:");
    cli_line_decimal(&url_line, port, 1u);
    cli_line_character(&url_line, '/');
    (void)cli_line_closed(&url_line);
    CliRequestsRoom *const room = cli_requests_room_taken();
    cli_requests_send(requests_sent("GET", url, NULL, NULL, "", 1), 0, room);
    const CliRequestsAnswer *const answer = &room->answer;
    cli_check(!answer->has_failed && (answer->status == 200u), "https unchecked is answered");
    cli_check(cli_requests_holds((const char *)answer->body, answer->size, "s_server") ||
                  cli_requests_holds((const char *)answer->body, answer->size, "ciphers"),
              "by the server");
    cli_check(answer->times.tls > 0.0, "TLS timed");
#if defined(_WIN32)
    // Schannel's sessions hold what came, what is opened and what is sealed in cells; libssl and Secure Transport hold
    // their own
    cli_check((g_cli_tls_cells.holds == 0ull) && (g_cli_tls_cells.hw > 0ull),
              "the session's room is held in cells while it is open, and given back as it closes");
#endif
    cli_requests_send(requests_sent("GET", url, NULL, NULL, "", 1), 1, room);
    cli_check(answer->has_failed &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = answer->failed, .other = "TLS:", .cap = CLI_CELLS_POOL})),
              "a certificate no root signs is refused");
    cli_requests_room_given(room);
    if (started)
    {
        cli_spawn_tree_end(server.id);
        (void)cli_spawn_wait(&server);
        cli_spawn_closed(&server);
    }
}

int main(void)
{
    requests_chunked();
    requests_redirects();
    requests_unbounded();
    requests_tls();
    return cli_checked();
}
