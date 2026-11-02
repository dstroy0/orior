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

static int s_tls_run_result;
static int s_openssl_found;
static int s_certificate_created;
static int s_openssl_started;
static int s_unchecked_failed;
static unsigned int s_unchecked_status;
static int s_unchecked_body_named;
static double s_tls_time;
static unsigned long long s_tls_in_use;
static unsigned long long s_tls_high_water;
static int s_checked_failed;
static char s_checked_error[256];

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_location_is_read_from_where_it_was_sent(void)
{
    char joined[256];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    EMBED_CALL(cli_requests_out_base_internal.join, CliRequestsOutBaseCtx, .base_text = "http://h:1/a/b?c", .location = "d", .out = &line);
    CLI_TEST_ASSERT_TEXT("http://h:1/a/d", joined, line.at);
}

void test_a_location_with_its_own_host_is_read_as_it_exists(void)
{
    char joined[256];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    EMBED_CALL(cli_requests_out_base_internal.join, CliRequestsOutBaseCtx, .base_text = "https://h/a/b", .location = "//o/p", .out = &line);
    CLI_TEST_ASSERT_TEXT("https://o/p", joined, line.at);
}

