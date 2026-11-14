// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_console.c: the commands the command line runs itself, in the terminal: the keys, what impero is and
// its tree, errors filed on their own turned on or off, the bug report page opened, a search of the tree, the plugins,
// the reader's stylesheet, the toolchains, a clone, the tree's dev container brought up and a window opened on it,
// files formatted, run and validated; the tree's jobs listed, displayed and run, its tests and its bridge listed, and the
// words of `imp protocol` sent to a window open on the tree or run here. And a word that names a file of the tree
#include "../deploy/devcontainer.h"
#include "../editor/files.h"
#include "../machine/install.h"
#include "../machine/reach.h"
#include "../machine/report.h"
#include "../programs/disasm_verify.h"
#include "../runs/protocol.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "callback_cfg.h"
#include "command_line_internal.h"

static char s_line[1u << 16u];
static char s_err_buf[1u << 14u];

typedef struct
{
    const char *one;
    const char *other;
    const VerbaLinea *err_msg;
    const char *word_text;
    const char *named;
    const char *const *words;
    unsigned int count;
    const char *root;
    const char *command;
    int *code;
    VerbaLinea *out_line;
} CommandLineConsoleCtx;
// why given written to the errors
EMBED_INLINE void console_err_print(const CommandLineConsoleCtx *args)
{
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = args->err_msg->out,
               .size = mmgr_line_ok(args->err_msg) ? args->err_msg->at : 0ULL);
}

// `imp help auto-report [on|off]`: errors filing on their own turned on or off, or which reported
EMBED_INLINE int console_auto_report(const CommandLineConsoleCtx *args)
{
    if (args->word_text == NULL)
    {
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = cli_report_auto() ? "on" : "off");
        return 0;
    }
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = args->word_text, .other = "on", .cap = CLI_POOL_BYTES})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->word_text, .other = "off", .cap = CLI_POOL_BYTES})))
    {
        VerbaLinea line = {s_line, sizeof(s_line), 0ULL};
        mmgr_line_put(&line, args->word_text);
        mmgr_line_put(&line, " is neither on nor off");
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_line,
                   .size = mmgr_line_ok(&line) ? line.at : 0ULL);
        return CLI_WRONG;
    }
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    if (!EMBED_CALL(cli_report_on.set_auto, CliReportOnCfg,
                    .on = cellul.eq(&(CatenaFinitaCfg){.src = args->word_text, .other = "on", .cap = CLI_POOL_BYTES}),
                    .err_msg = &err_msg))
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_NO_CODE;
    }
    return 0;
}

// `imp help report [category] [title]`: the bug report page opened with what was given filled in
EMBED_INLINE int console_report_page(const CommandLineConsoleCtx *args)
{
    const char *named = args->named;
    static char s_title[1u << 12u];
    VerbaLinea title = {s_title, sizeof(s_title), 0ULL};
    const int known = (args->count > 0u) && EMBED_CALL(cli_report_category_text.category_know, CliReportCategoryTextCfg,
                                                       .category = args->words[0]);
    for (unsigned int at = known ? 1u : 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&title, (title.at > 0ULL) ? " " : "");
        mmgr_line_put(&title, args->words[at]);
    }
    (void)mmgr_line_finish(&title);
    static char s_root[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    const int has_root = EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named, .root = s_root,
                                    .err_msg = &err_msg);
    const CliReport supplied = {known ? args->words[0] : "unknown", s_title, "", "", ""};
    static char s_url[CLI_REPORT_URL];
    VerbaLinea url = {s_url, sizeof(s_url), 0ULL};
    EMBED_CALL(cli_report_root_out.page, CliReportRootOutCfg, .report = &supplied, .root = has_root ? s_root : NULL,
               .out = &url);
    (void)mmgr_line_finish(&url);
    EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = s_url);
    err_msg.at = 0ULL;
    if (!EMBED_CALL(cli_report_err_msg.open_page, CliReportErrMsgCfg, .url = s_url, .err_msg = &err_msg))
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_NO_CODE;
    }
    return 0;
}

// a docker line printed as the dev container comes up
static void console_docker_line(const CliEachLineSizeCfg *args)
{
    const char *text = args->line;
    VerbaLinea line = {s_line, sizeof(s_line), 0ULL};
    mmgr_line_put(&line, "$ ");
    mmgr_line_put_n(&line, text, args->size);
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
               .size = mmgr_line_ok(&line) ? line.at : 0ULL);
}

