// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_help.c: the help, made from the menus: a line for each command, the ones that act in the window marked,
// its usage in a left column and what it does wrapped beside it; the keys of every command that has some, by menu;
// the words that can follow a menu, its commands and its jobs or their subjects; and the completion script for bash,
// zsh, fish or PowerShell, which queries orior_cli itself for the words after the first
#include "../text/text.h"
#include "command_line_internal.h"
#include "commands.h"

// the widest a usage in the help's left column is; a wider one stands on lines of its own, its label under it in the
// column
#define CLI_HELP_COLUMN 44ull

// the most groups one menu of jobs names
#define CLI_HELP_GROUPS 64u

// the longest row of the help, its usage or what it describes
#define CLI_HELP_ROW 4096u

static CliItem s_items[CLI_ITEMS_MOST];

// What the help is made into: the widest usage the left column takes, and where the help is written
typedef struct
{
    unsigned long long pad;
    VerbaLinea *out;
} CliHelpMade;

// A row's lines as wrapped makes them: the row's usage, written to the left of its first, where it does not stand
// alone; and the help made
typedef struct
{
    const char *left;
    unsigned long long pad;
    VerbaLinea *out;
    unsigned int count;
} CliHelpLines;

void cli_help_wrapped(const char *text, unsigned long long width, unsigned long long indent,
                      void (*each)(void *context, const char *line, unsigned long long size), void *context)
{
    static char s_line[CLI_HELP_ROW * 2u];
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    unsigned long long letters = 0ull;
    int any = 0;
    const char *word = text;
    for (;;)
    {
        const char *const space = cellul.chr(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL, .byte = ' '});
        const unsigned long long size = (space != NULL)
                                            ? (unsigned long long)(space - word)
                                            : cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL});
        const unsigned long long length = cli_text_letters(word, size);
        const unsigned long long lead = any ? indent : 0ull;
        if ((line.at > 0ull) && (lead + letters + 1ull + length > width))
        {
            char out[CLI_HELP_ROW * 2u + 128u];
            VerbaLinea led = {out, sizeof(out), 0ull};
            for (unsigned long long at = 0ull; at < lead; at += 1ull)
            {
                mmgr_line_ch(&led, ' ');
            }
            mmgr_line_put_n(&led, s_line, line.at);
            each(context, out, mmgr_line_ok(&led) ? led.at : 0ull);
            any = 1;
            line.at = 0ull;
            letters = 0ull;
        }
        if (line.at > 0ull)
        {
            mmgr_line_ch(&line, ' ');
            letters += 1ull;
        }
        mmgr_line_put_n(&line, word, size);
        letters += length;
        if (space == NULL)
        {
            break;
        }
        word = space + 1;
    }
    char out[CLI_HELP_ROW * 2u + 128u];
    VerbaLinea led = {out, sizeof(out), 0ull};
    for (unsigned long long at = 0ull, lead = any ? indent : 0ull; at < lead; at += 1ull)
    {
        mmgr_line_ch(&led, ' ');
    }
    mmgr_line_put_n(&led, s_line, mmgr_line_ok(&line) ? line.at : 0ull);
    each(context, out, mmgr_line_ok(&led) ? led.at : 0ull);
}

// a line of a usage that stands alone, written led by two spaces
static void help_alone(void *context, const char *line, unsigned long long size)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, "  ");
    mmgr_line_put_n(out, line, size);
    mmgr_line_ch(out, '\n');
}

// a line of what a row describes, the row's usage to its left on its first where it does not stand alone
static void help_beside(void *context, const char *line, unsigned long long size)
{
    CliHelpLines *const lines = context;
    const char *const left = ((lines->count == 0u) && (lines->left != NULL)) ? lines->left : "";
    mmgr_line_put(lines->out, "  ");
    mmgr_line_put(lines->out, left);
    for (unsigned long long at =
             cli_text_letters(left, cellul.len(&(CatenaFinitaCfg){.src = left, .cap = CLI_CELLS_POOL}));
         at < lines->pad; at += 1ull)
    {
        mmgr_line_ch(lines->out, ' ');
    }
    mmgr_line_put(lines->out, "  ");
    mmgr_line_put_n(lines->out, line, size);
    mmgr_line_ch(lines->out, '\n');
    lines->count += 1u;
}

// a row of the help: where `made->out` is NULL its usage counted toward the left column's width, else written
static void help_row(CliHelpMade *made, const char *usage, const char *desc)
{
    const unsigned long long letters =
        cli_text_letters(usage, cellul.len(&(CatenaFinitaCfg){.src = usage, .cap = CLI_CELLS_POOL}));
    if (made->out == NULL)
    {
        if ((letters <= CLI_HELP_COLUMN) && (letters > made->pad))
        {
            made->pad = letters;
        }
        return;
    }
    const int alone = letters > made->pad;
    if (alone)
    {
        cli_help_wrapped(usage, CLI_HELP_WIDTH - 2ull, 2ull, help_alone, made->out);
    }
    CliHelpLines lines = {alone ? NULL : usage, made->pad, made->out, 0u};
    cli_help_wrapped(desc, CLI_HELP_WIDTH - made->pad - 4ull, 0ull, help_beside, &lines);
}

