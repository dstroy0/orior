// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_sessions.c: the sessions one window holds: none lists as none, a send or a leave with none is refused with why,
// a far end of impero's joined as a host answers under its name, every host answers on a line of its own, a host left
// answers no more, a second command while one gathers is refused, and a far end that ended is reported
#include "ui/cli/src/comms/sessions.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_error[512];
static char s_text[CLI_ERR_MSG_MAX_LEN];
static char s_imp[CLI_PATH_MAX_LEN];
static char s_home[CLI_PATH_MAX_LEN];
static VerbaLinea s_err_msg;

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
}

void tearDown(void)
{
    cli_sessions_close();
}

// the answer waiting stepped until it is whole or ten seconds pass: 1 where it came, its text into s_text and whether
// every host ran the command into `done`
static int stepped(int *done)
{
    VerbaLinea line = {s_text, sizeof(s_text), 0ull};
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    while (cli_clock_milliseconds() < until)
    {
        if (EMBED_CALL(cli_sessions.step, CliSessionsCfg, .out = &line, .done = done))
        {
            (void)mmgr_line_finish(&line);
            return 1;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 5u);
    }
    return 0;
}

// impero as ORIOR_IMP names it, the test ignored where none is named, and orior's home a folder of this test's own,
// where the far end finds no window
static void imp_needed(void)
{
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_IMP", .value_out = s_imp, .capacity = sizeof(s_imp)))
    {
        TEST_IGNORE_MESSAGE("no impero is named for the tests");
    }
    cli_unity_folder("sessions_home", s_home);
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = s_home));
}

// a far end of impero's started as the session `address`, its join answered
static void relay_joined(const char *address)
{
    const char *const words[4] = {s_imp, "sessions", "relay", NULL};
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_sessions.join_words, CliSessionsCfg, .address = address, .words = words, .err_msg = &s_err_msg), s_error);
    int done = 0;
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_TRUE(done);
    CLI_TEST_ASSERT_CONTAINS("joined ", s_text);
}

// `about` of the Help menu sent to every host: whether every one took it
static int about_sent(void)
{
    return EMBED_CALL(cli_sessions.send, CliSessionsCfg, .menu = "help", .command = "about", .request = NULL, .count = 0u, .err_msg = &s_err_msg);
}

// the hosts listed into s_text
static void listed(void)
{
    VerbaLinea out = {s_text, sizeof(s_text), 0ull};
    EMBED_CALL(cli_sessions.list, CliSessionsCfg, .out = &out);
    (void)mmgr_line_finish(&out);
}

void test_no_session_lists_as_none(void)
{
    listed();
    TEST_ASSERT_EQUAL_STRING("no sessions", s_text);
}

void test_a_send_with_no_session_is_refused(void)
{
    TEST_ASSERT_FALSE(about_sent());
    CLI_TEST_ASSERT_CONTAINS("no session is open", s_error);
}

void test_leaving_a_host_with_no_session_is_refused(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_sessions.leave, CliSessionsCfg, .address = "nobody", .err_msg = &s_err_msg));
    CLI_TEST_ASSERT_CONTAINS("no session is open on nobody", s_error);
}

void test_a_relay_with_no_window_answers_that_none_is_open(void)
{
    imp_needed();
    relay_joined("one");
    TEST_ASSERT_TRUE_MESSAGE(about_sent(), s_error);
    int done = 1;
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_FALSE(done);
    TEST_ASSERT_EQUAL_STRING("one refused: no window is open on this host", s_text);
}

void test_every_host_answers_on_a_line_of_its_own(void)
{
    imp_needed();
    relay_joined("one");
    relay_joined("two");
    listed();
    TEST_ASSERT_EQUAL_STRING("one\ntwo", s_text);
    TEST_ASSERT_TRUE_MESSAGE(about_sent(), s_error);
    int done = 1;
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_EQUAL_STRING("one refused: no window is open on this host\ntwo refused: no window is open on this host", s_text);
}

void test_a_host_left_answers_no_more(void)
{
    imp_needed();
    relay_joined("one");
    relay_joined("two");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_sessions.leave, CliSessionsCfg, .address = "one", .err_msg = &s_err_msg), s_error);
    int done = 0;
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_EQUAL_STRING("left one", s_text);
    listed();
    TEST_ASSERT_EQUAL_STRING("two", s_text);
    TEST_ASSERT_TRUE_MESSAGE(about_sent(), s_error);
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_EQUAL_STRING("two refused: no window is open on this host", s_text);
}

void test_a_second_command_while_one_gathers_is_refused(void)
{
    imp_needed();
    relay_joined("one");
    TEST_ASSERT_TRUE_MESSAGE(about_sent(), s_error);
    TEST_ASSERT_FALSE(about_sent());
    CLI_TEST_ASSERT_CONTAINS("an answer is still gathered", s_error);
    int done = 0;
    TEST_ASSERT_TRUE(stepped(&done));
}

void test_a_relay_that_ended_gives_no_answer(void)
{
    imp_needed();
    const char *const words[4] = {s_imp, "help", "about", NULL};
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_sessions.join_words, CliSessionsCfg, .address = "gone", .words = words, .err_msg = &s_err_msg), s_error);
    int done = 0;
    TEST_ASSERT_TRUE(stepped(&done));
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 500u);
    TEST_ASSERT_TRUE_MESSAGE(about_sent(), s_error);
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_FALSE(done);
    TEST_ASSERT_EQUAL_STRING("gone gave no answer", s_text);
    listed();
    TEST_ASSERT_EQUAL_STRING("gone ended", s_text);
}
