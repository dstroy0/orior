// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_run.c: the words given on the command line run: --root and --remote taken out of them, then the help,
// the completions, the words after a menu, the version and the programs of their own; then the commands the terminal
// runs here and the menus, a command that acts in the window said to a window open on the tree, run here where the
// terminal can do it and no window is open, or the window opened to run it; and a word that names a file of the tree
// opened in the window
#include "../comms/link.h"
#include "../machine/reach.h"
#include "../machine/report.h"
#include "../text/text.h"
#include "at_terminal.h"
#include "command_line_internal.h"
#include "commands.h"

static char s_said[1u << 14u];
static const char *s_words[CLI_LAUNCH_ARGS];

// the outcome made an exit with `code`
static void run_exit(CliOutcome *outcome, int code)
{
    outcome->window = 0;
    outcome->code = code;
}

// `text` copied into `into`, which holds `room`, cut where it does not fit
static void run_copied(char *into, size_t room, const char *text)
{
    CliLine line = {into, room, 0ull};
    cli_line_text(&line, text);
    if (cli_line_closed(&line) == 0ull)
    {
        into[0] = '\0';
    }
}

// the outcome made the window to open, on the tree at `root` or on the one `remote` names where either is given, with
// the command of its menus to run once it is up and its words
static void run_launch(CliOutcome *outcome, const char *root, const char *remote, const char *menu, const char *command,
                       const char *const *args, unsigned int count)
{
    outcome->window = 1;
    outcome->code = 0;
    outcome->has_root = (root != NULL);
    run_copied(outcome->root, sizeof(outcome->root), (root != NULL) ? root : "");
    outcome->has_remote = (remote != NULL);
    run_copied(outcome->remote, sizeof(outcome->remote), (remote != NULL) ? remote : "");
    run_copied(outcome->menu, sizeof(outcome->menu), menu);
    run_copied(outcome->command, sizeof(outcome->command), command);
    outcome->args = 0u;
    outcome->used = 0u;
    for (unsigned int at = 0u; (at < count) && (at < CLI_LAUNCH_ARGS); at += 1u)
    {
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args[at], .cap = CLI_CELLS_POOL});
        if (outcome->used + size + 1u > CLI_LAUNCH_TEXT)
        {
            break;
        }
        memor.cpy(&(MemoriaCfg){.dst = outcome->text + outcome->used, .src = args[at], .bytes = size + 1u});
        outcome->arg_at[outcome->args] = outcome->used;
        outcome->args += 1u;
        // the text held is far below what an unsigned int counts
        outcome->used += (unsigned int)size + 1u;
    }
}

const char *cli_launch_arg(const CliOutcome *launch, unsigned int at)
{
    return launch->text + launch->arg_at[at];
}

void cli_launch_words(const CliOutcome *launch, CliLine *out)
{
    const unsigned long long start = out->at;
    const char *const named[2] = {launch->menu, launch->command};
    for (unsigned int at = 0u; at < 2u + launch->args; at += 1u)
    {
        const char *const word = (at < 2u) ? named[at] : cli_launch_arg(launch, at - 2u);
        if (word[0] != '\0')
        {
            cli_line_text(out, (out->at > start) ? " " : "");
            cli_line_text(out, word);
        }
    }
}

