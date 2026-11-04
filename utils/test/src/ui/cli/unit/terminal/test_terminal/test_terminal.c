// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_terminal.c: an xterm reads a shell's output: letters at the cursor in their colors, the cursor moved, lines
// erased and scrolled off the top to the history, the second screen, the answers to where the cursor is, the folder a
// shell reports, wrapping at the last column and letters past ASCII cut between two reads; a shell started in a
// pseudo-terminal, what it writes drained onto its xterm, text typed while it starts sent once it settles, and its end
// printed; and the terminal's tabs, the panel opened on a tab whose shell starts as its pane is sized, the keys it
// takes and those it lets go to the window, the panel and its panes as they stand, and the lines that leave the
// xterm's top in the tab's text and its history's file; the panel's place stored as the window ends and read back as
// it starts, a tab's shell started in its folder and the rows standing at the end written to its history; and the
// panel's menu, Split Terminal shown on while a shell stands beside the one shown, opened where it was asked for
#include "ui/cli/src/command_line/cells.h"
#include "ui/cli/src/editor/view.h"
#include "ui/cli/src/machine/state.h"
#include "ui/cli/src/terminal/keys.h"

#include "ui/cli/src/terminal/shells.h"
#include "ui/cli/src/terminal/terminal_tabs.h"
#include "ui/cli/src/terminal/xterm.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"
#include <stdlib.h>
#include <string.h>

static CliTerminalXterm s_xterm;
static char s_text[256];
// the lines the xterm handed its history, each's first letter, and how many
static char s_history[16];
static unsigned int s_history_count;

// a line the xterm let go from its top: its first letter put after those before it
static void history_taken(const CliTerminalXtermLineCfg *args)
{
    if (s_history_count < sizeof(s_history))
    {
        s_history[s_history_count] = (char)args->cells[0].letter;
        s_history_count += 1u;
    }
}

// the xterm opened `columns` wide and `rows` tall, the lines it lets go taken by history_taken
static void xterm_open(unsigned int columns, unsigned int rows)
{
    s_history_count = 0u;
    s_xterm.history = (CliTerminalXtermHistory){history_taken, NULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_terminal_xterm_size.init, CliTerminalXtermSizeCfg, .xterm = &s_xterm,
                                .columns = columns, .rows = rows));
}

// `text`, a C string, read onto the xterm
static void xterm_put(const char *text)
{
    unsigned long long size = 0ULL;
    while (text[size] != '\0')
    {
        size += 1ULL;
    }
    EMBED_CALL(cli_terminal_xterm_bytes.write, CliTerminalXtermBytesCfg, .xterm = &s_xterm,
               .bytes = (const unsigned char *)text, .size = size);
}

// the letters of row `y` of the screen drawn on, each below 128, its blanks at the end left off
static const char *xterm_row_text(unsigned int y)
{
    const CliTerminalCell *const row = s_xterm.shown + ((unsigned long long)y * s_xterm.columns);
    unsigned int end = s_xterm.columns;
    while ((end > 0u) && (row[end - 1u].letter == (unsigned int)' '))
    {
        end -= 1u;
    }
    for (unsigned int x = 0u; x < end; x += 1u)
    {
        s_text[x] = (char)row[x].letter;
    }
    s_text[end] = '\0';
    return s_text;
}

// the bytes the key `name` sends with the modifiers `mods` into s_text, ended by a 0: 1, or 0 where it sends nothing
static int key_sent(const char *name, unsigned int mods, int application_cursor)
{
    unsigned int size = 0u;
    while (name[size] != '\0')
    {
        size += 1u;
    }
    VerbaLinea out = {s_text, sizeof(s_text), 0ULL};
    const int sends = EMBED_CALL(cli_terminal_key.bytes, CliTerminalKeyCfg, .name = name, .name_size = size,
                                 .mods = mods, .application_cursor = application_cursor, .out = &out);
    (void)mmgr_line_finish(&out);
    return sends;
}

void setUp(void)
{
    cli_unity_begun();
}

// the terminal ended as the window ends it
static void terminal_ended(void)
{
    cli_terminal_tabs_end();
}

