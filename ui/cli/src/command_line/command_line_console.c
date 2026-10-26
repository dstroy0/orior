// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_console.c: the commands the command line runs itself, in the terminal: the keys, what orior_cli is and
// its tree, errors filed on their own turned on or off, the bug report page opened, a search of the tree, the plugins,
// the reader's stylesheet, the toolchains, a clone, the tree's dev container brought up and a window opened on it,
// files formatted, run and validated; the tree's jobs listed, shown and run, its tests and its bridge listed, and the
// words of `orior_cli protocol` sent to a window open on the tree or run here. And a word that names a file of the tree
#include "../deploy/devcontainer.h"
#include "../editor/files.h"
#include "../machine/reach.h"
#include "../machine/report.h"
#include "../programs/disasm_verify.h"
#include "../runs/protocol.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "command_line_internal.h"

static char s_line[1u << 16u];
static char s_err_buf[1u << 14u];

// a line of the output made of two texts
static void console_out_two(const char *one, const char *other)
{
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, one);
    mmgr_line_put(&line, other);
    cli_out_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
}

// why given written to the errors
static void console_err_print(const VerbaLinea *err_msg)
{
    cli_err_line(err_msg->out, mmgr_line_ok(err_msg) ? err_msg->at : 0ull);
}

// `orior_cli help auto-report [on|off]`: errors filing on their own turned on or off, or which reported
static int console_auto_report(const char *word)
{
    if (word == NULL)
    {
        cli_out(cli_report_auto() ? "on" : "off");
        return 0;
    }
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "on", .cap = CLI_CELLS_POOL})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "off", .cap = CLI_CELLS_POOL})))
    {
        VerbaLinea line = {s_line, sizeof(s_line), 0ull};
        mmgr_line_put(&line, word);
        mmgr_line_put(&line, " is neither on nor off");
        cli_err_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
        return CLI_WRONG;
    }
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    if (!cli_report_set_auto(cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "on", .cap = CLI_CELLS_POOL}),
                             &err_msg))
    {
        console_err_print(&err_msg);
        return CLI_NO_CODE;
    }
    return 0;
}

// `orior_cli help report [category] [title]`: the bug report page opened with what was given filled in
static int console_report_page(const char *named, const char *const *words, unsigned int count)
{
    static char s_title[1u << 12u];
    VerbaLinea title = {s_title, sizeof(s_title), 0ull};
    const int known = (count > 0u) && cli_report_category_known(words[0]);
    for (unsigned int at = known ? 1u : 0u; at < count; at += 1u)
    {
        mmgr_line_put(&title, (title.at > 0ull) ? " " : "");
        mmgr_line_put(&title, words[at]);
    }
    (void)mmgr_line_finish(&title);
    static char s_root[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    const int has_root = cli_command_line_tree(named, s_root, &err_msg);
    const CliReport given = {known ? words[0] : "unknown", s_title, "", "", ""};
    static char s_url[CLI_REPORT_URL];
    VerbaLinea url = {s_url, sizeof(s_url), 0ull};
    cli_report_page(&given, has_root ? s_root : NULL, &url);
    (void)mmgr_line_finish(&url);
    cli_out(s_url);
    err_msg.at = 0ull;
    if (!cli_report_open_page(s_url, &err_msg))
    {
        console_err_print(&err_msg);
        return CLI_NO_CODE;
    }
    return 0;
}

// a docker line printed as the dev container comes up
static void console_docker_line(void *context, const char *text, unsigned long long length)
{
    (void)context;
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "$ ");
    mmgr_line_put_n(&line, text, length);
    cli_out_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
}

