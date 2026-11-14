// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_requests_pane.c: the Requests view as the CLI positions it, over a file of requests of the suite's own with two
// answers stored for its first request: a request with no answer positions the one row that reads so; the line the cursor
// is on reads the answers of its request, the last first, the status in the tone of its class with its time and
// size, the groups closed but the last answers, the answers each a row and Compare inert; the same request is not read
// again unless forced; a group opened positions its headers; an answer displayed positions its JSON body arranged; two answers
// checked light Compare and a comparison positions its lines with their signs; the environments are the head's choices
// with the one chosen lit; the curl line of the request; and a send runs on its worker, Send reading Sending until the
// step takes its answers in
#include "ui/cli/src/editor/files.h"
#include "ui/cli/src/requests/requests_internal.h"
#include "embed_atomic.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define PANE_KEY "requests:calls"

static const char s_calls[] = "### make\nGET http://h/a\n\n### other\nGET http://h/b\n";
static CliRequestsAnswer s_answer;
static CliRequestsSent s_sent;
static char s_root[CLI_PATH_MAX_LEN];
static int s_begun;
static embed_atomic_u32 s_woken_count;

// the view's thread woken from the send's worker, counted
static void view_woken(void)
{
    (void)EMBED_ATOMIC_ADD_32(&s_woken_count, 1u);
}

// an answer of `status` with `reason`, the content type `type` and `body`, stored at `at` under the request `make`
static void answer_stored(unsigned long long at, unsigned int status, const char *reason, const char *type,
                          const char *body)
{
    memor.set(&(MemoriaCfg){.dst = &s_answer, .val = 0, .bytes = sizeof(s_answer)});
    VerbaLinea head = {s_answer.head, sizeof(s_answer.head), 0ULL};
    const char *const texts[4] = {"HTTP/1.1", reason, "Content-Type", type};
    CliRequestsText placed[4];
    for (unsigned int one = 0u; one < 4u; one += 1u)
    {
        placed[one] = (CliRequestsText){head.at, CLI_UNITY_LENGTH(texts[one])};
        mmgr_line_put(&head, texts[one]);
        mmgr_line_ch(&head, '\0');
    }
    s_answer.at = at;
    s_answer.status = status;
    s_answer.version = placed[0];
    s_answer.reason = placed[1];
    s_answer.header_name[0] = placed[2];
    s_answer.header_value[0] = placed[3];
    s_answer.headers = 1u;
    s_answer.head_used = head.at;
    s_answer.body = (const unsigned char *)body;
    s_answer.size = CLI_UNITY_LENGTH(body);
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_err_msg_internal.history_save, CliRequestsErrMsgCtx, .root = s_root,
                                .path = "calls.http", .key_text = "make", .key_size = 4ULL, .answer = &s_answer,
                                .sent = &s_sent, .err_msg = &err_msg));
}

void setUp(void)
{
    cli_unity_begun();
    if (s_begun)
    {
        return;
    }
    s_begun = 1;
    char home[CLI_PATH_MAX_LEN];
    cli_unity_folder("requests_pane_home", home);
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    cli_unity_folder("requests_pane_tree", s_root);
    cli_unity_file(s_root, "calls.http", s_calls);
    cli_unity_file(s_root, CLI_REQUESTS_PUBLIC, "{\"dev\": {\"host\": \"a\"}, \"prod\": {\"host\": \"b\"}}");
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_EQUAL_UINT(CLI_FILES_OPEN_WHOLE, EMBED_CALL(cli_files_opening.open, CliFilesOpenCfg, .root = s_root,
                                                            .file = "calls.http", .key = PANE_KEY, .err_msg = &err_msg));
    memor.set(&(MemoriaCfg){.dst = &s_sent, .val = 0, .bytes = sizeof(s_sent)});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.method, .src = "GET", .bytes = 4ULL});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.url, .src = "http://h/a", .bytes = 11ULL});
    s_sent.body = "";
    answer_stored(1000ULL, 200u, "OK", "application/json", "{\"a\":1}");
    answer_stored(2000ULL, 404u, "Not Found", "text/plain", "missing");
}

void tearDown(void)
{
}

