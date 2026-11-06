// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_help.c: the help, made from the menus: a line for each command, the ones that act in the window marked,
// its usage in a left column and what it does wrapped beside it; the keys of every command that has some, by menu;
// the words that can follow a menu, its commands and its jobs or their subjects; and the completion script for bash,
// zsh, fish or PowerShell, which queries orior_cli itself for the words after the first
#include "../text/text.h"
#include "callback_cfg.h"
#include "command_line_internal.h"
#include "commands.h"

// What the help is made into: the widest usage the left column takes, and where the help is written
typedef struct
{
    unsigned long long pad;
    VerbaLinea *out;
} CliHelpCreated;
// the widest a usage in the help's left column is; a wider one stands on lines of its own, its label under it in the
// column
#define CLI_HELP_COLUMN 44ULL

// the most groups one menu of jobs names
#define CLI_HELP_GROUPS 64u

// the longest row of the help, its usage or what it describes
#define CLI_HELP_ROW 4096u

static CliItem s_items[CLI_ITEMS_MAX];

// A row's lines as wrapped makes them: the row's usage, written to the left of its first, where it does not stand
// alone; and the help made
typedef struct
{
    const char *left;
    unsigned long long pad;
    VerbaLinea *out;
    unsigned int count;
} CliHelpLines;

void cli_help_wrap(const CliCommandLineTextEachCtx *args)
{
    const char *text = args->text;
    static char s_line[CLI_HELP_ROW * 2u];
    VerbaLinea line = {s_line, sizeof(s_line), 0ULL};
    unsigned long long letters = 0ULL;
    int any = 0;
    const char *word_text = text;
    for (;;)
    {
        const char *const space = cellul.chr(&(CatenaFinitaCfg){.src = word_text, .cap = CLI_POOL_BYTES, .byte = ' '});
        const unsigned long long size = (space != NULL)
                                            ? (unsigned long long)(space - word_text)
                                            : cellul.len(&(CatenaFinitaCfg){.src = word_text, .cap = CLI_POOL_BYTES});
        const unsigned long long length =
            EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = word_text, .size = size);
        const unsigned long long lead = any ? args->indent : 0ULL;
        if ((line.at > 0ULL) && (lead + letters + 1ULL + length > args->width))
        {
            char out[CLI_HELP_ROW * 2u + 128u];
            VerbaLinea led = {out, sizeof(out), 0ULL};
            for (unsigned long long at = 0ULL; at < lead; at += 1ULL)
            {
                mmgr_line_ch(&led, ' ');
            }
            mmgr_line_put_n(&led, s_line, line.at);
            EMBED_CALL(args->each, CliEachLineSizeCfg, .context = args->context, .line = out,
                       .size = mmgr_line_ok(&led) ? led.at : 0ULL);
            any = 1;
            line.at = 0ULL;
            letters = 0ULL;
        }
        if (line.at > 0ULL)
        {
            mmgr_line_ch(&line, ' ');
            letters += 1ULL;
        }
        mmgr_line_put_n(&line, word_text, size);
        letters += length;
        if (space == NULL)
        {
            break;
        }
        word_text = space + 1;
    }
    char out[CLI_HELP_ROW * 2u + 128u];
    VerbaLinea led = {out, sizeof(out), 0ULL};
    for (unsigned long long at = 0ULL, lead = any ? args->indent : 0ULL; at < lead; at += 1ULL)
    {
        mmgr_line_ch(&led, ' ');
    }
    mmgr_line_put_n(&led, s_line, mmgr_line_ok(&line) ? line.at : 0ULL);
    EMBED_CALL(args->each, CliEachLineSizeCfg, .context = args->context, .line = out,
               .size = mmgr_line_ok(&led) ? led.at : 0ULL);
}

// a line of a usage that stands alone, written led by two spaces
static void help_alone(const CliEachLineSizeCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, "  ");
    mmgr_line_put_n(out, args->line, args->size);
    mmgr_line_ch(out, '\n');
}

