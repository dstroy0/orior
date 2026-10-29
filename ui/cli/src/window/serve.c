// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve.c: the tree open and everything that runs on it: where what the server emits goes, through the comms layer
// once the window listens, the server a part that sends each event on the channel of its name and the window a part
// that listens on every channel, both parts of this process and the door taking neither key; the window opened to a
// command line run on its tree; the tree made the one open, the folders mounted beside one tree its own, another tree
// mounting its own, and a server answering for the tree it started in; and everything ended as the program ends
#include "../code/symbols.h"
#include "../comms/door.h"
#include "../debug/debug.h"
#include "../editor/files.h"
#include "../machine/reach.h"
#include "../plugins/plugins.h"
#include "../toolchains/toolchains.h"
#include "serve_internal.h"

CliServeState g_cli_serve = {CLI_LOCK_INIT};

void cli_serve_begun(const char *root, const char *detached_dir)
{
    cli_lock_take(&g_cli_serve.lock);
    g_cli_serve.has_root = 0;
    if (root != NULL)
    {
        VerbaLinea line = {g_cli_serve.root, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, root);
        g_cli_serve.has_root = (mmgr_line_finish(&line) != 0ull);
    }
    g_cli_serve.has_detached_dir = 0;
    if (detached_dir != NULL)
    {
        VerbaLinea line = {g_cli_serve.detached_dir, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, detached_dir);
        g_cli_serve.has_detached_dir = (mmgr_line_finish(&line) != 0ull);
    }
    cli_lock_give(&g_cli_serve.lock);
    if (root != NULL)
    {
        cli_toolchains_set_tree(root);
        cli_plugins_set_tree(root);
    }
}

// a message the window receives on any channel handed to where what the server emits goes
static int serve_window_recv(void *context, const char *channel, const char *from, const char *body,
                             unsigned long long size)
{
    (void)context;
    (void)from;
    cli_lock_take(&g_cli_serve.lock);
    const CliServeEmit emit = g_cli_serve.emit;
    void *const emit_context = g_cli_serve.emit_context;
    cli_lock_give(&g_cli_serve.lock);
    if (emit != NULL)
    {
        emit(emit_context, channel, body, size);
    }
    return 1;
}

void cli_serve_emit_set(CliServeEmit emit, void *context)
{
    cli_lock_take(&g_cli_serve.lock);
    g_cli_serve.emit = emit;
    g_cli_serve.emit_context = context;
    g_cli_serve.has_emit = 1;
    cli_lock_give(&g_cli_serve.lock);
    static const char *const s_every[1] = {"*"};
    char server[CLI_COMMS_KEY];
    char window[CLI_COMMS_KEY];
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    // both are parts of this process alone, and the door takes neither key
    if (!cli_comms_register("orior", 0, 0u, NULL, 0u, s_every, 1u, server, &err_msg) ||
        !cli_comms_register("window", 0, 0u, s_every, 1u, NULL, 0u, window, &err_msg))
    {
        return;
    }
    unsigned int listener = 0u;
    if (!cli_comms_listen(window, "*", 0, serve_window_recv, NULL, &listener, &err_msg))
    {
        return;
    }
    cli_lock_take(&g_cli_serve.lock);
    memor.cpy(&(MemoriaCfg){.dst = g_cli_serve.sender, .src = server, .bytes = sizeof(server)});
    g_cli_serve.sending = 1;
    cli_lock_give(&g_cli_serve.lock);
}

void cli_serve_emitted(const char *event, const char *body, unsigned long long size)
{
    cli_lock_take(&g_cli_serve.lock);
    const int sending = g_cli_serve.sending;
    char sender[CLI_COMMS_KEY];
    memor.cpy(&(MemoriaCfg){.dst = sender, .src = g_cli_serve.sender, .bytes = sizeof(sender)});
    const CliServeEmit emit = g_cli_serve.emit;
    void *const emit_context = g_cli_serve.emit_context;
    cli_lock_give(&g_cli_serve.lock);
    if (sending)
    {
        char quiet[256];
        VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
        unsigned long long delivered = 0ull;
        (void)cli_comms_send(sender, event, body, size, &delivered, &err_msg);
        return;
    }
    if (emit != NULL)
    {
        emit(emit_context, event, body, size);
    }
}

// what a server emits handed on as the server's own event
static void serve_servers_event(void *context, const char *event, const char *json, unsigned long long size)
{
    (void)context;
    cli_serve_emitted(event, json, size);
}

CliServersSink cli_serve_emitter(const char *tree)
{
    CliServersSink sink = {serve_servers_event, NULL, ""};
    VerbaLinea line = {sink.tree, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, tree);
    (void)mmgr_line_finish(&line);
    return sink;
}

int cli_serve_open_door(char *address, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_serve.lock);
    int open = g_cli_serve.has_door;
    if (!open)
    {
        open = cli_door_open(g_cli_serve.door, err_msg);
        g_cli_serve.has_door = open;
    }
    if (open)
    {
        memor.cpy(&(MemoriaCfg){.dst = address, .src = g_cli_serve.door, .bytes = CLI_PATH_MAX_LEN});
    }
    cli_lock_give(&g_cli_serve.lock);
    return open;
}