// the tree's dev container brought up as its description specifies, docker's output displayed as it comes, and its address
// written
EMBED_INLINE int console_dev_container(const CommandLineConsoleCtx *args)
{
    const char *named = args->named;
    static char s_root[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    if (!EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named, .root = s_root,
                    .err_msg = &err_msg))
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_NO_CODE;
    }
    static CliPlan s_plan;
    if (!EMBED_CALL(cli_devcontainer_root.plan, CliDevcontainerRootCfg, .root = s_root, .plan_out = &s_plan,
                    .err_msg = &err_msg))
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_WRONG;
    }
    char address[CLI_PATH_MAX_LEN];
    const QuerySink sink = {console_docker_line, NULL};
    if (!EMBED_CALL(cli_devcontainer_err_msg.up, CliDevcontainerErrMsgCfg, .plan = &s_plan, .sink = sink,
                    .address = address, .err_msg = &err_msg))
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_NO_CODE;
    }
    EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = address);
    return 0;
}

// a line of `imp protocol` written to the output
static void console_protocol_line(const CliEachLineSizeCfg *args)
{
    const char *text = args->line;
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = text,
               .size = args->size);
}

// the words of `imp protocol`: sent to a window open on the tree, which runs the protocol command as its job and
// shows the run in its protocol panel; with none open run here, and so does the job's own command, which carries the
// window's interface in its environment and must not reach the window again. A reading, `decode` and the two readings of a
// pulled table, prints here and is no job of the window's
EMBED_INLINE int console_protocol(const CommandLineConsoleCtx *args)
{
    const char *root = args->root;
    const char *command = args->command;
    const char *const *words = args->words;
    unsigned int count = args->count;
    char value[16];
    const int inside_a_job = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx,
                                        .name = CLI_RUN_VARIABLE, .value_out = value, .capacity = sizeof(value));
    const int read_state =
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "decode", .cap = CLI_POOL_BYTES})) ||
        ((cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "tables", .cap = CLI_POOL_BYTES})) && (count > 0u) &&
         ((cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "read", .cap = CLI_POOL_BYTES})) ||
          (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "fields", .cap = CLI_POOL_BYTES}))));
    if (!inside_a_job && !read_state)
    {
        static CliReachAnswer s_answer;
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
        const int requested =
            EMBED_CALL(cli_reach_menu.request, CliReachMenuCfg, .root = root, .menu = "protocol", .command = command,
                       .request = words, .count = count, .answer = &s_answer, .err_msg = &err_msg);
        if (requested == 0)
        {
            EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
            return CLI_WRONG;
        }
        if (requested > 0)
        {
            const int answered = s_answer.text[0] != '\0';
            if (answered && s_answer.done)
            {
                EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = s_answer.text);
            }
            if (answered && !s_answer.done)
            {
                EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = s_answer.text);
            }
            return s_answer.done ? 0 : CLI_WRONG;
        }
    }
    static const char *s_all[CLI_LAUNCH_ARGS + 1u];
    const unsigned int arg_count = (count < CLI_LAUNCH_ARGS) ? count : CLI_LAUNCH_ARGS;
    s_all[0] = command;
    for (unsigned int at = 0u; at < arg_count; at += 1u)
    {
        s_all[at + 1u] = words[at];
    }
    const QuerySink sink = {console_protocol_line, NULL};
    const int code = EMBED_CALL(cli_protocol.run, CliProtocolCfg, .root = root, .words = s_all, .count = arg_count + 1u,
                                .sink = sink);
    return (code < 0) ? CLI_WRONG : code;
}

// `imp file user-css`: the stylesheet the window positions over its own named, made empty but for a note where it is
// not there yet
static int console_user_css(void)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    if (!EMBED_CALL(cli_path_err_msg_type_internal.home_ensure_user_css, CliPathErrMsgTypeCtx, .path_out = path,
                    .err_msg = &err_msg))
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_NO_CODE;
    }
    EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = path);
    return 0;
}

// a line of the install's account printed
static void console_install_said(void *context, const char *text)
{
    (void)context;
    EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = text);
}

