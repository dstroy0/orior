// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_notices.c: the notices: one answering a command shown at once for five seconds, a failure fifteen in its first
// line, one answering nothing waiting for the keys to rest or a break, Focus stopping every one until it ends, and
// Silence putting a kind away until it is shown again
#include "ui/cli/src/command_line/command_line.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"
#include <string.h>

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// the clock let run `milliseconds`
static void clock_passes(unsigned long long milliseconds)
{
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while (cli_clock_milliseconds() < until)
    {
    }
}

// the key `name` pressed in the editor, a key with a modifier commanding
static void key_pressed(const char *name)
{
    cli_notices_key(name, 1);
}

// the notices at rest: Focus off, the timers whose time came taken, and the notice shown taken away
static void notices_rested(void)
{
    cli_notices_focus(0);
    (void)cli_notices_step();
    if (cli_notice_shown() != NULL)
    {
        cli_notices_press(CLI_NOTICES_PRESS_NOTICE);
    }
}

// whether the status bar shows `text`
static int showing(const char *text)
{
    return (cli_notice_shown() != NULL) && (strcmp(cli_notice_shown(), text) == 0);
}

void test_a_notice_answering_a_command_shows_at_once_and_stands_five_seconds(void)
{
    notices_rested();
    key_pressed("Ctrl+S");
    cli_notice_post("Formatted 3 places.", 0);
    TEST_ASSERT_TRUE(showing("Formatted 3 places."));
    TEST_ASSERT_FALSE(cli_notice_failed());
    const unsigned long long deadline = cli_notices_due_ms();
    const unsigned long long now = cli_clock_milliseconds();
    TEST_ASSERT_TRUE_MESSAGE((deadline > now + CLI_NOTICES_SHOWN_MS - 500ULL) && (deadline <= now + CLI_NOTICES_SHOWN_MS),
                             "the word stands five seconds");
    cli_notices_press(CLI_NOTICES_PRESS_NOTICE);
    TEST_ASSERT_NULL_MESSAGE(cli_notice_shown(), "a press on the word takes it away");
}

void test_a_failure_stands_fifteen_seconds_and_shows_its_first_line(void)
{
    notices_rested();
    key_pressed("Ctrl+S");
    cli_notice_post("saved.txt: it could not be written\nAccess is denied.", 1);
    TEST_ASSERT_TRUE(showing("saved.txt: it could not be written"));
    TEST_ASSERT_TRUE(cli_notice_failed());
    TEST_ASSERT_TRUE(cli_notices_due_ms() > cli_clock_milliseconds() + CLI_NOTICES_SHOWN_MS);
    notices_rested();
}

void test_a_notice_answering_nothing_waits_for_the_keys_to_rest(void)
{
    notices_rested();
    clock_passes(CLI_NOTICES_ANSWERS_MS + 100ULL);
    cli_notices_input();
    cli_notice_post("The run ended.", 0);
    TEST_ASSERT_NULL_MESSAGE(cli_notice_shown(), "typing goes on, and the notice waits");
    clock_passes(CLI_NOTICES_REST_MS + 100ULL);
    (void)cli_notices_step();
    TEST_ASSERT_TRUE_MESSAGE(showing("The run ended."), "the keys at rest, it shows");
    notices_rested();
}

void test_a_break_shows_the_last_notice_waiting(void)
{
    notices_rested();
    clock_passes(CLI_NOTICES_ANSWERS_MS + 100ULL);
    cli_notices_input();
    cli_notice_post("First.", 0);
    cli_notice_post("Second.", 0);
    TEST_ASSERT_NULL(cli_notice_shown());
    cli_notices_break();
    TEST_ASSERT_TRUE(showing("Second."));
    notices_rested();
}

void test_focus_stops_every_notice_until_it_ends(void)
{
    notices_rested();
    cli_notices_focus(1);
    key_pressed("Ctrl+S");
    cli_notice_post("Stopped by Focus.", 0);
    TEST_ASSERT_NULL_MESSAGE(cli_notice_shown(), "Focus stops a notice that answers a command");
    TEST_ASSERT_TRUE(cli_notices_focused());
    TEST_ASSERT_EQUAL_UINT(1u, cli_notices_waiting());
    cli_notices_break();
    TEST_ASSERT_NULL_MESSAGE(cli_notice_shown(), "a break does not end Focus");
    cli_notices_press(CLI_NOTICES_PRESS_FOCUS);
    TEST_ASSERT_FALSE_MESSAGE(cli_notices_focused(), "a press on Focus ends it");
    TEST_ASSERT_TRUE(showing("Stopped by Focus."));
    TEST_ASSERT_EQUAL_UINT(0u, cli_notices_waiting());
    notices_rested();
}

void test_silence_puts_a_kind_away_for_good_and_show_again_brings_it_back(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME",
                                .value = "build/harness/notices_home"));
    notices_rested();
    key_pressed("Ctrl+S");
    cli_notice_post("Formatted 3 places.", 0);
    cli_notices_press(CLI_NOTICES_PRESS_SILENCE);
    TEST_ASSERT_NULL(cli_notice_shown());
    key_pressed("Ctrl+S");
    cli_notice_post("Formatted 12 places.", 0);
    TEST_ASSERT_NULL_MESSAGE(cli_notice_shown(), "the kind, its counts any, is put away");
    cli_notice_post("Already formatted.", 0);
    TEST_ASSERT_TRUE_MESSAGE(showing("Already formatted."), "another kind still shows");
    cli_notices_show_again("Formatted # places.");
    key_pressed("Ctrl+S");
    cli_notice_post("Formatted 4 places.", 0);
    TEST_ASSERT_TRUE(showing("Formatted 4 places."));
    notices_rested();
}
