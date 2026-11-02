// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_tree.c: the tree the window opens on and what stands with it as the window begins and ends: the command
// line run first and the window it leaves to open recorded, the machine's own programs reached from inside a sandbox,
// every process the window starts contained under it, the tree found, and the servers, sessions and processes ended as
// the window ends
#include "controller_internal.h"

#include "../command_line/cells.h"
#include "../command_line/command_line.h"
#include "../debug/debug.h"
#include "../editor/watch.h"
#include "../machine/host.h"
#include "../machine/reach.h"
#include "../plugins/plugins.h"
#include "../servers/servers.h"
#include "../toolchains/toolchains.h"

static CliOutcome s_outcome;
static char *s_root;

typedef struct
{
    const VerbaLinea *err_msg;
} ControllerTreeCtx;

// a line of the errors, `orior: ` before what was written
EMBED_INLINE void tree_err(const ControllerTreeCtx *args)
{
    char text[CLI_ERR_MSG_MAX_LEN + 8u];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, "orior: ");
    mmgr_line_put_n(&line, args->err_msg->out, mmgr_line_ok(args->err_msg) ? args->err_msg->at : 0ull);
    mmgr_line_ch(&line, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = text,
               .length = mmgr_line_ok(&line) ? line.at : 0ull);
}

int cli_controller_command_line(int *window)
{
    // the window runs no main of orior's and enters here first: the pool every later call takes from is made here
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    static const char *s_words[CLI_LAUNCH_ARGS];
    const unsigned int count = EMBED_CALL(cli_capacity_count_internal.arguments, CliCapacityCountCtx, .count_i32 = 0,
                                          .supplied = NULL, .words_read = s_words, .capacity_u32 = CLI_LAUNCH_ARGS);
    const char *const *const words = s_words;
    if (count == 0u)
    {
        cli_console_let_go();
        memor.set(&(MemoriaCfg){.dst = &s_outcome, .val = 0, .bytes = sizeof(s_outcome)});
        s_outcome.window = 1;
        *window = 1;
        return 0;
    }
    EMBED_CALL(cli_command_line_input.run, CliCommandLineInputCfg, .supplied = words, .count = count,
               .outcome = &s_outcome);
    *window = s_outcome.window;
    return s_outcome.code;
}

void cli_controller_tree_begin(void)
{
    char error[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {error, sizeof(error), 0ull};
    // inside a Flatpak's sandbox the machine's own programs run on the machine
    if (EMBED_CALL(cli_host_into.prepare, CliHostIntoCfg, .err_msg = &err_msg) == -1ll)
    {
        EMBED_CALL(tree_err, ControllerTreeCtx, .err_msg = &err_msg);
    }
    // every process the window starts stands under it, and ends with it
    (void)cli_container_init();
    char found[CLI_PATH_MAX_LEN];
    const char *const root = s_outcome.has_root                                                   ? s_outcome.root
                             : EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = found) ? found
                                                                                                  : NULL;
    if (root != NULL)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_PATH_MAX_LEN}) + 1ull;
        s_root = g_cli_cells->persistent_buf_alloc(size, &g_cli_controller_cells);
    }
    if (s_root != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_root,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_PATH_MAX_LEN}) + 1ull});
        EMBED_CALL(cli_toolchains_root.set_tree, CliToolchainsRootCfg, .root = s_root);
        EMBED_CALL(cli_plugins_root.set_tree, CliPluginsRootCfg, .root = s_root);
    }
    err_msg.at = 0ull;
    if (!EMBED_CALL(cli_model_door.open, CliModelCtx, .path = s_root, .err_msg = &err_msg))
    {
        EMBED_CALL(tree_err, ControllerTreeCtx, .err_msg = &err_msg);
    }
    if (s_root != NULL)
    {
        EMBED_CALL(cli_watch.start, CliWatchCfg, .root = s_root, .take = cli_controller_watch_taken, .context = NULL);
    }
}

void cli_controller_tree_end(void)
{
    // a language server orior_cli started ends with it: on Windows a child outlives its parent
    cli_reach_forget();
    cli_servers_stop_all();
    cli_debug_stop_all();
    cli_container_members_end();
    EMBED_CALL(cli_model_door.end, CliModelCtx, .at = 0u);
    g_cli_cells->persistent_buf_release(s_root, &g_cli_controller_cells);
    s_root = NULL;
}

const char *cli_controller_root(void)
{
    return s_root;
}

const CliOutcome *cli_controller_launch(void)
{
    return &s_outcome;
}