// a line of what a row describes, the row's usage to its left on its first where it does not stand alone
static void help_adjacent(const CliEachLineSizeCfg *args)
{
    unsigned long long size = args->size;
    CliHelpLines *const lines = args->context;
    const char *const left = ((lines->count == 0u) && (lines->left != NULL)) ? lines->left : "";
    mmgr_line_put(lines->out, "  ");
    mmgr_line_put(lines->out, left);
    for (unsigned long long at = EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = left,
                                            .size = cellul.len(&(CatenaFinitaCfg){.src = left, .cap = CLI_POOL_BYTES}));
         at < lines->pad; at += 1ULL)
    {
        mmgr_line_ch(lines->out, ' ');
    }
    mmgr_line_put(lines->out, "  ");
    mmgr_line_put_n(lines->out, args->line, size);
    mmgr_line_ch(lines->out, '\n');
    lines->count += 1u;
}

typedef struct
{
    CliHelpCreated *made;
    const char *usage;
    const char *desc;
    VerbaLinea *out;
    int quoted;
} CommandLineHelpCtx;
// a row of the help: where `made->out` is NULL its usage counted toward the left column's width, else written
EMBED_INLINE void help_row(const CommandLineHelpCtx *args)
{
    const unsigned long long letters =
        EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = args->usage,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->usage, .cap = CLI_POOL_BYTES}));
    if (args->made->out == NULL)
    {
        if ((letters <= CLI_HELP_COLUMN) && (letters > args->made->pad))
        {
            args->made->pad = letters;
        }
        return;
    }
    const int alone = letters > args->made->pad;
    if (alone)
    {
        EMBED_CALL(cli_command_line_text_each_internal.help_wrap, CliCommandLineTextEachCtx, .text = args->usage,
                   .width = CLI_HELP_WIDTH - 2ULL, .indent = 2ULL, .each = help_alone, .context = args->made->out);
    }
    CliHelpLines lines = {alone ? NULL : args->usage, args->made->pad, args->made->out, 0u};
    EMBED_CALL(cli_command_line_text_each_internal.help_wrap, CliCommandLineTextEachCtx, .text = args->desc,
               .width = CLI_HELP_WIDTH - args->made->pad - 4ULL, .indent = 0ULL, .each = help_adjacent,
               .context = &lines);
}