// the row `at`, in a row of the suite's own whose text is until the next is read; the test failed where there is
// none
static const CliRequestsRow *row_at(unsigned int at)
{
    static CliRequestsRow s_row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.row, CliRequestsPaneCfg, .at = at, .row = &s_row));
    return &s_row;
}

// the place of the first row of `type` at or after `from`, the rows' count where none is
static unsigned int row_of(unsigned int type, unsigned int from)
{
    const unsigned int rows = EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u);
    unsigned int at = from;
    while ((at < rows) && (row_at(at)->type != type))
    {
        at += 1u;
    }
    return at;
}

// the answers followed to the line `line` of the file: whether the rows changed
static int followed(unsigned long long line, int force)
{
    return EMBED_CALL(cli_requests_pane.follow, CliRequestsPaneCfg, .root = s_root, .path = "calls.http",
                      .key = PANE_KEY, .line = line, .force = force);
}

void test_a_request_with_no_answer_positions_the_row_that_reads_so(void)
{
    TEST_ASSERT_TRUE(followed(4ULL, 0));
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u));
    const CliRequestsRow *const none = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_ROW_NOTE, none->type);
    CLI_TEST_ASSERT_TEXT("No answer yet. Ctrl+Enter sends the request the cursor is in.", none->text,
                         none->text_length);
    CliRequestsRow past;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_requests_pane.row, CliRequestsPaneCfg, .at = 1u, .row = &past));
}

void test_the_line_reads_its_requests_answers_the_last_first(void)
{
    TEST_ASSERT_TRUE(followed(1ULL, 0));
    TEST_ASSERT_EQUAL_UINT(9u, EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u));
    const CliRequestsRow *const status = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_ROW_STATUS, status->type);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_TONE_FAILED, status->tone);
    CLI_TEST_ASSERT_TEXT("404 Not Found  0.0 ms, 7 B", status->text, status->text_length);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_ROW_TIMES, row_at(1u)->type);
    const CliRequestsRow *const headers = row_at(2u);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_ROW_GROUP, headers->type);
    TEST_ASSERT_FALSE(headers->open);
    CLI_TEST_ASSERT_TEXT("Headers (1)", headers->text, headers->text_length);
    const CliRequestsRow *const sent = row_at(3u);
    CLI_TEST_ASSERT_TEXT("Sent", sent->text, sent->text_length);
    const CliRequestsRow *const body = row_at(4u);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_ROW_BODY, body->type);
    CLI_TEST_ASSERT_TEXT("missing", body->text, body->text_length);
    const CliRequestsRow *const answers = row_at(5u);
    TEST_ASSERT_TRUE(answers->open);
    CLI_TEST_ASSERT_TEXT("Last answers", answers->text, answers->text_length);
    // the two answers' rows read one after the other, each read into the suite's one row
    const CliRequestsRow *const newer = row_at(6u);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_ROW_ANSWER, newer->type);
    TEST_ASSERT_TRUE(newer->displayed);
    CLI_TEST_ASSERT_TEXT("00:00:02  404  0.0 ms  7 B", newer->text, newer->text_length);
    const CliRequestsRow *const older = row_at(7u);
    TEST_ASSERT_FALSE(older->displayed);
    CLI_TEST_ASSERT_TEXT("00:00:01  200  0.0 ms  7 B", older->text, older->text_length);
    const CliRequestsRow *const compare = row_at(8u);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_ROW_COMPARE, compare->type);
    TEST_ASSERT_TRUE(compare->inert);
}

void test_the_same_request_is_not_read_again_unless_forced(void)
{
    TEST_ASSERT_FALSE(followed(1ULL, 0));
    TEST_ASSERT_FALSE(followed(0ULL, 0));
    TEST_ASSERT_TRUE(followed(1ULL, 1));
}

void test_a_group_opened_positions_its_headers_and_closes(void)
{
    EMBED_CALL(cli_requests_pane.toggle, CliRequestsPaneCfg, .at = 2u);
    TEST_ASSERT_EQUAL_UINT(10u, EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u));
    const CliRequestsRow *const header = row_at(3u);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_ROW_HEADER, header->type);
    CLI_TEST_ASSERT_TEXT("Content-Type: text/plain", header->text, header->text_length);
    EMBED_CALL(cli_requests_pane.toggle, CliRequestsPaneCfg, .at = 2u);
    TEST_ASSERT_EQUAL_UINT(9u, EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u));
    EMBED_CALL(cli_requests_pane.toggle, CliRequestsPaneCfg, .at = 0u);
    TEST_ASSERT_EQUAL_UINT(9u, EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u));
}

