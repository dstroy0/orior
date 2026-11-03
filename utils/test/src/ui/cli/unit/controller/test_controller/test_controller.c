// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_controller.c: the controller's menus as it publishes them: Reduce Motion shown on while the saved setting or the
// system asks for less motion, its command setting it on or off by its word and else turning it from what shows; a
// menu's serial, new as it opens and the same while it stands open; the tabs' saves: a tab changed while its text
// differs from the file as saved, a letter typed and taken out again no change, Save and Save All writing the texts with
// their line ends, a changed tab's close waiting on a second, and Save and Save All chosen only while a text is changed;
// and the notices: one answering a command shown at once for five seconds, a failure fifteen in its first line, one
// answering nothing waiting for the work to rest or a break, Focus holding every one until it ends, Silence putting a
// kind away until it is shown again, and a save that cannot write posting its failure
#include "ui/cli/src/controller/controller_internal.h"
#include "ui/cli/src/controller/controller_work.h"
#include "ui/cli/src/editor/view.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// the catalog read, once for every case
static void catalog_read_once(void)
{
    static int s_read;
    if (!s_read)
    {
        TEST_ASSERT_TRUE(EMBED_CALL(cli_model_commands.catalog_read, CliModelCtx, .at = 0u));
        s_read = 1;
    }
}

// the menu `name` opened on the bar, the menus as the controller then publishes them
static CliMenus *menu_opened(const char *name)
{
    CliMenus *const bar = (CliMenus *)EMBED_CALL(cli_model_commands.menus_alloc, CliModelCtx, .at = 0u);
    TEST_ASSERT_NOT_NULL(bar);
    unsigned int title = 0u;
    while ((title < bar->titles) && (strcmp(bar->title[title], name) != 0))
    {
        title += 1u;
    }
    TEST_ASSERT_LESS_THAN_UINT(bar->titles, title);
    g_cli_cells->persistent_buf_release(bar, &g_cli_controller_cells);
    const CliInput pressed = {.kind = CLI_INPUT_POINTER_DOWN, .hit = CLI_HIT_MENU_TITLE, .index = title};
    EMBED_CALL(cli_model_commands.menu_input, CliModelCtx, .input = &pressed);
    CliMenus *const menus = (CliMenus *)EMBED_CALL(cli_model_commands.menus_alloc, CliModelCtx, .at = 0u);
    TEST_ASSERT_NOT_NULL(menus);
    return menus;
}

// the View menu opened on the bar
static CliMenus *view_menu_opened(void)
{
    return menu_opened("View");
}

// the menus closed by a press away from them
static void menus_closed(void)
{
    const CliInput away = {.kind = CLI_INPUT_POINTER_DOWN, .hit = CLI_HIT_NONE};
    EMBED_CALL(cli_model_commands.menu_input, CliModelCtx, .input = &away);
}

// the state Reduce Motion shows in the View menu opened on the bar, the menus closed again after
static int reduce_motion_checked(void)
{
    CliMenus *const menus = view_menu_opened();
    int checked = -2;
    for (unsigned int at = 0u; (menus->levels != 0u) && (at < menus->level[0].rows); at += 1u)
    {
        checked = (strcmp(menus->level[0].row[at].label, "Reduce Motion") == 0) ? menus->level[0].row[at].checked
                                                                                 : checked;
    }
    g_cli_cells->persistent_buf_release(menus, &g_cli_controller_cells);
    menus_closed();
    return checked;
}

// the system's wish for less motion handed to the controller, as the window hands it on
static void system_wish(unsigned int reduced)
{
    const CliInput wished = {.kind = CLI_INPUT_MOTION_SYSTEM, .index = reduced};
    cli_controller_model_input(&wished, NULL);
}

// the reduce-motion command run with its word, NULL for none
static void reduce_motion_run(const char *word)
{
    const char *const words[1] = {word};
    EMBED_CALL(cli_model_commands.command_run, CliModelCtx, .path = "reduce-motion", .words = words,
               .at = (word != NULL) ? 1u : 0u);
}

