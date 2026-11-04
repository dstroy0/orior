// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_window.c: the command line of the window's program and the tree the window opens on: the command line
// run first and the window it leaves to open recorded, the machine's own programs reached from inside a sandbox, every
// process the window starts contained under it, the tree found, the door opened and the tree watched, and the
// servers, sessions and processes ended as the window ends. A batch the watch takes is noted among the tree's runs,
// walked into the tree's branches and handed to the servers on the watch's own thread, which takes the next batch once
// they are done. A file git leaves out, as a build's output is, is shown in the tree and not checked
#include "command_line.h"

#include "../code/symbols.h"
#include "../debug/debug.h"
#include "../editor/files.h"
#include "../editor/watch.h"
#include "../machine/host.h"
#include "../machine/reach.h"
#include "../plugins/plugins.h"
#include "../runs/qry.h"
#include "../servers/servers.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "cells.h"

#include <stdlib.h>

// The window's command line: what the words came to, and the tree open, in a cell of its own, NULL for none
typedef struct
{
    CliOutcome outcome;
    char *root;
} CliCommandLineWindow;

static CliCommandLineWindow s_window;

typedef struct
{
    const VerbaLinea *err_msg;
} CliCommandLineWindowCtx;

// a line of the errors, `orior: ` before what was written
EMBED_INLINE void window_err(const CliCommandLineWindowCtx *args)
{
    char text[CLI_ERR_MSG_MAX_LEN + 8u];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&line, "orior: ");
    mmgr_line_put_n(&line, args->err_msg->out, mmgr_line_ok(args->err_msg) ? args->err_msg->at : 0ULL);
    mmgr_line_ch(&line, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = text,
               .length = mmgr_line_ok(&line) ? line.at : 0ULL);
}

int cli_command_line_window(int *window)
{
    // the window runs no main of orior's and enters here first: the pool every later call takes from starts here
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    static const char *s_words[CLI_LAUNCH_ARGS];
    const unsigned int count = EMBED_CALL(cli_capacity_count_internal.arguments, CliCapacityCountCtx, .count_i32 = 0,
                                          .supplied = NULL, .words_read = s_words, .capacity_u32 = CLI_LAUNCH_ARGS);
    if (count == 0u)
    {
        cli_console_let_go();
        memor.set(&(MemoriaCfg){.dst = &s_window.outcome, .val = 0, .bytes = sizeof(s_window.outcome)});
        s_window.outcome.window = 1;
        *window = 1;
        return 0;
    }
    EMBED_CALL(cli_command_line_input.run, CliCommandLineInputCfg, .supplied = s_words, .count = count,
               .outcome = &s_window.outcome);
    *window = s_window.outcome.window;
    return s_window.outcome.code;
}

// two paths in the order of their bytes
static int window_path_order(const void *one, const void *other)
{
    return EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = *(const char *const *)one,
                      .other_pointer = *(const char *const *)other);
}

// on the watch's thread: each file written noted among the tree's runs, a lost batch marked for them; the files in
// order, each once, those git leaves out passed over, walked into the tree's branches and handed to the servers and
// the tree's check
static void window_watch_take(const CliWatchBatchCfg *args)
{
    const char *const root = s_window.root;
    if (args->lost)
    {
        EMBED_CALL(cli_qry_out_root.runs_lose, CliQryOutRootCfg, .root = root);
    }
    const unsigned int files = args->files->count;
    for (unsigned int at = 0u; at < files; at += 1u)
    {
        const char *const path = EMBED_CALL(cli_files.at, CliFilesCfg, .files = args->files, .index = at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        EMBED_CALL(cli_qry_path_root.run_write, CliQryPathRootCfg, .root = root, .path = path, .length = length);
    }
    const char **const path =
        (files != 0u) ? g_cli_cells->persistent_buf_alloc(files * sizeof(*path), &g_cli_command_line_cells) : NULL;
    if (path == NULL)
    {
        return;
    }
    for (unsigned int at = 0u; at < files; at += 1u)
    {
        path[at] = EMBED_CALL(cli_files.at, CliFilesCfg, .files = args->files, .index = at);
    }
    qsort(path, files, sizeof(path[0]), window_path_order);
    unsigned int unique = 0u;
    for (unsigned int at = 0u; at < files; at += 1u)
    {
        const int again =
            (unique > 0u) &&
            cellul.eq(&(CatenaFinitaCfg){.src = path[unique - 1u], .other = path[at], .cap = CLI_PATH_MAX_LEN});
        if (!again && EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = root, .path = path[at]))
        {
            path[unique] = path[at];
            unique += 1u;
        }
    }
    if (unique > 0u)
    {
        EMBED_CALL(cli_symbols_root.tree_branches_walk, CliSymbolsRootCfg, .root = root, .paths = path,
                   .count = unique);
        EMBED_CALL(cli_servers_root_sink_count.change, CliServersRootSinkCountCfg, .root = root, .files = path,
                   .count = unique);
    }
    g_cli_cells->persistent_buf_release(path, &g_cli_command_line_cells);
}

void cli_command_line_tree_begin(void (*woken)(void))
{
    char error[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {error, sizeof(error), 0ULL};
    // inside a Flatpak's sandbox the machine's own programs run on the machine
    if (EMBED_CALL(cli_host_into.prepare, CliHostIntoCfg, .err_msg = &err_msg) == -1LL)
    {
        EMBED_CALL(window_err, CliCommandLineWindowCtx, .err_msg = &err_msg);
    }
    // every process the window starts stands under it, and ends with it
    (void)cli_container_init();
    char found[CLI_PATH_MAX_LEN];
    const char *root = NULL;
    if (s_window.outcome.has_root)
    {
        root = s_window.outcome.root;
    }
    else if (EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = found))
    {
        root = found;
    }
    const unsigned long long size =
        (root != NULL) ? (cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_PATH_MAX_LEN}) + 1ULL) : 0ULL;
    s_window.root = (root != NULL) ? g_cli_cells->persistent_buf_alloc(size, &g_cli_command_line_cells) : NULL;
    if (s_window.root != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_window.root, .src = root, .bytes = size});
        EMBED_CALL(cli_toolchains_root.set_tree, CliToolchainsRootCfg, .root = s_window.root);
        EMBED_CALL(cli_plugins_root.set_tree, CliPluginsRootCfg, .root = s_window.root);
    }
    err_msg.at = 0ULL;
    if (!cli_reach_open(s_window.root, woken, &err_msg))
    {
        EMBED_CALL(window_err, CliCommandLineWindowCtx, .err_msg = &err_msg);
    }
    if (s_window.root != NULL)
    {
        EMBED_CALL(cli_watch.start, CliWatchCfg, .root = s_window.root, .take = window_watch_take, .context = NULL);
    }
}

void cli_command_line_tree_end(void)
{
    cli_watch_stop();
    // a language server orior_cli started ends with it: on Windows a child outlives its parent
    cli_reach_forget();
    cli_servers_stop_all();
    cli_debug_stop_all();
    cli_container_members_end();
    cli_reach_close();
    g_cli_cells->persistent_buf_release(s_window.root, &g_cli_command_line_cells);
    s_window.root = NULL;
}

const char *cli_command_line_root(void)
{
    return s_window.root;
}

const char *cli_command_line_tree_name(void)
{
    if (s_window.root == NULL)
    {
        return "";
    }
    return s_window.root +
           EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = s_window.root,
                      .at = cellul.len(&(CatenaFinitaCfg){.src = s_window.root, .cap = CLI_PATH_MAX_LEN}),
                      .set = "/\\");
}

const CliOutcome *cli_command_line_launched(void)
{
    return &s_window.outcome;
}
