// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands_test.c: the menus' check: every word names one thing, a menu's word and a command's within its menu, a
// command is one word, and no command of a menu of jobs reads as a job
#include "../../../../../ui/cli/src/command_line/commands.h"
#include "cli_check.h"

int main(void)
{
    static CliItem s_items[CLI_ITEMS_MOST];
    static char s_words[CLI_MENUS_MOST][CLI_LABEL_LONGEST];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_commands_read(&said), "the menus read");
    int once = 1;
    int commands_once = 1;
    int one_word = 1;
    int no_job = 1;
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        cli_menu_word(menu, s_words[menu]);
        for (unsigned int before = 0u; before < menu; before += 1u)
        {
            once &=
                (!cellul.eq(&(CatenaFinitaCfg){.src = s_words[before], .other = s_words[menu], .cap = CLI_CELLS_POOL}));
        }
        const unsigned int items = cli_menu_items(menu, s_items, CLI_ITEMS_MOST);
        char group[1][CLI_LABEL_LONGEST];
        const int of_jobs = cli_menu_groups(menu, group, 1u) > 0u;
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            for (unsigned int before = 0u; before < at; before += 1u)
            {
                commands_once &= (!cellul.eq(&(CatenaFinitaCfg){
                    .src = s_items[before].command, .other = s_items[at].command, .cap = CLI_CELLS_POOL}));
            }
            one_word &=
                (cellul.chr(&(CatenaFinitaCfg){.src = s_items[at].command, .cap = CLI_CELLS_POOL, .byte = ' '}) ==
                 NULL);
            no_job &= !of_jobs ||
                      (cellul.chr(&(CatenaFinitaCfg){.src = s_items[at].command, .cap = CLI_CELLS_POOL, .byte = '/'}) ==
                       NULL);
        }
    }
    cli_check(once, "no two menus share a word");
    cli_check(commands_once, "no menu holds a command twice");
    cli_check(one_word, "every command is one word");
    cli_check(no_job, "no command of a menu of jobs reads as a job");
    return cli_checked();
}