// the window a command of its menus opens: on another machine where --remote names one; else said to a window open on
// the tree, which runs the command itself, as reach.h says; else run here where the terminal can do it, as
// at_terminal.h says; else the window opened to run it
static void run_window(CliOutcome *outcome, const char *named, const char *remote, const char *menu,
                       const char *command, const char *const *args, unsigned int count)
{
    CliLine said = {s_said, sizeof(s_said), 0ull};
    if (remote != NULL)
    {
        static CliLinkAddress s_address;
        if (!cli_link_address_parse(remote, &s_address, &said))
        {
            cli_err_line(s_said, cli_line_fits(&said) ? said.at : 0ull);
            run_exit(outcome, CLI_WRONG);
            return;
        }
        run_launch(outcome, NULL, remote, menu, command, args, count);
        return;
    }
    static char s_found[CLI_PATH_LONGEST];
    const int found = (named != NULL) ? cli_command_line_tree(named, s_found, &said) : cli_tree_found(s_found);
    said.at = 0ull;
    if (command[0] != '\0')
    {
        static CliReachAnswer s_answer;
        const int asked = cli_reach_ask(found ? s_found : NULL, menu, command, args, count, &s_answer, &said);
        if (asked == 0)
        {
            cli_err_line(s_said, cli_line_fits(&said) ? said.at : 0ull);
            run_exit(outcome, CLI_WRONG);
            return;
        }
        if (asked > 0)
        {
            if (s_answer.said[0] != '\0')
            {
                (s_answer.done ? cli_out : cli_err)(s_answer.said);
            }
            run_exit(outcome, s_answer.done ? 0 : CLI_WRONG);
            return;
        }
    }
    if (cli_at_terminal_runs(command))
    {
        if (!found && !cli_folder_current(s_found, sizeof(s_found)))
        {
            cli_err("orior finds no folder to run in");
            run_exit(outcome, CLI_WRONG);
            return;
        }
        static char s_out[1u << 20u];
        CliLine out = {s_out, sizeof(s_out), 0ull};
        said.at = 0ull;
        if (!cli_at_terminal_run(s_found, command, args, count, &out, &said))
        {
            cli_err_line(s_said, cli_line_fits(&said) ? said.at : 0ull);
            run_exit(outcome, CLI_WRONG);
            return;
        }
        if (cli_line_fits(&out) && (out.at > 0ull))
        {
            cli_out_line(s_out, out.at);
        }
        run_exit(outcome, 0);
        return;
    }
    static char s_root[CLI_PATH_LONGEST];
    said.at = 0ull;
    if ((named != NULL) && !cli_command_line_tree(named, s_root, &said))
    {
        cli_err_line(s_said, cli_line_fits(&said) ? said.at : 0ull);
        run_exit(outcome, CLI_NO_CODE);
        return;
    }
    run_launch(outcome, (named != NULL) ? s_root : NULL, NULL, menu, command, args, count);
}

// the help written to the output, its spaces at the end not
static void run_help(void)
{
    static char s_help[1u << 18u];
    CliLine help = {s_help, sizeof(s_help), 0ull};
    cli_help(&help);
    const unsigned long long size = cli_line_fits(&help) ? help.at : 0ull;
    cli_out_line(s_help, cli_text_trimmed_end(s_help, size));
}

// the words run as the menus run them, a command that acts in the window opened on the tree `remote` names where it
// names one; the words before the menus were the command line's, and each still names what it named
static void run_menus(CliOutcome *outcome, const char *named, const char *remote, const char *const *words,
                      unsigned int count)
{
    const char *const first = words[0];
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "list", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "show", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "bridge", .cap = CLI_CELLS_POOL})))
    {
        run_exit(outcome, cli_console(first, named, words + 1, count - 1u));
        return;
    }
    unsigned int menu = 0u;
    CliLine said = {s_said, sizeof(s_said), 0ull};
    if (!cli_menu_named(first, &menu))
    {
        static char s_file[CLI_PATH_LONGEST * 2u];
        CliLine file = {s_file, sizeof(s_file), 0ull};
        const int named_file = cli_file_word(named, first, &file, &said);
        if (named_file > 0)
        {
            (void)cli_line_closed(&file);
            const char *const opened[1] = {s_file};
            run_window(outcome, named, remote, "go", "file", opened, 1u);
            return;
        }
        if (named_file == 0)
        {
            cli_err_line(s_said, cli_line_fits(&said) ? said.at : 0ull);
            run_exit(outcome, CLI_NO_CODE);
            return;
        }
        CliLine line = {s_said, sizeof(s_said), 0ull};
        cli_line_text(&line, "orior takes no ");
        cli_line_text(&line, first);
        cli_line_text(&line, ": orior help names what it takes");
        cli_err_line(s_said, cli_line_fits(&line) ? line.at : 0ull);
        run_exit(outcome, CLI_WRONG);
        return;
    }
    if (count < 2u)
    {
        static char s_root[CLI_PATH_LONGEST];
        const int has_root = cli_command_line_tree(named, s_root, &said);
        run_exit(outcome, cli_menu_lines(has_root ? s_root : NULL, menu));
        return;
    }
    static CliItem s_item;
    char word[CLI_LABEL_LONGEST];
    cli_menu_word(menu, word);
    if (cli_menu_command(menu, words[1], &s_item))
    {
        if (s_item.console)
        {
            run_exit(outcome, cli_console(s_item.command, named, words + 2, count - 2u));
            return;
        }
        run_window(outcome, named, remote, word, s_item.command, words + 2, count - 2u);
        return;
    }
    char group[1][CLI_LABEL_LONGEST];
    if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "run", .cap = CLI_CELLS_POOL}))
    {
        run_exit(outcome, cli_console("start", named, words + 1, count - 1u));
        return;
    }
    if (cli_menu_groups(menu, group, 1u) > 0u)
    {
        run_exit(outcome, cli_menu_job(named, menu, words + 1, count - 1u));
        return;
    }
    char title[CLI_LABEL_LONGEST];
    cli_menu_title(menu, title);
    CliLine line = {s_said, sizeof(s_said), 0ull};
    cli_line_text(&line, title);
    cli_line_text(&line, " has no ");
    cli_line_text(&line, words[1]);
    cli_line_text(&line, ": orior ");
    cli_line_text(&line, first);
    cli_line_text(&line, " names what it has");
    cli_err_line(s_said, cli_line_fits(&line) ? line.at : 0ull);
    run_exit(outcome, CLI_WRONG);
}