void test_reduce_motion_shows_on_while_the_saved_setting_or_the_system_asks_for_less_motion(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/controller_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/controller_home", 1));
#endif
    catalog_read_once();
    reduce_motion_run("off");
    system_wish(0u);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, reduce_motion_checked(), "off while neither asks for less motion");
    system_wish(1u);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(), "on while the system asks for less motion");
    reduce_motion_run(NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(),
                                  "turned from on, the saved setting is off and the system's wish still shows");
    system_wish(0u);
    TEST_ASSERT_EQUAL_INT(0, reduce_motion_checked());
    reduce_motion_run("on");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(), "its word on sets it on");
    reduce_motion_run("on");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(), "on again stays on");
    reduce_motion_run("off");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, reduce_motion_checked(), "its word off sets it off");
    reduce_motion_run(NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(), "with no word it turns from what shows");
    reduce_motion_run("off");
}

void test_a_menu_opened_anew_takes_a_new_serial_and_keeps_it_while_it_stands_open(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/controller_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/controller_home", 1));
#endif
    catalog_read_once();
    CliMenus *const opened = view_menu_opened();
    const unsigned int serial = opened->level[0].serial;
    g_cli_cells->persistent_buf_release(opened, &g_cli_controller_cells);
    // the View menu's second row is a line, which no pointer marks: its third is marked
    const CliInput moved = {.kind = CLI_INPUT_POINTER_MOVED, .hit = CLI_HIT_MENU_ROW, .level = 0u, .index = 2u};
    EMBED_CALL(cli_model_commands.menu_input, CliModelCtx, .input = &moved);
    CliMenus *const pointed = (CliMenus *)EMBED_CALL(cli_model_commands.menus_alloc, CliModelCtx, .at = 0u);
    TEST_ASSERT_NOT_NULL(pointed);
    TEST_ASSERT_EQUAL_INT(2, pointed->level[0].pointed);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(serial, pointed->level[0].serial, "a row marked leaves the menu the one opened");
    g_cli_cells->persistent_buf_release(pointed, &g_cli_controller_cells);
    menus_closed();
    CliMenus *const again = view_menu_opened();
    TEST_ASSERT_NOT_EQUAL_MESSAGE(serial, again->level[0].serial, "the menu closed and opened again is opened anew");
    g_cli_cells->persistent_buf_release(again, &g_cli_controller_cells);
    menus_closed();
}

// the tree the tabs open files of, the folder the window would be open on
static char s_root[CLI_PATH_MAX_LEN];
// what a file read back holds
static unsigned char s_read[256];

// the controller's work run until `done` reads 1, ten seconds at the most
static void work_until(const int *done)
{
    const unsigned long long until = cli_clock_milliseconds() + 10000ULL;
    while (!*done && (cli_clock_milliseconds() < until))
    {
        (void)cli_controller_work_step();
    }
    TEST_ASSERT_TRUE_MESSAGE(*done, "the work came back within ten seconds");
}

// every tab closed, a changed one by the second close it waits on
static void tabs_closed(void)
{
    while (g_cli_model.tabs != 0u)
    {
        EMBED_CALL(cli_model.tab_close, CliModelCtx, .at = g_cli_model.tabs - 1u);
    }
}

// the tree written afresh, its files `first` and `second` reading `first_text` and `second_text`, NULL for none
static void tree_made(const char *first, const char *first_text, const char *second, const char *second_text)
{
    static int s_lanes;
    if (!s_lanes)
    {
        TEST_ASSERT_TRUE(cli_controller_work_begin());
        s_lanes = 1;
    }
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/controller_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/controller_home", 1));
#endif
    catalog_read_once();
    tabs_closed();
    cli_unity_folder("build/harness/controller_tree", s_root);
    cli_unity_file(s_root, first, first_text);
    if (second != NULL)
    {
        cli_unity_file(s_root, second, second_text);
    }
    g_cli_model.root = s_root;
}