// the tree's dev container brought up as its description specifies, docker's output shown as it comes, and a window
// opened on the tree in it; a program not orior_gui, which opens no window, names the address to open
static int console_dev_container(const char *named)
{
    static char s_root[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    if (!cli_command_line_tree(named, s_root, &err_msg))
    {
        console_err_print(&err_msg);
        return CLI_NO_CODE;
    }
    static CliPlan s_plan;
    if (!cli_devcontainer_plan(s_root, &s_plan, &err_msg))
    {
        console_err_print(&err_msg);
        return CLI_WRONG;
    }
    char address[CLI_PATH_MAX_LEN];
    const QuerySink sink = {console_docker_line, NULL};
    if (!cli_devcontainer_up(&s_plan, sink, address, &err_msg))
    {
        console_err_print(&err_msg);
        return CLI_NO_CODE;
    }
    char program[CLI_PATH_MAX_LEN] = "";
    const int own = cli_program_own(program, sizeof(program));
    const char *leaf = program;
    for (const char *at = program; *at != '\0'; at += 1)
    {
        leaf = ((*at == '/') || (*at == '\\')) ? at + 1 : leaf;
    }
    const char *const dot =
        cli_text_last(leaf, cellul.len(&(CatenaFinitaCfg){.src = leaf, .cap = CLI_CELLS_POOL}), '.');
    const size_t stem = ((dot != NULL) && (dot != leaf))
                            ? (size_t)(dot - leaf)
                            : cellul.len(&(CatenaFinitaCfg){.src = leaf, .cap = CLI_CELLS_POOL});
    if (!own || (stem != 9u) ||
        (!cellul.starts(&(CatenaFinitaCfg){.src = leaf, .other = "orior_gui", .cap = CLI_CELLS_POOL})))
    {
        console_out_two("orior_gui --remote ", address);
        return 0;
    }
    const char *const words[4] = {program, "--remote", address, NULL};
    unsigned int id = 0u;
    if (!cli_started_outside(words, &id))
    {
        VerbaLinea line = {s_line, sizeof(s_line), 0ull};
        mmgr_line_put(&line, "orior_gui --remote ");
        mmgr_line_put(&line, address);
        mmgr_line_put(&line, ": it could not be started");
        cli_err_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
        return CLI_NO_CODE;
    }
    console_out_two("Opened a window on ", address);
    return 0;
}

// a line of `orior_cli protocol` written to the output
static void console_protocol_line(void *context, const char *text, unsigned long long length)
{
    (void)context;
    cli_out_line(text, length);
}

// the words of `orior_cli protocol`: sent to a window open on the tree, which runs the protocol command as its job and
// shows the run in its protocol panel; with none open run here, and so does the job's own command, which carries the
// window's door in its environment and must not reach the window again. A reading, `decode` and the two readings of a
// pulled table, prints here and is no job of the window's
static int console_protocol(const char *root, const char *command, const char *const *words, unsigned int count)
{
    char value[16];
    const int inside_a_job = query_environment_read(CLI_RUN_VARIABLE, value, sizeof(value));
    const int reading =
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "decode", .cap = CLI_CELLS_POOL})) ||
        ((cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "tables", .cap = CLI_CELLS_POOL})) && (count > 0u) &&
         ((cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "read", .cap = CLI_CELLS_POOL})) ||
          (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "fields", .cap = CLI_CELLS_POOL}))));
    if (!inside_a_job && !reading)
    {
        static CliReachAnswer s_answer;
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
        const int requested = cli_reach_request(root, "protocol", command, words, count, &s_answer, &err_msg);
        if (requested == 0)
        {
            console_err_print(&err_msg);
            return CLI_WRONG;
        }
        if (requested > 0)
        {
            if (s_answer.text[0] != '\0')
            {
                if (s_answer.done)
                {
                    cli_out(s_answer.text);
                }
                else
                {
                    cli_err(s_answer.text);
                }
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
    const int code = cli_protocol_run(root, s_all, arg_count + 1u, sink);
    return (code < 0) ? CLI_WRONG : code;
}

// `orior_cli file user-css`: the stylesheet the window lays over its own named, made empty but for a note where it is
// not there yet
static int console_user_css(void)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    if (!cli_home_ensure_user_css(path, &err_msg))
    {
        console_err_print(&err_msg);
        return CLI_NO_CODE;
    }
    cli_out(path);
    return 0;
}

// the commands that need no tree, each run: 1 with the code to exit with into `code`, or 0 where `command` is none of
// them
static int console_treeless(const char *command, const char *named, const char *const *words, unsigned int count,
                            int *code)
{
    const char *const first = (count > 0u) ? words[0] : NULL;
    static char s_root[CLI_PATH_MAX_LEN];
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "keys", .cap = CLI_CELLS_POOL}))
    {
        cli_keys();
        *code = 0;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "about", .cap = CLI_CELLS_POOL}))
    {
        cli_out("orior_cli " CLI_VERSION);
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
        if (cli_command_line_tree(named, s_root, &err_msg))
        {
            cli_out(s_root);
        }
        *code = 0;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "auto-report", .cap = CLI_CELLS_POOL}))
    {
        *code = console_auto_report(first);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "search", .cap = CLI_CELLS_POOL}))
    {
        *code = cli_console_search(named, words, count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "plugins", .cap = CLI_CELLS_POOL}))
    {
        *code = cli_plugins_list();
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "new-plugin", .cap = CLI_CELLS_POOL}))
    {
        *code = cli_new_plugin(words, count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "user-css", .cap = CLI_CELLS_POOL}))
    {
        *code = console_user_css();
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "toolchains", .cap = CLI_CELLS_POOL}))
    {
        *code = cli_toolchains_words(words, count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "clone", .cap = CLI_CELLS_POOL}))
    {
        *code = cli_clone_words(words, count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "dev-container", .cap = CLI_CELLS_POOL}))
    {
        *code = console_dev_container(named);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "format", .cap = CLI_CELLS_POOL}))
    {
        *code = cli_format_files(words, count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "run-file", .cap = CLI_CELLS_POOL}))
    {
        *code = cli_run_file_words(named, words, count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "validate", .cap = CLI_CELLS_POOL}))
    {
        *code = cli_validate_files(words, count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "disasm-verify", .cap = CLI_CELLS_POOL}))
    {
        *code = cli_disasm_verify_words(words, count);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "report", .cap = CLI_CELLS_POOL}))
    {
        *code = console_report_page(named, words, count);
    }
    else
    {
        return 0;
    }
    return 1;
}

