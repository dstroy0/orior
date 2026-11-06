// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_commands.c: the command catalog: menu words and commands are unique, a command is one word, a job menu's commands are not
// job paths, and an item requires each <argument> outside brackets; and the menus open: a menu's serial, new as it opens and
// the same while it stands open
#include "ui/cli/src/command_line/commands.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliItem s_items[CLI_ITEMS_MAX];
static char s_words[CLI_MENUS_MAX][CLI_LABEL_MAX_LEN];

// the item with `args` requires `needed` arguments
static void assert_arguments_required(unsigned int needed, const char *args)
{
    CliItem item = {0};
    item.args = args;
    TEST_ASSERT_EQUAL_UINT_MESSAGE(needed, EMBED_CALL(cli_commands_item.item_words_need, CliCommandsItemCfg, .item_read = &item), args);
}

void setUp(void)
{
    cli_unity_begun();
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &err_msg));
}

void tearDown(void)
{
}

void test_no_two_menus_share_a_word(void)
{
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        EMBED_CALL(cli_commands_menu_word.menu_word, CliCommandsMenuWordCfg, .menu = menu, .word_out = s_words[menu]);
        for (unsigned int before = 0u; before < menu; before += 1u)
        {
            TEST_ASSERT_FALSE_MESSAGE(cellul.eq(&(CatenaFinitaCfg){.src = s_words[before], .other = s_words[menu], .cap = CLI_POOL_BYTES}), s_words[menu]);
        }
    }
}

void test_no_menu_lists_a_command_twice(void)
{
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg, .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            for (unsigned int before = 0u; before < at; before += 1u)
            {
                TEST_ASSERT_FALSE_MESSAGE(cellul.eq(&(CatenaFinitaCfg){.src = s_items[before].command, .other = s_items[at].command, .cap = CLI_POOL_BYTES}), s_items[at].command);
            }
        }
    }
}

void test_every_command_is_one_word(void)
{
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg, .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            TEST_ASSERT_NULL_MESSAGE(cellul.chr(&(CatenaFinitaCfg){.src = s_items[at].command, .cap = CLI_POOL_BYTES, .byte = ' '}), s_items[at].command);
        }
    }
}

void test_no_command_of_a_job_menu_is_a_job_path(void)
{
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        char group[1][CLI_LABEL_MAX_LEN];
        if (EMBED_CALL(cli_commands_menu_groups.menu_groups, CliCommandsMenuGroupsCfg, .menu = menu, .groups = group, .capacity = 1u) == 0u)
        {
            continue;
        }
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg, .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            TEST_ASSERT_NULL_MESSAGE(cellul.chr(&(CatenaFinitaCfg){.src = s_items[at].command, .cap = CLI_POOL_BYTES, .byte = '/'}), s_items[at].command);
        }
    }
}

void test_an_item_that_is_on_or_off_reads_the_name_of_its_state_and_every_other_none(void)
{
    unsigned int found = 0u;
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg, .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){.src = s_items[at].command, .other = "split", .cap = CLI_POOL_BYTES}))
            {
                TEST_ASSERT_EQUAL_STRING("terminal-split", s_items[at].checks);
                found += 1u;
            }
            else if (cellul.eq(&(CatenaFinitaCfg){.src = s_items[at].command, .other = "toggle", .cap = CLI_POOL_BYTES}))
            {
                TEST_ASSERT_EQUAL_STRING("", s_items[at].checks);
                found += 1u;
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT(2u, found);
}

// the place on the bar of the menu titled `name`
static unsigned int title_at(const char *name)
{
    unsigned int at = 0u;
    while ((at < cli_menu_titles()) && !cellul.eq(&(CatenaFinitaCfg){.src = cli_menu_title_text(cli_menu_title_menu(at)), .other = name, .cap = CLI_LABEL_MAX_LEN}))
    {
        at += 1u;
    }
    TEST_ASSERT_LESS_THAN_UINT_MESSAGE(cli_menu_titles(), at, name);
    return at;
}

void test_a_menu_opened_anew_takes_a_new_serial_and_keeps_it_while_it_stands_open(void)
{
    cli_menus_bar(cli_menu_titles(), 0);
    cli_menus_title_press(title_at("View"));
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_menus_open.levels);
    const unsigned int serial = g_cli_menus_open.level[0].serial;
    // the View menu's second row is a line, which no pointer marks: its third is marked
    cli_menus_row_move(0u, 1u);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(1, g_cli_menus_open.level[0].pointed, "a line is never marked");
    cli_menus_row_move(0u, 2u);
    TEST_ASSERT_EQUAL_INT(2, g_cli_menus_open.level[0].pointed);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(serial, g_cli_menus_open.level[0].serial, "a row marked leaves the menu the one opened");
    cli_menus_close(0u);
    TEST_ASSERT_EQUAL_UINT(0u, g_cli_menus_open.levels);
    cli_menus_title_press(title_at("View"));
    TEST_ASSERT_NOT_EQUAL_MESSAGE(serial, g_cli_menus_open.level[0].serial, "the menu closed and opened again is opened anew");
    cli_menus_close(0u);
}

void test_arguments_outside_brackets_are_required(void)
{
    assert_arguments_required(2u, "<file> <line> [condition [hits [message]]]");
}

void test_arguments_inside_brackets_are_optional(void)
{
    assert_arguments_required(0u, "[folder]");
}

void test_a_choice_counts_as_one_argument(void)
{
    assert_arguments_required(2u, "<bar|menu|pane> <percent>");
}

void test_a_required_argument_after_an_optional_group_is_required(void)
{
    assert_arguments_required(1u, "[<from> [<words>]] <place>");
}