// the file `name` of the tree opened as the tab shown and its view opened on it: the tab's place
static unsigned int tab_opened(const char *name)
{
    EMBED_CALL(cli_model.file_open, CliModelCtx, .path = name);
    TEST_ASSERT_TRUE(g_cli_model.active >= 0);
    const unsigned int at = (unsigned int)g_cli_model.active;
    work_until(&g_cli_model.tab[at]->viewed);
    return at;
}

// `text` typed into the tab shown
static void typed(const char *text)
{
    const CliViewInput input = {.kind = CLI_VIEW_TEXT, .text = text, .length = strlen(text)};
    EMBED_CALL(cli_model.view_input, CliModelCtx, .input = &input, .on = 1);
}

// the key `name` pressed in the tab shown
static void pressed(const char *name)
{
    const CliViewInput input = {.kind = CLI_VIEW_KEY, .name = name};
    EMBED_CALL(cli_model.view_input, CliModelCtx, .input = &input, .on = 1);
}

// the command `name` run and its saves let finish
static void saved_by(const char *name)
{
    EMBED_CALL(cli_model_commands.command_run, CliModelCtx, .path = name);
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        int finished = 0;
        const unsigned long long until = cli_clock_milliseconds() + 10000ULL;
        while (!finished && (cli_clock_milliseconds() < until))
        {
            (void)cli_controller_work_step();
            finished = !g_cli_model.tab[at]->saving;
        }
        TEST_ASSERT_TRUE_MESSAGE(finished, "the save came back within ten seconds");
    }
}

// the file `name` of the tree asserted to read `required` whole
static void file_reads(const char *name, const char *required)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, s_root);
    mmgr_line_put(&line, "/");
    mmgr_line_put(&line, name);
    TEST_ASSERT_NOT_EQUAL_UINT64(0ULL, mmgr_line_finish(&line));
    const unsigned long long size = qry_member_file_read(path, s_read, sizeof(s_read));
    CLI_TEST_ASSERT_TEXT(required, (const char *)s_read, size);
}

// whether the File menu's row `label` can be chosen, the menus closed again after
static int file_row_enabled(const char *label)
{
    CliMenus *const menus = menu_opened("File");
    int enabled = -1;
    for (unsigned int at = 0u; (menus->levels != 0u) && (at < menus->level[0].rows); at += 1u)
    {
        enabled = (strcmp(menus->level[0].row[at].label, label) == 0) ? !menus->level[0].row[at].disabled : enabled;
    }
    g_cli_cells->persistent_buf_release(menus, &g_cli_controller_cells);
    menus_closed();
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(-1, enabled, label);
    return enabled;
}

void test_a_tab_is_changed_while_its_text_differs_from_the_file_and_not_once_the_letter_is_taken_out(void)
{
    tree_made("saved.txt", "alpha\nbeta\n", NULL, NULL);
    const unsigned int at = tab_opened("saved.txt");
    const CliModelTab *const tab = g_cli_model.tab[at];
    TEST_ASSERT_FALSE_MESSAGE(tab->changed, "a tab opened is the file as it is");
    typed("x");
    TEST_ASSERT_TRUE(tab->changed);
    pressed("Backspace");
    TEST_ASSERT_FALSE_MESSAGE(tab->changed, "the letter typed and taken out again leaves the file's text");
    typed("x");
    TEST_ASSERT_TRUE(tab->changed);
    pressed("Mod+Z");
    TEST_ASSERT_FALSE_MESSAGE(tab->changed, "undo back to the text as it opened");
    pressed("Mod+Y");
    TEST_ASSERT_TRUE(tab->changed);
    pressed("Enter");
    pressed("Backspace");
    TEST_ASSERT_TRUE_MESSAGE(tab->changed, "the line broken and joined again leaves the x typed");
}

