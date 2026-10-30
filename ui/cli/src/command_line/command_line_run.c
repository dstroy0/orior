// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_run.c: the words given on the command line run: --root and --remote taken out of them, then the help,
// the completions, the words after a menu, the version and the programs of their own; then the commands the terminal
// runs here and the menus, a command that acts in the window sent to a window open on the tree, run here where the
// terminal can do it and no window is open, or the window opened to run it; and a word that names a file of the tree
// opened in the window
#include "../comms/link.h"
#include "../machine/reach.h"
#include "../machine/report.h"
#include "../text/text.h"
#include "at_terminal.h"
#include "command_line_internal.h"
#include "commands.h"

// the bytes what a command run at the terminal writes and the help are begun to be written in, doubling as they fill
#define CLI_RUN_OUT_FIRST 4096u
#define CLI_RUN_HELP_FIRST 16384u

static char s_msg_buf[1u << 14u];
static const char *s_words[CLI_LAUNCH_ARGS];

// the outcome made an exit with `code`
static void run_exit(CliOutcome *outcome, int code)
{
    outcome->window = 0;
    outcome->code = code;
}

// `text` copied into `into`, sized for `room`, cut where it does not fit
static void run_copied(char *into, size_t room, const char *text)
{
    VerbaLinea line = {into, room, 0ull};
    mmgr_line_put(&line, text);
    if (mmgr_line_finish(&line) == 0ull)
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
        // the text stored is far below what an unsigned int counts
        outcome->used += (unsigned int)size + 1u;
    }
}

const char *cli_launch_arg(const CliOutcome *launch, unsigned int at)
{
    return launch->text + launch->arg_at[at];
}

void cli_launch_words(const CliOutcome *launch, VerbaLinea *out)
{
    const unsigned long long start = out->at;
    const char *const named[2] = {launch->menu, launch->command};
    for (unsigned int at = 0u; at < 2u + launch->args; at += 1u)
    {
        const char *const word = (at < 2u) ? named[at] : cli_launch_arg(launch, at - 2u);
        if (word[0] != '\0')
        {
            mmgr_line_put(out, (out->at > start) ? " " : "");
            mmgr_line_put(out, word);
        }
    }
}