// every row of the help, each handed to help_row
static void help_rows(const CommandLineHelpCtx *args)
{
    static char s_usage[CLI_HELP_ROW];
    static char s_desc_buf[CLI_HELP_ROW];
    static char s_groups[CLI_HELP_GROUPS][CLI_LABEL_MAX_LEN];
    EMBED_CALL(help_row, CommandLineHelpCtx, .made = args->made, .usage = "orior", .desc = "open the window");
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        char word_text[CLI_LABEL_MAX_LEN];
        EMBED_CALL(cli_commands_menu_word.menu_word, CliCommandsMenuWordCfg, .menu = menu, .word_out = word_text);
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                              .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            VerbaLinea usage = {s_usage, sizeof(s_usage), 0ULL};
            mmgr_line_put(&usage, "orior_cli ");
            mmgr_line_put(&usage, word_text);
            mmgr_line_ch(&usage, ' ');
            mmgr_line_put(&usage, s_items[at].command);
            mmgr_line_ch(&usage, ' ');
            mmgr_line_put(&usage, s_items[at].args);
            usage.at = EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = s_usage,
                                  .size = mmgr_line_ok(&usage) ? usage.at : 0ULL);
            (void)mmgr_line_finish(&usage);
            VerbaLinea desc = {s_desc_buf, sizeof(s_desc_buf), 0ULL};
            mmgr_line_put(&desc, s_items[at].console ? "" : "* ");
            mmgr_line_put(&desc, s_items[at].label);
            (void)mmgr_line_finish(&desc);
            EMBED_CALL(help_row, CommandLineHelpCtx, .made = args->made, .usage = s_usage, .desc = s_desc_buf);
        }
        const unsigned int groups = EMBED_CALL(cli_commands_menu_groups.menu_groups, CliCommandsMenuGroupsCfg,
                                               .menu = menu, .groups = s_groups, .capacity = CLI_HELP_GROUPS);
        if (groups > 0u)
        {
            char split[CLI_LABEL_MAX_LEN];
            EMBED_CALL(cli_commands_menu_split.menu_split, CliCommandsMenuSplitCfg, .menu = menu, .split = split);
            VerbaLinea usage = {s_usage, sizeof(s_usage), 0ULL};
            mmgr_line_put(&usage, "orior_cli ");
            mmgr_line_put(&usage, word_text);
            if (split[0] != '\0')
            {
                mmgr_line_put(&usage, " [");
                mmgr_line_put(&usage, split);
                mmgr_line_ch(&usage, ']');
            }
            mmgr_line_put(&usage, " [job]");
            (void)mmgr_line_finish(&usage);
            VerbaLinea desc = {s_desc_buf, sizeof(s_desc_buf), 0ULL};
            mmgr_line_put(&desc, "the ");
            for (unsigned int group = 0u; group < groups; group += 1u)
            {
                mmgr_line_put(&desc, (group > 0u) ? " and " : "");
                mmgr_line_put(&desc, s_groups[group]);
            }
            mmgr_line_put(&desc, " jobs, or one of them");
            (void)mmgr_line_finish(&desc);
            EMBED_CALL(help_row, CommandLineHelpCtx, .made = args->made, .usage = s_usage, .desc = s_desc_buf);
        }
    }
    EMBED_CALL(help_row, CommandLineHelpCtx, .made = args->made, .usage = "orior_cli run <job> [key=value] [-- words]",
               .desc = "start a job, as orior_cli run start");
    EMBED_CALL(help_row, CommandLineHelpCtx, .made = args->made, .usage = "orior_cli list | show <job> | bridge [key]",
               .desc = "as orior_cli run list, run show and go bridge");
    EMBED_CALL(help_row, CommandLineHelpCtx, .made = args->made, .usage = "orior_cli <file>[:line[:column]]",
               .desc = "* open a file of the tree in the window, at a line and column");
    EMBED_CALL(help_row, CommandLineHelpCtx, .made = args->made, .usage = "orior_cli --completions <shell>",
               .desc = "the completions for bash, zsh, fish or powershell");
    EMBED_CALL(help_row, CommandLineHelpCtx, .made = args->made, .usage = "orior_cli help", .desc = "this");
}

void cli_help(const CliCommandLineOutErrMsgShellCtx *args)
{
    VerbaLinea *out = args->out;
    CliHelpCreated made = {0ULL, NULL};
    EMBED_CALL(help_rows, CommandLineHelpCtx, .made = &made);
    made.pad = (made.pad > 0ULL) ? made.pad : CLI_HELP_COLUMN;
    made.out = out;
    mmgr_line_put(out,
                  "orior: the window, and every job it runs, from one program. A menu's title and one of its\n"
                  "commands are the words for it here. A command marked * acts in the window, which it opens.\n\n");
    EMBED_CALL(help_rows, CommandLineHelpCtx, .made = &made);
    mmgr_line_put(out, "\n  --root <folder>  the orior tree to work on; else ORIOR_ROOT, else the tree the working\n"
                       "                   folder or the program sits in\n\n"
                       "A job is named by its id, by the end of its id after a slash, or by its title, where that\n"
                       "names only one. A run exits with the code of its last step. A key given twice gives two "
                       "values.\n");
}