void test_letters_moved_are_a_change_the_census_of_the_text_misses(void)
{
    tree_made("moved.txt", "ab", NULL, NULL);
    const unsigned int at = tab_opened("moved.txt");
    const CliModelTab *const tab = g_cli_model.tab[at];
    typed("b");
    pressed("End");
    pressed("Backspace");
    TEST_ASSERT_TRUE_MESSAGE(tab->changed, "ba holds the letters of ab and is not ab");
    pressed("Enter");
    TEST_ASSERT_TRUE_MESSAGE(tab->changed, "a line more is a change");
}

void test_save_writes_the_text_to_its_file_and_the_tab_is_no_longer_changed(void)
{
    tree_made("saved.txt", "alpha\n", NULL, NULL);
    const unsigned int at = tab_opened("saved.txt");
    const CliModelTab *const tab = g_cli_model.tab[at];
    typed("x");
    saved_by("save");
    file_reads("saved.txt", "xalpha\n");
    TEST_ASSERT_FALSE(tab->changed);
    pressed("Backspace");
    TEST_ASSERT_TRUE_MESSAGE(tab->changed, "the text as it opened is no longer the file's");
    typed("x");
    TEST_ASSERT_FALSE_MESSAGE(tab->changed, "the text as saved, though by another step");
}

void test_a_file_whose_lines_end_in_a_carriage_return_saves_with_them(void)
{
    tree_made("crlf.txt", "a\r\nb\r\n", NULL, NULL);
    (void)tab_opened("crlf.txt");
    typed("x");
    saved_by("save");
    file_reads("crlf.txt", "xa\r\nb\r\n");
}

void test_a_changed_tab_closes_on_the_second_close_and_an_unchanged_one_on_the_first(void)
{
    tree_made("first.txt", "one\n", "second.txt", "two\n");
    (void)tab_opened("first.txt");
    const unsigned int at = tab_opened("second.txt");
    typed("x");
    EMBED_CALL(cli_model.tab_close, CliModelCtx, .at = at);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, g_cli_model.tabs, "the changed tab stands after the first close");
    TEST_ASSERT_TRUE(g_cli_model.tab[at]->closing);
    EMBED_CALL(cli_model.tab_close, CliModelCtx, .at = at);
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_model.tabs);
    EMBED_CALL(cli_model.tab_close, CliModelCtx, .at = 0u);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, g_cli_model.tabs, "the tab with no change closes at once");
}

void test_a_save_ends_the_wait_on_a_second_close(void)
{
    tree_made("saved.txt", "one\n", NULL, NULL);
    const unsigned int at = tab_opened("saved.txt");
    typed("x");
    EMBED_CALL(cli_model.tab_close, CliModelCtx, .at = at);
    saved_by("save");
    TEST_ASSERT_FALSE(g_cli_model.tab[at]->closing);
}

void test_close_all_closes_the_unchanged_tabs_and_marks_the_changed_ones(void)
{
    tree_made("first.txt", "one\n", "second.txt", "two\n");
    (void)tab_opened("first.txt");
    (void)tab_opened("second.txt");
    typed("x");
    EMBED_CALL(cli_model_commands.command_run, CliModelCtx, .path = "close-all");
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_model.tabs);
    TEST_ASSERT_TRUE(g_cli_model.tab[0]->closing);
}

void test_save_all_writes_every_changed_tab(void)
{
    tree_made("first.txt", "one\n", "second.txt", "two\n");
    const unsigned int first = tab_opened("first.txt");
    typed("x");
    const unsigned int second = tab_opened("second.txt");
    typed("y");
    saved_by("save-all");
    file_reads("first.txt", "xone\n");
    file_reads("second.txt", "ytwo\n");
    TEST_ASSERT_FALSE(g_cli_model.tab[first]->changed);
    TEST_ASSERT_FALSE(g_cli_model.tab[second]->changed);
}