// the window a command of its menus opens: on another machine where --remote names one; else sent to a window open on
// the tree, which runs the command itself, as reach.h specifies; else run here where the terminal can do it, as
// at_terminal.h specifies; else the window opened to run it
static void run_window(CliOutcome *outcome, const char *named, const char *remote, const char *menu,
                       const char *command, const char *const *args, unsigned int count)
{
    VerbaLinea err_msg = {s_msg_buf, sizeof(s_msg_buf), 0ull};
    if (remote != NULL)
    {
        static CliLinkAddress s_address;
        if (!cli_link_address_parse(remote, &s_address, &err_msg))
        {
            cli_err_line(s_msg_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
            run_exit(outcome, CLI_WRONG);
            return;
        }
        run_launch(outcome, NULL, remote, menu, command, args, count);
        return;
    }
    static char s_found[CLI_PATH_MAX_LEN];
    const int found = (named != NULL) ? cli_command_line_tree(named, s_found, &err_msg) : cli_tree_found(s_found);
    err_msg.at = 0ull;
    if (command[0] != '\0')
    {
        static CliReachAnswer s_answer;
        const int requested =
            cli_reach_request(found ? s_found : NULL, menu, command, args, count, &s_answer, &err_msg);
        if (requested == 0)
        {
            cli_err_line(s_msg_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
            run_exit(outcome, CLI_WRONG);
            return;
        }
        if (requested > 0)
        {
            if (s_answer.text[0] != '\0')
            {
                (s_answer.done ? cli_out : cli_err)(s_answer.text);
            }
            run_exit(outcome, s_answer.done ? 0 : CLI_WRONG);
            return;
        }
    }
    if (cli_at_terminal_runs(command))
    {
        if (!found && !cli_folder_current(s_found, sizeof(s_found)))
        {
            cli_err("orior_cli finds no folder to run in");
            run_exit(outcome, CLI_WRONG);
            return;
        }
        VerbaLinea out = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
        (void)cli_growing_line_made(&out, CLI_RUN_OUT_FIRST);
        err_msg.at = 0ull;
        const int ran = cli_at_terminal_run(s_found, command, args, count, &out, &err_msg);
        if (!ran)
        {
            cli_err_line(s_msg_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        }
        if (ran && mmgr_line_ok(&out) && (out.at > 0ull))
        {
            cli_out_line(out.out, out.at);
        }
        cli_cell_release(out.out, NULL);
        run_exit(outcome, ran ? 0 : CLI_WRONG);
        return;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    err_msg.at = 0ull;
    if ((named != NULL) && !cli_command_line_tree(named, s_root, &err_msg))
    {
        cli_err_line(s_msg_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        run_exit(outcome, CLI_NO_CODE);
        return;
    }
    run_launch(outcome, (named != NULL) ? s_root : NULL, NULL, menu, command, args, count);
}

// the help written to the output, its spaces at the end not
static void run_help(void)
{
    VerbaLinea help = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_growing_line_made(&help, CLI_RUN_HELP_FIRST);
    cli_help(&help);
    const unsigned long long size = mmgr_line_ok(&help) ? help.at : 0ull;
    cli_out_line(help.out, (help.out != NULL) ? cli_text_trimmed_end(help.out, size) : 0ull);
    cli_cell_release(help.out, NULL);
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
    VerbaLinea err_msg = {s_msg_buf, sizeof(s_msg_buf), 0ull};
    if (!cli_menu_named(first, &menu))
    {
        static char s_file[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea file = {s_file, sizeof(s_file), 0ull};
        const int named_file = cli_file_word(named, first, &file, &err_msg);
        if (named_file > 0)
        {
            (void)mmgr_line_finish(&file);
            const char *const opened[1] = {s_file};
            run_window(outcome, named, remote, "go", "file", opened, 1u);
            return;
        }
        if (named_file == 0)
        {
            cli_err_line(s_msg_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
            run_exit(outcome, CLI_NO_CODE);
            return;
        }
        VerbaLinea line = {s_msg_buf, sizeof(s_msg_buf), 0ull};
        mmgr_line_put(&line, "orior_cli takes no ");
        mmgr_line_put(&line, first);
        mmgr_line_put(&line, ": orior_cli help names what it takes");
        cli_err_line(s_msg_buf, mmgr_line_ok(&line) ? line.at : 0ull);
        run_exit(outcome, CLI_WRONG);
        return;
    }
    if (count < 2u)
    {
        static char s_root[CLI_PATH_MAX_LEN];
        const int has_root = cli_command_line_tree(named, s_root, &err_msg);
        run_exit(outcome, cli_menu_lines(has_root ? s_root : NULL, menu));
        return;
    }
    static CliItem s_item;
    char word[CLI_LABEL_MAX_LEN];
    cli_menu_word(menu, word);
    if (cli_menu_command(menu, words[1], &s_item))
    {
        if (s_item.console)
        {
            run_exit(outcome, cli_console(s_item.command, named, words + 2, count - 2u));
            return;
        }
        // a command the window runs is given the words it needs, else it is refused here and never sent there
        if ((count - 2u) < cli_item_words_needed(&s_item))
        {
            VerbaLinea line = {s_msg_buf, sizeof(s_msg_buf), 0ull};
            mmgr_line_put(&line, word);
            mmgr_line_ch(&line, ' ');
            mmgr_line_put(&line, s_item.command);
            mmgr_line_put(&line, " takes ");
            mmgr_line_put(&line, s_item.args);
            cli_err_line(s_msg_buf, mmgr_line_ok(&line) ? line.at : 0ull);
            run_exit(outcome, CLI_WRONG);
            return;
        }
        run_window(outcome, named, remote, word, s_item.command, words + 2, count - 2u);
        return;
    }
    char group[1][CLI_LABEL_MAX_LEN];
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
    char title[CLI_LABEL_MAX_LEN];
    cli_menu_title(menu, title);
    VerbaLinea line = {s_msg_buf, sizeof(s_msg_buf), 0ull};
    mmgr_line_put(&line, title);
    mmgr_line_put(&line, " has no ");
    mmgr_line_put(&line, words[1]);
    mmgr_line_put(&line, ": orior_cli ");
    mmgr_line_put(&line, first);
    mmgr_line_put(&line, " names what it has");
    cli_err_line(s_msg_buf, mmgr_line_ok(&line) ? line.at : 0ull);
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
        VerbaLinea script = {s_script, sizeof(s_script), 0ull};
        VerbaLinea err_msg = {s_msg_buf, sizeof(s_msg_buf), 0ull};
        if (!cli_completions(shell, &script, &err_msg))
        {
            cli_err_line(s_msg_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
            run_exit(outcome, CLI_WRONG);
            return 1;
        }
        query_terminal_written(0, s_script, mmgr_line_ok(&script) ? script.at : 0ull);
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
        cli_out("orior_cli " CLI_VERSION);
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
    VerbaLinea err_msg = {s_msg_buf, sizeof(s_msg_buf), 0ull};
    if (!cli_commands_read(&err_msg))
    {
        cli_err_line(s_msg_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
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
    // the first run at a terminal prompts whether errors file on their own, where the installer did not, and a run that
    // answers it itself prompts nothing
    if ((words < 2u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = s_words[1], .other = "auto-report", .cap = CLI_CELLS_POOL})))
    {
        cli_report_prompt_once();
    }
    run_menus(outcome, named, remote, s_words, words);
}
