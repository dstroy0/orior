// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_tree.c: the window's calls on the tree open here and on what it runs: the tree's root, paths of it made
// whole, refused where they climb out; the command the window was opened to run; the link to a tree on another
// machine; and another window opened
#include "../command_line/command_line.h"
#include "../comms/link.h"
#include "window_internal.h"

// the most paths one call names
#define CLI_WINDOW_PATHS 4096u

static CliLock s_lock = CLI_LOCK_INIT;

// the tree open here, or null where none is
static int tree_root_now(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (cli_serve_root_now(root))
    {
        cli_window_quoted(out, root);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    return 1;
}

// the tree open here, or why there is none
static int tree_root(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_root(root, err_msg))
    {
        return 0;
    }
    cli_window_quoted(out, root);
    return 1;
}

// the paths `paths` of the tree open here made whole, refused where one climbs out of it
static int tree_inside(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    if (!cli_serve_root(root, err_msg))
    {
        return 0;
    }
    const char **paths = NULL;
    int whole = cli_serve_strings_taken(args, "paths", CLI_WINDOW_PATHS, &paths, &count, err_msg);
    char (*const full)[CLI_PATH_MAX_LEN] =
        whole ? cli_serve_cell((count + 1ull) * CLI_PATH_MAX_LEN, "the paths made whole", err_msg) : NULL;
    whole = whole && (full != NULL);
    for (unsigned int at = 0u; whole && (at < count); at += 1u)
    {
        whole = cli_inside(root, paths[at], full[at], err_msg);
    }
    if (whole)
    {
        mmgr_line_ch(out, '[');
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            mmgr_line_put(out, (at > 0u) ? "," : "");
            cli_window_quoted(out, full[at]);
        }
        mmgr_line_ch(out, ']');
    }
    g_cli_cells->persistent_buf_release(full, NULL);
    cli_serve_strings_given(paths);
    return whole;
}

// the address of the tree on another machine the link is open to, as it reads, and the machine, or null where none is
static int tree_link_address(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    CliLinkAddress address;
    if (!cli_link_address(&address))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    char text[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea written = {text, sizeof(text), 0ull};
    cli_link_address_text(&address, &written);
    mmgr_line_put(out, "{\"address\":");
    cli_window_counted(out, text, mmgr_line_ok(&written) ? written.at : 0ull);
    written.at = 0ull;
    cli_link_machine(&address, &written);
    mmgr_line_put(out, ",\"machine\":");
    cli_window_counted(out, text, mmgr_line_ok(&written) ? written.at : 0ull);
    mmgr_line_ch(out, '}');
    return 1;
}

// how the link to the tree's machine stands, or null where none is open
static int tree_link_standing(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    CliLinkAddress address;
    if (!cli_link_address(&address))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    cli_link_standing(out);
    return 1;
}

// the link to the tree's machine let go, for it to be made again
static int tree_link_drop(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)out;
    (void)err_msg;
    CliLinkAddress address;
    if (cli_link_address(&address))
    {
        cli_link_drop_now();
    }
    return 1;
}

// the tree another window opens on and how it is named, into `window_words` after the program: the folder `root` made
// whole where one is named; else the tree on another machine at `remote`, or the one the link is open to; else the tree
// open here; the place named written into `place`, sized for CLI_PATH_MAX_LEN * 2: 1, or 0 with why given
static int tree_window_named(const char *root, const char *remote, const char **window_words, char *place,
                             VerbaLinea *err_msg)
{
    CliLinkAddress address;
    VerbaLinea written = {place, CLI_PATH_MAX_LEN * 2u, 0ull};
    window_words[2] = place;
    if (root != NULL)
    {
        window_words[1] = "--root";
        return cli_command_line_tree(root, place, err_msg);
    }
    if (remote != NULL)
    {
        window_words[1] = "--remote";
        mmgr_line_put(&written, remote);
        return cli_link_address_parse(remote, &address, err_msg) && (mmgr_line_finish(&written) != 0ull);
    }
    if (cli_link_address(&address))
    {
        window_words[1] = "--remote";
        cli_link_address_text(&address, &written);
        return mmgr_line_finish(&written) != 0ull;
    }
    window_words[1] = "--root";
    return cli_serve_root(place, err_msg);
}

// another window opened, a program of its own with its own tabs, terminals and servers, on the tree `root` names, else
// on the tree on another machine `remote` names or the one the link is open to, else on the tree open here, to run the
// menus' command `words` names once its page is up; started outside the window's container, the reader's to keep as
// this window closes
static int tree_window_open(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    static char s_program[CLI_PATH_MAX_LEN];
    static char s_place[CLI_PATH_MAX_LEN * 2u];
    static const char *s_window_words[CLI_WINDOW_PATHS + 4u];
    const char **words = NULL;
    unsigned int count = 0u;
    if (!cli_serve_strings_taken(args, "words", CLI_WINDOW_PATHS, &words, &count, err_msg))
    {
        cli_serve_strings_given(words);
        return 0;
    }
    cli_lock_take(&s_lock);
    int opened = cli_program_own(s_program, sizeof(s_program));
    if (!opened)
    {
        mmgr_line_put(err_msg, "the window's own program could not be read");
    }
    s_window_words[0] = s_program;
    opened = opened && tree_window_named(cli_serve_string_or(args, "root", NULL),
                                         cli_serve_string_or(args, "remote", NULL), s_window_words, s_place, err_msg);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_window_words[3u + at] = words[at];
    }
    s_window_words[3u + count] = NULL;
    unsigned int id = 0u;
    if (opened && !cli_started_outside(s_window_words, &id))
    {
        mmgr_line_put(err_msg, s_program);
        mmgr_line_put(err_msg, ": it could not be started");
        opened = 0;
    }
    cli_lock_give(&s_lock);
    cli_serve_strings_given(words);
    return opened;
}

// the command the window was opened to run, once
static int tree_launch_take(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_window_launch_taken(out);
    return 1;
}

const CliServeCall g_cli_window_tree_calls[] = {
    {"root_now", tree_root_now},         {"root", tree_root},
    {"tree_inside", tree_inside},        {"launch_take", tree_launch_take},
    {"link_address", tree_link_address}, {"link_standing", tree_link_standing},
    {"link_drop", tree_link_drop},       {"window_open", tree_window_open},
};
const unsigned int g_cli_window_tree_count = sizeof(g_cli_window_tree_calls) / sizeof(g_cli_window_tree_calls[0]);
