// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_run.c: the words given on the command line run: --root taken out of them, then the help,
// the completions, the words after a menu, the version and the programs of their own; then the commands the terminal
// runs here and the menus, a command that acts in the window sent to a window open on the tree, run here where the
// terminal can do it and no window is open, or the window opened to run it; and a word that names a file of the tree
// opened in the window
#include "../machine/reach.h"
#include "../machine/report.h"
#include "../text/text.h"
#include "at_terminal.h"
#include "command_line_internal.h"
#include "commands.h"

// the bytes what a command run at the terminal writes and the help are written in
#define CLI_RUN_OUT_CAPACITY (64ULL << 20u)
#define CLI_RUN_HELP_CAPACITY (1ULL << 20u)

static char s_msg_buf[1u << 14u];
static const char *s_words[CLI_LAUNCH_ARGS];

typedef struct
{
    CliOutcome *outcome;
    int code;
    char *into;
    size_t capacity;
    const char *text;
    const char *root;
    const char *menu;
    const char *command;
    const char *const *request;
    unsigned int count;
    const char *named;
    const char *const *words;
} CommandLineRunCtx;
// the outcome made an exit with `code`
EMBED_INLINE void run_exit(const CommandLineRunCtx *args)
{
    int code = args->code;
    args->outcome->window = 0;
    args->outcome->code = code;
}

// `text` copied into `into`, sized for `capacity`, cut where it does not fit
EMBED_INLINE void run_copy(const CommandLineRunCtx *args)
{
    VerbaLinea line = {args->into, args->capacity, 0ULL};
    mmgr_line_put(&line, args->text);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        args->into[0] = '\0';
    }
}

// the outcome made the window to open, on the tree at `root` where one is given, with the command of its menus to run
// once it is up and its words
EMBED_INLINE void run_launch(const CommandLineRunCtx *args)
{
    const char *const *arguments = args->request;
    args->outcome->window = 1;
    args->outcome->code = 0;
    args->outcome->has_root = (args->root != NULL);
    EMBED_CALL(run_copy, CommandLineRunCtx, .into = args->outcome->root, .capacity = sizeof(args->outcome->root),
               .text = (args->root != NULL) ? args->root : "");
    EMBED_CALL(run_copy, CommandLineRunCtx, .into = args->outcome->menu, .capacity = sizeof(args->outcome->menu),
               .text = args->menu);
    EMBED_CALL(run_copy, CommandLineRunCtx, .into = args->outcome->command, .capacity = sizeof(args->outcome->command),
               .text = args->command);
    args->outcome->args = 0u;
    args->outcome->used = 0u;
    for (unsigned int at = 0u; (at < args->count) && (at < CLI_LAUNCH_ARGS); at += 1u)
    {
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = arguments[at], .cap = CLI_CELLS_POOL});
        if (args->outcome->used + size + 1u > CLI_LAUNCH_TEXT)
        {
            break;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = args->outcome->text + args->outcome->used, .src = arguments[at], .bytes = size + 1u});
        args->outcome->arg_at[args->outcome->args] = args->outcome->used;
        args->outcome->args += 1u;
        // the text stored is far below what an unsigned int counts
        args->outcome->used += (unsigned int)size + 1u;
    }
}

const char *cli_launch_arg(const CliCommandLineCfg *args)
{
    return args->launch->text + args->launch->arg_at[args->at];
}

void cli_launch_words(const CliCommandLineLaunchOutCfg *args)
{
    const unsigned long long start = args->out->at;
    const char *const named[2] = {args->launch->menu, args->launch->command};
    for (unsigned int at = 0u; at < 2u + args->launch->args; at += 1u)
    {
        const char *const word_text = (at < 2u) ? named[at]
                                                : EMBED_CALL(cli_command_line_table.launch_arg, CliCommandLineCfg,
                                                             .launch = args->launch, .at = at - 2u);
        if (word_text[0] != '\0')
        {
            mmgr_line_put(args->out, (args->out->at > start) ? " " : "");
            mmgr_line_put(args->out, word_text);
        }
    }
}