void test_an_answer_displayed_positions_its_json_body_positioned_out(void)
{
    EMBED_CALL(cli_requests_pane.show, CliRequestsPaneCfg, .at = 7u);
    const CliRequestsRow *const status = row_at(0u);
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_TONE_GOOD, status->tone);
    CLI_TEST_ASSERT_TEXT("200 OK  0.0 ms, 7 B", status->text, status->text_length);
    const unsigned int body = row_of(CLI_REQUESTS_ROW_BODY, 0u);
    CLI_TEST_ASSERT_TEXT("{", row_at(body)->text, row_at(body)->text_length);
    CLI_TEST_ASSERT_TEXT("  \"a\": 1", row_at(body + 1u)->text, row_at(body + 1u)->text_length);
    CLI_TEST_ASSERT_TEXT("}", row_at(body + 2u)->text, row_at(body + 2u)->text_length);
    TEST_ASSERT_TRUE(row_at(row_of(CLI_REQUESTS_ROW_ANSWER, 0u) + 1u)->displayed);
}

void test_two_answers_checked_light_compare_and_position_the_comparison(void)
{
    const unsigned int first = row_of(CLI_REQUESTS_ROW_ANSWER, 0u);
    EMBED_CALL(cli_requests_pane.check, CliRequestsPaneCfg, .at = first);
    TEST_ASSERT_TRUE(row_at(first)->checked);
    TEST_ASSERT_TRUE(row_at(row_of(CLI_REQUESTS_ROW_COMPARE, 0u))->inert);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_requests_pane.compare, CliRequestsPaneCfg, .at = 0u));
    EMBED_CALL(cli_requests_pane.check, CliRequestsPaneCfg, .at = first + 1u);
    TEST_ASSERT_FALSE(row_at(row_of(CLI_REQUESTS_ROW_COMPARE, 0u))->inert);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.compare, CliRequestsPaneCfg, .at = 0u));
    const unsigned int signs = row_of(CLI_REQUESTS_ROW_SIGN, 0u);
    const unsigned int rows = EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u);
    TEST_ASSERT_TRUE(signs < rows);
    // the older answer's status line is taken out first, the newer's put in after, the blank line both have once
    const CliRequestsRow *const gone = row_at(signs);
    TEST_ASSERT_EQUAL_CHAR('-', gone->sign);
    CLI_TEST_ASSERT_TEXT("HTTP/1.1 200 OK", gone->text, gone->text_length);
    unsigned int put_at = signs;
    while ((put_at < rows) && (row_at(put_at)->sign != '+'))
    {
        put_at += 1u;
    }
    TEST_ASSERT_TRUE(put_at < rows);
    CLI_TEST_ASSERT_TEXT("HTTP/1.1 404 Not Found", row_at(put_at)->text, row_at(put_at)->text_length);
    unsigned int blank_shared = 0u;
    for (unsigned int at = signs; at < rows; at += 1u)
    {
        const CliRequestsRow *const line = row_at(at);
        blank_shared += ((line->sign == ' ') && (line->text_length == 0ULL)) ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT(1u, blank_shared);
    EMBED_CALL(cli_requests_pane.check, CliRequestsPaneCfg, .at = first);
    const unsigned int unchecked = EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u);
    TEST_ASSERT_EQUAL_UINT(unchecked, row_of(CLI_REQUESTS_ROW_SIGN, 0u));
    TEST_ASSERT_TRUE(row_at(row_of(CLI_REQUESTS_ROW_COMPARE, 0u))->inert);
}

