// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_commands.c: the command catalog: menu words and commands are unique, a command is one word, a job menu's
// commands are not job paths, and an item requires each <argument> outside brackets; and the menus open: a menu's
// serial, new as it opens and the same while it is open
#include "ui/cli/src/command_line/commands.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliItem s_items[CLI_ITEMS_MAX];
static char s_words[CLI_MENUS_MAX][CLI_LABEL_MAX_LEN];

// the item with `args` requires `needed` arguments
static void assert_arguments_required(unsigned int needed, const char *args)
{
    CliItem item = {0};
    item.args = args;
    TEST_ASSERT_EQUAL_UINT_MESSAGE(
        needed, EMBED_CALL(cli_commands_item.item_words_need, CliCommandsItemCfg, .item_read = &item), args);
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
            TEST_ASSERT_FALSE_MESSAGE(
                cellul.eq(&(CatenaFinitaCfg){.src = s_words[before], .other = s_words[menu], .cap = CLI_POOL_BYTES}),
                s_words[menu]);
        }
    }
}

void test_no_menu_lists_a_command_twice(void)
{
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                              .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            for (unsigned int before = 0u; before < at; before += 1u)
            {
                TEST_ASSERT_FALSE_MESSAGE(cellul.eq(&(CatenaFinitaCfg){.src = s_items[before].command,
                                                                       .other = s_items[at].command,
                                                                       .cap = CLI_POOL_BYTES}),
                                          s_items[at].command);
            }
        }
    }
}

void test_every_command_is_one_word(void)
{
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                              .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            TEST_ASSERT_NULL_MESSAGE(
                cellul.chr(&(CatenaFinitaCfg){.src = s_items[at].command, .cap = CLI_POOL_BYTES, .byte = ' '}),
                s_items[at].command);
        }
    }
}

void test_no_command_of_a_job_menu_is_a_job_path(void)
{
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        char group[1][CLI_LABEL_MAX_LEN];
        if (EMBED_CALL(cli_commands_menu_groups.menu_groups, CliCommandsMenuGroupsCfg, .menu = menu, .groups = group,
                       .capacity = 1u) == 0u)
        {
            continue;
        }
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                              .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            TEST_ASSERT_NULL_MESSAGE(
                cellul.chr(&(CatenaFinitaCfg){.src = s_items[at].command, .cap = CLI_POOL_BYTES, .byte = '/'}),
                s_items[at].command);
        }
    }
}

void test_an_item_that_is_on_or_off_reads_the_name_of_its_state_and_every_other_none(void)
{
    unsigned int found = 0u;
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                              .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){.src = s_items[at].command, .other = "split", .cap = CLI_POOL_BYTES}))
            {
                TEST_ASSERT_EQUAL_STRING("terminal-split", s_items[at].checks);
                found += 1u;
            }
            else if (cellul.eq(
                         &(CatenaFinitaCfg){.src = s_items[at].command, .other = "toggle", .cap = CLI_POOL_BYTES}))
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
    while ((at < cli_menu_titles()) &&
           !cellul.eq(&(CatenaFinitaCfg){
               .src = cli_menu_title_text(&(CliMenusCfg){.menu = cli_menu_title_menu(&(CliMenusCfg){.at = at})}),
               .other = name,
               .cap = CLI_LABEL_MAX_LEN}))
    {
        at += 1u;
    }
    TEST_ASSERT_LESS_THAN_UINT_MESSAGE(cli_menu_titles(), at, name);
    return at;
}

void test_a_menu_opened_anew_takes_a_new_serial_and_retains_it_while_it_is_open(void)
{
    cli_menus_bar(&(CliMenusCfg){.displayed = cli_menu_titles(), .folded = 0});
    cli_menus_title_press(&(CliMenusCfg){.at = title_at("View")});
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_menus_open.levels);
    const unsigned int serial = g_cli_menus_open.level[0].serial;
    // the View menu's second row is a line, which no pointer marks: its third is marked
    cli_menus_row_move(&(CliMenusCfg){.level = 0u, .at = 1u});
    TEST_ASSERT_NOT_EQUAL_MESSAGE(1, g_cli_menus_open.level[0].pointed, "a line is never marked");
    cli_menus_row_move(&(CliMenusCfg){.level = 0u, .at = 2u});
    TEST_ASSERT_EQUAL_INT(2, g_cli_menus_open.level[0].pointed);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(serial, g_cli_menus_open.level[0].serial,
                                   "a row marked leaves the menu the one opened");
    cli_menus_close(&(CliMenusCfg){.level = 0u});
    TEST_ASSERT_EQUAL_UINT(0u, g_cli_menus_open.levels);
    cli_menus_title_press(&(CliMenusCfg){.at = title_at("View")});
    TEST_ASSERT_NOT_EQUAL_MESSAGE(serial, g_cli_menus_open.level[0].serial,
                                  "the menu closed and opened again is opened anew");
    cli_menus_close(&(CliMenusCfg){.level = 0u});
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

