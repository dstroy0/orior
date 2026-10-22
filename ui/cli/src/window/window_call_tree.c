// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_tree.c: the window's calls on the tree open here and on what it runs: the tree's root, paths of it made
// whole, refused where they climb out; files pasted into it, and files of this machine brought over to a tree on
// another, each told as it goes; the command the window was opened to run; the bash, the PATH and the programs on it
// a terminal runs with; the processes the window started; and the language servers running, those a language needs,
// whether the tree's check is on, and servers stopped
#include "../command_line/command_line.h"
#include "../comms/link.h"
#include "../editor/files.h"
#include "../runs/runner.h"
#include "../servers/servers.h"
#include "../toolchains/toolchains.h"
#include "window_internal.h"

// the most paths one call names, and the most processes listed
#define CLI_WINDOW_PATHS 4096u
#define CLI_WINDOW_PROCESSES 8192u

static CliLock s_lock = CLI_LOCK_INIT;

// the tree open here, or null where none is
static int tree_root_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    char root[CLI_PATH_LONGEST];
    if (cli_serve_root_now(root))
    {
        cli_window_quoted(out, root);
    }
    else
    {
        cli_line_text(out, "null");
    }
    return 1;
}

// the tree open here, or why there is none
static int tree_root(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_root(root, said))
    {
        return 0;
    }
    cli_window_quoted(out, root);
    return 1;
}

// whether `text` reads as the address of a tree on another machine, or why it does not
static int tree_address_parse(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    const char *const text = cli_serve_string(args, "text", said);
    CliLinkAddress address;
    return (text != NULL) && cli_link_address_parse(text, &address, said);
}

// the paths `paths` of the tree open here made whole, refused where one climbs out of it
static int tree_inside(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    if (!cli_serve_root(root, said))
    {
        return 0;
    }
    const char **paths = NULL;
    int whole = cli_serve_strings_taken(args, "paths", CLI_WINDOW_PATHS, &paths, &count, said);
    char (*const full)[CLI_PATH_LONGEST] =
        whole ? cli_serve_cell((count + 1ull) * CLI_PATH_LONGEST, "the paths made whole", said) : NULL;
    whole = whole && (full != NULL);
    for (unsigned int at = 0u; whole && (at < count); at += 1u)
    {
        whole = cli_inside(root, paths[at], full[at], said);
    }
    if (whole)
    {
        cli_line_character(out, '[');
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            cli_line_text(out, (at > 0u) ? "," : "");
            cli_window_quoted(out, full[at]);
        }
        cli_line_character(out, ']');
    }
    cli_cell_given(full);
    cli_serve_strings_given(paths);
    return whole;
}

// the files and folders `paths` of this machine pasted into the folder `into` of the tree, moved where `cut`: where
// each went
static int tree_files_paste(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    int cut = 0;
    const char *const into = cli_serve_string(args, "into", said);
    if ((into == NULL) || !cli_serve_bool(args, "cut", &cut, said) || !cli_serve_root(root, said))
    {
        return 0;
    }
    const char **paths = NULL;
    const int pasted = cli_serve_strings_taken(args, "paths", CLI_WINDOW_PATHS, &paths, &count, said) &&
                       cli_files_paste(root, into, paths, count, cut, out, said);
    cli_serve_strings_given(paths);
    return pasted;
}

// a call of the tree's side on another machine, over the link
static int tree_linked(void *context, const char *name, const char *arguments, CliLine *out, CliLine *said)
{
    (void)context;
    return cli_link_call(name, arguments, cellul.len(&(CatenaFinitaCfg){.src = arguments, .cap = CLI_CELLS_POOL}), out,
                         said);
}

// how far along a file being brought over is, told as bring-progress
static void tree_bring_told(void *context, const char *path, unsigned long long done, unsigned long long size)
{
    (void)context;
    char body[CLI_PATH_LONGEST * 2u];
    CliLine told = {body, sizeof(body), 0ull};
    cli_line_text(&told, "{\"path\":");
    cli_window_quoted(&told, path);
    cli_line_text(&told, ",\"done\":");
    cli_line_decimal(&told, done, 1u);
    cli_line_text(&told, ",\"size\":");
    cli_line_decimal(&told, size, 1u);
    cli_line_character(&told, '}');
    cli_serve_emitted("bring-progress", body, cli_line_fits(&told) ? told.at : 0ull);
}

