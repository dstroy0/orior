// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_terminal.c: a terminal's screen reads a shell's output as an xterm reads it: letters at the cursor in their
// colors, the cursor moved, lines erased and scrolled into the scrollback, the second screen, the answers to where the
// cursor is, the folder a shell reports, wrapping at the last column and letters past ASCII cut between two reads
#include "ui/cli/src/controller/controller.h"
#include "ui/cli/src/terminal/keys.h"
#include "ui/cli/src/terminal/screen.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliTerminalScreen s_screen;
static char s_text[256];

// the screen made `columns` wide and `rows` tall
static void screen_open(unsigned int columns, unsigned int rows)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_terminal_screen_size.init, CliTerminalScreenSizeCfg, .screen = &s_screen,
                                .columns = columns, .rows = rows));
}

// `text`, a C string, read onto the screen
static void screen_put(const char *text)
{
    unsigned long long size = 0ULL;
    while (text[size] != '\0')
    {
        size += 1ULL;
    }
    EMBED_CALL(cli_terminal_screen_bytes.write, CliTerminalScreenBytesCfg, .screen = &s_screen,
               .bytes = (const unsigned char *)text, .size = size);
}

// the letters of row `y` of the screen drawn on, each below 128, its blanks at the end left off
static const char *screen_row_text(unsigned int y)
{
    const CliTerminalCell *const row = s_screen.shown + ((unsigned long long)y * s_screen.columns);
    unsigned int end = s_screen.columns;
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

void test_the_cursor_keys_send_plain_application_and_modified_sequences(void)
{
    TEST_ASSERT_TRUE(key_sent("Up", 0u, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b[A", s_text);
    TEST_ASSERT_TRUE(key_sent("Up", 0u, 1));
    TEST_ASSERT_EQUAL_STRING("\x1bOA", s_text);
    TEST_ASSERT_TRUE(key_sent("Left", CLI_INPUT_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b[1;5D", s_text);
    TEST_ASSERT_TRUE(key_sent("End", CLI_INPUT_MOD_SHIFT | CLI_INPUT_MOD_ALT, 1));
    TEST_ASSERT_EQUAL_STRING("\x1b[1;4F", s_text);
}

void test_the_keys_with_sequences_of_their_own_send_them_alt_putting_an_escape_first(void)
{
    TEST_ASSERT_TRUE(key_sent("Tab", CLI_INPUT_MOD_SHIFT, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b[Z", s_text);
    TEST_ASSERT_TRUE(key_sent("Enter", CLI_INPUT_MOD_ALT, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b\r", s_text);
    TEST_ASSERT_TRUE(key_sent("F5", 0u, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b[15~", s_text);
    TEST_ASSERT_TRUE(key_sent("Backspace", CLI_INPUT_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_STRING("\x08", s_text);
}

void test_control_sends_control_letters_and_alt_an_escape_and_the_letter(void)
{
    TEST_ASSERT_TRUE(key_sent("A", CLI_INPUT_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_STRING("\x01", s_text);
    TEST_ASSERT_TRUE(key_sent("[", CLI_INPUT_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b", s_text);
    TEST_ASSERT_TRUE(key_sent("Space", CLI_INPUT_MOD_CONTROL, 0));
    TEST_ASSERT_EQUAL_UINT(0u, (unsigned int)s_text[0]);
    TEST_ASSERT_TRUE(key_sent("/", CLI_INPUT_MOD_CONTROL | CLI_INPUT_MOD_SHIFT, 0));
    TEST_ASSERT_EQUAL_STRING("\x7f", s_text);
    TEST_ASSERT_TRUE(key_sent("A", CLI_INPUT_MOD_ALT, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b"
                             "a",
                             s_text);
    TEST_ASSERT_TRUE(key_sent("A", CLI_INPUT_MOD_ALT | CLI_INPUT_MOD_SHIFT, 0));
    TEST_ASSERT_EQUAL_STRING("\x1b"
                             "A",
                             s_text);
    TEST_ASSERT_FALSE(key_sent("A", 0u, 0));
    TEST_ASSERT_FALSE(key_sent("2", CLI_INPUT_MOD_CONTROL, 0));
}

void tearDown(void)
{
    EMBED_CALL(cli_terminal_screen.free, CliTerminalScreenCfg, .screen = &s_screen);
}

void test_letters_land_at_the_cursor_and_a_return_and_a_feed_start_the_next_line(void)
{
    screen_open(10u, 4u);
    screen_put("ab\r\ncd");
    TEST_ASSERT_EQUAL_STRING("ab", screen_row_text(0u));
    TEST_ASSERT_EQUAL_STRING("cd", screen_row_text(1u));
    TEST_ASSERT_EQUAL_UINT(2u, s_screen.cursor.x);
    TEST_ASSERT_EQUAL_UINT(1u, s_screen.cursor.y);
}

void test_the_16_colors_and_bold_are_set_and_a_reset_takes_them_off(void)
{
    screen_open(10u, 2u);
    screen_put("\x1b[31;1mX\x1b[0mY\x1b[94mZ");
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_INDEXED | 1u, s_screen.shown[0].foreground);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_BOLD, s_screen.shown[0].marks);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_DEFAULT, s_screen.shown[1].foreground);
    TEST_ASSERT_EQUAL_HEX32(0u, s_screen.shown[1].marks);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_INDEXED | 12u, s_screen.shown[2].foreground);
}

void test_the_256_colors_and_direct_colors_are_read_an_empty_color_space_among_them(void)
{
    screen_open(10u, 2u);
    screen_put("\x1b[38;5;200mA\x1b[48;2;1;2;3mB\x1b[38:2::4:5:6mC");
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_INDEXED | 200u, s_screen.shown[0].foreground);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_DIRECT | 0x010203u, s_screen.shown[1].background);
    TEST_ASSERT_EQUAL_HEX32(CLI_TERMINAL_COLOR_DIRECT | 0x040506u, s_screen.shown[2].foreground);
}

void test_where_the_cursor_is_is_answered_after_it_is_placed(void)
{
    screen_open(10u, 5u);
    screen_put("\x1b[3;5H\x1b[6n");
    TEST_ASSERT_EQUAL_UINT(4u, s_screen.cursor.x);
    TEST_ASSERT_EQUAL_UINT(2u, s_screen.cursor.y);
    TEST_ASSERT_EQUAL_UINT(6u, s_screen.answer_size);
    TEST_ASSERT_EQUAL_MEMORY("\x1b[3;5R", s_screen.answer, 6u);
}

void test_a_line_is_erased_from_the_cursor_to_its_end(void)
{
    screen_open(10u, 2u);
    screen_put("abcdef\x1b[3G\x1b[K");
    TEST_ASSERT_EQUAL_STRING("ab", screen_row_text(0u));
}

void test_lines_scrolled_off_the_main_screen_go_to_the_scrollback(void)
{
    screen_open(10u, 3u);
    screen_put("1\r\n2\r\n3\r\n4\r\n5");
    TEST_ASSERT_EQUAL_STRING("3", screen_row_text(0u));
    TEST_ASSERT_EQUAL_UINT(2u, s_screen.scrollback_count);
    unsigned int columns = 0u;
    const CliTerminalCell *const oldest = EMBED_CALL(cli_terminal_screen_line.scrollback, CliTerminalScreenLineCfg,
                                                     .screen_read = &s_screen, .line = 0u, .columns = &columns);
    TEST_ASSERT_NOT_NULL(oldest);
    TEST_ASSERT_EQUAL_UINT(1u, columns);
    TEST_ASSERT_EQUAL_UINT((unsigned int)'1', oldest[0].letter);
}

void test_the_second_screen_is_drawn_on_and_left_with_the_main_screen_as_it_was(void)
{
    screen_open(10u, 3u);
    screen_put("main\x1b[?1049h");
    TEST_ASSERT_TRUE(s_screen.shown == s_screen.second);
    TEST_ASSERT_EQUAL_STRING("", screen_row_text(0u));
    screen_put("full\x1b[?1049l");
    TEST_ASSERT_TRUE(s_screen.shown == s_screen.main);
    TEST_ASSERT_EQUAL_STRING("main", screen_row_text(0u));
    TEST_ASSERT_EQUAL_UINT(0u, s_screen.scrollback_count);
}

void test_the_folder_a_shell_reports_is_read_from_osc_7_ended_by_a_bell_or_a_string_terminator(void)
{
    screen_open(10u, 2u);
    screen_put("\x1b]7;file://host/d/x\x07");
    TEST_ASSERT_TRUE(s_screen.place_reported);
    TEST_ASSERT_EQUAL_STRING("file://host/d/x", s_screen.place);
    screen_put("\x1b]7;file://host/e\x1b\\");
    TEST_ASSERT_EQUAL_STRING("file://host/e", s_screen.place);
    TEST_ASSERT_EQUAL_STRING("", screen_row_text(0u));
}

void test_a_letter_at_the_last_column_wraps_the_next_onto_the_next_line(void)
{
    screen_open(4u, 3u);
    screen_put("abcde");
    TEST_ASSERT_EQUAL_STRING("abcd", screen_row_text(0u));
    TEST_ASSERT_EQUAL_STRING("e", screen_row_text(1u));
}

void test_a_letter_past_ascii_is_read_whole_across_two_reads(void)
{
    screen_open(10u, 2u);
    screen_put("\xc3");
    screen_put("\xa9!");
    TEST_ASSERT_EQUAL_HEX32(0xe9u, s_screen.shown[0].letter);
    TEST_ASSERT_EQUAL_UINT((unsigned int)'!', s_screen.shown[1].letter);
    screen_put("\xff");
    TEST_ASSERT_EQUAL_HEX32(0xfffdu, s_screen.shown[2].letter);
}

void test_lines_inserted_and_deleted_stay_inside_the_region(void)
{
    screen_open(10u, 4u);
    screen_put("a\r\nb\r\nc\r\nd\x1b[2;3r\x1b[2;1H\x1b[L");
    TEST_ASSERT_EQUAL_STRING("a", screen_row_text(0u));
    TEST_ASSERT_EQUAL_STRING("", screen_row_text(1u));
    TEST_ASSERT_EQUAL_STRING("b", screen_row_text(2u));
    TEST_ASSERT_EQUAL_STRING("d", screen_row_text(3u));
    screen_put("\x1b[M\x1b[M");
    TEST_ASSERT_EQUAL_STRING("", screen_row_text(1u));
    TEST_ASSERT_EQUAL_STRING("", screen_row_text(2u));
    TEST_ASSERT_EQUAL_STRING("d", screen_row_text(3u));
    TEST_ASSERT_EQUAL_UINT(0u, s_screen.scrollback_count);
}

void test_a_smaller_screen_sends_the_lines_above_the_cursor_to_the_scrollback(void)
{
    screen_open(10u, 4u);
    screen_put("1\r\n2\r\n3\r\n4");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_terminal_screen_size.resize, CliTerminalScreenSizeCfg, .screen = &s_screen,
                                .columns = 3u, .rows = 2u));
    TEST_ASSERT_EQUAL_STRING("3", screen_row_text(0u));
    TEST_ASSERT_EQUAL_STRING("4", screen_row_text(1u));
    TEST_ASSERT_EQUAL_UINT(1u, s_screen.cursor.y);
    TEST_ASSERT_EQUAL_UINT(2u, s_screen.scrollback_count);
}