// the item of the list `list` whose command is `command`, into `item_out`: 1, or 0 where the list has none
static int list_item(unsigned int list, const char *command, CliItem *item_out)
{
    const unsigned int count = cli_menu_rows(&(CliMenusCfg){.list = list, .items = s_items, .capacity = CLI_ITEMS_MAX});
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((s_items[at].command != NULL) &&
            cellul.eq(&(CatenaFinitaCfg){.src = s_items[at].command, .other = command, .cap = CLI_POOL_BYTES}))
        {
            *item_out = s_items[at];
            return 1;
        }
    }
    return 0;
}

void test_the_preferences_list_the_terminals_folder_and_lines_as_settings(void)
{
    unsigned int preferences = cli_menus();
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        preferences = cellul.eq(&(CatenaFinitaCfg){.src = cli_menu_title_text(&(CliMenusCfg){.menu = menu}),
                                                   .other = "Preferences",
                                                   .cap = CLI_POOL_BYTES})
                          ? menu
                          : preferences;
    }
    TEST_ASSERT_LESS_THAN_UINT(cli_menus(), preferences);
    CliItem terminal;
    TEST_ASSERT_TRUE(list_item(cli_menu_list(&(CliMenusCfg){.menu = preferences}), "terminal", &terminal));
    TEST_ASSERT_NOT_EQUAL_UINT(0u, terminal.list);
    CliItem folder;
    TEST_ASSERT_TRUE(list_item(terminal.list, "folder", &folder));
    TEST_ASSERT_TRUE(folder.setting);
    TEST_ASSERT_EQUAL_STRING("[path]", folder.args);
    CliItem lines;
    TEST_ASSERT_TRUE(list_item(terminal.list, "lines", &lines));
    TEST_ASSERT_TRUE(lines.setting);
    TEST_ASSERT_EQUAL_STRING("<count>", lines.args);
}

void test_a_menu_is_found_by_its_word_and_names_its_title_word_and_split(void)
{
    unsigned int menu = 0u;
    char title[CLI_LABEL_MAX_LEN];
    char word[CLI_LABEL_MAX_LEN];
    char split[CLI_LABEL_MAX_LEN];
    TEST_ASSERT_TRUE(cli_menu_by_name(&(CliCommandsWordCfg){.word_text = "help", .menu_pointer = &menu}));
    cli_menu_title(&(CliCommandsMenuTitleCfg){.menu = menu, .title = title});
    TEST_ASSERT_EQUAL_STRING("Help", title);
    cli_menu_word(&(CliCommandsMenuWordCfg){.menu = menu, .word_out = word});
    TEST_ASSERT_EQUAL_STRING("help", word);
    cli_menu_split(&(CliCommandsMenuSplitCfg){.menu = menu, .split = split});
    TEST_ASSERT_TRUE(cli_menu_displayed(&(CliMenusCfg){.menu = menu}));
    TEST_ASSERT_FALSE(cli_menu_by_name(&(CliCommandsWordCfg){.word_text = "nonesuch", .menu_pointer = &menu}));
}