// the files `paths` of this machine read here and written into `into` of the tree on another machine a part at a
// time, each telling how far along it is: what was brought
static int tree_bring_over(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned int count = 0u;
    const char *const into = cli_serve_string(args, "into", said);
    if (into == NULL)
    {
        return 0;
    }
    const char **paths = NULL;
    const int brought = cli_serve_strings_taken(args, "paths", CLI_WINDOW_PATHS, &paths, &count, said) &&
                        cli_files_bring_over(tree_linked, NULL, into, paths, count, tree_bring_told, NULL, out, said);
    cli_serve_strings_given(paths);
    return brought;
}

// the bash a terminal runs
static int tree_runner_bash(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char bash[CLI_PATH_LONGEST];
    if (!cli_runner_bash(bash, said))
    {
        return 0;
    }
    cli_window_quoted(out, bash);
    return 1;
}

// the PATH every step runs with
static int tree_run_path(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    static char s_path[CLI_PATH_TEXT];
    cli_lock_take(&s_lock);
    cli_toolchains_run_path(s_path, sizeof(s_path));
    cli_window_quoted(out, s_path);
    cli_lock_give(&s_lock);
    return 1;
}

// the program `program` as a file in the first folder of the PATH that holds one, or null where none does
static int tree_path_program(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const program = cli_serve_string(args, "program", said);
    if (program == NULL)
    {
        return 0;
    }
    char found[CLI_PATH_LONGEST];
    const int any = cli_toolchains_on_path(&program, 1u, found);
    if (any)
    {
        cli_window_quoted(out, found);
    }
    else
    {
        cli_line_text(out, "null");
    }
    return 1;
}

// every process the window started, by id
static int tree_container_members(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    static unsigned int s_ids[CLI_WINDOW_PROCESSES];
    cli_lock_take(&s_lock);
    const unsigned int count = cli_container_members(s_ids, CLI_WINDOW_PROCESSES);
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_line_decimal(out, s_ids[at], 1u);
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
    return 1;
}

// every process the system lists with its parent, each `[id, parent]`
static int tree_container_parents(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    static CliProcess s_processes[CLI_WINDOW_PROCESSES];
    cli_lock_take(&s_lock);
    const unsigned int count = cli_processes_listed(s_processes, CLI_WINDOW_PROCESSES);
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? ",[" : "[");
        cli_line_decimal(out, s_processes[at].id, 1u);
        cli_line_character(out, ',');
        cli_line_decimal(out, s_processes[at].parent, 1u);
        cli_line_character(out, ']');
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
    return 1;
}

// the tree the folder `root` names, made whole and the tree whose tools come first
static int tree_of(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const named = cli_serve_string(args, "root", said);
    char root[CLI_PATH_LONGEST];
    if ((named == NULL) || !cli_command_line_tree(named, root, said))
    {
        return 0;
    }
    cli_window_quoted(out, root);
    return 1;
}

// the address of the tree on another machine the link is open to, as it reads, and the machine, or null where none is
static int tree_link_address(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    CliLinkAddress address;
    if (!cli_link_address(&address))
    {
        cli_line_text(out, "null");
        return 1;
    }
    char text[CLI_PATH_LONGEST * 2u];
    CliLine written = {text, sizeof(text), 0ull};
    cli_link_address_text(&address, &written);
    cli_line_text(out, "{\"address\":");
    cli_window_counted(out, text, cli_line_fits(&written) ? written.at : 0ull);
    written.at = 0ull;
    cli_link_machine(&address, &written);
    cli_line_text(out, ",\"machine\":");
    cli_window_counted(out, text, cli_line_fits(&written) ? written.at : 0ull);
    cli_line_character(out, '}');
    return 1;
}

// how the link to the tree's machine stands, or null where none is open
static int tree_link_standing(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    CliLinkAddress address;
    if (!cli_link_address(&address))
    {
        cli_line_text(out, "null");
        return 1;
    }
    cli_link_standing(out);
    return 1;
}

