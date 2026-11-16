// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_notices.c: the notices: one answering a command displayed at once for five seconds, a failure fifteen in its
// first line, one answering nothing waiting for the keys to rest or a break, Focus stopping every one until it ends,
// and Silence putting a type away until it is displayed again
#include "ui/cli/src/command_line/command_line.h"
#include "ui/cli/src/machine/state.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

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
    cli_notices_key(&(CliNoticesCfg){.name = name, .editing = 1});
}

// the notices at rest: Focus off, the timers whose time came taken, and the notice displayed taken away
static void notices_rested(void)
{
    cli_notices_focus(&(CliNoticesCfg){.on = 0});
    (void)cli_notices_step();
    if (cli_notice_displayed() != NULL)
    {
        cli_notices_press(&(CliNoticesCfg){.part = CLI_NOTICES_PRESS_NOTICE});
    }
}

// whether the status bar shows `text`
static int showing(const char *text)
{
    return (cli_notice_displayed() != NULL) &&
           cellul.eq(&(CatenaFinitaCfg){
               .src = cli_notice_displayed(), .cap = CLI_POOL_BYTES, .other = text, .other_cap = CLI_POOL_BYTES});
}

void test_a_notice_answering_a_command_shows_at_once_and_is_five_seconds(void)
{
    notices_rested();
    key_pressed("Ctrl+S");
    cli_notice_post(&(CliNoticesCfg){.text = "Formatted 3 places.", .failed = 0});
    TEST_ASSERT_TRUE(showing("Formatted 3 places."));
    TEST_ASSERT_FALSE(cli_notice_failed());
    const unsigned long long deadline = cli_notices_due_ms();
    const unsigned long long now = cli_clock_milliseconds();
    TEST_ASSERT_TRUE_MESSAGE((deadline > now + CLI_NOTICES_DISPLAYED_MS - 500ULL) &&
                                 (deadline <= now + CLI_NOTICES_DISPLAYED_MS),
                             "the word is five seconds");
    cli_notices_press(&(CliNoticesCfg){.part = CLI_NOTICES_PRESS_NOTICE});
    TEST_ASSERT_NULL_MESSAGE(cli_notice_displayed(), "a press on the word takes it away");
}

void test_a_failure_is_fifteen_seconds_and_shows_its_first_line(void)
{
    notices_rested();
    key_pressed("Ctrl+S");
    cli_notice_post(&(CliNoticesCfg){.text = "saved.txt: it could not be written\nAccess is denied.", .failed = 1});
    TEST_ASSERT_TRUE(showing("saved.txt: it could not be written"));
    TEST_ASSERT_TRUE(cli_notice_failed());
    TEST_ASSERT_TRUE(cli_notices_due_ms() > cli_clock_milliseconds() + CLI_NOTICES_DISPLAYED_MS);
    notices_rested();
}

void test_a_notice_answering_nothing_waits_for_the_keys_to_rest(void)
{
    notices_rested();
    clock_passes(CLI_NOTICES_ANSWERS_MS + 100ULL);
    cli_notices_input();
    cli_notice_post(&(CliNoticesCfg){.text = "The run ended.", .failed = 0});
    TEST_ASSERT_NULL_MESSAGE(cli_notice_displayed(), "typing goes on, and the notice waits");
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
    cli_notice_post(&(CliNoticesCfg){.text = "First.", .failed = 0});
    cli_notice_post(&(CliNoticesCfg){.text = "Second.", .failed = 0});
    TEST_ASSERT_NULL(cli_notice_displayed());
    cli_notices_break();
    TEST_ASSERT_TRUE(showing("Second."));
    notices_rested();
}

void test_focus_stops_every_notice_until_it_ends(void)
{
    notices_rested();
    cli_notices_focus(&(CliNoticesCfg){.on = 1});
    key_pressed("Ctrl+S");
    cli_notice_post(&(CliNoticesCfg){.text = "Stopped by Focus.", .failed = 0});
    TEST_ASSERT_NULL_MESSAGE(cli_notice_displayed(), "Focus stops a notice that answers a command");
    TEST_ASSERT_TRUE(cli_notices_focused());
    TEST_ASSERT_EQUAL_UINT(1u, cli_notices_waiting());
    cli_notices_break();
    TEST_ASSERT_NULL_MESSAGE(cli_notice_displayed(), "a break does not end Focus");
    cli_notices_press(&(CliNoticesCfg){.part = CLI_NOTICES_PRESS_FOCUS});
    TEST_ASSERT_FALSE_MESSAGE(cli_notices_focused(), "a press on Focus ends it");
    TEST_ASSERT_TRUE(showing("Stopped by Focus."));
    TEST_ASSERT_EQUAL_UINT(0u, cli_notices_waiting());
    notices_rested();
}