void test_a_menus_items_are_counted_and_read_into_a_cell_sized_for_them(void)
{
    unsigned int menu = 0u;
    TEST_ASSERT_TRUE(cli_menu_by_name(&(CliCommandsWordCfg){.word_text = "help", .menu_pointer = &menu}));
    const unsigned int count = cli_menu_items_count(&(CliMenusCfg){.menu = menu});
    TEST_ASSERT_GREATER_THAN_UINT(2u, count);
    const unsigned int list = cli_menu_list(&(CliMenusCfg){.menu = menu});
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(count, cli_rows_count(&(CliMenusCfg){.list = list}));
    CliItem *const items = cli_items_alloc(&(CliMenusCfg){.count = count});
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQUAL_UINT(count, cli_menu_items(&(CliCommandsMenuCapacityCfg){.menu = menu, .items = items, .capacity = count}));
    TEST_ASSERT_EQUAL_STRING("keys", items[0].command);
    cli_items_release(&(CliMenusCfg){.items = items, .count = count});
    TEST_ASSERT_NULL(cli_items_alloc(&(CliMenusCfg){.count = 0u}));
}

void test_a_menus_command_a_chains_item_and_a_setting_node_are_found_by_their_words(void)
{
    unsigned int help = 0u;
    unsigned int preferences = 0u;
    CliItem item;
    TEST_ASSERT_TRUE(cli_menu_by_name(&(CliCommandsWordCfg){.word_text = "help", .menu_pointer = &help}));
    TEST_ASSERT_TRUE(cli_menu_command(&(CliCommandsMenuCfg){.menu = help, .word_text = "keys", .item = &item}));
    TEST_ASSERT_EQUAL_STRING("Keyboard Shortcuts", item.label);
    TEST_ASSERT_FALSE(cli_menu_command(&(CliCommandsMenuCfg){.menu = help, .word_text = "nonesuch", .item = &item}));
    TEST_ASSERT_TRUE(cli_menu_by_name(&(CliCommandsWordCfg){.word_text = CLI_SETTINGS_MENU, .menu_pointer = &preferences}));
    CliItem terminal;
    TEST_ASSERT_TRUE(cli_menu_command(&(CliCommandsMenuCfg){.menu = preferences, .word_text = "terminal", .item = &terminal}));
    TEST_ASSERT_TRUE(cli_item_command(&(CliCommandsMenuCfg){.list = terminal.list, .word_text = "lines", .item = &item}));
    TEST_ASSERT_TRUE(item.setting);
    TEST_ASSERT_TRUE(cli_setting_node(&(CliCommandsMenuCfg){.word_text = "terminal.lines", .item = &item}));
    TEST_ASSERT_EQUAL_STRING("lines", item.command);
    TEST_ASSERT_FALSE(cli_setting_node(&(CliCommandsMenuCfg){.word_text = "terminal.nonesuch", .item = &item}));
    TEST_ASSERT_TRUE(cli_chain_item(&(CliCommandsMenuCfg){.menu = preferences, .word_text = "terminal.folder", .item = &item}));
    TEST_ASSERT_EQUAL_STRING("[path]", item.args);
}

void test_a_title_opened_marks_its_first_row_and_a_row_pressed_is_chosen_with_every_menu_closed(void)
{
    CliItem chosen;
    cli_menus_bar(&(CliMenusCfg){.displayed = cli_menu_titles(), .folded = 0});
    cli_menus_title_open(&(CliMenusCfg){.at = title_at("Help"), .marked = 1});
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_menus_open.levels);
    TEST_ASSERT_EQUAL_INT(0, g_cli_menus_open.level[0].pointed);
    TEST_ASSERT_TRUE(cli_menus_row_press(&(CliMenusCfg){.level = 0u, .at = 0u, .chosen = &chosen}));
    TEST_ASSERT_EQUAL_STRING("keys", chosen.command);
    TEST_ASSERT_EQUAL_UINT(0u, g_cli_menus_open.levels);
    // the pointer moved onto another title while a menu is open opens that title's
    cli_menus_title_press(&(CliMenusCfg){.at = title_at("View")});
    cli_menus_title_move(&(CliMenusCfg){.at = title_at("Help")});
    TEST_ASSERT_EQUAL_INT((int)title_at("Help"), g_cli_menus_open.open);
    cli_menus_close(&(CliMenusCfg){.level = 0u});
    TEST_ASSERT_FALSE(cli_menus_rest());
}

