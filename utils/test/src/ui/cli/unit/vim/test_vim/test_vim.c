// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_vim.c: Vim's keys on a view of a stored text, each key sent as the window sends it, through the view's input:
// the motions over words, blanks, punctuation and line ends with counts; f F t T and ; ,; % { }; the operators on
// motions, text objects and lines with counts and registers; x X D C Y s S J ~ r p P; undo and redo; the ways into
// insert mode and out; the dot repeat; visual and visual line mode; the search prompt, n N * #; which keys each mode
// takes; the label and the block caret; and Vim turned off. Each case opens its own view
#include "ui/cli/src/editor/document.h"
#include "ui/cli/src/editor/view.h"
#include "ui/cli/src/editor/vim.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_key[] = "check:vim";
static char s_error[256];

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
}

// the input sent to the view: whether the view took it
static int sent(const CliViewInput *input)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ULL};
    return EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = input, .err_msg = &err_msg);
}

// a key pressed, by the window's name for it
static void pressed(const char *name)
{
    const CliViewInput input = {.type = CLI_VIEW_KEY, .name = name};
    TEST_ASSERT_TRUE(sent(&input));
}

// each character of `text` typed as the window types it, a key and then its text
static void typed(const char *text)
{
    const unsigned long long length = CLI_UNITY_LENGTH(text);
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        char name[2] = {text[at], '\0'};
        name[0] = ((name[0] >= 'a') && (name[0] <= 'z')) ? (char)(name[0] - 'a' + 'A') : name[0];
        const CliViewInput key = {.type = CLI_VIEW_KEY, .name = name};
        const CliViewInput character = {.type = CLI_VIEW_TEXT, .text = text + at, .length = 1ULL};
        TEST_ASSERT_TRUE(sent(&key));
        TEST_ASSERT_TRUE(sent(&character));
    }
}

// a view opened anew on `text` with Vim's keys on, twenty rows of eighty columns
static void fresh(const char *text)
{
    CliShape shape;
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = s_key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = text,
                                .size = CLI_UNITY_LENGTH(text), .shape = &shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = "check/vim.txt"));
    const CliViewInput size = {.type = CLI_VIEW_SIZE, .rows = 20u, .columns = 80u};
    TEST_ASSERT_TRUE(sent(&size));
    EMBED_CALL(cli_vim_view.set, CliVimCfg, .key = s_key, .on = 1);
}

// the cursor asserted at `line`, `col`
static void at(unsigned long long line, unsigned long long col)
{
    CliPosition cursor;
    CliPosition anchor;
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = s_key, .cursor = &cursor, .anchor = &anchor));
    TEST_ASSERT_EQUAL_UINT64(line, cursor.line);
    TEST_ASSERT_EQUAL_UINT64(col, cursor.col);
}

// the whole text asserted to read as `required`, its lines joined by "\n"
static void reads(const char *required)
{
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = s_key, .shape = &shape));
    unsigned long long size = 0ULL;
    char *const lines = EMBED_CALL(cli_document_key.lines_alloc, CliDocumentKeyCfg, .key = s_key, .first = 0ULL,
                                   .last = shape.lines - 1ULL, .size_pointer = &size, .state = NULL,
                                   .capacity = 0ULL, .owner = g_checks_owner);
    TEST_ASSERT_NOT_NULL(lines);
    CLI_TEST_ASSERT_TEXT(required, lines, size);
    g_cli_guard->persistent_buf_release(lines, g_checks_owner);
}

// the label asserted to read `required`
static void labelled(const char *required)
{
    const CliVimShape *const shape = EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key);
    CLI_TEST_ASSERT_TEXT(required, shape->label, shape->label_length);
}

void test_the_motions_move_over_words_blanks_punctuation_and_line_ends_with_counts(void)
{
    fresh("one two-three  four\n  five six\n\nseven");
    typed("w");
    at(0ULL, 4ULL);
    typed("w");
    at(0ULL, 7ULL);
    typed("W");
    at(0ULL, 15ULL);
    typed("e");
    at(0ULL, 18ULL);
    typed("w");
    at(1ULL, 2ULL);
    typed("b");
    at(0ULL, 15ULL);
    typed("2b");
    at(0ULL, 7ULL);
    typed("B");
    at(0ULL, 4ULL);
    typed("E");
    at(0ULL, 12ULL);
    typed("0");
    at(0ULL, 0ULL);
    typed("$");
    at(0ULL, 18ULL);
    typed("j");
    at(1ULL, 9ULL);
    typed("^");
    at(1ULL, 2ULL);
    typed("k");
    at(0ULL, 2ULL);
    typed("3l");
    at(0ULL, 5ULL);
    typed("2h");
    at(0ULL, 3ULL);
    typed("G");
    at(3ULL, 0ULL);
    typed("gg");
    at(0ULL, 0ULL);
    typed("2G");
    at(1ULL, 2ULL);
    typed("3j");
    at(3ULL, 2ULL);
    typed("5k");
    at(0ULL, 2ULL);
}