// every row of the help, each handed to help_row
static void help_rows(CliHelpMade *made)
{
    static char s_usage[CLI_HELP_ROW];
    static char s_desc_buf[CLI_HELP_ROW];
    static char s_groups[CLI_HELP_GROUPS][CLI_LABEL_MAX_LEN];
    help_row(made, "orior", "open the window");
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        char word[CLI_LABEL_MAX_LEN];
        cli_menu_word(menu, word);
        const unsigned int items = cli_menu_items(menu, s_items, CLI_ITEMS_MOST);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            VerbaLinea usage = {s_usage, sizeof(s_usage), 0ull};
            mmgr_line_put(&usage, "orior_cli ");
            mmgr_line_put(&usage, word);
            mmgr_line_ch(&usage, ' ');
            mmgr_line_put(&usage, s_items[at].command);
            mmgr_line_ch(&usage, ' ');
            mmgr_line_put(&usage, s_items[at].args);
            usage.at = cli_text_trimmed_end(s_usage, mmgr_line_ok(&usage) ? usage.at : 0ull);
            (void)mmgr_line_finish(&usage);
            VerbaLinea desc = {s_desc_buf, sizeof(s_desc_buf), 0ull};
            mmgr_line_put(&desc, s_items[at].console ? "" : "* ");
            mmgr_line_put(&desc, s_items[at].label);
            (void)mmgr_line_finish(&desc);
            help_row(made, s_usage, s_desc_buf);
        }
        const unsigned int groups = cli_menu_groups(menu, s_groups, CLI_HELP_GROUPS);
        if (groups > 0u)
        {
            char split[CLI_LABEL_MAX_LEN];
            cli_menu_split(menu, split);
            VerbaLinea usage = {s_usage, sizeof(s_usage), 0ull};
            mmgr_line_put(&usage, "orior_cli ");
            mmgr_line_put(&usage, word);
            if (split[0] != '\0')
            {
                mmgr_line_put(&usage, " [");
                mmgr_line_put(&usage, split);
                mmgr_line_ch(&usage, ']');
            }
            mmgr_line_put(&usage, " [job]");
            (void)mmgr_line_finish(&usage);
            VerbaLinea desc = {s_desc_buf, sizeof(s_desc_buf), 0ull};
            mmgr_line_put(&desc, "the ");
            for (unsigned int group = 0u; group < groups; group += 1u)
            {
                mmgr_line_put(&desc, (group > 0u) ? " and " : "");
                mmgr_line_put(&desc, s_groups[group]);
            }
            mmgr_line_put(&desc, " jobs, or one of them");
            (void)mmgr_line_finish(&desc);
            help_row(made, s_usage, s_desc_buf);
        }
    }
    help_row(made, "orior_cli run <job> [key=value] [-- words]", "start a job, as orior_cli run start");
    help_row(made, "orior_cli list | show <job> | bridge [key]", "as orior_cli run list, run show and go bridge");
    help_row(made, "orior_cli <file>[:line[:column]]", "* open a file of the tree in the window, at a line and column");
    help_row(made, "orior_cli --completions <shell>", "the completions for bash, zsh, fish or powershell");
    help_row(made, "orior_cli help", "this");
}

void cli_help(VerbaLinea *out)
{
    CliHelpMade made = {0ull, NULL};
    help_rows(&made);
    made.pad = (made.pad > 0ull) ? made.pad : CLI_HELP_COLUMN;
    made.out = out;
    mmgr_line_put(out,
                  "orior: the window, and every job it runs, from one program. A menu's title and one of its\n"
                  "commands are the words for it here. A command marked * acts in the window, which it opens.\n\n");
    help_rows(&made);
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
        const unsigned int items = cli_menu_items(menu, s_items, CLI_ITEMS_MOST);
        unsigned long long pad = 0ull;
        unsigned int count = 0u;
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            if (s_items[at].keys[0] != '\0')
            {
                const unsigned long long letters = cli_text_letters(
                    s_items[at].label, cellul.len(&(CatenaFinitaCfg){.src = s_items[at].label, .cap = CLI_CELLS_POOL}));
                pad = (letters > pad) ? letters : pad;
                count += 1u;
            }
        }
        if (count == 0u)
        {
            continue;
        }
        char title[CLI_LABEL_MAX_LEN];
        cli_menu_title(menu, title);
        cli_out(title);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            if (s_items[at].keys[0] == '\0')
            {
                continue;
            }
            char line[CLI_LABEL_MAX_LEN * 3u];
            VerbaLinea row = {line, sizeof(line), 0ull};
            mmgr_line_put(&row, "  ");
            mmgr_line_put(&row, s_items[at].label);
            for (unsigned long long letters =
                     cli_text_letters(s_items[at].label,
                                      cellul.len(&(CatenaFinitaCfg){.src = s_items[at].label, .cap = CLI_CELLS_POOL}));
                 letters < pad; letters += 1ull)
            {
                mmgr_line_ch(&row, ' ');
            }
            mmgr_line_put(&row, "  ");
            mmgr_line_put(&row, s_items[at].keys);
            cli_out_line(line, mmgr_line_ok(&row) ? row.at : 0ull);
        }
    }
}