void cli_keys(void)
{
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                              .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
        unsigned long long pad = 0ULL;
        unsigned int count = 0u;
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            if (s_items[at].keys[0] != '\0')
            {
                const unsigned long long letters =
                    EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = s_items[at].label,
                               .size = cellul.len(&(CatenaFinitaCfg){.src = s_items[at].label, .cap = CLI_POOL_BYTES}));
                pad = (letters > pad) ? letters : pad;
                count += 1u;
            }
        }
        if (count == 0u)
        {
            continue;
        }
        char title[CLI_LABEL_MAX_LEN];
        EMBED_CALL(cli_commands_menu_title.menu_title, CliCommandsMenuTitleCfg, .menu = menu, .title = title);
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = title);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            if (s_items[at].keys[0] == '\0')
            {
                continue;
            }
            char line[CLI_LABEL_MAX_LEN * 3u];
            VerbaLinea row = {line, sizeof(line), 0ULL};
            mmgr_line_put(&row, "  ");
            mmgr_line_put(&row, s_items[at].label);
            for (unsigned long long letters = EMBED_CALL(
                     cli_text_size_byte.letters, CliTextSizeByteCfg, .text = s_items[at].label,
                     .size = cellul.len(&(CatenaFinitaCfg){.src = s_items[at].label, .cap = CLI_POOL_BYTES}));
                 letters < pad; letters += 1ULL)
            {
                mmgr_line_ch(&row, ' ');
            }
            mmgr_line_put(&row, "  ");
            mmgr_line_put(&row, s_items[at].keys);
            EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = line,
                       .size = mmgr_line_ok(&row) ? row.at : 0ULL);
        }
    }
}

void cli_next_words(const CliCommandLineNamedCtx *args)
{
    const char *named = args->named;
    unsigned int menu = 0u;
    if (!EMBED_CALL(cli_commands_word.menu_by_name, CliCommandsWordCfg, .word_text = args->menu_word,
                    .menu_pointer = &menu))
    {
        return;
    }
    const unsigned int items = EMBED_CALL(cli_commands_menu_capacity.menu_items, CliCommandsMenuCapacityCfg,
                                          .menu = menu, .items = s_items, .capacity = CLI_ITEMS_MAX);
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = s_items[at].command);
    }
    char group[1][CLI_LABEL_MAX_LEN];
    static char s_root[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if ((EMBED_CALL(cli_commands_menu_groups.menu_groups, CliCommandsMenuGroupsCfg, .menu = menu, .groups = group,
                    .capacity = 1u) == 0u) ||
        !EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named, .root = s_root,
                    .err_msg = &err_msg))
    {
        return;
    }
    const CliCatalog *const catalog =
        EMBED_CALL(cli_command_line_word_internal.catalog, CliCommandLineWordCtx, .root = s_root);
    static unsigned int s_list_result[CLI_LINE_JOBS];
    const unsigned int count =
        EMBED_CALL(cli_command_line_catalog_menu_internal.menu_jobs, CliCommandLineCatalogMenuCtx, .catalog = catalog,
                   .menu = menu, .list_entries = s_list_result);
    char split[CLI_LABEL_MAX_LEN];
    EMBED_CALL(cli_commands_menu_split.menu_split, CliCommandsMenuSplitCfg, .menu = menu, .split = split);
    char subject[CLI_NAME_MAX_LEN];
    char last[CLI_NAME_MAX_LEN];
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (split[0] == '\0')
        {
            EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx,
                       .text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                          .text_u32 = catalog->job[s_list_result[at]].id));
            continue;
        }
        EMBED_CALL(cli_command_line_catalog_job_internal.job_subject, CliCommandLineCatalogJobCtx, .catalog = catalog,
                   .job = s_list_result[at], .subject = subject);
        if ((at == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = subject, .other = last, .cap = CLI_POOL_BYTES})))
        {
            EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = subject);
        }
        memor.cpy(&(MemoriaCfg){.dst = last, .src = subject, .bytes = sizeof(last)});
    }
}