// the window a command of its menus opens: sent to a window open on the tree, which runs the command itself, as reach.h
// specifies; else run here where the terminal can do it, as at_terminal.h specifies; else the window opened to run it
static void run_window(const CommandLineRunCtx *args)
{
    const char *named = args->named;
    const char *menu = args->menu;
    const char *command = args->command;
    const char *const *arguments = args->request;
    unsigned int count = args->count;
    VerbaLinea err_msg = {s_msg_buf, sizeof(s_msg_buf), 0ULL};
    static char s_found[CLI_PATH_MAX_LEN];
    const int found = (named != NULL) ? EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named,
                                                   .root = s_found, .err_msg = &err_msg)
                                      : EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = s_found);
    err_msg.at = 0ULL;
    if (command[0] != '\0')
    {
        static CliReachAnswer s_answer;
        const int requested = EMBED_CALL(cli_reach_menu.request, CliReachMenuCfg, .root = found ? s_found : NULL,
                                         .menu = menu, .command = command, .request = arguments, .count = count,
                                         .answer = &s_answer, .err_msg = &err_msg);
        if (requested == 0)
        {
            EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
                       .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
            EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_WRONG);
            return;
        }
        if (requested > 0)
        {
            if (s_answer.text[0] != '\0')
            {
                if (s_answer.done)
                {
                    EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = s_answer.text);
                }
                else
                {
                    EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = s_answer.text);
                }
            }
            EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = s_answer.done ? 0 : CLI_WRONG);
            return;
        }
    }
    if (EMBED_CALL(cli_at_terminal_command.runs, CliAtTerminalCommandCfg, .command = command))
    {
        if (!found && !EMBED_CALL(cli_path_capacity_internal.folder_current, CliPathCapacityCtx, .path_out = s_found,
                                  .capacity_bytes = sizeof(s_found)))
        {
            EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "orior_cli finds no folder to run in");
            EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_WRONG);
            return;
        }
        char *const out_cell = g_cli_cells->temporary_buf_alloc(CLI_RUN_OUT_CAPACITY, &g_cli_command_line_cells);
        VerbaLinea out = mmgr_line_over(out_cell, CLI_RUN_OUT_CAPACITY);
        err_msg.at = 0ULL;
        const int run_result =
            EMBED_CALL(cli_at_terminal_command.run, CliAtTerminalCommandCfg, .root = s_found, .command = command,
                       .request = arguments, .count = count, .out = &out, .err_msg = &err_msg);
        if (!run_result)
        {
            EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
                       .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        }
        if (run_result && mmgr_line_ok(&out) && (out.at > 0ULL))
        {
            EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = out.out,
                       .size = out.at);
        }
        g_cli_cells->persistent_buf_release(out.out, &g_cli_command_line_cells);
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = run_result ? 0 : CLI_WRONG);
        return;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    err_msg.at = 0ULL;
    if ((named != NULL) && !EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named,
                                       .root = s_root, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_NO_CODE);
        return;
    }
    EMBED_CALL(run_launch, CommandLineRunCtx, .outcome = args->outcome, .root = (named != NULL) ? s_root : NULL,
               .menu = menu, .command = command, .request = arguments, .count = count);
}

// the help written to the output, its spaces at the end not
static void run_help(void)
{
    char *const help_cell = g_cli_cells->temporary_buf_alloc(CLI_RUN_HELP_CAPACITY, &g_cli_command_line_cells);
    VerbaLinea help = mmgr_line_over(help_cell, CLI_RUN_HELP_CAPACITY);
    EMBED_CALL(cli_command_line_out_err_msg_shell_internal.help, CliCommandLineOutErrMsgShellCtx, .out = &help);
    const unsigned long long size = mmgr_line_ok(&help) ? help.at : 0ULL;
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = help.out,
               .size = (help.out != NULL) ? EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg,
                                                       .text = help.out, .size = size)
                                          : 0ULL);
    g_cli_cells->persistent_buf_release(help.out, &g_cli_command_line_cells);
}