// the link to the tree's machine let go, for it to be made again
static int tree_link_drop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)out;
    (void)said;
    CliLinkAddress address;
    if (cli_link_address(&address))
    {
        cli_link_drop_now();
    }
    return 1;
}

// the program and its words that open the reader's shell on the tree's machine with a terminal, in `folder` or in the
// tree's folder where none is given; null where no link is open
static int tree_link_shell_words(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)said;
    static CliWords s_words;
    CliLinkAddress address;
    if (!cli_link_address(&address))
    {
        cli_line_text(out, "null");
        return 1;
    }
    const unsigned int folder = cli_serve_arg(args, "folder");
    const char *const named = cli_json_string(args->json, folder, NULL);
    char line[CLI_PATH_LONGEST * 4u];
    CliLine shell = {line, sizeof(line), 0ull};
    cli_lock_take(&s_lock);
    cli_link_shell_line(&address, named, &shell);
    (void)cli_line_closed(&shell);
    cli_link_runs(&address, line, 1, &s_words);
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < s_words.count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_window_quoted(out, s_words.word[at]);
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
    return 1;
}

// a program started outside the window's container with its words, the reader's to keep as the window closes
static int tree_started_outside(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    const char **words = NULL;
    unsigned int count = 0u;
    int started = cli_serve_strings_taken(args, "words", CLI_WINDOW_PATHS, &words, &count, said) && (count > 0u);
    if (started)
    {
        unsigned int id = 0u;
        started = cli_started_outside(words, &id);
        if (!started)
        {
            cli_line_text(said, words[0]);
            cli_line_text(said, ": it could not be started");
        }
    }
    cli_serve_strings_given(words);
    return started;
}

// the command the window was opened to run, once
static int tree_launch_take(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_window_launch_taken(out);
    return 1;
}

// every language server running
static int tree_servers_running(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_servers_running(out);
    return 1;
}

// whether the tree's check goes on, which needs every server
static int tree_servers_checking(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_line_text(out, cli_servers_checking() ? "true" : "false");
    return 1;
}

// the toolchains whose servers serve any of `languages`
static int tree_servers_tools_for(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned int count = 0u;
    const char **languages = NULL;
    const int read = cli_serve_strings_taken(args, "languages", CLI_WINDOW_PATHS, &languages, &count, said);
    if (read)
    {
        cli_servers_tools_for(languages, count, out);
    }
    cli_serve_strings_given(languages);
    return read;
}

// the servers of `tools` stopped: the files they had open
static int tree_servers_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned int count = 0u;
    const char **tools = NULL;
    const int read = cli_serve_strings_taken(args, "tools", CLI_WINDOW_PATHS, &tools, &count, said);
    if (read)
    {
        cli_servers_stop(tools, count, out);
    }
    cli_serve_strings_given(tools);
    return read;
}

const CliServeCall g_cli_window_tree_calls[] = {
    {"root_now", tree_root_now},
    {"root", tree_root},
    {"address_parse", tree_address_parse},
    {"tree_inside", tree_inside},
    {"files_paste", tree_files_paste},
    {"files_bring_over", tree_bring_over},
    {"runner_bash", tree_runner_bash},
    {"run_path", tree_run_path},
    {"path_program", tree_path_program},
    {"container_members", tree_container_members},
    {"container_parents", tree_container_parents},
    {"launch_take", tree_launch_take},
    {"tree_of", tree_of},
    {"link_address", tree_link_address},
    {"link_standing", tree_link_standing},
    {"link_drop", tree_link_drop},
    {"link_shell_words", tree_link_shell_words},
    {"started_outside", tree_started_outside},
    {"servers_running", tree_servers_running},
    {"servers_checking", tree_servers_checking},
    {"servers_tools_for", tree_servers_tools_for},
    {"servers_stop", tree_servers_stop},
};
const unsigned int g_cli_window_tree_count = sizeof(g_cli_window_tree_calls) / sizeof(g_cli_window_tree_calls[0]);