void test_the_cursor_keys_send_plain_application_and_modified_sequences(void)
{
    TEST_ASSERT_TRUE(key_sent("Up", 0u, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b[A", s_text);
    TEST_ASSERT_TRUE(key_sent("Up", 0u, 1));
    TEST_ASSERT_EQUAL_STRING("\x1bOA", s_text);
    TEST_ASSERT_TRUE(key_sent("Left", CLI_TERMINAL_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b[1;5D", s_text);
    TEST_ASSERT_TRUE(key_sent("End", CLI_TERMINAL_MOD_SHIFT | CLI_TERMINAL_MOD_ALT, 1));
    TEST_ASSERT_EQUAL_STRING("\x1b[1;4F", s_text);
}

void test_the_keys_with_sequences_of_their_own_send_them_alt_putting_an_escape_first(void)
{
    TEST_ASSERT_TRUE(key_sent("Tab", CLI_TERMINAL_MOD_SHIFT, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b[Z", s_text);
    TEST_ASSERT_TRUE(key_sent("Enter", CLI_TERMINAL_MOD_ALT, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b\r", s_text);
    TEST_ASSERT_TRUE(key_sent("F5", 0u, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b[15~", s_text);
    TEST_ASSERT_TRUE(key_sent("Backspace", CLI_TERMINAL_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_STRING("\x08", s_text);
}

void test_control_sends_control_letters_and_alt_an_escape_and_the_letter(void)
{
    TEST_ASSERT_TRUE(key_sent("A", CLI_TERMINAL_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_STRING("\x01", s_text);
    TEST_ASSERT_TRUE(key_sent("[", CLI_TERMINAL_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b", s_text);
    TEST_ASSERT_TRUE(key_sent("Space", CLI_TERMINAL_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_UINT(0u, (unsigned int)s_text[0]);
    TEST_ASSERT_TRUE(key_sent("/", CLI_TERMINAL_MOD_CONTROL | CLI_TERMINAL_MOD_SHIFT, 0));
    TEST_ASSERT_EQUAL_STRING("\x7f", s_text);
    TEST_ASSERT_TRUE(key_sent("A", CLI_TERMINAL_MOD_ALT, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b"
                             "a",
                             s_text);
    TEST_ASSERT_TRUE(key_sent("A", CLI_TERMINAL_MOD_ALT | CLI_TERMINAL_MOD_SHIFT, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b"
                             "A",
                             s_text);
    TEST_ASSERT_FALSE(key_sent("A", 0u, 0));
    TEST_ASSERT_FALSE(key_sent("2", CLI_TERMINAL_MOD_CONTROL, 0));
}

void tearDown(void)
{
    EMBED_CALL(cli_terminal_xterm.free, CliTerminalXtermCfg, .xterm = &s_xterm);
}

void test_letters_land_at_the_cursor_and_a_return_and_a_feed_start_the_next_line(void)
{
    xterm_open(10u, 4u);
    xterm_put("ab\r\ncd");
    TEST_ASSERT_EQUAL_STRING("ab", xterm_row_text(0u));
    TEST_ASSERT_EQUAL_STRING("cd", xterm_row_text(1u));
    TEST_ASSERT_EQUAL_UINT(2u, s_xterm.cursor.x);
    TEST_ASSERT_EQUAL_UINT(1u, s_xterm.cursor.y);
}

void test_the_16_colors_and_bold_are_set_and_a_reset_takes_them_off(void)
{
    xterm_open(10u, 2u);
    xterm_put("\x1b[31;1mX\x1b[0mY\x1b[94mZ");
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_INDEXED | 1u, s_xterm.shown[0].foreground);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_BOLD, s_xterm.shown[0].marks);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_DEFAULT, s_xterm.shown[1].foreground);
    TEST_ASSERT_EQUAL_HEX32(0u, s_xterm.shown[1].marks);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_INDEXED | 12u, s_xterm.shown[2].foreground);
}

void test_the_256_colors_and_direct_colors_are_read_an_empty_color_space_among_them(void)
{
    xterm_open(10u, 2u);
    xterm_put("\x1b[38;5;200mA\x1b[48;2;1;2;3mB\x1b[38:2::4:5:6mC");
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_INDEXED | 200u, s_xterm.shown[0].foreground);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_DIRECT | 0x010203u, s_xterm.shown[1].background);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_DIRECT | 0x040506u, s_xterm.shown[2].foreground);
}

void test_where_the_cursor_is_is_answered_after_it_is_placed(void)
{
    xterm_open(10u, 5u);
    xterm_put("\x1b[3;5H\x1b[6n");
    TEST_ASSERT_EQUAL_UINT(4u, s_xterm.cursor.x);
    TEST_ASSERT_EQUAL_UINT(2u, s_xterm.cursor.y);
    TEST_ASSERT_EQUAL_UINT(6u, s_xterm.answer_size);
    TEST_ASSERT_EQUAL_MEMORY("\x1b[3;5R", s_xterm.answer, 6u);
}

void test_a_line_is_erased_from_the_cursor_to_its_end(void)
{
    xterm_open(10u, 2u);
    xterm_put("abcdef\x1b[3G\x1b[K");
    TEST_ASSERT_EQUAL_STRING("ab", xterm_row_text(0u));
}

void test_lines_scrolled_off_the_main_screen_go_to_the_history_and_a_clear_of_it_is_marked(void)
{
    xterm_open(10u, 3u);
    xterm_put("1\r\n2\r\n3\r\n4\r\n5");
    TEST_ASSERT_EQUAL_STRING("3", xterm_row_text(0u));
    TEST_ASSERT_EQUAL_UINT(2u, s_history_count);
    TEST_ASSERT_EQUAL_MEMORY("12", s_history, 2u);
    TEST_ASSERT_FALSE(s_xterm.history_cleared);
    xterm_put("\x1b[3J");
    TEST_ASSERT_TRUE(s_xterm.history_cleared);
}

void test_the_second_screen_is_drawn_on_and_left_with_the_main_screen_as_it_was(void)
{
    xterm_open(10u, 3u);
    xterm_put("main\x1b[?1049h");
    TEST_ASSERT_TRUE(s_xterm.shown == s_xterm.second);
    TEST_ASSERT_EQUAL_STRING("", xterm_row_text(0u));
    xterm_put("full\x1b[?1049l");
    TEST_ASSERT_TRUE(s_xterm.shown == s_xterm.main);
    TEST_ASSERT_EQUAL_STRING("main", xterm_row_text(0u));
    TEST_ASSERT_EQUAL_UINT(0u, s_history_count);
}

void test_the_folder_a_shell_reports_is_read_from_osc_7_ended_by_a_bell_or_a_string_terminator(void)
{
    xterm_open(10u, 2u);
    xterm_put("\x1b]7;file://host/d/x\x07");
    TEST_ASSERT_TRUE(s_xterm.place_reported);
    TEST_ASSERT_EQUAL_STRING("file://host/d/x", s_xterm.place);
    xterm_put("\x1b]7;file://host/e\x1b\\");
    TEST_ASSERT_EQUAL_STRING("file://host/e", s_xterm.place);
    TEST_ASSERT_EQUAL_STRING("", xterm_row_text(0u));
}

void test_a_letter_at_the_last_column_wraps_the_next_onto_the_next_line(void)
{
    xterm_open(4u, 3u);
    xterm_put("abcde");
    TEST_ASSERT_EQUAL_STRING("abcd", xterm_row_text(0u));
    TEST_ASSERT_EQUAL_STRING("e", xterm_row_text(1u));
}

void test_a_letter_past_ascii_is_read_whole_across_two_reads(void)
{
    xterm_open(10u, 2u);
    xterm_put("\xc3");
    xterm_put("\xa9!");
    TEST_ASSERT_EQUAL_HEX32(0xe9u, s_xterm.shown[0].letter);
    TEST_ASSERT_EQUAL_UINT((unsigned int)'!', s_xterm.shown[1].letter);
    xterm_put("\xff");
    TEST_ASSERT_EQUAL_HEX32(0xfffdu, s_xterm.shown[2].letter);
}

void test_lines_inserted_and_deleted_stay_inside_the_region(void)
{
    xterm_open(10u, 4u);
    xterm_put("a\r\nb\r\nc\r\nd\x1b[2;3r\x1b[2;1H\x1b[L");
    TEST_ASSERT_EQUAL_STRING("a", xterm_row_text(0u));
    TEST_ASSERT_EQUAL_STRING("", xterm_row_text(1u));
    TEST_ASSERT_EQUAL_STRING("b", xterm_row_text(2u));
    TEST_ASSERT_EQUAL_STRING("d", xterm_row_text(3u));
    xterm_put("\x1b[M\x1b[M");
    TEST_ASSERT_EQUAL_STRING("", xterm_row_text(1u));
    TEST_ASSERT_EQUAL_STRING("", xterm_row_text(2u));
    TEST_ASSERT_EQUAL_STRING("d", xterm_row_text(3u));
    TEST_ASSERT_EQUAL_UINT(0u, s_history_count);
}

void test_a_smaller_xterm_sends_the_lines_above_the_cursor_to_the_history(void)
{
    xterm_open(10u, 4u);
    xterm_put("1\r\n2\r\n3\r\n4");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_terminal_xterm_size.resize, CliTerminalXtermSizeCfg, .xterm = &s_xterm,
                                .columns = 3u, .rows = 2u));
    TEST_ASSERT_EQUAL_STRING("3", xterm_row_text(0u));
    TEST_ASSERT_EQUAL_STRING("4", xterm_row_text(1u));
    TEST_ASSERT_EQUAL_UINT(1u, s_xterm.cursor.y);
    TEST_ASSERT_EQUAL_UINT(2u, s_history_count);
    TEST_ASSERT_EQUAL_MEMORY("12", s_history, 2u);
}

// the shell `words` started on an 80 by 24 xterm, `typed` typed into it at once where it is not NULL, and drained
// onto the xterm until it stands exited or ten seconds pass, then hung up on and drained until it ends: 1 where it
// ended of itself
static int shell_run(const char *const *words, const char *typed)
{
    static CliTerminalShell s_shell;
    static CliWake s_pause;
    xterm_open(80u, 24u);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int started = EMBED_CALL(cli_terminal_shell.start, CliTerminalShellCfg, .shell = &s_shell, .xterm = &s_xterm,
                                   .words = words, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    TEST_ASSERT_TRUE_MESSAGE(started, why);
    unsigned long long typed_size = 0ULL;
    while ((typed != NULL) && (typed[typed_size] != '\0'))
    {
        typed_size += 1ULL;
    }
    EMBED_CALL(cli_terminal_shell.write, CliTerminalShellCfg, .shell = &s_shell, .bytes = (const unsigned char *)typed,
               .size = typed_size);
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while ((s_shell.state == CLI_TERMINAL_SHELL_RUNNING) && (cli_clock_milliseconds() < last_ms))
    {
        (void)EMBED_CALL(cli_terminal_shell.drain, CliTerminalShellCfg, .shell = &s_shell, .xterm = &s_xterm,
                         .now_ms = cli_clock_milliseconds());
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_pause, .milliseconds_u64 = 5ULL);
    }
    const int ended = s_shell.state == CLI_TERMINAL_SHELL_EXITED;
    EMBED_CALL(cli_terminal_shell.end, CliTerminalShellCfg, .shell = &s_shell, .now_ms = cli_clock_milliseconds());
    while (s_shell.state == CLI_TERMINAL_SHELL_RUNNING)
    {
        (void)EMBED_CALL(cli_terminal_shell.drain, CliTerminalShellCfg, .shell = &s_shell, .xterm = NULL,
                         .now_ms = cli_clock_milliseconds());
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_pause, .milliseconds_u64 = 5ULL);
    }
    return ended;
}

// whether a row of the screen drawn on holds `text`, a C string
static int xterm_holds(const char *text)
{
    for (unsigned int y = 0u; y < s_xterm.rows; y += 1u)
    {
        const char *const row = xterm_row_text(y);
        for (unsigned int from = 0u; row[from] != '\0'; from += 1u)
        {
            unsigned int at = 0u;
            while ((text[at] != '\0') && (row[from + at] == text[at]))
            {
                at += 1u;
            }
            if (text[at] == '\0')
            {
                return 1;
            }
        }
    }
    return 0;
}

void test_what_a_shell_writes_reaches_its_xterm_and_its_end_is_printed_after_it(void)
{
#if defined(_WIN32)
    const char *const words[4] = {"cmd.exe", "/c", "echo shell_heard", NULL};
#else
    const char *const words[4] = {"/bin/sh", "-c", "printf shell_heard", NULL};
#endif
    TEST_ASSERT_TRUE(shell_run(words, NULL));
    TEST_ASSERT_TRUE(xterm_holds("shell_heard"));
    TEST_ASSERT_TRUE(xterm_holds("[exited 0]"));
}

void test_text_typed_while_a_shell_starts_reaches_it_once_the_shell_settles(void)
{
#if defined(_WIN32)
    const char *const words[5] = {"cmd.exe", "/v:on", "/c", "set /p line=? & echo got !line!", NULL};
#else
    const char *const words[4] = {"/bin/sh", "-c", "printf '? '; read line; printf 'got %s' \"$line\"", NULL};
#endif
    TEST_ASSERT_TRUE(shell_run(words, "abc\r"));
    TEST_ASSERT_TRUE(xterm_holds("got abc"));
    TEST_ASSERT_TRUE(xterm_holds("[exited 0]"));
}

// whether a row of the pane `view` shows `text`, a C string
static int pane_holds(unsigned int view, const char *text)
{
    CliTerminalView shown;
    if (!cli_terminal_tabs_view(view, &shown) || (shown.cell == NULL))
    {
        return 0;
    }
    for (unsigned int y = 0u; y < shown.rows; y += 1u)
    {
        for (unsigned int from = 0u; from < shown.columns; from += 1u)
        {
            unsigned int at = 0u;
            while ((text[at] != '\0') && ((from + at) < shown.columns) &&
                   (shown.cell[((unsigned long long)y * shown.columns) + from + at].letter == (unsigned int)text[at]))
            {
                at += 1u;
            }
            if (text[at] == '\0')
            {
                return 1;
            }
        }
    }
    return 0;
}

// the tabs stepped until the pane shown shows `text` or ten seconds pass: 1 where it does
static int tabs_until(const char *text)
{
    static CliWake s_pause;
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while (cli_clock_milliseconds() < last_ms)
    {
        (void)cli_terminal_tabs_step();
        if (pane_holds(0u, text))
        {
            return 1;
        }
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_pause,
                         .milliseconds_u64 = CLI_TERMINAL_LOOK_MS);
    }
    return 0;
}

// the label of the tab `at` into s_text: 1 where its shell ended
static int tab_label(unsigned int at)
{
    VerbaLinea out = {s_text, sizeof(s_text), 0ULL};
    const int exited = cli_terminal_tabs_label(at, &out);
    (void)mmgr_line_finish(&out);
    return exited;
}

// `text`, a C string, typed where the panel takes the keys: 1 where it does
static int typed(const char *text)
{
    return cli_terminal_tabs_text(text, strlen(text));
}

void test_the_terminal_opens_on_a_tab_whose_shell_takes_what_is_typed_and_shows_what_it_writes(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_SHELL", "cmd.exe"));
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/terminal_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_SHELL", "/bin/sh", 1));
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/terminal_home", 1));
#endif
    // the first tab's history emptied, which it reads back as it starts
    char home[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home));
    char history_path[CLI_PATH_MAX_LEN];
    VerbaLinea history_line = {history_path, sizeof(history_path), 0ull};
    mmgr_line_put(&history_line, home);
    mmgr_line_put(&history_line, "/terminals/1");
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_line_finish(&history_line));
    (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = history_path, .bytes = "",
                     .size = 0ull, .fresh = 1);
    cli_terminal_tabs_restore(NULL, NULL);
    TEST_ASSERT_TRUE(cli_terminal_tabs_command("toggle", NULL, 0u));
    cli_terminal_tabs_size(0u, 80u, 24u);
    const CliTerminalPanel opened = cli_terminal_tabs_panel();
    TEST_ASSERT_TRUE(opened.open);
    TEST_ASSERT_TRUE(opened.keys);
    TEST_ASSERT_EQUAL_UINT(1u, opened.tabs);
    TEST_ASSERT_FALSE(tab_label(0u));
    TEST_ASSERT_EQUAL_STRING("1 shell", s_text);
    TEST_ASSERT_EQUAL_UINT(1u, opened.views);
    CliTerminalView pane;
    TEST_ASSERT_TRUE(cli_terminal_tabs_view(0u, &pane));
    TEST_ASSERT_EQUAL_UINT(80u, pane.columns);
    TEST_ASSERT_EQUAL_UINT(24u, pane.rows);
    // the pane is a view of the tab's text, standing on the xterm's rows
    TEST_ASSERT_NOT_NULL(pane.key);
    CliViewFrame *const frame = cli_view_frame_alloc(pane.key);
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_UINT64(pane.xterm_line, frame->top);
    cli_view_frame_release(frame);
    // Control with a letter goes to the shell, a plain letter comes as text, and Ctrl+` goes on to the window
    TEST_ASSERT_TRUE(cli_terminal_tabs_key("A"));
    TEST_ASSERT_FALSE(cli_terminal_tabs_key("Ctrl+`"));
    // what is typed and echoed reads `tabs^_heard` or `tabs''_heard`; only the shell's output reads `tabs_heard`
#if defined(_WIN32)
    TEST_ASSERT_TRUE(typed("echo tabs^_heard"));
#else
    TEST_ASSERT_TRUE(typed("echo tabs''_heard"));
#endif
    TEST_ASSERT_TRUE(cli_terminal_tabs_key("Enter"));
    TEST_ASSERT_TRUE(tabs_until("tabs_heard"));
    // the Space key sends nothing of itself: the space it types comes as text
    TEST_ASSERT_TRUE(typed("echo key"));
    TEST_ASSERT_TRUE(cli_terminal_tabs_key("Space"));
    TEST_ASSERT_TRUE(typed("_alone"));
    TEST_ASSERT_TRUE(cli_terminal_tabs_key("Enter"));
    TEST_ASSERT_TRUE(tabs_until("key_alone"));
    TEST_ASSERT_TRUE(typed("exit"));
    TEST_ASSERT_TRUE(cli_terminal_tabs_key("Enter"));
    TEST_ASSERT_TRUE(tabs_until("[exited 0]"));
    TEST_ASSERT_TRUE(tab_label(0u));
    terminal_ended();
}

// the values stored in orior's folder, read as the window reads them as it starts: NULL where none could be read
static CliStateEntries *entries_read(void)
{
    char folder[CLI_PATH_MAX_LEN];
    return EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder)
               ? cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = &g_checks_cells})
               : NULL;
}

// the tabs' place read back from what orior's folder stores, as the window reads it as it starts
static void place_restored(void)
{
    CliStateEntries *const entries = entries_read();
    TEST_ASSERT_NOT_NULL(entries);
    cli_terminal_tabs_restore(entries, NULL);
    g_cli_cells->persistent_buf_release(entries, &g_checks_cells);
}

void test_the_terminal_place_is_stored_as_the_window_ends_and_read_back_as_it_starts(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/terminal_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/terminal_home", 1));
#endif
    cli_terminal_tabs_restore(NULL, NULL);
    TEST_ASSERT_TRUE(cli_terminal_tabs_command("toggle", NULL, 0u));
    TEST_ASSERT_TRUE(cli_terminal_tabs_command("new", NULL, 0u));
    TEST_ASSERT_TRUE(cli_terminal_tabs_command("split", NULL, 0u));
    cli_terminal_tabs_height(300u);
    const CliTerminalPanel before = cli_terminal_tabs_panel();
    const int shown = before.shown;
    const int beside = before.beside;
    TEST_ASSERT_EQUAL_UINT(2u, before.tabs);
    TEST_ASSERT_TRUE((shown >= 0) && (beside >= 0) && (shown != beside));
    terminal_ended();
    place_restored();
    const CliTerminalPanel after = cli_terminal_tabs_panel();
    // the panel opens as it stood, without taking the keys, its tabs, the one shown, the one beside it and its height
    TEST_ASSERT_TRUE(after.open);
    TEST_ASSERT_FALSE(after.keys);
    TEST_ASSERT_EQUAL_UINT(2u, after.tabs);
    TEST_ASSERT_EQUAL_INT(shown, after.shown);
    TEST_ASSERT_EQUAL_INT(beside, after.beside);
    TEST_ASSERT_EQUAL_UINT(300u, after.height);
    (void)tab_label(0u);
    TEST_ASSERT_EQUAL_STRING("1 shell", s_text);
    (void)tab_label(1u);
    TEST_ASSERT_EQUAL_STRING("2 shell", s_text);
    terminal_ended();
}

void test_a_tab_read_back_starts_its_shell_in_its_folder_and_its_rows_reach_its_history_as_the_window_ends(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_SHELL", "cmd.exe"));
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/terminal_home"));
    const char *const asked = "cd";
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_SHELL", "/bin/sh", 1));
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/terminal_home", 1));
    const char *const asked = "pwd";
#endif
    char home[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home));
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea folder_line = {folder, sizeof(folder), 0ull};
    mmgr_line_put(&folder_line, home);
    mmgr_line_put(&folder_line, "/place_folder");
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_line_finish(&folder_line));
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder));
    // the place as the window stores it: open, the first tab shown and none beside, the height, then a tab a line
    char place[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea place_line = {place, sizeof(place), 0ull};
    mmgr_line_put(&place_line, "1 1 0 256\n1 ");
    mmgr_line_put(&place_line, folder);
    mmgr_line_ch(&place_line, '\n');
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_line_finish(&place_line));
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(cli_state_value_save(
        &(CliStateValueCfg){.folder = home, .key = "terminal.place", .value = place, .err_msg = &err_msg}));
    char history_path[CLI_PATH_MAX_LEN];
    VerbaLinea history_line = {history_path, sizeof(history_path), 0ull};
    mmgr_line_put(&history_line, home);
    mmgr_line_put(&history_line, "/terminals/1");
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_line_finish(&history_line));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = history_path,
                                .bytes = "line_before\n", .size = 12ull, .fresh = 1));
    place_restored();
    cli_terminal_tabs_size(0u, 120u, 24u);
    // the history read back stands in the tab's text above the xterm's rows, its one line
    CliTerminalView restored;
    TEST_ASSERT_TRUE(cli_terminal_tabs_view(0u, &restored));
    TEST_ASSERT_EQUAL_UINT64(1ull, restored.xterm_line);
    // the panel opens as it stood, without the keys, which a press in its pane gives it
    TEST_ASSERT_FALSE(cli_terminal_tabs_panel().keys);
    cli_terminal_tabs_press(&(CliTerminalTabsPressCfg){.hit = CLI_TERMINAL_HIT_PANE, .index = 0u, .clicks = 1u});
    TEST_ASSERT_TRUE(cli_terminal_tabs_pointer(&(CliTerminalTabsPressCfg){.hit = CLI_TERMINAL_HIT_PANE, .up = 1}));
    TEST_ASSERT_TRUE(cli_terminal_tabs_panel().keys);
    TEST_ASSERT_TRUE(typed(asked));
    TEST_ASSERT_TRUE(cli_terminal_tabs_key("Enter"));
    TEST_ASSERT_TRUE(tabs_until("place_folder"));
    terminal_ended();
    unsigned long long size = 0ull;
    const unsigned char *const history = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = history_path,
                                                    .size_pointer = &size, .owner = &g_cli_terminal_cells);
    TEST_ASSERT_NOT_NULL(history);
    int found = 0;
    for (unsigned long long at = 0ull; (at + 12ull) <= size; at += 1ull)
    {
        found = found || (memcmp(history + at, "place_folder", 12u) == 0);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .owner = &g_cli_terminal_cells, .bytes_bytes_other = history);
    TEST_ASSERT_TRUE_MESSAGE(found, "the rows standing as the window ended are in the tab's history");
}