// a question of the install printed and the reader's line read from the terminal: 1 where it begins with y; 0 where
// it does not, or where no terminal is there to answer
static int console_install_asked(void *context, const char *question)
{
    (void)context;
    EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = question);
    if (!query_terminal_is())
    {
        return 0;
    }
    char answer[64];
    const unsigned long long read = EMBED_CALL(query_capacity_line_internal.terminal_line_read, QueryCapacityLineCtx,
                                               .line = answer, .capacity = sizeof(answer));
    return (read != QRY_MEMBER_UNREAD) && ((answer[0] == 'y') || (answer[0] == 'Y'));
}

// `imp file install [folder] [--accept] [--yes] [--no-imp] [--no-tab] [--aliases] [--debug-port <port|off>]` and
// `imp file uninstall [--files] [--yes]`: the code to exit with
EMBED_INLINE int console_install(const CommandLineConsoleCtx *args)
{
    CliInstallOptions options;
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    const int uninstall =
        cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "uninstall", .cap = CLI_POOL_BYTES});
    int done = EMBED_CALL(cli_install.options, CliInstallCfg, .words = args->words, .count = args->count,
                          .options = &options, .err_msg = &err_msg);
    if (done)
    {
        const CliInstallCfg run = {
            .options = &options, .err_msg = &err_msg, .say = console_install_said, .asked = console_install_asked};
        done = uninstall ? cli_install.uninstall(&run) : cli_install.install(&run);
    }
    if (!done)
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_NO_CODE;
    }
    return 0;
}

// the commands that need no tree, each run: 1 with the code to exit with into `code`, or 0 where `command` is none of
// them
EMBED_INLINE int console_treeless(const CommandLineConsoleCtx *args)
{
    const char *named = args->named;
    const char *const *words = args->words;
    unsigned int count = args->count;
    int *code = args->code;
    const char *const first = (count > 0u) ? words[0] : NULL;
    static char s_root[CLI_PATH_MAX_LEN];
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "keys", .cap = CLI_POOL_BYTES}))
    {
        cli_keys();
        *code = 0;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "about", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "imp " CLI_VERSION);
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
        if (EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named, .root = s_root,
                       .err_msg = &err_msg))
        {
            EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = s_root);
        }
        *code = 0;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "auto-report", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(console_auto_report, CommandLineConsoleCtx, .word_text = first);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "search", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(cli_command_line_words_count_internal.console_search, CliCommandLineWordsCountCtx,
                           .named = named, .words = words, .count = count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "plugins", .cap = CLI_POOL_BYTES}))
    {
        *code = cli_plugins_list();
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "new-plugin", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(cli_command_line_words_count_internal.new_plugin, CliCommandLineWordsCountCtx,
                           .words = words, .count = count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "user-css", .cap = CLI_POOL_BYTES}))
    {
        *code = console_user_css();
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "toolchains", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(cli_command_line_words_count_internal.toolchains_words, CliCommandLineWordsCountCtx,
                           .words = words, .count = count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "install", .cap = CLI_POOL_BYTES}) ||
             cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "uninstall", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(console_install, CommandLineConsoleCtx, .command = args->command, .words = words,
                           .count = count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "clone", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(cli_command_line_words_count_internal.clone_words, CliCommandLineWordsCountCtx,
                           .words = words, .count = count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "dev-container", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(console_dev_container, CommandLineConsoleCtx, .named = named);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "format", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(cli_command_line_words_count_internal.format_files, CliCommandLineWordsCountCtx,
                           .words = words, .count = count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "run-file", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(cli_command_line_words_count_internal.run_file_words, CliCommandLineWordsCountCtx,
                           .named = named, .words = words, .count = count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "validate", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(cli_command_line_words_count_internal.validate_files, CliCommandLineWordsCountCtx,
                           .words = words, .count = count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "disasm-verify", .cap = CLI_POOL_BYTES}))
    {
        *code =
            EMBED_CALL(cli_disasm_verify_words_table.words, CliDisasmVerifyWordsCfg, .words = words, .count = count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "report", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(console_report_page, CommandLineConsoleCtx, .named = named, .words = words, .count = count);
    }
    else
    {
        return 0;
    }
    return 1;
}

// the value a setting's words write, as the node's `args_text` takes them: `[on|off]` as true or false, words that
// repeat, their args ending `...`, one to a line, and any other joined by spaces, into `out_line`: 1, or 0 with why
// written
EMBED_INLINE int console_setting_value(const CommandLineConsoleCtx *args)
{
    VerbaLinea *const value = args->out_line;
    const char *const takes = (args->one != NULL) ? args->one : "";
    if (cellul.eq(&(CatenaFinitaCfg){.src = takes, .other = "[on|off]", .cap = CLI_POOL_BYTES}))
    {
        const int on = cellul.eq(&(CatenaFinitaCfg){.src = args->words[0], .other = "on", .cap = CLI_POOL_BYTES});
        if (!on && !cellul.eq(&(CatenaFinitaCfg){.src = args->words[0], .other = "off", .cap = CLI_POOL_BYTES}))
        {
            VerbaLinea line = {s_line, sizeof(s_line), 0ULL};
            mmgr_line_put(&line, args->words[0]);
            mmgr_line_put(&line, " is neither on nor off");
            EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_line,
                       .size = mmgr_line_ok(&line) ? line.at : 0ULL);
            return 0;
        }
        mmgr_line_put(value, on ? "true" : "false");
        return mmgr_line_finish(value) != 0ULL;
    }
    const unsigned long long takes_size = cellul.len(&(CatenaFinitaCfg){.src = takes, .cap = CLI_POOL_BYTES});
    const int lines = (takes_size >= 3ULL) &&
                      cellul.eq(&(CatenaFinitaCfg){.src = takes + takes_size - 3ULL, .other = "...", .cap = 4ULL});
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (at > 0u)
        {
            mmgr_line_ch(value, lines ? '\n' : ' ');
        }
        mmgr_line_put(value, args->words[at]);
    }
    return mmgr_line_finish(value) != 0ULL;
}