int cli_console(const char *command, const char *named, const char *const *words, unsigned int count)
{
    static const char *const s_protocol[] = {"course", "tables", "decode", "query",     "learn", "cross",
                                             "write",  "pairs",  "stick",  "retention", "sweep"};
    int code = 0;
    if (console_treeless(command, named, words, count, &code))
    {
        return code;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    if (!cli_command_line_tree(named, s_root, &err_msg))
    {
        console_err_print(&err_msg);
        return CLI_NO_CODE;
    }
    const char *const first = (count > 0u) ? words[0] : NULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "list", .cap = CLI_CELLS_POOL}))
    {
        return cli_jobs_list(s_root, first);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "tests", .cap = CLI_CELLS_POOL}))
    {
        return cli_tests_list(s_root, words, count);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "bridge", .cap = CLI_CELLS_POOL}))
    {
        return cli_bridge_words(s_root, first);
    }
    for (unsigned int at = 0u; at < sizeof(s_protocol) / sizeof(s_protocol[0]); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = s_protocol[at], .cap = CLI_CELLS_POOL}))
        {
            return console_protocol(s_root, command, words, count);
        }
    }
    if (first == NULL)
    {
        VerbaLinea line = {s_line, sizeof(s_line), 0ull};
        mmgr_line_put(&line, command);
        mmgr_line_put(&line, " needs a job: orior_cli run list names them");
        cli_err_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
        return CLI_WRONG;
    }
    const CliCatalog *const catalog = cli_command_line_catalog(s_root);
    err_msg.at = 0ull;
    const unsigned int job = cli_job_named(catalog, NULL, 0u, first, &err_msg);
    if (job == CLI_CATALOG_NONE)
    {
        console_err_print(&err_msg);
        return CLI_NO_CODE;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "show", .cap = CLI_CELLS_POOL}))
    {
        cli_job_show(s_root, catalog, job);
        return 0;
    }
    return cli_job_run(s_root, catalog, job, words + 1, count - 1u);
}

int cli_file_word(const char *named, const char *word, VerbaLinea *out, VerbaLinea *err_msg)
{
    // the path is the word before a line and a column, each a colon and digits, that follow it
    size_t path = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL});
    for (unsigned int part = 0u; part < 2u; part += 1u)
    {
        size_t colon = path;
        while ((colon > 0u) && (word[colon - 1u] != ':'))
        {
            colon -= 1u;
        }
        int digits = (colon > 0u) && (colon < path);
        for (size_t at = colon; (at < path) && digits; at += 1u)
        {
            digits = (word[at] >= '0') && (word[at] <= '9');
        }
        if (digits && (colon > 1u))
        {
            path = colon - 1u;
        }
    }
    char given[CLI_PATH_MAX_LEN];
    if (path >= sizeof(given))
    {
        return -1;
    }
    memor.cpy(&(MemoriaCfg){.dst = given, .src = word, .bytes = path});
    given[path] = '\0';
    char full[CLI_PATH_MAX_LEN];
    if (!query_path_full(given, full, sizeof(full)) || !query_is_file(full))
    {
        return -1;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    if (!cli_command_line_tree(named, s_root, err_msg))
    {
        return 0;
    }
    const size_t root = cellul.len(&(CatenaFinitaCfg){.src = s_root, .cap = CLI_CELLS_POOL});
    const int separated = (root > 0u) && ((s_root[root - 1u] == '/') || (s_root[root - 1u] == '\\'));
    if (!cli_text_same(full, s_root, root) ||
        (!separated && (full[root] != '/') && (full[root] != '\\') && (full[root] != '\0')))
    {
        mmgr_line_put(err_msg, full);
        mmgr_line_put(err_msg, " is not in the tree ");
        mmgr_line_put(err_msg, s_root);
        return 0;
    }
    const char *inner = full + root;
    while ((*inner == '/') || (*inner == '\\'))
    {
        inner += 1;
    }
    for (; *inner != '\0'; inner += 1)
    {
        mmgr_line_ch(out, (*inner == '\\') ? '/' : *inner);
    }
    mmgr_line_put(out, word + path);
    return 1;
}