void cli_next_words(const char *named, const char *menu_word)
{
    unsigned int menu = 0u;
    if (!cli_menu_named(menu_word, &menu))
    {
        return;
    }
    const unsigned int items = cli_menu_items(menu, s_items, CLI_ITEMS_MOST);
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        cli_out(s_items[at].command);
    }
    char group[1][CLI_LABEL_MAX_LEN];
    static char s_root[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if ((cli_menu_groups(menu, group, 1u) == 0u) || !cli_command_line_tree(named, s_root, &err_msg))
    {
        return;
    }
    const CliCatalog *const catalog = cli_command_line_catalog(s_root);
    static unsigned int s_listed[CLI_LINE_JOBS];
    const unsigned int count = cli_menu_jobs(catalog, menu, s_listed);
    char split[CLI_LABEL_MAX_LEN];
    cli_menu_split(menu, split);
    char subject[CLI_NAME_MAX_LEN];
    char last[CLI_NAME_MAX_LEN];
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (split[0] == '\0')
        {
            cli_out(cli_catalog_at(catalog, catalog->job[s_listed[at]].id));
            continue;
        }
        cli_job_subject(catalog, s_listed[at], subject);
        if ((at == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = subject, .other = last, .cap = CLI_CELLS_POOL})))
        {
            cli_out(subject);
        }
        memor.cpy(&(MemoriaCfg){.dst = last, .src = subject, .bytes = sizeof(last)});
    }
}

// the first words, every menu's then the command line's own, joined by spaces; each quoted for PowerShell where
// `quoted` is 1, joined by a comma and a space
static void help_first(VerbaLinea *out, int quoted)
{
    static const char *const s_own[] = {"list", "show", "bridge", "--root", "--help", "--version", "--completions"};
    const char *const between = quoted ? ", " : " ";
    const char *const quote = quoted ? "'" : "";
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        char word[CLI_LABEL_MAX_LEN];
        cli_menu_word(menu, word);
        mmgr_line_put(out, (menu > 0u) ? between : "");
        mmgr_line_put(out, quote);
        mmgr_line_put(out, word);
        mmgr_line_put(out, quote);
    }
    for (unsigned int at = 0u; at < sizeof(s_own) / sizeof(s_own[0]); at += 1u)
    {
        mmgr_line_put(out, ((at > 0u) || (cli_menus() > 0u)) ? between : "");
        mmgr_line_put(out, quote);
        mmgr_line_put(out, s_own[at]);
        mmgr_line_put(out, quote);
    }
}

// the bash script
static void help_bash(VerbaLinea *out)
{
    mmgr_line_put(out, "_orior() {\n  local cur=${COMP_WORDS[COMP_CWORD]}\n  if [ \"$COMP_CWORD\" -eq 1 ]; then\n"
                       "    COMPREPLY=($(compgen -W \"");
    help_first(out, 0);
    mmgr_line_put(out, "\" -- \"$cur\"))\n  else\n    COMPREPLY=($(compgen -W \"$(orior_cli __words "
                       "\"${COMP_WORDS[1]}\" 2>/dev/null)\" -- \"$cur\"))\n  fi\n}\n"
                       "complete -o default -F _orior orior\n");
}

int cli_completions(const char *shell, VerbaLinea *out, VerbaLinea *err_msg)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = shell, .other = "bash", .cap = CLI_CELLS_POOL}))
    {
        help_bash(out);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = shell, .other = "zsh", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(out, "autoload -U +X bashcompinit && bashcompinit\n");
        help_bash(out);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = shell, .other = "fish", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(out, "complete -c orior_cli -n __fish_use_subcommand -a \"");
        help_first(out, 0);
        mmgr_line_put(out, "\"\ncomplete -c orior_cli -n 'not __fish_use_subcommand' -a "
                           "'(orior_cli __words (commandline -opc)[2])'\n");
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = shell, .other = "powershell", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(out, "Register-ArgumentCompleter -Native -CommandName orior_cli -ScriptBlock {\n"
                           "  param($word, $ast, $at)\n"
                           "  $given = @($ast.CommandElements | ForEach-Object { $_.ToString() })\n"
                           "  if ($given.Count -le 1 -or ($given.Count -eq 2 -and $word)) { $all = @(");
        help_first(out, 1);
        mmgr_line_put(out, ") } else { $all = @(& orior_cli __words $given[1]) }\n"
                           "  $all | Where-Object { $_ -like \"$word*\" } | ForEach-Object { "
                           "[System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }\n"
                           "}\n");
        return 1;
    }
    mmgr_line_put(err_msg, "orior_cli has no completions for ");
    mmgr_line_put(err_msg, shell);
    mmgr_line_put(err_msg, ": bash, zsh, fish or powershell");
    return 0;
}