int cli_console_setting(const CliCommandLineWordsCountCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "impero finds no folder of its own");
        return CLI_NO_CODE;
    }
    const char *const key = args->command;
    if (args->count == 0u)
    {
        CliStateEntries entries =
            cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = g_cli_command_line_owner});
        // the text read out of the entries' cell into a cell of this call
        const unsigned long long size = cli_state_entry_size(&entries, key);
        char *const located_text =
            (size != CLI_STATE_NONE) ? g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_command_line_owner) : NULL;
        const char *const located =
            (located_text != NULL) ? cli_state_entry_value(&entries, key, located_text, size + 1ULL) : NULL;
        if (located != NULL)
        {
            EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = located);
        }
        g_cli_guard->persistent_buf_release(located_text, g_cli_command_line_owner);
        cli_state_entries_release(&entries);
        return (located != NULL) ? 0 : CLI_WRONG;
    }
    VerbaLinea value = {s_line, sizeof(s_line), 0ULL};
    if (!EMBED_CALL(console_setting_value, CommandLineConsoleCtx, .one = args->args_text, .words = args->words,
                    .count = args->count, .out_line = &value))
    {
        return CLI_WRONG;
    }
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    if (!cli_state_value_save(&(CliStateValueCfg){.folder = folder, .key = key, .value = s_line, .err_msg = &err_msg}))
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_WRONG;
    }
    static char s_found[CLI_PATH_MAX_LEN];
    const int found = (args->named != NULL) ? EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg,
                                                         .named = args->named, .root = s_found, .err_msg = &err_msg)
                                            : EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = s_found);
    err_msg.at = 0ULL;
    static CliReachAnswer s_answer;
    const char *const sent[2] = {key, s_line};
    const int requested =
        EMBED_CALL(cli_reach_menu.request, CliReachMenuCfg, .root = found ? s_found : NULL, .menu = "file",
                   .command = "preferences", .request = sent, .count = 2u, .answer = &s_answer, .err_msg = &err_msg);
    if (requested == 0)
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_WRONG;
    }
    if ((requested > 0) && !s_answer.done)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = s_answer.text);
        return CLI_WRONG;
    }
    return 0;
}

