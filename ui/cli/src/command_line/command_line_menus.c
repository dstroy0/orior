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

unsigned int cli_menu_jobs(const CliCommandLineCatalogMenuCtx *args)
{
    static char s_groups[CLI_MENU_GROUPS][CLI_LABEL_MAX_LEN];
    const unsigned int groups = EMBED_CALL(cli_commands_menu_groups.menu_groups, CliCommandsMenuGroupsCfg,
                                           .menu = args->menu, .groups = s_groups, .capacity = CLI_MENU_GROUPS);
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < args->catalog->jobs) && (count < CLI_LINE_JOBS); at += 1u)
    {
        for (unsigned int group = 0u; group < groups; group += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){
                    .src = args->catalog->job[at].group, .other = s_groups[group], .cap = CLI_POOL_BYTES}))
            {
                args->list_entries[count] = at;
                count += 1u;
                break;
            }
        }
    }
    return count;
}

typedef struct
{
    const CliCatalog *catalog;
    const unsigned int *list_entries;
    unsigned int count;
} CommandLineMenusCtx;
// the jobs `list_entries`, `count` of them, written by their subjects, a subject the one before has not written again
EMBED_INLINE void menus_subjects(const CommandLineMenusCtx *args)
{
    const CliCatalog *catalog = args->catalog;
    char subject[CLI_NAME_MAX_LEN];
    char last[CLI_NAME_MAX_LEN];
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(cli_command_line_catalog_job_internal.job_subject, CliCommandLineCatalogJobCtx, .catalog = catalog,
                   .job = args->list_entries[at], .subject = subject);
        if ((at == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = subject, .other = last, .cap = CLI_POOL_BYTES})))
        {
            EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = subject);
        }
        memor.cpy(&(MemoriaCfg){.dst = last, .src = subject, .bytes = sizeof(last)});
    }
}

// the depth of the tree of commands a menu's chains are written to at the most
#define CLI_MENU_CHAIN_DEPTH 8u

typedef struct
{
    const char *words;
    const CliItem *items;
    unsigned int count;
    unsigned int depth;
} CommandLineChainsCtx;
// each of `count` rows written as `imp <words> <command> <args>`, marked where it acts in the window, a row with
// rows under it written by its rows in its place, its command added to the words
static void menus_chains(const CommandLineChainsCtx *args)
{
    static CliItem s_rows[CLI_MENU_CHAIN_DEPTH][CLI_ITEMS_MAX];
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const CliItem *const item = &args->items[at];
        char line[CLI_LABEL_MAX_LEN * 4u];
        VerbaLinea usage = {line, sizeof(line), 0ULL};
        mmgr_line_put(&usage, args->words);
        mmgr_line_ch(&usage, ' ');
        mmgr_line_put(&usage, item->command);
        if ((item->list != 0u) && (args->depth < CLI_MENU_CHAIN_DEPTH))
        {
            (void)mmgr_line_finish(&usage);
            const unsigned int rows = cli_menu_rows(item->list, s_rows[args->depth], CLI_ITEMS_MAX);
            menus_chains(&(CommandLineChainsCtx){
                .words = line, .items = s_rows[args->depth], .count = rows, .depth = args->depth + 1u});
            continue;
        }
        mmgr_line_ch(&usage, ' ');
        mmgr_line_put(&usage, item->args);
        usage.at = EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = line, .size = usage.at);
        mmgr_line_put(&usage, (item->console || item->setting) ? "" : "  *");
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = line,
                   .size = mmgr_line_ok(&usage) ? usage.at : 0ULL);
    }
}

int cli_menu_lines(const CliCommandLineWordCtx *args)
{
    static CliItem s_items[CLI_ITEMS_MAX];
    char word_text[CLI_LABEL_MAX_LEN];
    EMBED_CALL(cli_commands_menu_word.menu_word, CliCommandsMenuWordCfg, .menu = args->menu, .word_out = word_text);
    const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                          .menu = args->menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
    char words[CLI_LABEL_MAX_LEN];
    VerbaLinea first = {words, sizeof(words), 0ULL};
    mmgr_line_put(&first, "imp ");
    mmgr_line_put(&first, word_text);
    (void)mmgr_line_finish(&first);
    menus_chains(&(CommandLineChainsCtx){.words = words, .items = s_items, .count = items, .depth = 0u});
    char split[CLI_LABEL_MAX_LEN];
    char group[1][CLI_LABEL_MAX_LEN];
    if (EMBED_CALL(cli_commands_menu_groups.menu_groups, CliCommandsMenuGroupsCfg, .menu = args->menu, .groups = group,
                   .capacity = 1u) == 0u)
    {
        return 0;
    }
    if (args->root == NULL)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = "no orior tree here: run from inside one, or name one with --root");
        return CLI_NO_CODE;
    }
    const CliCatalog *const catalog = cli_command_line_word_internal.catalog(args);
    static unsigned int s_list_result[CLI_LINE_JOBS];
    const unsigned int count =
        EMBED_CALL(cli_command_line_catalog_menu_internal.menu_jobs, CliCommandLineCatalogMenuCtx, .catalog = catalog,
                   .menu = args->menu, .list_entries = s_list_result);
    EMBED_CALL(cli_commands_menu_split.menu_split, CliCommandsMenuSplitCfg, .menu = args->menu, .split = split);
    if (split[0] != '\0')
    {
        EMBED_CALL(menus_subjects, CommandLineMenusCtx, .catalog = catalog, .list_entries = s_list_result,
                   .count = count);
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx,
                   .text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                      .text_u32 = catalog->job[s_list_result[at]].id));
    }
    return 0;
}

