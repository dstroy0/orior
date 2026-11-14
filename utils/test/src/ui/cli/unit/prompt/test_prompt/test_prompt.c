// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_prompt.c: the one ask's rows are the sentence, the statement, the field and the buttons in order; text typed is
// put at the field's end and Backspace takes one character off it; Enter answers the first button and Escape closes the ask
// with no answer; a password row carries its characters' count and no text; an answer calls the continuation once with the
// field's text and gives the ask's cells back; a continuation may open the next ask; and a statement is cut to its max
#include "ui/cli/src/command_line/prompt.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char *const s_run_choices[2] = {"Run It", "Do Not Run It"};
static const char *const s_connect_choices[1] = {"Connect"};
static unsigned int s_answers;
static unsigned int s_answered_at;
static char s_answered_text[64];
static unsigned long long s_answered_size;
static int s_context_seen;

void setUp(void)
{
    cli_unity_begun();
    EMBED_CALL(cli_prompt.close, CliPromptCfg, .at = 0u);
    s_answers = 0u;
    s_answered_at = 0u;
    s_answered_size = 0ULL;
    s_answered_text[0] = '\0';
    s_context_seen = 0;
}

void tearDown(void)
{
    EMBED_CALL(cli_prompt.close, CliPromptCfg, .at = 0u);
}

// the continuation: what it was passed, counted
static void answered(const CliPromptCfg *args)
{
    s_answers += 1u;
    s_answered_at = args->at;
    s_answered_size = args->size;
    const unsigned long long length = (args->size < sizeof(s_answered_text) - 1u) ? args->size : 0ULL;
    if (length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_answered_text, .src = args->text, .bytes = length});
    }
    s_answered_text[length] = '\0';
    s_context_seen = args->context == &s_answers;
}

// a continuation that opens the next ask
static void answered_again(const CliPromptCfg *args)
{
    (void)args;
    s_answers += 1u;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_ASK_LINE, .sentence = "Again",
                                .choices = s_connect_choices, .count = 1u, .then = answered));
}

// the row `at`'s type, CLI_PROMPT_NONE past the last
static unsigned int type_at(unsigned int at)
{
    CliPromptRow row;
    return EMBED_CALL(cli_prompt_rows.row, CliPromptRowsCfg, .at = at, .row = &row) ? row.type : CLI_PROMPT_NONE;
}

// the field's row
static CliPromptRow field_row(void)
{
    CliPromptRow row = {0};
    for (unsigned int at = 0u; EMBED_CALL(cli_prompt_rows.row, CliPromptRowsCfg, .at = at, .row = &row); at += 1u)
    {
        if (row.type == CLI_PROMPT_ROW_FIELD)
        {
            return row;
        }
    }
    TEST_FAIL_MESSAGE("the ask has no field");
    return row;
}

void test_a_confirm_has_the_sentence_the_statement_and_the_buttons_in_order(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_CONFIRM,
                                .sentence = "This DELETE reaches every row", .statement = "DELETE FROM t",
                                .choices = s_run_choices, .count = 2u, .then = answered));
    TEST_ASSERT_EQUAL_UINT(3u, EMBED_CALL(cli_prompt_rows.rows, CliPromptRowsCfg, .at = 0u));
    TEST_ASSERT_EQUAL_UINT(CLI_PROMPT_ROW_SENTENCE, type_at(0u));
    TEST_ASSERT_EQUAL_UINT(CLI_PROMPT_ROW_STATEMENT, type_at(1u));
    TEST_ASSERT_EQUAL_UINT(CLI_PROMPT_ROW_BUTTONS, type_at(2u));
    TEST_ASSERT_EQUAL_UINT(CLI_PROMPT_NONE, type_at(3u));
    CliPromptRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt_rows.row, CliPromptRowsCfg, .at = 1u, .row = &row));
    CLI_TEST_ASSERT_TEXT("DELETE FROM t", row.text, row.length);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt_rows.row, CliPromptRowsCfg, .at = 2u, .row = &row));
    TEST_ASSERT_EQUAL_UINT(2u, row.choices);
    CLI_TEST_ASSERT_TEXT("Do Not Run It", row.choice[1], row.choice_length[1]);
}

void test_an_ask_for_a_line_has_the_sentence_the_field_and_the_buttons(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_ASK_LINE, .sentence = "The file",
                                .preset = "rows.csv", .choices = s_connect_choices, .count = 1u, .then = answered));
    TEST_ASSERT_EQUAL_UINT(3u, EMBED_CALL(cli_prompt_rows.rows, CliPromptRowsCfg, .at = 0u));
    TEST_ASSERT_EQUAL_UINT(CLI_PROMPT_ROW_SENTENCE, type_at(0u));
    TEST_ASSERT_EQUAL_UINT(CLI_PROMPT_ROW_FIELD, type_at(1u));
    TEST_ASSERT_EQUAL_UINT(CLI_PROMPT_ROW_BUTTONS, type_at(2u));
    const CliPromptRow row = field_row();
    CLI_TEST_ASSERT_TEXT("rows.csv", row.text, row.length);
}