void test_save_and_save_all_can_be_chosen_only_while_a_text_is_changed(void)
{
    tree_made("first.txt", "one\n", "second.txt", "two\n");
    (void)tab_opened("first.txt");
    TEST_ASSERT_FALSE(file_row_enabled("Save"));
    TEST_ASSERT_FALSE(file_row_enabled("Save All"));
    typed("x");
    TEST_ASSERT_TRUE(file_row_enabled("Save"));
    TEST_ASSERT_TRUE(file_row_enabled("Save All"));
    (void)tab_opened("second.txt");
    TEST_ASSERT_FALSE_MESSAGE(file_row_enabled("Save"), "the tab shown is not changed");
    TEST_ASSERT_TRUE_MESSAGE(file_row_enabled("Save All"), "another tab is");
}

// the clock let run `milliseconds`
static void clock_passes(unsigned long long milliseconds)
{
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while (cli_clock_milliseconds() < until)
    {
    }
}

// the key `name` pressed as the window hands it on, the reader commanding where it is no typing
static void key_pressed(const char *name)
{
    const CliInput keyed = {.kind = CLI_INPUT_KEY};
    EMBED_CALL(cli_model_notices.input, CliModelCtx, .input = &keyed, .path = name);
}

// text typed, which commands nothing
static void text_typed(void)
{
    const CliInput typed = {.kind = CLI_INPUT_TEXT, .length = 1u};
    EMBED_CALL(cli_model_notices.input, CliModelCtx, .input = &typed, .path = "x");
}

// the notices at rest: Focus off, the timers whose time came taken, and the notice shown taken away
static void notices_rested(void)
{
    EMBED_CALL(cli_model_notices.focus, CliModelCtx, .on = 0);
    (void)EMBED_CALL(cli_model_notices.step, CliModelCtx, .at = 0u);
    if (g_cli_model.notice != NULL)
    {
        EMBED_CALL(cli_model_notices.press, CliModelCtx, .at = CLI_HIT_STATUS_NOTICE);
    }
}

// whether the status bar shows `text`
static int showing(const char *text)
{
    return (g_cli_model.notice != NULL) && (strcmp(g_cli_model.notice, text) == 0);
}

void test_a_notice_answering_a_command_shows_at_once_and_stands_five_seconds(void)
{
    notices_rested();
    key_pressed("Ctrl+S");
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "Formatted 3 places.");
    TEST_ASSERT_TRUE(showing("Formatted 3 places."));
    TEST_ASSERT_FALSE(g_cli_model.notice_failed);
    const unsigned long long deadline = EMBED_CALL(cli_model_notices.due_ms, CliModelCtx, .at = 0u);
    const unsigned long long now = cli_clock_milliseconds();
    TEST_ASSERT_TRUE_MESSAGE((deadline > now + CLI_NOTICES_SHOWN_MS - 500ULL) && (deadline <= now + CLI_NOTICES_SHOWN_MS),
                             "the word stands five seconds");
    EMBED_CALL(cli_model_notices.press, CliModelCtx, .at = CLI_HIT_STATUS_NOTICE);
    TEST_ASSERT_NULL_MESSAGE(g_cli_model.notice, "a press on the word takes it away");
}

void test_a_failure_stands_fifteen_seconds_and_shows_its_first_line(void)
{
    notices_rested();
    key_pressed("Ctrl+S");
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "saved.txt: it could not be written\nAccess is denied.",
               .on = 1);
    TEST_ASSERT_TRUE(showing("saved.txt: it could not be written"));
    TEST_ASSERT_TRUE(g_cli_model.notice_failed);
    const unsigned long long deadline = EMBED_CALL(cli_model_notices.due_ms, CliModelCtx, .at = 0u);
    TEST_ASSERT_TRUE(deadline > cli_clock_milliseconds() + CLI_NOTICES_SHOWN_MS);
    notices_rested();
}

void test_a_notice_answering_nothing_waits_for_the_work_to_rest(void)
{
    notices_rested();
    clock_passes(CLI_NOTICES_ANSWERS_MS + 100ULL);
    text_typed();
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "The run ended.");
    TEST_ASSERT_NULL_MESSAGE(g_cli_model.notice, "typing goes on, and the notice waits");
    clock_passes(CLI_NOTICES_REST_MS + 100ULL);
    (void)EMBED_CALL(cli_model_notices.step, CliModelCtx, .at = 0u);
    TEST_ASSERT_TRUE_MESSAGE(showing("The run ended."), "the keys at rest, it shows");
    notices_rested();
}