// the words run as the menus run them; the words before the menus were the command line's, and each still names what it
// named
EMBED_INLINE void run_menus(const CommandLineRunCtx *args)
{
    const char *named = args->named;
    const char *const *words = args->words;
    unsigned int count = args->count;
    const char *const first = words[0];
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "list", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "show", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "bridge", .cap = CLI_CELLS_POOL})))
    {
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome,
                   .code = EMBED_CALL(cli_command_line_words_count_internal.console, CliCommandLineWordsCountCtx,
                                      .command = first, .named = named, .words = words + 1, .count = count - 1u));
        return;
    }
    unsigned int menu = 0u;
    VerbaLinea err_msg = {s_msg_buf, sizeof(s_msg_buf), 0ULL};
    if (!EMBED_CALL(cli_commands_word.menu_by_name, CliCommandsWordCfg, .word_text = first, .menu_pointer = &menu))
    {
        static char s_file[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea file = {s_file, sizeof(s_file), 0ULL};
        const int named_file = EMBED_CALL(cli_command_line_out_internal.file_word, CliCommandLineOutCtx, .named = named,
                                          .word_text = first, .out = &file, .err_msg = &err_msg);
        if (named_file > 0)
        {
            (void)mmgr_line_finish(&file);
            const char *const opened[1] = {s_file};
            EMBED_CALL(run_window, CommandLineRunCtx, .outcome = args->outcome, .named = named, .menu = "go",
                       .command = "file", .request = opened, .count = 1u);
            return;
        }
        if (named_file == 0)
        {
            EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
                       .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
            EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_NO_CODE);
            return;
        }
        VerbaLinea line = {s_msg_buf, sizeof(s_msg_buf), 0ULL};
        mmgr_line_put(&line, "orior_cli takes no ");
        mmgr_line_put(&line, first);
        mmgr_line_put(&line, ": orior_cli help names what it takes");
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
                   .size = mmgr_line_ok(&line) ? line.at : 0ULL);
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_WRONG);
        return;
    }
    if (count < 2u)
    {
        static char s_root[CLI_PATH_MAX_LEN];
        const int has_root = EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named,
                                        .root = s_root, .err_msg = &err_msg);
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome,
                   .code = EMBED_CALL(cli_command_line_word_internal.menu_lines, CliCommandLineWordCtx,
                                      .root = has_root ? s_root : NULL, .menu = menu));
        return;
    }
    static CliItem s_item;
    char word_text[CLI_LABEL_MAX_LEN];
    EMBED_CALL(cli_commands_menu_word.menu_word, CliCommandsMenuWordCfg, .menu = menu, .word_out = word_text);
    if (EMBED_CALL(cli_commands_menu.menu_command, CliCommandsMenuCfg, .menu = menu, .word_text = words[1],
                   .item = &s_item))
    {
        if (s_item.console)
        {
            EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome,
                       .code = EMBED_CALL(cli_command_line_words_count_internal.console, CliCommandLineWordsCountCtx,
                                          .command = s_item.command, .named = named, .words = words + 2,
                                          .count = count - 2u));
            return;
        }
        // a command the window runs is given the words it needs, else it is refused here and never sent there
        if ((count - 2u) < EMBED_CALL(cli_commands_item.item_words_need, CliCommandsItemCfg, .item_read = &s_item))
        {
            VerbaLinea line = {s_msg_buf, sizeof(s_msg_buf), 0ULL};
            mmgr_line_put(&line, word_text);
            mmgr_line_ch(&line, ' ');
            mmgr_line_put(&line, s_item.command);
            mmgr_line_put(&line, " takes ");
            mmgr_line_put(&line, s_item.args);
            EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
                       .size = mmgr_line_ok(&line) ? line.at : 0ULL);
            EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_WRONG);
            return;
        }
        EMBED_CALL(run_window, CommandLineRunCtx, .outcome = args->outcome, .named = named, .menu = word_text,
                   .command = s_item.command, .request = words + 2, .count = count - 2u);
        return;
    }
    char group[1][CLI_LABEL_MAX_LEN];
    if (cellul.eq(&(CatenaFinitaCfg){.src = word_text, .other = "run", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome,
                   .code = EMBED_CALL(cli_command_line_words_count_internal.console, CliCommandLineWordsCountCtx,
                                      .command = "start", .named = named, .words = words + 1, .count = count - 1u));
        return;
    }
    if (EMBED_CALL(cli_commands_menu_groups.menu_groups, CliCommandsMenuGroupsCfg, .menu = menu, .groups = group,
                   .capacity = 1u) > 0u)
    {
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome,
                   .code =
                       EMBED_CALL(cli_command_line_words_count_menu_internal.menu_job, CliCommandLineWordsCountMenuCtx,
                                  .named = named, .menu = menu, .words = words + 1, .count = count - 1u));
        return;
    }
    char title[CLI_LABEL_MAX_LEN];
    EMBED_CALL(cli_commands_menu_title.menu_title, CliCommandsMenuTitleCfg, .menu = menu, .title = title);
    VerbaLinea line = {s_msg_buf, sizeof(s_msg_buf), 0ULL};
    mmgr_line_put(&line, title);
    mmgr_line_put(&line, " has no ");
    mmgr_line_put(&line, words[1]);
    mmgr_line_put(&line, ": orior_cli ");
    mmgr_line_put(&line, first);
    mmgr_line_put(&line, " names what it has");
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
               .size = mmgr_line_ok(&line) ? line.at : 0ULL);
    EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_WRONG);
}