void test_f_F_t_T_go_to_a_character_on_the_line_and_semicolon_and_comma_go_again(void)
{
    fresh("a,b,c;d,e");
    typed("f,");
    at(0ULL, 1ULL);
    typed(";");
    at(0ULL, 3ULL);
    typed(",");
    at(0ULL, 1ULL);
    typed("t;");
    at(0ULL, 4ULL);
    typed("Fa");
    at(0ULL, 0ULL);
    typed("2f,");
    at(0ULL, 3ULL);
    typed("Ta");
    at(0ULL, 1ULL);
}

void test_percent_goes_to_the_pairing_bracket_and_braces_to_the_empty_lines(void)
{
    fresh("if (a[1]) {\n  x;\n}\n\nend");
    typed("%");
    at(0ULL, 8ULL);
    typed("%");
    at(0ULL, 3ULL);
    typed("}");
    at(3ULL, 0ULL);
    typed("}");
    at(4ULL, 2ULL);
    typed("{");
    at(3ULL, 0ULL);
}

void test_the_operators_act_on_motions_objects_and_lines_with_counts_and_registers(void)
{
    fresh("one two three four");
    typed("dw");
    reads("two three four");
    at(0ULL, 0ULL);
    typed("d2w");
    reads("four");
    fresh("one two three");
    typed("cwTWO");
    pressed("Escape");
    reads("TWO two three");
    at(0ULL, 2ULL);
    fresh("foo bar baz");
    typed("wdiw");
    reads("foo  baz");
    fresh("foo bar baz");
    typed("wdaw");
    reads("foo baz");
    fresh("say \"hi there\" now");
    typed("6ldi\"");
    reads("say \"\" now");
    fresh("f(a, b) x");
    typed("3lda(");
    reads("f x");
    fresh("{ab}");
    typed("lci{z");
    pressed("Escape");
    reads("{z}");
    fresh("l1\nl2\nl3");
    typed("jdd");
    reads("l1\nl3");
    at(1ULL, 0ULL);
    typed("dd");
    reads("l1");
    fresh("a\nb\nc");
    typed("2dd");
    reads("c");
    fresh("a\nb");
    typed("\"ayyj\"ap");
    reads("a\nb\na");
    typed("yyP");
    reads("a\nb\na\na");
    fresh("one");
    typed(">>");
    reads("    one");
    at(0ULL, 4ULL);
    typed("<<");
    reads("one");
    fresh("ab cd");
    typed("yw$p");
    reads("ab cdab ");
}

void test_the_character_commands_act_with_counts_and_put_as_the_register_took(void)
{
    fresh("abcdef");
    typed("x");
    reads("bcdef");
    typed("2x");
    reads("def");
    typed("p");
    reads("dbcef");
    at(0ULL, 2ULL);
    fresh("abcdef");
    typed("$X");
    reads("abcdf");
    at(0ULL, 4ULL);
    fresh("abcdef");
    typed("2lD");
    reads("ab");
    at(0ULL, 1ULL);
    fresh("abcdef");
    typed("2lCxy");
    pressed("Escape");
    reads("abxy");
    fresh("one\ntwo");
    typed("Yp");
    reads("one\none\ntwo");
    at(1ULL, 0ULL);
    fresh("abc");
    typed("sz");
    pressed("Escape");
    reads("zbc");
    fresh("  abc\nd");
    typed("Sx");
    pressed("Escape");
    reads("  x\nd");
    fresh("a\n  b\nc");
    typed("J");
    reads("a b\nc");
    at(0ULL, 1ULL);
    fresh("a\nb\nc");
    typed("3J");
    reads("a b c");
    fresh("abC");
    typed("3~");
    reads("ABc");
    at(0ULL, 2ULL);
    fresh("abc");
    typed("rx");
    reads("xbc");
    typed("2ry");
    reads("yyc");
    at(0ULL, 1ULL);
    fresh("ab");
    typed("ylP");
    reads("aab");
}

void test_u_and_ctrl_r_undo_and_redo_with_the_cursor_at_the_edit(void)
{
    fresh("one two");
    typed("wdw");
    reads("one ");
    typed("u");
    reads("one two");
    at(0ULL, 4ULL);
    pressed("Mod+R");
    reads("one ");
}

void test_i_a_I_A_o_O_enter_insert_mode_and_escape_returns_one_column_back(void)
{
    fresh("  abc");
    typed("Ad");
    pressed("Escape");
    reads("  abcd");
    at(0ULL, 5ULL);
    typed("Ix");
    pressed("Escape");
    reads("  xabcd");
    at(0ULL, 2ULL);
    typed("on");
    pressed("Escape");
    reads("  xabcd\n  n");
    at(1ULL, 2ULL);
    typed("Om");
    pressed("Escape");
    reads("  xabcd\n  m\n  n");
    fresh("ab");
    typed("az");
    pressed("Escape");
    reads("azb");
    at(0ULL, 1ULL);
    typed("iy");
    pressed("Mod+[");
    reads("ayzb");
    at(0ULL, 1ULL);
}