// the answers the window gives the menus, as they read for the terminal's rows: an item that needs a state of the
// terminal's cannot be chosen while it is off, and an item shows the terminal's state its `checks` names
static int menu_item_disabled(const CliItem *item)
{
    return (item->needs[0] != '\0') && !cli_terminal_tabs_checked(item->needs);
}

static int menu_item_checked(const CliItem *item)
{
    return cli_terminal_tabs_checked(item->checks);
}

// the catalog read and the menus handed the window's answers
static void menus_answered(void)
{
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &err_msg));
    g_cli_menus_open.disabled = menu_item_disabled;
    g_cli_menus_open.checked = menu_item_checked;
}

// the row of the first menu open whose label is `label`, a C string, into `item`, asserted found
static void menu_row(const char *label, CliItem *item)
{
    for (unsigned int at = 0u; (g_cli_menus_open.levels != 0u) && (at < g_cli_menus_open.level[0].rows); at += 1u)
    {
        if (cli_menus_row(0u, at, item) && (strcmp(item->label, label) == 0))
        {
            return;
        }
    }
    TEST_FAIL_MESSAGE(label);
}

// the Terminal menu opened on the bar, every title shown
static void terminal_menu_opened(void)
{
    unsigned int title = 0u;
    while ((title < cli_menu_titles()) && (strcmp(cli_menu_title_text(cli_menu_title_menu(title)), "Terminal") != 0))
    {
        title += 1u;
    }
    TEST_ASSERT_LESS_THAN_UINT(cli_menu_titles(), title);
    cli_menus_bar(cli_menu_titles(), 0);
    cli_menus_title_press(title);
}