// the words the command line runs before the menus: the help, the completions, the words after a menu, the version
// and the programs of their own: 1 where `first` is one of them, the outcome made, else 0
EMBED_INLINE int run_own(const CommandLineRunCtx *args)
{
    const char *named = args->named;
    const char *const *words = args->words;
    unsigned int count = args->count;
    const char *const first = words[0];
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "-h", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "--help", .cap = CLI_CELLS_POOL})) ||
        ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "help", .cap = CLI_CELLS_POOL})) && (count == 1u)))
    {
        run_help();
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = 0);
        return 1;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "--completions", .cap = CLI_CELLS_POOL})) ||
        (cellul.starts(&(CatenaFinitaCfg){.src = first, .other = "--completions=", .cap = CLI_CELLS_POOL})))
    {
        const char *shell;
        if (first[13] == '=')
        {
            shell = first + 14;
        }
        else if (count > 1u)
        {
            shell = words[1];
        }
        else
        {
            shell = "";
        }
        static char s_script[1u << 14u];
        VerbaLinea script = {s_script, sizeof(s_script), 0ULL};
        VerbaLinea err_msg = {s_msg_buf, sizeof(s_msg_buf), 0ULL};
        if (!EMBED_CALL(cli_command_line_out_err_msg_shell_internal.completions, CliCommandLineOutErrMsgShellCtx,
                        .shell = shell, .out = &script, .err_msg = &err_msg))
        {
            EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
                       .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
            EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_WRONG);
            return 1;
        }
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 0, .text = s_script,
                   .length = mmgr_line_ok(&script) ? script.at : 0ULL);
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = 0);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "__words", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(cli_command_line_named_internal.next_words, CliCommandLineNamedCtx, .named = named,
                   .menu_word = (count > 1u) ? words[1] : "");
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = 0);
        return 1;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "--version", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "-V", .cap = CLI_CELLS_POOL})))
    {
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "orior_cli " CLI_VERSION);
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = 0);
        return 1;
    }
    int code = 0;
    if (EMBED_CALL(cli_command_line_words_internal.line_program, CliCommandLineWordsCtx, .first = first, .named = named,
                   .words = words, .count = count, .code = &code))
    {
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = code);
        return 1;
    }
    return 0;
}

void cli_command_line_run(const CliCommandLineInputCfg *args)
{
    const char *named = NULL;
    unsigned int words = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const word_text = args->supplied[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = word_text, .other = "--", .cap = CLI_CELLS_POOL}))
        {
            for (; (at < args->count) && (words < CLI_LAUNCH_ARGS); at += 1u)
            {
                s_words[words] = args->supplied[at];
                words += 1u;
            }
            break;
        }
        if (cellul.eq(&(CatenaFinitaCfg){.src = word_text, .other = "--root", .cap = CLI_CELLS_POOL}))
        {
            if (at + 1u >= args->count)
            {
                EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "--root needs a folder");
                EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_WRONG);
                return;
            }
            at += 1u;
            named = args->supplied[at];
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = word_text, .other = "--root=", .cap = CLI_CELLS_POOL}))
        {
            named = word_text + 7;
        }
        else if (words < CLI_LAUNCH_ARGS)
        {
            s_words[words] = word_text;
            words += 1u;
        }
    }
    VerbaLinea err_msg = {s_msg_buf, sizeof(s_msg_buf), 0ULL};
    if (!EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = CLI_NO_CODE);
        return;
    }
    if (words == 0u)
    {
        if (named != NULL)
        {
            EMBED_CALL(run_window, CommandLineRunCtx, .outcome = args->outcome, .named = named, .menu = "",
                       .command = "", .request = NULL, .count = 0u);
            return;
        }
        run_help();
        EMBED_CALL(run_exit, CommandLineRunCtx, .outcome = args->outcome, .code = 0);
        return;
    }
    if (EMBED_CALL(run_own, CommandLineRunCtx, .outcome = args->outcome, .named = named, .words = s_words,
                   .count = words))
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
    EMBED_CALL(run_menus, CommandLineRunCtx, .outcome = args->outcome, .named = named, .words = s_words,
               .count = words);
}