void test_the_environments_are_the_heads_choices_the_one_chosen_lit(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_REQUESTS_CHOICE_ENV + 3u, EMBED_CALL(cli_requests_pane.choices, CliRequestsPaneCfg,
                                                                   .at = 0u));
    CliRequestsRow choice;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.choice, CliRequestsPaneCfg, .at = CLI_REQUESTS_CHOICE_CURL,
                                .row = &choice));
    CLI_TEST_ASSERT_TEXT("Copy as curl", choice.text, choice.text_length);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.choice, CliRequestsPaneCfg, .at = CLI_REQUESTS_CHOICE_ENV,
                                .row = &choice));
    CLI_TEST_ASSERT_TEXT("No environment", choice.text, choice.text_length);
    TEST_ASSERT_TRUE(choice.open);
    EMBED_CALL(cli_requests_pane.env, CliRequestsPaneCfg, .at = CLI_REQUESTS_CHOICE_ENV + 2u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.choice, CliRequestsPaneCfg, .at = CLI_REQUESTS_CHOICE_ENV + 2u,
                                .row = &choice));
    CLI_TEST_ASSERT_TEXT("prod", choice.text, choice.text_length);
    TEST_ASSERT_TRUE(choice.open);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.choice, CliRequestsPaneCfg, .at = CLI_REQUESTS_CHOICE_ENV,
                                .row = &choice));
    TEST_ASSERT_FALSE(choice.open);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_requests_pane.choice, CliRequestsPaneCfg, .at = CLI_REQUESTS_CHOICE_ENV + 3u,
                                 .row = &choice));
    EMBED_CALL(cli_requests_pane.env, CliRequestsPaneCfg, .at = CLI_REQUESTS_CHOICE_ENV);
}

void test_the_curl_line_of_the_request(void)
{
    char text[1024];
    VerbaLinea out = {text, sizeof(text), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.curl, CliRequestsPaneCfg, .root = s_root, .path = "calls.http",
                                .key = PANE_KEY, .line = 1ULL, .out = &out));
    TEST_ASSERT_TRUE(mmgr_line_finish(&out) != 0ULL);
    TEST_ASSERT_EQUAL_MEMORY("curl", text, 4u);
    CLI_TEST_ASSERT_CONTAINS("'http://h/a'", text);
}

void test_no_view_displayed_leaves_nothing_to_copy(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_requests_pane.copy, CliRequestsPaneCfg, .root = s_root));
    char text[64];
    VerbaLinea out = {text, sizeof(text), 0ULL};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_requests_pane.curl, CliRequestsPaneCfg, .root = s_root, .out = &out));
}

void test_a_send_runs_on_its_worker_send_reading_sending_until_its_answers_come(void)
{
    EMBED_CALL(cli_requests_pane.begin, CliRequestsPaneCfg, .woken = view_woken);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.send, CliRequestsPaneCfg, .root = s_root, .path = "calls.http",
                                .key = PANE_KEY, .line = 1ULL));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.sending, CliRequestsPaneCfg, .at = 0u));
    CliRequestsRow choice;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.choice, CliRequestsPaneCfg, .at = CLI_REQUESTS_CHOICE_SEND,
                                .row = &choice));
    CLI_TEST_ASSERT_TEXT("Sending", choice.text, choice.text_length);
    TEST_ASSERT_TRUE(choice.inert);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_requests_pane.send, CliRequestsPaneCfg, .root = s_root, .path = "calls.http",
                                 .key = PANE_KEY, .line = 1ULL));
    const unsigned long long until = cli_clock_milliseconds() + 10000ULL;
    int taken = 0;
    while (!taken && (cli_clock_milliseconds() < until))
    {
        taken = EMBED_CALL(cli_requests_pane.step, CliRequestsPaneCfg, .root = s_root, .displayed = 0);
        cli_thread_yield();
    }
    TEST_ASSERT_TRUE(taken);
    while ((EMBED_ATOMIC_LOAD(&s_woken_count) == 0u) && (cli_clock_milliseconds() < until))
    {
        cli_thread_yield();
    }
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_ATOMIC_LOAD(&s_woken_count));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_requests_pane.sending, CliRequestsPaneCfg, .at = 0u));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_requests_pane.choice, CliRequestsPaneCfg, .at = CLI_REQUESTS_CHOICE_SEND,
                                .row = &choice));
    CLI_TEST_ASSERT_TEXT("Send", choice.text, choice.text_length);
    TEST_ASSERT_FALSE(choice.inert);
    CLI_TEST_ASSERT_TEXT("404 Not Found  0.0 ms, 7 B", row_at(0u)->text, row_at(0u)->text_length);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_requests_pane.step, CliRequestsPaneCfg, .root = s_root, .displayed = 0));
}