void test_the_terminal_menu_shows_split_terminal_on_while_a_shell_stands_beside_the_one_shown(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/terminal_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/terminal_home", 1));
#endif
    menus_answered();
    cli_terminal_tabs_restore(NULL, NULL);
    TEST_ASSERT_TRUE(cli_terminal_tabs_command("toggle", NULL, 0u));
    terminal_menu_opened();
    CliItem split;
    CliItem toggle;
    menu_row("Split Terminal", &split);
    menu_row("Toggle Terminal", &toggle);
    TEST_ASSERT_FALSE_MESSAGE(g_cli_menus_open.checked(&split), "Split Terminal is off with one shell shown");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", toggle.checks, "Toggle Terminal is neither on nor off");
    cli_menus_close(0u);
    TEST_ASSERT_TRUE(cli_terminal_tabs_command("split", NULL, 0u));
    terminal_menu_opened();
    menu_row("Split Terminal", &split);
    TEST_ASSERT_TRUE_MESSAGE(g_cli_menus_open.checked(&split), "Split Terminal is on while a shell stands beside");
    cli_menus_close(0u);
    terminal_ended();
}

// the panel's menu opened at x, y, as the window opens it where it is asked for on the panel
static void terminal_menu_asked(int x, int y)
{
    cli_menus_own_open(cli_terminal_tabs_menu(), CLI_TERMINAL_MENU_ROWS, CLI_MENU_PLACE_POINT, x, y);
}