// the words the command line runs before the menus: the help, the completions, the words after a menu, the version
// and the programs of their own: 1 where `first` is one of them, the outcome made, else 0
static int run_own(CliOutcome *outcome, const char *named, const char *const *words, unsigned int count)
{
    const char *const first = words[0];
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "-h", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "--help", .cap = CLI_CELLS_POOL})) ||
        ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "help", .cap = CLI_CELLS_POOL})) && (count == 1u)))
    {
        run_help();
        run_exit(outcome, 0);
        return 1;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "--completions", .cap = CLI_CELLS_POOL})) ||
        (cellul.starts(&(CatenaFinitaCfg){.src = first, .other = "--completions=", .cap = CLI_CELLS_POOL})))
    {
        const char *const shell = (first[13] == '=') ? first + 14 : ((count > 1u) ? words[1] : "");
        static char s_script[1u << 14u];
        CliLine script = {s_script, sizeof(s_script), 0ull};
        CliLine said = {s_said, sizeof(s_said), 0ull};
        if (!cli_completions(shell, &script, &said))
        {
            cli_err_line(s_said, cli_line_fits(&said) ? said.at : 0ull);
            run_exit(outcome, CLI_WRONG);
            return 1;
        }
        query_terminal_written(0, s_script, cli_line_fits(&script) ? script.at : 0ull);
        run_exit(outcome, 0);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "__words", .cap = CLI_CELLS_POOL}))
    {
        cli_next_words(named, (count > 1u) ? words[1] : "");
        run_exit(outcome, 0);
        return 1;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "--version", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "-V", .cap = CLI_CELLS_POOL})))
    {
        cli_out("orior " CLI_VERSION);
        run_exit(outcome, 0);
        return 1;
    }
    int code = 0;
    if (cli_line_program(first, named, words, count, &code))
    {
        run_exit(outcome, code);
        return 1;
    }
    return 0;
}

void cli_command_line_run(const char *const *given, unsigned int count, CliOutcome *outcome)
{
    const char *named = NULL;
    const char *remote = NULL;
    unsigned int words = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const word = given[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--", .cap = CLI_CELLS_POOL}))
        {
            for (; (at < count) && (words < CLI_LAUNCH_ARGS); at += 1u)
            {
                s_words[words] = given[at];
                words += 1u;
            }
            break;
        }
        if ((cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--root", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--remote", .cap = CLI_CELLS_POOL})))
        {
            const int root = (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--root", .cap = CLI_CELLS_POOL}));
            if (at + 1u >= count)
            {
                cli_err(root ? "--root needs a folder"
                             : "--remote needs an address: user@host:folder, docker:container:folder or "
                               "wsl:distribution:folder");
                run_exit(outcome, CLI_WRONG);
                return;
            }
            at += 1u;
            *(root ? &named : &remote) = given[at];
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = word, .other = "--root=", .cap = CLI_CELLS_POOL}))
        {
            named = word + 7;
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = word, .other = "--remote=", .cap = CLI_CELLS_POOL}))
        {
            remote = word + 9;
        }
        else if (words < CLI_LAUNCH_ARGS)
        {
            s_words[words] = word;
            words += 1u;
        }
    }
    CliLine said = {s_said, sizeof(s_said), 0ull};
    if (!cli_commands_read(&said))
    {
        cli_err_line(s_said, cli_line_fits(&said) ? said.at : 0ull);
        run_exit(outcome, CLI_NO_CODE);
        return;
    }
    if (words == 0u)
    {
        if ((named != NULL) || (remote != NULL))
        {
            run_window(outcome, named, remote, "", "", NULL, 0u);
            return;
        }
        run_help();
        run_exit(outcome, 0);
        return;
    }
    if (run_own(outcome, named, s_words, words))
    {
        return;
    }
    // the first run at a terminal asks whether errors file on their own, where the installer did not, and a run that
    // answers it itself asks nothing
    if ((words < 2u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = s_words[1], .other = "auto-report", .cap = CLI_CELLS_POOL})))
    {
        cli_report_ask_once();
    }
    run_menus(outcome, named, remote, s_words, words);
}