void test_a_break_shows_the_last_notice_waiting(void)
{
    notices_rested();
    clock_passes(CLI_NOTICES_ANSWERS_MS + 100ULL);
    text_typed();
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "First.");
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "Second.");
    TEST_ASSERT_NULL(g_cli_model.notice);
    EMBED_CALL(cli_model_notices.taken, CliModelCtx, .at = 0u);
    TEST_ASSERT_TRUE(showing("Second."));
    notices_rested();
}

void test_focus_holds_every_notice_until_it_ends(void)
{
    notices_rested();
    EMBED_CALL(cli_model_notices.focus, CliModelCtx, .on = 1);
    key_pressed("Ctrl+S");
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "Held by Focus.");
    TEST_ASSERT_NULL_MESSAGE(g_cli_model.notice, "Focus holds a notice that answers a command");
    TEST_ASSERT_TRUE(g_cli_model.focus);
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_model.focus_waiting);
    EMBED_CALL(cli_model_notices.taken, CliModelCtx, .at = 0u);
    TEST_ASSERT_NULL_MESSAGE(g_cli_model.notice, "a break does not end Focus");
    EMBED_CALL(cli_model_notices.press, CliModelCtx, .at = CLI_HIT_STATUS_FOCUS);
    TEST_ASSERT_FALSE_MESSAGE(g_cli_model.focus, "a press on Focus ends it");
    TEST_ASSERT_TRUE(showing("Held by Focus."));
    TEST_ASSERT_EQUAL_UINT(0u, g_cli_model.focus_waiting);
    notices_rested();
}

void test_silence_puts_a_kind_away_for_good_and_show_again_brings_it_back(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/controller_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/controller_home", 1));
#endif
    notices_rested();
    key_pressed("Ctrl+S");
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "Formatted 3 places.");
    EMBED_CALL(cli_model_notices.press, CliModelCtx, .at = CLI_HIT_STATUS_SILENCE);
    TEST_ASSERT_NULL(g_cli_model.notice);
    key_pressed("Ctrl+S");
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "Formatted 12 places.");
    TEST_ASSERT_NULL_MESSAGE(g_cli_model.notice, "the kind, its counts any, is put away");
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "Already formatted.");
    TEST_ASSERT_TRUE_MESSAGE(showing("Already formatted."), "another kind still shows");
    EMBED_CALL(cli_model_notices.show_again, CliModelCtx, .path = "Formatted # places.");
    key_pressed("Ctrl+S");
    EMBED_CALL(cli_model_notices.post, CliModelCtx, .path = "Formatted 4 places.");
    TEST_ASSERT_TRUE(showing("Formatted 4 places."));
    notices_rested();
}

void test_a_save_that_cannot_write_posts_a_failure_notice(void)
{
    tree_made("saved.txt", "one\n", NULL, NULL);
    (void)tab_opened("saved.txt");
    notices_rested();
    typed("x");
    // a folder where the file stood, which no text is moved over
    cli_unity_folder("build/harness/controller_tree", s_root);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_out_dir.create_folder, CliFilesRootOutDirCfg, .root = s_root,
                                .dir = "saved.txt", .err_msg = &err_msg));
    key_pressed("Ctrl+S");
    saved_by("save");
    TEST_ASSERT_TRUE_MESSAGE(g_cli_model.tab[g_cli_model.active]->changed, "the tab stays changed");
    TEST_ASSERT_NOT_NULL(g_cli_model.notice);
    TEST_ASSERT_TRUE(g_cli_model.notice_failed);
    CLI_TEST_ASSERT_CONTAINS("saved.txt", g_cli_model.notice);
    notices_rested();
}