// the key `name` taken by the menus, a row chosen run as the window runs the terminal's commands
static void menus_key(const char *name)
{
    CliItem chosen;
    int chose = 0;
    (void)cli_menus_key(name, &chosen, &chose);
    if (chose)
    {
        TEST_ASSERT_TRUE(cli_terminal_tabs_command(chosen.command, NULL, 0u));
    }
}

void test_a_menu_asked_for_on_the_terminal_opens_its_items_where_it_was_asked_the_first_it_can_choose_marked(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/terminal_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/terminal_home", 1));
#endif
    menus_answered();
    cli_terminal_tabs_restore(NULL, NULL);
    TEST_ASSERT_TRUE(cli_terminal_tabs_command("toggle", NULL, 0u));
    terminal_menu_asked(300, 400);
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_menus_open.levels);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, g_cli_menus_open.open, "a menu asked for stands under no title of the bar");
    TEST_ASSERT_EQUAL_UINT(CLI_MENU_PLACE_POINT, g_cli_menus_open.place);
    TEST_ASSERT_EQUAL_INT(300, g_cli_menus_open.x);
    TEST_ASSERT_EQUAL_INT(400, g_cli_menus_open.y);
    static const char *const labels[7] = {"Copy", "Paste", "", "Clear", "New Terminal", "Split Terminal", "Close"};
    static const char *const keys[7] = {"Ctrl+Shift+C", "Ctrl+Shift+V", "", "Ctrl+L", "Ctrl+Shift+`", "", "Ctrl+`"};
    TEST_ASSERT_EQUAL_UINT(7u, g_cli_menus_open.level[0].rows);
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        CliItem row;
        TEST_ASSERT_TRUE(cli_menus_row(0u, at, &row));
        TEST_ASSERT_EQUAL_STRING(labels[at], row.label);
        TEST_ASSERT_EQUAL_STRING(keys[at], row.keys);
        TEST_ASSERT_EQUAL_STRING_MESSAGE((at == 5u) ? "terminal-split" : "", row.checks,
                                         "Split Terminal alone shows a state");
        TEST_ASSERT_EQUAL_INT_MESSAGE(at == 2u, row.line, "the third row is a line");
    }
    CliItem split;
    TEST_ASSERT_TRUE(cli_menus_row(0u, 5u, &split));
    TEST_ASSERT_FALSE_MESSAGE(g_cli_menus_open.checked(&split), "Split Terminal is off with one shell");
    TEST_ASSERT_FALSE_MESSAGE(cli_menus_selectable(0u, 0u), "Copy cannot be chosen while nothing is chosen");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, g_cli_menus_open.level[0].pointed, "the first row that can be chosen is marked");
    // Right opens no title of the bar from a menu asked for, Down marks the next row, and Enter chooses it
    menus_key("Right");
    menus_key("Down");
    TEST_ASSERT_EQUAL_INT(-1, g_cli_menus_open.open);
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_menus_open.levels);
    TEST_ASSERT_EQUAL_INT(3, g_cli_menus_open.level[0].pointed);
    menus_key("Down");
    menus_key("Down");
    menus_key("Enter");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, g_cli_menus_open.levels, "a row chosen closes the menu");
    TEST_ASSERT_TRUE_MESSAGE(cli_terminal_tabs_panel().beside >= 0,
                             "Split Terminal chosen shows a shell beside the one shown");
    terminal_menu_asked(20, 30);
    TEST_ASSERT_TRUE(g_cli_menus_open.checked(&split));
    // Tab closes it
    menus_key("Tab");
    TEST_ASSERT_EQUAL_UINT(0u, g_cli_menus_open.levels);
    terminal_ended();
}

void test_the_panel_takes_the_keys_as_it_opens_and_a_press_elsewhere_takes_them_away(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/terminal_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/terminal_home", 1));
#endif
    cli_terminal_tabs_restore(NULL, NULL);
    TEST_ASSERT_FALSE_MESSAGE(cli_terminal_tabs_checked("terminal-keys"), "a panel closed takes no keys");
    TEST_ASSERT_TRUE(cli_terminal_tabs_command("toggle", NULL, 0u));
    TEST_ASSERT_TRUE_MESSAGE(cli_terminal_tabs_checked("terminal-keys"), "the panel opened takes the keys");
    cli_terminal_tabs_press(&(CliTerminalTabsPressCfg){.hit = 0u});
    TEST_ASSERT_FALSE_MESSAGE(cli_terminal_tabs_checked("terminal-keys"), "a press elsewhere takes them away");
    TEST_ASSERT_FALSE_MESSAGE(cli_terminal_tabs_key("A"), "a key goes on to the window");
    terminal_ended();
}