int cli_serve_open_to_command_line(VerbaLinea *err_msg)
{
    char address[CLI_PATH_MAX_LEN];
    if (!cli_serve_open_door(address, err_msg))
    {
        return 0;
    }
    cli_lock_take(&g_cli_serve.lock);
    if (!g_cli_serve.has_command_line)
    {
        static const char *const s_listen[1] = {"answer/*"};
        static const char *const s_send_grant[1] = {"command"};
        g_cli_serve.has_command_line = cli_comms_register_persistent("command line", s_listen, 1u, s_send_grant, 1u,
                                                                     g_cli_serve.command_line, err_msg);
    }
    const int registered = g_cli_serve.has_command_line;
    char key[CLI_COMMS_KEY];
    memor.cpy(&(MemoriaCfg){.dst = key, .src = g_cli_serve.command_line, .bytes = sizeof(key)});
    char root[CLI_PATH_MAX_LEN];
    const int has_root = g_cli_serve.has_root;
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_serve.root, .bytes = sizeof(root)});
    cli_lock_give(&g_cli_serve.lock);
    return registered && cli_reach_store(has_root ? root : NULL, address, key, err_msg);
}

int cli_serve_command_answer(const char *id, int done, const char *answer_text, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_serve.lock);
    const int sending = g_cli_serve.sending;
    char sender[CLI_COMMS_KEY];
    memor.cpy(&(MemoriaCfg){.dst = sender, .src = g_cli_serve.sender, .bytes = sizeof(sender)});
    cli_lock_give(&g_cli_serve.lock);
    if (!sending)
    {
        mmgr_line_put(err_msg, "the server sends on no channel yet");
        return 0;
    }
    char channel[CLI_NAME_MAX_LEN];
    VerbaLinea named = {channel, sizeof(channel), 0ull};
    mmgr_line_put(&named, "answer/");
    mmgr_line_put(&named, id);
    (void)mmgr_line_finish(&named);
    static char s_body[1u << 16u];
    static CliLock s_body_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_body_lock);
    VerbaLinea body = {s_body, sizeof(s_body), 0ull};
    mmgr_line_put(&body, done ? "{\"done\":true,\"message\":" : "{\"done\":false,\"message\":");
    cli_json_quoted(&body, answer_text, cellul.len(&(CatenaFinitaCfg){.src = answer_text, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&body, '}');
    unsigned long long delivered = 0ull;
    const int sent = cli_comms_send(sender, channel, s_body, body.at, &delivered, err_msg);
    cli_lock_give(&s_body_lock);
    return sent;
}

int cli_serve_root(char *root, VerbaLinea *err_msg)
{
    if (!cli_serve_root_now(root))
    {
        mmgr_line_put(err_msg, "no orior tree is open");
        return 0;
    }
    return 1;
}

int cli_serve_root_now(char *root)
{
    cli_lock_take(&g_cli_serve.lock);
    const int has = g_cli_serve.has_root;
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_serve.root, .bytes = CLI_PATH_MAX_LEN});
    cli_lock_give(&g_cli_serve.lock);
    return has;
}

void cli_serve_stop_all(void)
{
    cli_servers_stop_all();
    cli_debug_stop_all();
    cli_serve_kernels_stopped();
    cli_serve_databases_closed();
}

int cli_serve_root_set(const char *path, VerbaLinea *out, VerbaLinea *err_msg)
{
    char full[CLI_PATH_MAX_LEN];
    if (!cli_tree_at(path, full, err_msg))
    {
        return 0;
    }
    cli_lock_take(&g_cli_serve.lock);
    const int moved = !g_cli_serve.has_root ||
                      (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_serve.root, .other = full, .cap = CLI_CELLS_POOL}));
    memor.cpy(&(MemoriaCfg){.dst = g_cli_serve.root, .src = full, .bytes = sizeof(full)});
    g_cli_serve.has_root = 1;
    const int command_line = g_cli_serve.has_command_line;
    cli_lock_give(&g_cli_serve.lock);
    cli_toolchains_set_tree(full);
    cli_plugins_set_tree(full);
    // the folders mounted beside one tree are its own; another tree mounts its own; a server answers for the tree it
    // started in, another tree starting its own as its files open
    if (moved)
    {
        cli_serve_kernels_stopped();
        cli_serve_databases_closed();
        cli_unmounted_all();
        cli_servers_let_go();
        cli_symbols_forget();
        // the pool made anew for the tree: its files' bytes over the fixed pool, where no cell is allocated across the
        // move
        (void)cli_cells_made(cli_tree_bytes(full) + CLI_CELLS_POOL);
        cli_serve_watch();
        cli_files_warm(full);
        if (command_line)
        {
            char quiet[1024];
            VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
            (void)cli_serve_open_to_command_line(&ignored);
        }
    }
    cli_json_quoted_lossy(out, (const unsigned char *)full,
                          cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}));
    return 1;
}