void test_a_row_with_items_opens_them_beside_it(void)
{
    // the first title on the bar whose menu has a row with items under it, that menu opened
    CliItem row;
    int with_items = -1;
    cli_menus_bar(&(CliMenusCfg){.displayed = cli_menu_titles(), .folded = 0});
    for (unsigned int title = 0u; (with_items < 0) && (title < cli_menu_titles()); title += 1u)
    {
        cli_menus_title_open(&(CliMenusCfg){.at = title, .marked = 0});
        for (unsigned int at = 0u; (with_items < 0) && cli_menus_row(&(CliMenusCfg){.level = 0u, .at = at, .item = &row}); at += 1u)
        {
            // a row's place is far inside what an int represents
            with_items = (row.list != 0u) ? (int)at : -1;
        }
        if (with_items < 0)
        {
            cli_menus_close(&(CliMenusCfg){.level = 0u});
        }
    }
    TEST_ASSERT_TRUE(with_items >= 0);
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_menus_open.levels);
    cli_menus_row_open(&(CliMenusCfg){.level = 0u, .at = (unsigned int)with_items, .marked = 1});
    TEST_ASSERT_EQUAL_UINT(2u, g_cli_menus_open.levels);
    TEST_ASSERT_EQUAL_INT(with_items, g_cli_menus_open.level[0].opened);
    cli_menus_close(&(CliMenusCfg){.level = 0u});
    TEST_ASSERT_EQUAL_UINT(0u, g_cli_menus_open.levels);
}

void test_keys_are_named_with_modifiers_in_any_order_and_an_item_is_found_by_its_keys(void)
{
    CliItem item;
    TEST_ASSERT_TRUE(cli_menus_keys_name(&(CliMenusCfg){.keys = "Ctrl+Shift+P", .name = "Shift+Mod+P"}));
    TEST_ASSERT_FALSE(cli_menus_keys_name(&(CliMenusCfg){.keys = "Ctrl+P", .name = "P"}));
    TEST_ASSERT_TRUE(cli_menus_keyed(&(CliMenusCfg){.name = "Ctrl+O", .item = &item}));
    TEST_ASSERT_EQUAL_STRING("open-file", item.command);
    TEST_ASSERT_FALSE(cli_menus_keyed(&(CliMenusCfg){.name = "Ctrl+Shift+Alt+F12", .item = &item}));
}

void test_the_readers_own_keys_stand_over_the_lists_in_every_read_of_the_item(void)
{
    CliItem item;
    TEST_ASSERT_TRUE(cli_menus_named(&(CliMenusCfg){.name = "keys", .item = &item}));
    static char s_listed_keys[CLI_LABEL_MAX_LEN];
    VerbaLinea listed = {s_listed_keys, sizeof(s_listed_keys), 0ull};
    mmgr_line_put(&listed, (item.keys != NULL) ? item.keys : "");
    (void)mmgr_line_finish(&listed);
    cli_commands_keys_own(&(CliMenusCfg){.command = "keys", .keys = "Ctrl+Alt+K"});
    TEST_ASSERT_TRUE(cli_menus_named(&(CliMenusCfg){.name = "keys", .item = &item}));
    TEST_ASSERT_EQUAL_STRING("Ctrl+Alt+K", item.keys);
    TEST_ASSERT_TRUE(cli_menus_keyed(&(CliMenusCfg){.name = "Ctrl+Alt+K", .item = &item}));
    TEST_ASSERT_EQUAL_STRING("keys", item.command);
    cli_commands_keys_own(&(CliMenusCfg){.command = "keys", .keys = s_listed_keys});
}

void test_a_parts_own_rows_take_a_popup_and_a_row_relabeled(void)
{
    CliItem row;
    cli_menus_own_begin();
    TEST_ASSERT_TRUE(cli_menus_popup(&(CliMenusCfg){.title = "Files", .item_args = ""}));
    TEST_ASSERT_GREATER_THAN_UINT(0u, cli_menus_own_count());
    TEST_ASSERT_FALSE(cli_menus_popup(&(CliMenusCfg){.title = "Nonesuch", .item_args = ""}));
    TEST_ASSERT_TRUE(cli_menus_own_read(&(CliMenusCfg){.at = 0u, .item = &row}));
    cli_menus_own_relabel(&(CliMenusCfg){.at = 0u, .label = "Renamed"});
    TEST_ASSERT_TRUE(cli_menus_own_read(&(CliMenusCfg){.at = 0u, .item = &row}));
    TEST_ASSERT_EQUAL_STRING("Renamed", row.label);
}