int cli_menu_job(const CliCommandLineWordsCountMenuCtx *args)
{
    const char *named = args->named;
    static char s_root[CLI_PATH_MAX_LEN];
    char why[4096];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named, .root = s_root,
                    .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = why,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return CLI_NO_CODE;
    }
    const CliCatalog *const catalog =
        EMBED_CALL(cli_command_line_word_internal.catalog, CliCommandLineWordCtx, .root = s_root);
    static unsigned int s_list_result[CLI_LINE_JOBS];
    unsigned int jobs = EMBED_CALL(cli_command_line_catalog_menu_internal.menu_jobs, CliCommandLineCatalogMenuCtx,
                                   .catalog = catalog, .menu = args->menu, .list_entries = s_list_result);
    unsigned int next = 0u;
    char split[CLI_LABEL_MAX_LEN];
    EMBED_CALL(cli_commands_menu_split.menu_split, CliCommandsMenuSplitCfg, .menu = args->menu, .split = split);
    if (split[0] != '\0')
    {
        if (args->count == 0u)
        {
            return EMBED_CALL(cli_command_line_word_internal.menu_lines, CliCommandLineWordCtx, .root = s_root,
                              .menu = args->menu);
        }
        const char *const name = args->words[0];
        next = 1u;
        unsigned int listing_count = 0u;
        char subject[CLI_NAME_MAX_LEN];
        for (unsigned int at = 0u; at < jobs; at += 1u)
        {
            EMBED_CALL(cli_command_line_catalog_job_internal.job_subject, CliCommandLineCatalogJobCtx,
                       .catalog = catalog, .job = s_list_result[at], .subject = subject);
            if (cellul.eq(&(CatenaFinitaCfg){.src = subject, .other = name, .cap = CLI_POOL_BYTES}))
            {
                s_list_result[listing_count] = s_list_result[at];
                listing_count += 1u;
            }
        }
        jobs = listing_count;
        if (jobs == 0u)
        {
            char title[CLI_LABEL_MAX_LEN];
            char word_text[CLI_LABEL_MAX_LEN];
            EMBED_CALL(cli_commands_menu_title.menu_title, CliCommandsMenuTitleCfg, .menu = args->menu, .title = title);
            EMBED_CALL(cli_commands_menu_word.menu_word, CliCommandsMenuWordCfg, .menu = args->menu,
                       .word_out = word_text);
            err_msg.at = 0ULL;
            mmgr_line_put(&err_msg, title);
            mmgr_line_put(&err_msg, " has no ");
            mmgr_line_put(&err_msg, name);
            mmgr_line_put(&err_msg, ": imp ");
            mmgr_line_put(&err_msg, word_text);
            mmgr_line_put(&err_msg, " names what it has");
            EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = why,
                       .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
            return CLI_NO_CODE;
        }
        if (args->count == next)
        {
            for (unsigned int at = 0u; at < jobs; at += 1u)
            {
                EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx,
                           .text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                              .text_u32 = catalog->job[s_list_result[at]].id));
            }
            return 0;
        }
    }
    if (args->count == next)
    {
        return EMBED_CALL(cli_command_line_word_internal.menu_lines, CliCommandLineWordCtx, .root = s_root,
                          .menu = args->menu);
    }
    err_msg.at = 0ULL;
    const unsigned int job = EMBED_CALL(cli_command_line_catalog_among_internal.job_by_name,
                                        CliCommandLineCatalogAmongCtx, .catalog = catalog, .among = s_list_result,
                                        .among_count = jobs, .name = args->words[next], .err_msg = &err_msg);
    if (job == CLI_CATALOG_NONE)
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = why,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return CLI_NO_CODE;
    }
    EMBED_CALL(cli_command_line_root_internal.job_show, CliCommandLineRootCtx, .root = s_root, .catalog = catalog,
               .job = job);
    return 0;
}