void test_text_typed_goes_at_the_fields_end_and_backspace_takes_one_character(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_ASK_LINE, .sentence = "The file",
                                .choices = s_connect_choices, .count = 1u, .then = answered));
    EMBED_CALL(cli_prompt_input.type, CliPromptInputCfg, .text = "ab", .size = 2ULL);
    EMBED_CALL(cli_prompt_input.type, CliPromptInputCfg, .text = "\xC3\xA9", .size = 2ULL);
    CliPromptRow row = field_row();
    CLI_TEST_ASSERT_TEXT("ab\xC3\xA9", row.text, row.length);
    TEST_ASSERT_EQUAL_UINT64(3ULL, row.characters);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt_input.key, CliPromptInputCfg, .name = "Backspace"));
    row = field_row();
    CLI_TEST_ASSERT_TEXT("ab", row.text, row.length);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_prompt_input.key, CliPromptInputCfg, .name = "Tab"));
}

void test_enter_answers_the_first_button_with_the_fields_text(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_ASK_LINE, .sentence = "The file",
                                .choices = s_run_choices, .count = 2u, .then = answered, .context = &s_answers));
    EMBED_CALL(cli_prompt_input.type, CliPromptInputCfg, .text = "out.tsv", .size = 7ULL);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt_input.key, CliPromptInputCfg, .name = "Enter"));
    TEST_ASSERT_EQUAL_UINT(1u, s_answers);
    TEST_ASSERT_EQUAL_UINT(0u, s_answered_at);
    TEST_ASSERT_EQUAL_STRING("out.tsv", s_answered_text);
    TEST_ASSERT_TRUE(s_context_seen);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_prompt.asked, CliPromptCfg, .at = 0u));
}

void test_escape_closes_the_ask_with_no_answer(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_CONFIRM, .sentence = "Run it?",
                                .statement = "DELETE FROM t", .choices = s_run_choices, .count = 2u,
                                .then = answered));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.asked, CliPromptCfg, .at = 0u));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt_input.key, CliPromptInputCfg, .name = "Escape"));
    TEST_ASSERT_EQUAL_UINT(0u, s_answers);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_prompt.asked, CliPromptCfg, .at = 0u));
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_prompt_rows.rows, CliPromptRowsCfg, .at = 0u));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_prompt_input.key, CliPromptInputCfg, .name = "Enter"));
}

void test_a_password_row_carries_the_count_of_its_characters_only(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_PASSWORD,
                                .sentence = "The server refused the password", .choices = s_connect_choices,
                                .count = 1u, .then = answered));
    EMBED_CALL(cli_prompt_input.type, CliPromptInputCfg, .text = "s3cr\xC3\xA9t", .size = 7ULL);
    const CliPromptRow row = field_row();
    TEST_ASSERT_TRUE(row.secret);
    TEST_ASSERT_NULL(row.text);
    TEST_ASSERT_EQUAL_UINT64(0ULL, row.length);
    TEST_ASSERT_EQUAL_UINT64(6ULL, row.characters);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.answer, CliPromptCfg, .at = 0u));
    TEST_ASSERT_EQUAL_STRING("s3cr\xC3\xA9t", s_answered_text);
}

void test_an_answer_calls_the_continuation_once_and_gives_the_cells_back(void)
{
    const unsigned long long before = mmgr_owner_in_use(g_cli_command_line_owner);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_ASK_LINE, .sentence = "The file",
                                .preset = "a.csv", .choices = s_run_choices, .count = 2u, .then = answered));
    TEST_ASSERT_GREATER_THAN_UINT64(before, mmgr_owner_in_use(g_cli_command_line_owner));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.answer, CliPromptCfg, .at = 1u));
    TEST_ASSERT_EQUAL_UINT(1u, s_answers);
    TEST_ASSERT_EQUAL_UINT(1u, s_answered_at);
    TEST_ASSERT_EQUAL_UINT64(5ULL, s_answered_size);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_prompt.answer, CliPromptCfg, .at = 0u));
    TEST_ASSERT_EQUAL_UINT(1u, s_answers);
    TEST_ASSERT_EQUAL_UINT64(before, mmgr_owner_in_use(g_cli_command_line_owner));
}

void test_a_button_past_the_last_is_no_answer(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_CONFIRM, .sentence = "Run it?",
                                .choices = s_run_choices, .count = 2u, .then = answered));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_prompt.answer, CliPromptCfg, .at = 2u));
    TEST_ASSERT_EQUAL_UINT(0u, s_answers);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.asked, CliPromptCfg, .at = 0u));
}

void test_a_continuation_may_open_the_next_ask(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_PASSWORD, .sentence = "First",
                                .choices = s_connect_choices, .count = 1u, .then = answered_again));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.answer, CliPromptCfg, .at = 0u));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.asked, CliPromptCfg, .at = 0u));
    CliPromptRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt_rows.row, CliPromptRowsCfg, .at = 0u, .row = &row));
    CLI_TEST_ASSERT_TEXT("Again", row.text, row.length);
}

void test_a_statement_is_cut_to_its_max(void)
{
    static char s_long[1000];
    memor.set(&(MemoriaCfg){.dst = s_long, .val = 'x', .bytes = sizeof(s_long) - 1u});
    s_long[sizeof(s_long) - 1u] = '\0';
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt.open, CliPromptCfg, .mode = CLI_PROMPT_CONFIRM, .sentence = "Run it?",
                                .statement = s_long, .choices = s_run_choices, .count = 2u, .then = answered));
    CliPromptRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_prompt_rows.row, CliPromptRowsCfg, .at = 1u, .row = &row));
    TEST_ASSERT_EQUAL_UINT64(CLI_PROMPT_STATEMENT_MAX, row.length);
}