void test_the_dot_does_the_last_change_again_with_the_text_typed_after_it(void)
{
    fresh("a b c d");
    typed("dw.");
    reads("c d");
    fresh("one two three");
    typed("cwX");
    pressed("Escape");
    typed("w.");
    reads("X X three");
    fresh("ab\ncd");
    typed("Ax");
    pressed("Escape");
    typed("j.");
    reads("abx\ncdx");
}

void test_visual_modes_select_swap_their_ends_and_act_on_the_selection(void)
{
    fresh("abcdef");
    typed("v");
    labelled("VISUAL");
    typed("lld");
    reads("def");
    labelled("NORMAL");
    fresh("l1\nl2\nl3");
    typed("V");
    labelled("VISUAL LINE");
    typed("jd");
    reads("l3");
    fresh("abcdef");
    typed("vl~");
    reads("ABcdef");
    fresh("abcdef");
    typed("lvlohy$p");
    reads("abcdefabc");
    fresh("abcdef");
    typed("vlx");
    reads("cdef");
    fresh("a\nb");
    typed("VjJ");
    reads("a b");
    fresh("one\ntwo");
    typed("Vj>");
    reads("    one\n    two");
    fresh("ab cd");
    typed("ywwviwp");
    reads("ab ab ");
    fresh("foo bar");
    typed("wviwcx");
    pressed("Escape");
    reads("foo x");
}

void test_the_prompt_searches_n_N_go_on_and_star_and_hash_search_the_word_whole(void)
{
    fresh("foo bar foo baz foo");
    typed("/");
    const CliVimShape *const open = EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key);
    TEST_ASSERT_TRUE(open->prompt_open);
    TEST_ASSERT_FALSE(open->prompt_back);
    typed("foo");
    const CliVimShape *const typed_in = EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key);
    CLI_TEST_ASSERT_TEXT("foo", typed_in->prompt, typed_in->prompt_length);
    pressed("Enter");
    at(0ULL, 8ULL);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key)->prompt_open);
    typed("n");
    at(0ULL, 16ULL);
    typed("n");
    at(0ULL, 0ULL);
    typed("N");
    at(0ULL, 16ULL);
    typed("/bar");
    pressed("Escape");
    at(0ULL, 16ULL);
    typed("?b.r");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key)->prompt_back);
    pressed("Enter");
    at(0ULL, 4ULL);
    fresh("foo food foo");
    typed("*");
    at(0ULL, 9ULL);
    typed("#");
    at(0ULL, 0ULL);
    fresh("a[x b");
    typed("/[x");
    pressed("Enter");
    at(0ULL, 1ULL);
}

void test_insert_mode_takes_every_key_but_escape_and_other_modes_leave_ctrl_chords(void)
{
    fresh("abc");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "J"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "Shift+4"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "Tab"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "Mod+S"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "Alt+X"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "F3"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_vim_view.text, CliVimCfg, .key = s_key, .text = "l", .length = 1ULL));
    at(0ULL, 1ULL);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "Right"));
    at(0ULL, 2ULL);
    typed("i");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "J"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "Mod+R"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_vim_view.text, CliVimCfg, .key = s_key, .text = "q", .length = 1ULL));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = s_key, .name = "Escape"));
    reads("abc");
}

void test_the_label_reads_the_mode_and_the_keys_typed_and_the_caret_is_a_block_but_in_insert(void)
{
    fresh("abc def");
    labelled("NORMAL");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key)->block);
    typed("2d");
    labelled("NORMAL 2d");
    pressed("Escape");
    labelled("NORMAL");
    typed("\"a");
    labelled("NORMAL \"a");
    pressed("Escape");
    typed("i");
    labelled("INSERT");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key)->block);
    TEST_ASSERT_EQUAL_UINT(CLI_VIM_INSERT, EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key)->mode);
    pressed("Escape");
    typed("V");
    labelled("VISUAL LINE");
    pressed("Escape");
    labelled("NORMAL");
}

void test_turning_vim_off_ends_the_prompt_and_the_mode(void)
{
    fresh("abc");
    typed("/b");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key)->prompt_open);
    EMBED_CALL(cli_vim_view.set, CliVimCfg, .key = s_key, .on = 0);
    const CliVimShape *const off = EMBED_CALL(cli_vim_view.shape, CliVimCfg, .key = s_key);
    TEST_ASSERT_FALSE(off->on);
    TEST_ASSERT_FALSE(off->prompt_open);
    TEST_ASSERT_EQUAL_UINT64(0ULL, off->label_length);
    typed("x");
    reads("xabc");
    EMBED_CALL(cli_vim_view.set, CliVimCfg, .key = s_key, .on = 1);
    labelled("NORMAL");
    typed("0x");
    reads("abc");
}