int cli_console(const CliCommandLineWordsCountCtx *args)
{
    const char *named = args->named;
    static const char *const s_protocol[] = {"course", "tables", "decode", "query",     "learn", "cross",
                                             "write",  "pairs",  "stick",  "retention", "sweep"};
    int code = 0;
    if (EMBED_CALL(console_treeless, CommandLineConsoleCtx, .command = args->command, .named = named,
                   .words = args->words, .count = args->count, .code = &code))
    {
        return code;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    if (!EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named, .root = s_root,
                    .err_msg = &err_msg))
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_NO_CODE;
    }
    const char *const first = (args->count > 0u) ? args->words[0] : NULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "list", .cap = CLI_POOL_BYTES}))
    {
        return EMBED_CALL(cli_command_line_word_internal.jobs_list, CliCommandLineWordCtx, .root = s_root,
                          .word_text = first);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "tests", .cap = CLI_POOL_BYTES}))
    {
        return EMBED_CALL(cli_command_line_root_internal.tests_list, CliCommandLineRootCtx, .root = s_root,
                          .words = args->words, .count = args->count);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "bridge", .cap = CLI_POOL_BYTES}))
    {
        return EMBED_CALL(cli_command_line_word_internal.bridge_words, CliCommandLineWordCtx, .root = s_root,
                          .word_text = first);
    }
    for (unsigned int at = 0u; at < sizeof(s_protocol) / sizeof(s_protocol[0]); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = s_protocol[at], .cap = CLI_POOL_BYTES}))
        {
            return EMBED_CALL(console_protocol, CommandLineConsoleCtx, .root = s_root, .command = args->command,
                              .words = args->words, .count = args->count);
        }
    }
    if (first == NULL)
    {
        VerbaLinea line = {s_line, sizeof(s_line), 0ULL};
        mmgr_line_put(&line, args->command);
        mmgr_line_put(&line, " needs a job: imp run list names them");
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_line,
                   .size = mmgr_line_ok(&line) ? line.at : 0ULL);
        return CLI_WRONG;
    }
    const CliCatalog *const catalog =
        EMBED_CALL(cli_command_line_word_internal.catalog, CliCommandLineWordCtx, .root = s_root);
    err_msg.at = 0ULL;
    const unsigned int job =
        EMBED_CALL(cli_command_line_catalog_among_internal.job_by_name, CliCommandLineCatalogAmongCtx,
                   .catalog = catalog, .among = NULL, .among_count = 0u, .name = first, .err_msg = &err_msg);
    if (job == CLI_CATALOG_NONE)
    {
        EMBED_CALL(console_err_print, CommandLineConsoleCtx, .err_msg = &err_msg);
        return CLI_NO_CODE;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "show", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(cli_command_line_root_internal.job_show, CliCommandLineRootCtx, .root = s_root, .catalog = catalog,
                   .job = job);
        return 0;
    }
    return EMBED_CALL(cli_command_line_root_internal.job_run, CliCommandLineRootCtx, .root = s_root, .catalog = catalog,
                      .job = job, .words = args->words + 1, .count = args->count - 1u);
}

int cli_file_word(const CliCommandLineOutCtx *args)
{
    const char *named = args->named;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    // the path is the word before a line and a column, each a colon and digits, that follow it
    embed_word path = cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_POOL_BYTES});
    for (unsigned int part = 0u; part < 2u; part += 1u)
    {
        embed_word colon = path;
        colon = (embed_word)EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                       .text = args->word_text, .at = colon, .set = ":");
        int digits = (colon > 0u) && (colon < path);
        {
            embed_word at = colon;
            while ((at < path) && digits)
            {
                digits = (args->word_text[at] >= '0') && (args->word_text[at] <= '9');
                at += 1u;
            }
        }
        if (digits && (colon > 1u))
        {
            path = colon - 1u;
        }
    }
    char supplied[CLI_PATH_MAX_LEN];
    if (path >= sizeof(supplied))
    {
        return -1;
    }
    memor.cpy(&(MemoriaCfg){.dst = supplied, .src = args->word_text, .bytes = path});
    supplied[path] = '\0';
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = supplied, .full = full,
                    .capacity = sizeof(full)) ||
        !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = full))
    {
        return -1;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named, .root = s_root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const embed_word root = cellul.len(&(CatenaFinitaCfg){.src = s_root, .cap = CLI_POOL_BYTES});
    const int separated = (root > 0u) && ((s_root[root - 1u] == '/') || (s_root[root - 1u] == '\\'));
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = full, .other_pointer = s_root, .size = root) ||
        (!separated && (full[root] != '/') && (full[root] != '\\') && (full[root] != '\0')))
    {
        mmgr_line_put(err_msg, full);
        mmgr_line_put(err_msg, " is not in the tree ");
        mmgr_line_put(err_msg, s_root);
        return 0;
    }
    const char *inner = full + root;
    inner += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = inner, .at = 0ULL,
                        .size = cellul.len(&(CatenaFinitaCfg){.src = inner, .cap = CLI_POOL_BYTES}), .set = "/\\");
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = out, .text = inner, .from = '\\', .to = '/');
    mmgr_line_put(out, args->word_text + path);
    return 1;
}
