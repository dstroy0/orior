// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_menus.c: a menu at the command line: its commands, each marked where it acts in the window, and the
// jobs of a menu of jobs, or their subjects where it splits them; and a job of a menu of jobs, named after its subject
// where the menu splits them, shown as the window shows it on choosing
#include "../text/text.h"
#include "command_line_internal.h"
#include "commands.h"

// the most groups one menu of jobs names
#define CLI_MENU_GROUPS 64u

unsigned int cli_menu_jobs(const CliCatalog *catalog, unsigned int menu, unsigned int *listed)
{
    static char s_groups[CLI_MENU_GROUPS][CLI_LABEL_LONGEST];
    const unsigned int groups = cli_menu_groups(menu, s_groups, CLI_MENU_GROUPS);
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < catalog->jobs) && (count < CLI_LINE_JOBS); at += 1u)
    {
        for (unsigned int group = 0u; group < groups; group += 1u)
        {
            if (cellul.eq(
                    &(CatenaFinitaCfg){.src = catalog->job[at].group, .other = s_groups[group], .cap = CLI_CELLS_POOL}))
            {
                listed[count] = at;
                count += 1u;
                break;
            }
        }
    }
    return count;
}

// the jobs `listed`, `count` of them, written by their subjects, a subject the one before has not written again
static void menus_subjects(const CliCatalog *catalog, const unsigned int *listed, unsigned int count)
{
    char subject[CLI_NAME_LONGEST];
    char last[CLI_NAME_LONGEST];
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_job_subject(catalog, listed[at], subject);
        if ((at == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = subject, .other = last, .cap = CLI_CELLS_POOL})))
        {
            cli_out(subject);
        }
        memor.cpy(&(MemoriaCfg){.dst = last, .src = subject, .bytes = sizeof(last)});
    }
}

int cli_menu_lines(const char *root, unsigned int menu)
{
    static CliItem s_items[CLI_ITEMS_MOST];
    char word[CLI_LABEL_LONGEST];
    cli_menu_word(menu, word);
    const unsigned int items = cli_menu_items(menu, s_items, CLI_ITEMS_MOST);
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        char line[CLI_LABEL_LONGEST * 4u];
        CliLine usage = {line, sizeof(line), 0ull};
        cli_line_text(&usage, "orior ");
        cli_line_text(&usage, word);
        cli_line_character(&usage, ' ');
        cli_line_text(&usage, s_items[at].command);
        cli_line_character(&usage, ' ');
        cli_line_text(&usage, s_items[at].args);
        usage.at = cli_text_trimmed_end(line, usage.at);
        cli_line_text(&usage, s_items[at].console ? "" : "  *");
        cli_out_line(line, cli_line_fits(&usage) ? usage.at : 0ull);
    }
    char split[CLI_LABEL_LONGEST];
    char group[1][CLI_LABEL_LONGEST];
    if (cli_menu_groups(menu, group, 1u) == 0u)
    {
        return 0;
    }
    if (root == NULL)
    {
        cli_err("no orior tree here: run from inside one, or name one with --root");
        return CLI_NO_CODE;
    }
    const CliCatalog *const catalog = cli_command_line_catalog(root);
    static unsigned int s_listed[CLI_LINE_JOBS];
    const unsigned int count = cli_menu_jobs(catalog, menu, s_listed);
    cli_menu_split(menu, split);
    if (split[0] != '\0')
    {
        menus_subjects(catalog, s_listed, count);
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_out(cli_catalog_at(catalog, catalog->job[s_listed[at]].id));
    }
    return 0;
}

int cli_menu_job(const char *named, unsigned int menu, const char *const *words, unsigned int count)
{
    static char s_root[CLI_PATH_LONGEST];
    char why[4096];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_command_line_tree(named, s_root, &said))
    {
        cli_err_line(why, cli_line_fits(&said) ? said.at : 0ull);
        return CLI_NO_CODE;
    }
    const CliCatalog *const catalog = cli_command_line_catalog(s_root);
    static unsigned int s_listed[CLI_LINE_JOBS];
    unsigned int jobs = cli_menu_jobs(catalog, menu, s_listed);
    unsigned int next = 0u;
    char split[CLI_LABEL_LONGEST];
    cli_menu_split(menu, split);
    if (split[0] != '\0')
    {
        if (count == 0u)
        {
            return cli_menu_lines(s_root, menu);
        }
        const char *const name = words[0];
        next = 1u;
        unsigned int kept = 0u;
        char subject[CLI_NAME_LONGEST];
        for (unsigned int at = 0u; at < jobs; at += 1u)
        {
            cli_job_subject(catalog, s_listed[at], subject);
            if (cellul.eq(&(CatenaFinitaCfg){.src = subject, .other = name, .cap = CLI_CELLS_POOL}))
            {
                s_listed[kept] = s_listed[at];
                kept += 1u;
            }
        }
        jobs = kept;
        if (jobs == 0u)
        {
            char title[CLI_LABEL_LONGEST];
            char word[CLI_LABEL_LONGEST];
            cli_menu_title(menu, title);
            cli_menu_word(menu, word);
            said.at = 0ull;
            cli_line_text(&said, title);
            cli_line_text(&said, " has no ");
            cli_line_text(&said, name);
            cli_line_text(&said, ": orior ");
            cli_line_text(&said, word);
            cli_line_text(&said, " names what it has");
            cli_err_line(why, cli_line_fits(&said) ? said.at : 0ull);
            return CLI_NO_CODE;
        }
        if (count == next)
        {
            for (unsigned int at = 0u; at < jobs; at += 1u)
            {
                cli_out(cli_catalog_at(catalog, catalog->job[s_listed[at]].id));
            }
            return 0;
        }
    }
    if (count == next)
    {
        return cli_menu_lines(s_root, menu);
    }
    said.at = 0ull;
    const unsigned int job = cli_job_named(catalog, s_listed, jobs, words[next], &said);
    if (job == CLI_CATALOG_NONE)
    {
        cli_err_line(why, cli_line_fits(&said) ? said.at : 0ull);
        return CLI_NO_CODE;
    }
    cli_job_show(s_root, catalog, job);
    return 0;
}