void test_silence_puts_a_type_away_for_good_and_show_again_brings_it_back(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME",
                                .value = "build/harness/notices_home"));
    notices_rested();
    key_pressed("Ctrl+S");
    cli_notice_post(&(CliNoticesCfg){.text = "Formatted 3 places.", .failed = 0});
    cli_notices_press(&(CliNoticesCfg){.part = CLI_NOTICES_PRESS_SILENCE});
    TEST_ASSERT_NULL(cli_notice_displayed());
    key_pressed("Ctrl+S");
    cli_notice_post(&(CliNoticesCfg){.text = "Formatted 12 places.", .failed = 0});
    TEST_ASSERT_NULL_MESSAGE(cli_notice_displayed(), "the type, its counts any, is put away");
    cli_notice_post(&(CliNoticesCfg){.text = "Already formatted.", .failed = 0});
    TEST_ASSERT_TRUE_MESSAGE(showing("Already formatted."), "another type still shows");
    cli_notices_show_again(&(CliNoticesCfg){.type = "Formatted # places."});
    key_pressed("Ctrl+S");
    cli_notice_post(&(CliNoticesCfg){.text = "Formatted 4 places.", .failed = 0});
    TEST_ASSERT_TRUE(showing("Formatted 4 places."));
    notices_rested();
}

void test_the_types_put_away_are_listed_one_a_line_and_none_where_none_is(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME",
                                .value = "build/harness/notices_home"));
    notices_rested();
    cli_notices_show_again(&(CliNoticesCfg){.type = "Formatted # places."});
    cli_notices_show_again(&(CliNoticesCfg){.type = "Saved # files."});
    char *away = cli_notices_away_alloc();
    if (away != NULL)
    {
        cli_notices_away_end(&(CliNoticesCfg){.away = away});
    }
    key_pressed("Ctrl+S");
    cli_notice_post(&(CliNoticesCfg){.text = "Saved 2 files.", .failed = 0});
    cli_notices_press(&(CliNoticesCfg){.part = CLI_NOTICES_PRESS_SILENCE});
    away = cli_notices_away_alloc();
    TEST_ASSERT_NOT_NULL(away);
    CLI_TEST_ASSERT_CONTAINS("Saved # files.", away);
    cli_notices_away_end(&(CliNoticesCfg){.away = away});
    cli_notices_show_again(&(CliNoticesCfg){.type = "Saved # files."});
    notices_rested();
}

void test_the_types_put_away_are_restored_from_the_folders_entries(void)
{
    static const char s_changes[] = "{\"notices-away\":\"Restored # times.\\n\"}";
    char folder[CLI_PATH_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME",
                                .value = "build/harness/notices_home"));
    notices_rested();
    cli_unity_folder("notices_state", folder);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_state_folder.save, CliStateFolderCfg, .folder = folder, .changes = s_changes,
                                .length = sizeof(s_changes) - 1u, .err_msg = &err_msg));
    CliStateEntries entries = cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = g_cli_text_owner});
    TEST_ASSERT_NOT_NULL(entries.cell);
    cli_notices_restore(&(CliNoticesCfg){.entries = &entries});
    cli_state_entries_release(&(CliStateEntriesCfg){.entries = &entries, .owner = g_cli_text_owner});
    char *const away = cli_notices_away_alloc();
    TEST_ASSERT_NOT_NULL(away);
    CLI_TEST_ASSERT_CONTAINS("Restored # times.", away);
    cli_notices_away_end(&(CliNoticesCfg){.away = away});
    key_pressed("Ctrl+S");
    cli_notice_post(&(CliNoticesCfg){.text = "Restored 3 times.", .failed = 0});
    TEST_ASSERT_NULL_MESSAGE(cli_notice_displayed(), "a type restored as put away stays away");
    cli_notices_show_again(&(CliNoticesCfg){.type = "Restored # times."});
    notices_rested();
}

void test_a_pointer_press_is_answered_at_once_and_the_notices_end_takes_every_notice_away(void)
{
    notices_rested();
    cli_notices_pointer();
    cli_notice_post(&(CliNoticesCfg){.text = "Pressed.", .failed = 0});
    TEST_ASSERT_TRUE_MESSAGE(showing("Pressed."), "a notice answering a pointer press shows at once");
    notices_rested();
    clock_passes(CLI_NOTICES_ANSWERS_MS + 100ULL);
    cli_notices_input();
    cli_notice_post(&(CliNoticesCfg){.text = "Nothing answered.", .failed = 0});
    TEST_ASSERT_NULL_MESSAGE(cli_notice_displayed(), "a notice answering no command waits after input");
    cli_notices_end();
    TEST_ASSERT_NULL(cli_notice_displayed());
    TEST_ASSERT_EQUAL_UINT(0u, cli_notices_waiting());
}
