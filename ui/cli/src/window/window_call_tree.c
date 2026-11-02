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

// the tree open here, or null where none is
static int tree_root_now(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char root[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = root))
    {
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = root);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    return 1;
}

// the tree open here, or why there is none
static int tree_root(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = root, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = root);
    return 1;
}

// the paths `paths` of the tree open here made whole, refused where one climbs out of it
static int tree_inside(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    if (!EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = root, .err_msg = err_msg))
    {
        return 0;
    }
    const char **paths = NULL;
    int whole =
        EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = arguments, .key = "paths",
                   .most = CLI_WINDOW_PATHS, .items_pointer = &paths, .count = &count, .err_msg = err_msg);
    char (*const full)[CLI_PATH_MAX_LEN] =
        whole
            ? EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                         .size = (count + 1ull) * CLI_PATH_MAX_LEN, .what = "the paths made whole", .err_msg = err_msg)
            : NULL;
    whole = whole && (full != NULL);
    for (unsigned int at = 0u; whole && (at < count); at += 1u)
    {
        whole = EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = paths[at],
                           .joined = full[at], .err_msg = err_msg);
    }
    if (whole)
    {
        mmgr_line_ch(out, '[');
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            mmgr_line_put(out, (at > 0u) ? "," : "");
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = full[at]);
        }
        mmgr_line_ch(out, ']');
    }
    g_cli_cells->persistent_buf_release(full, &g_cli_window_cells);
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = paths);
    return whole;
}

// the address of the tree on another machine the link is open to, as it reads, and the machine, or null where none is
static int tree_link_address(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    CliLinkAddress address;
    if (!EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    char text[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea written = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_link_out.address_text, CliLinkOutCfg, .address = &address, .out = &written);
    mmgr_line_put(out, "{\"address\":");
    EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out, .text = text,
               .size = mmgr_line_ok(&written) ? written.at : 0ull);
    written.at = 0ull;
    EMBED_CALL(cli_link_out.machine, CliLinkOutCfg, .address = &address, .out = &written);
    mmgr_line_put(out, ",\"machine\":");
    EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out, .text = text,
               .size = mmgr_line_ok(&written) ? written.at : 0ull);
    mmgr_line_ch(out, '}');
    return 1;
}

// how the link to the tree's machine stands, or null where none is open
static int tree_link_standing(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    CliLinkAddress address;
    if (!EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    EMBED_CALL(cli_link_out_path.standing, CliLinkOutPathCfg, .out = out);
    return 1;
}

// the link to the tree's machine let go, for it to be made again
static int tree_link_drop(const CliServeAnswerCfg *args)
{
    (void)args;
    CliLinkAddress address;
    if (EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address))
    {
        cli_link_drop_now();
    }
    return 1;
}

typedef struct
{
    const char *root;
    const char *remote;
    const char **window_words;
    char *place;
    VerbaLinea *err_msg;
} WindowCallTreeCtx;
// the tree another window opens on and how it is named, into `window_words` after the program: the folder `root` made
// whole where one is named; else the tree on another machine at `remote`, or the one the link is open to; else the tree
// open here; the place named written into `place`, sized for CLI_PATH_MAX_LEN * 2: 1, or 0 with why given
EMBED_INLINE int tree_window_by_name(const WindowCallTreeCtx *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    CliLinkAddress address;
    VerbaLinea written = {args->place, CLI_PATH_MAX_LEN * 2u, 0ull};
    args->window_words[2] = args->place;
    if (root != NULL)
    {
        args->window_words[1] = "--root";
        return EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = root, .root = args->place,
                          .err_msg = err_msg);
    }
    if (args->remote != NULL)
    {
        args->window_words[1] = "--remote";
        mmgr_line_put(&written, args->remote);
        return EMBED_CALL(cli_link_text.address_parse, CliLinkTextCfg, .text = args->remote, .address_out = &address,
                          .err_msg = err_msg) &&
               (mmgr_line_finish(&written) != 0ull);
    }
    if (EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address))
    {
        args->window_words[1] = "--remote";
        EMBED_CALL(cli_link_out.address_text, CliLinkOutCfg, .address = &address, .out = &written);
        return mmgr_line_finish(&written) != 0ull;
    }
    args->window_words[1] = "--root";
    return EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = args->place, .err_msg = err_msg);
}

// another window opened, a program of its own with its own tabs, terminals and servers, on the tree `root` names, else
// on the tree on another machine `remote` names or the one the link is open to, else on the tree open here, to run the
// menus' command `words` names once its page is up; started outside the window's container, the reader's to save as
// this window closes
static int tree_window_open(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    static char s_program[CLI_PATH_MAX_LEN];
    static char s_place[CLI_PATH_MAX_LEN * 2u];
    static const char *s_window_words[CLI_WINDOW_PATHS + 4u];
    const char **words = NULL;
    unsigned int count = 0u;
    if (!EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = arguments, .key = "words",
                    .most = CLI_WINDOW_PATHS, .items_pointer = &words, .count = &count, .err_msg = err_msg))
    {
        EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = words);
        return 0;
    }
    int opened = EMBED_CALL(cli_path_room_internal.program_own, CliPathRoomCtx, .path_out = s_program,
                            .room_size = sizeof(s_program));
    if (!opened)
    {
        mmgr_line_put(err_msg, "the window's own program could not be read");
    }
    s_window_words[0] = s_program;
    opened = opened && EMBED_CALL(tree_window_by_name, WindowCallTreeCtx,
                                  .root = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                                     .request = arguments, .key = "root", .otherwise = NULL),
                                  .remote = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                                       .request = arguments, .key = "remote", .otherwise = NULL),
                                  .window_words = s_window_words, .place = s_place, .err_msg = err_msg);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_window_words[3u + at] = words[at];
    }
    s_window_words[3u + count] = NULL;
    unsigned int id = 0u;
    if (opened && !EMBED_CALL(cli_words_id_internal.started_outside, CliWordsIdCtx, .words = s_window_words, .id = &id))
    {
        mmgr_line_put(err_msg, s_program);
        mmgr_line_put(err_msg, ": it could not be started");
        opened = 0;
    }
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = words);
    return opened;
}

// the command the window was opened to run, once
static int tree_launch_take(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_window_out_request_internal.launch_take, CliWindowOutRequestCtx, .out = out);
    return 1;
}

const CliServeCall g_cli_window_tree_calls[] = {
    {"root_now", tree_root_now},         {"root", tree_root},
    {"tree_inside", tree_inside},        {"launch_take", tree_launch_take},
    {"link_address", tree_link_address}, {"link_standing", tree_link_standing},
    {"link_drop", tree_link_drop},       {"window_open", tree_window_open},
};
const unsigned int g_cli_window_tree_count = sizeof(g_cli_window_tree_calls) / sizeof(g_cli_window_tree_calls[0]);
