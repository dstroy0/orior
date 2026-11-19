// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_sessions.c: the sessions one window holds: none lists as none, a send or a leave with none is refused with why,
// a far end of impero's joined as a host runs the command as impero and answers under its name, a command impero
// cannot run is refused with its errors, every host answers on a line of its own, a host left answers no more, a
// second command while one gathers is refused, and a far end that ended is reported
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

void test_a_far_end_with_no_window_runs_the_command_as_impero(void)
{
    imp_needed();
    relay_joined("one");
    TEST_ASSERT_TRUE_MESSAGE(about_sent(), s_error);
    int done = 0;
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_TRUE_MESSAGE(done, s_text);
    CLI_TEST_ASSERT_CONTAINS("one: imp ", s_text);
}

void test_a_command_the_far_end_cannot_run_is_refused_with_its_errors(void)
{
    imp_needed();
    relay_joined("one");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_sessions.send, CliSessionsCfg, .menu = "nowhere", .command = "nothing", .request = NULL, .count = 0u, .err_msg = &s_err_msg), s_error);
    int done = 1;
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_FALSE(done);
    CLI_TEST_ASSERT_CONTAINS("one refused: ", s_text);
    CLI_TEST_ASSERT_CONTAINS("nowhere", s_text);
}

void test_every_host_answers_on_a_line_of_its_own(void)
{
    imp_needed();
    relay_joined("one");
    relay_joined("two");
    listed();
    TEST_ASSERT_EQUAL_STRING("one\ntwo", s_text);
    TEST_ASSERT_TRUE_MESSAGE(about_sent(), s_error);
    int done = 0;
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_TRUE_MESSAGE(done, s_text);
    CLI_TEST_ASSERT_CONTAINS("one: imp ", s_text);
    CLI_TEST_ASSERT_CONTAINS("\ntwo: imp ", s_text);
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
    CLI_TEST_ASSERT_CONTAINS("two: imp ", s_text);
    CLI_TEST_ASSERT_NOT_CONTAINS("one", s_text);
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

// a repository made at `root` with one commit of the file `name` reading `text`, and the hash of its HEAD into `hash`,
// CLI_NAME_MAX_LEN bytes
static void repository_committed(const char *root, const char *name, const char *text, char *hash)
{
    cli_unity_file(root, name, text);
    const char *const add[2] = {"add", name};
    cli_unity_git(root, add, 2u);
    const char *const commit[4] = {"commit", "-q", "-m", "one"};
    cli_unity_git(root, commit, 4u);
    const char *const head[4] = {"git", "rev-parse", "HEAD", NULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = head, .folder = root, .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &s_err_msg));
    VerbaLinea line = {hash, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put_n(&line, run_result->out, (run_result->out_size > 40ull) ? 40ull : run_result->out_size);
    (void)mmgr_line_finish(&line);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
}

// a repository at `root` begun with the file `name` reading `text`, its HEAD's hash into `hash`
static void repository_made(const char *root, const char *name, const char *text, char *hash)
{
    const char *const init[2] = {"init", "-q"};
    cli_unity_git(root, init, 2u);
    const char *const user[4] = {"config", "user.email", "sessions@localhost"};
    cli_unity_git(root, user, 3u);
    const char *const named[3] = {"config", "user.name", "sessions"};
    cli_unity_git(root, named, 3u);
    repository_committed(root, name, text, hash);
}

void test_the_tree_is_served_on_the_loopback(void)
{
    static char s_tree[CLI_PATH_MAX_LEN];
    static char s_url[CLI_PATH_MAX_LEN];
    char hash[CLI_NAME_MAX_LEN];
    cli_unity_folder("sessions_served", s_tree);
    repository_made(s_tree, "one.txt", "one\n", hash);
    VerbaLinea url = {s_url, sizeof(s_url), 0ull};
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_sessions.served, CliSessionsCfg, .root = s_tree, .address = "me@host:tree", .out = &url, .err_msg = &s_err_msg), s_error);
    (void)mmgr_line_finish(&url);
    CLI_TEST_ASSERT_CONTAINS("git://127.0.0.1:", s_url);
    CLI_TEST_ASSERT_CONTAINS("/sessions_served", s_url);
    const char *const listed[4] = {"git", "ls-remote", s_url, NULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = listed, .folder = NULL, .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_INT64(0, run_result->code);
    CLI_TEST_ASSERT_CONTAINS(hash, run_result->out);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    // a container reaches the server by the host's name a container knows
    url.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_sessions.served, CliSessionsCfg, .root = s_tree, .address = "docker:box:/tree", .out = &url, .err_msg = &s_err_msg));
    (void)mmgr_line_finish(&url);
    CLI_TEST_ASSERT_CONTAINS("git://host.docker.internal:", s_url);
}

void test_the_far_end_clones_the_served_tree_and_pulls_each_state(void)
{
    imp_needed();
    static char s_tree[CLI_PATH_MAX_LEN];
    static char s_clone[CLI_PATH_MAX_LEN];
    char first[CLI_NAME_MAX_LEN];
    char second[CLI_NAME_MAX_LEN];
    cli_unity_folder("sessions_tree", s_tree);
    repository_made(s_tree, "one.txt", "one\n", first);
    cli_unity_folder("sessions_clone_parent", s_clone);
    VerbaLinea clone = {s_clone, sizeof(s_clone), CLI_UNITY_LENGTH(s_clone)};
    mmgr_line_put(&clone, "/clone");
    (void)mmgr_line_finish(&clone);
    // the far end, given a path as the served address, clones from the path: the protocol is git's own either way
    const char *const words[7] = {s_imp, "sessions", "relay", s_clone, "--git", s_tree, NULL};
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_sessions.join_words, CliSessionsCfg, .address = "clone", .words = words, .err_msg = &s_err_msg), s_error);
    int done = 0;
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_sessions.send, CliSessionsCfg, .menu = "sessions", .command = "pull", .request = NULL, .count = 0u, .err_msg = &s_err_msg), s_error);
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_TRUE_MESSAGE(done, s_text);
    CLI_TEST_ASSERT_CONTAINS(first, s_text);
    repository_committed(s_tree, "two.txt", "two\n", second);
    TEST_ASSERT_FALSE(cellul.eq(&(CatenaFinitaCfg){.src = first, .other = second, .cap = CLI_POOL_BYTES}));
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_sessions.send, CliSessionsCfg, .menu = "sessions", .command = "pull", .request = NULL, .count = 0u, .err_msg = &s_err_msg), s_error);
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_TRUE_MESSAGE(done, s_text);
    CLI_TEST_ASSERT_CONTAINS(second, s_text);
    // a state named gets checked out, not the newest
    const char *const back[1] = {first};
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_sessions.send, CliSessionsCfg, .menu = "sessions", .command = "pull", .request = back, .count = 1u, .err_msg = &s_err_msg), s_error);
    TEST_ASSERT_TRUE(stepped(&done));
    TEST_ASSERT_TRUE_MESSAGE(done, s_text);
    CLI_TEST_ASSERT_CONTAINS(first, s_text);
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