// the first words, every menu's then the command line's own, joined by spaces; each quoted for PowerShell where
// `quoted` is 1, joined by a comma and a space
EMBED_INLINE void help_first(const CommandLineHelpCtx *args)
{
    static const char *const s_own[] = {"list", "show", "bridge", "--root", "--help", "--version", "--completions"};
    const char *const between = args->quoted ? ", " : " ";
    const char *const quote = args->quoted ? "'" : "";
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        char word_text[CLI_LABEL_MAX_LEN];
        EMBED_CALL(cli_commands_menu_word.menu_word, CliCommandsMenuWordCfg, .menu = menu, .word_out = word_text);
        mmgr_line_put(args->out, (menu > 0u) ? between : "");
        mmgr_line_put(args->out, quote);
        mmgr_line_put(args->out, word_text);
        mmgr_line_put(args->out, quote);
    }
    for (unsigned int at = 0u; at < sizeof(s_own) / sizeof(s_own[0]); at += 1u)
    {
        mmgr_line_put(args->out, ((at > 0u) || (cli_menus() > 0u)) ? between : "");
        mmgr_line_put(args->out, quote);
        mmgr_line_put(args->out, s_own[at]);
        mmgr_line_put(args->out, quote);
    }
}

// the bash script
EMBED_INLINE void help_bash(const CommandLineHelpCtx *args)
{
    mmgr_line_put(args->out, "_orior() {\n  local cur=${COMP_WORDS[COMP_CWORD]}\n  if [ \"$COMP_CWORD\" -eq 1 ]; then\n"
                             "    COMPREPLY=($(compgen -W \"");
    EMBED_CALL(help_first, CommandLineHelpCtx, .out = args->out, .quoted = 0);
    mmgr_line_put(args->out, "\" -- \"$cur\"))\n  else\n    COMPREPLY=($(compgen -W \"$(orior_cli __words "
                             "\"${COMP_WORDS[1]}\" 2>/dev/null)\" -- \"$cur\"))\n  fi\n}\n"
                             "complete -o default -F _orior orior\n");
}

int cli_completions(const CliCommandLineOutErrMsgShellCtx *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->shell, .other = "bash", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(help_bash, CommandLineHelpCtx, .out = args->out);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->shell, .other = "zsh", .cap = CLI_POOL_BYTES}))
    {
        mmgr_line_put(args->out, "autoload -U +X bashcompinit && bashcompinit\n");
        EMBED_CALL(help_bash, CommandLineHelpCtx, .out = args->out);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->shell, .other = "fish", .cap = CLI_POOL_BYTES}))
    {
        mmgr_line_put(args->out, "complete -c orior_cli -n __fish_use_subcommand -a \"");
        EMBED_CALL(help_first, CommandLineHelpCtx, .out = args->out, .quoted = 0);
        mmgr_line_put(args->out, "\"\ncomplete -c orior_cli -n 'not __fish_use_subcommand' -a "
                                 "'(orior_cli __words (commandline -opc)[2])'\n");
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->shell, .other = "powershell", .cap = CLI_POOL_BYTES}))
    {
        mmgr_line_put(args->out, "Register-ArgumentCompleter -Native -CommandName orior_cli -ScriptBlock {\n"
                                 "  param($word, $ast, $at)\n"
                                 "  $given = @($ast.CommandElements | ForEach-Object { $_.ToString() })\n"
                                 "  if ($given.Count -le 1 -or ($given.Count -eq 2 -and $word)) { $all = @(");
        EMBED_CALL(help_first, CommandLineHelpCtx, .out = args->out, .quoted = 1);
        mmgr_line_put(args->out,
                      ") } else { $all = @(& orior_cli __words $given[1]) }\n"
                      "  $all | Where-Object { $_ -like \"$word*\" } | ForEach-Object { "
                      "[System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }\n"
                      "}\n");
        return 1;
    }
    mmgr_line_put(args->err_msg, "orior_cli has no completions for ");
    mmgr_line_put(args->err_msg, args->shell);
    mmgr_line_put(args->err_msg, ": bash, zsh, fish or powershell");
    return 0;
}
