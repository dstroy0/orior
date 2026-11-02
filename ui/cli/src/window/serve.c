// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve.c: the tree open and everything that runs on it: where what the server emits goes, through the comms layer
// once the window listens, the server a part that sends each event on the channel of its name and the window a part
// that listens on every channel, both parts of this process and the door taking neither key; the window opened to a
// command line run on its tree; the tree made the one open, the folders mounted beside one tree its own, another tree
// mounting its own, and a server answering for the tree it started in; and everything ended as the program ends
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../comms/door.h"
#include "../debug/debug.h"
#include "../editor/files.h"
#include "../machine/reach.h"
#include "../plugins/plugins.h"
#include "../toolchains/toolchains.h"
#include "serve_internal.h"

CliServeState g_cli_serve = {CLI_LOCK_INIT};

void cli_serve_begin(const CliServeRootCfg *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    g_cli_serve.has_root = 0;
    if (root != NULL)
    {
        VerbaLinea line = {g_cli_serve.root, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, root);
        g_cli_serve.has_root = (mmgr_line_finish(&line) != 0ull);
    }
    g_cli_serve.has_detached_dir = 0;
    if (args->detached_dir != NULL)
    {
        VerbaLinea line = {g_cli_serve.detached_dir, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, args->detached_dir);
        g_cli_serve.has_detached_dir = (mmgr_line_finish(&line) != 0ull);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    if (root != NULL)
    {
        EMBED_CALL(cli_toolchains_root.set_tree, CliToolchainsRootCfg, .root = root);
        EMBED_CALL(cli_plugins_root.set_tree, CliPluginsRootCfg, .root = root);
    }
}

// a message the window receives on any channel handed to where what the server emits goes
static int serve_window_recv(const CliCommsRecvCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    const CliServeEmit emit = g_cli_serve.emit;
    void *const emit_context = g_cli_serve.emit_context;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    if (emit != NULL)
    {
        emit(emit_context, args->channel, args->body, args->size);
    }
    return 1;
}

void cli_serve_emit_set(const CliServeEmitCfg *args)
{
    CliServeEmit emit = args->emit;
    void *context = args->context;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    g_cli_serve.emit = emit;
    g_cli_serve.emit_context = context;
    g_cli_serve.has_emit = 1;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    static const char *const s_every[1] = {"*"};
    char server[CLI_COMMS_KEY];
    char window[CLI_COMMS_KEY];
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    // both are parts of this process alone, and the door takes neither key
    if (!EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = "orior", .has_process = 0, .process = 0u,
                    .listen = NULL, .listens = 0u, .send_grant = s_every, .send_grants = 1u, .key_out = server,
                    .err_msg = &err_msg) ||
        !EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = "window", .has_process = 0, .process = 0u,
                    .listen = s_every, .listens = 1u, .send_grant = NULL, .send_grants = 0u, .key_out = window,
                    .err_msg = &err_msg))
    {
        return;
    }
    unsigned int listener = 0u;
    if (!EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = window, .channel = "*", .queued = 0,
                    .recv = serve_window_recv, .context = NULL, .listener = &listener, .err_msg = &err_msg))
    {
        return;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    memor.cpy(&(MemoriaCfg){.dst = g_cli_serve.sender, .src = server, .bytes = sizeof(server)});
    g_cli_serve.sending = 1;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
}

void cli_serve_on_emit(const CliServeEventCtx *args)
{
    const char *body = args->body;
    unsigned long long size = args->size;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    const int sending = g_cli_serve.sending;
    char sender[CLI_COMMS_KEY];
    memor.cpy(&(MemoriaCfg){.dst = sender, .src = g_cli_serve.sender, .bytes = sizeof(sender)});
    const CliServeEmit emit = g_cli_serve.emit;
    void *const emit_context = g_cli_serve.emit_context;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    if (sending)
    {
        char quiet[256];
        VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
        unsigned long long delivered = 0ull;
        (void)EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = sender,
                         .channel = args->event, .body = body, .size = size, .delivered = &delivered,
                         .err_msg = &err_msg);
        return;
    }
    if (emit != NULL)
    {
        emit(emit_context, args->event, body, size);
    }
}

// what a server emits handed on as the server's own event
static void serve_servers_event(const CliEachEventJsonSizeCfg *args)
{
    const char *event = args->event;
    unsigned long long size = args->size;
    EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = event, .body = args->json, .size = size);
}

CliServersSink cli_serve_emitter(const CliServeCtx *args)
{
    CliServersSink sink = {serve_servers_event, NULL, ""};
    VerbaLinea line = {sink.tree, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->tree);
    (void)mmgr_line_finish(&line);
    return sink;
}

int cli_serve_open_door(const CliServeErrMsgAddressCfg *args)
{
    char *address = args->address;
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    int open = g_cli_serve.has_door;
    if (!open)
    {
        open = EMBED_CALL(cli_door_address.open, CliDoorAddressCfg, .address = g_cli_serve.door, .err_msg = err_msg);
        g_cli_serve.has_door = open;
    }
    if (open)
    {
        memor.cpy(&(MemoriaCfg){.dst = address, .src = g_cli_serve.door, .bytes = CLI_PATH_MAX_LEN});
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    return open;
}

int cli_serve_open_to_command_line(const CliServeErrMsgAddressCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char address[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_address.open_door, CliServeErrMsgAddressCfg, .address = address,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    if (!g_cli_serve.has_command_line)
    {
        static const char *const s_listen[1] = {"answer/*"};
        static const char *const s_send_grant[1] = {"command"};
        g_cli_serve.has_command_line =
            EMBED_CALL(cli_comms_name.register_persistent, CliCommsNameCfg, .name = "command line", .listen = s_listen,
                       .listens = 1u, .send_grant = s_send_grant, .send_grants = 1u,
                       .key_out = g_cli_serve.command_line, .err_msg = err_msg);
    }
    const int registered = g_cli_serve.has_command_line;
    char key[CLI_COMMS_KEY];
    memor.cpy(&(MemoriaCfg){.dst = key, .src = g_cli_serve.command_line, .bytes = sizeof(key)});
    char root[CLI_PATH_MAX_LEN];
    const int has_root = g_cli_serve.has_root;
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_serve.root, .bytes = sizeof(root)});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    return registered && EMBED_CALL(cli_reach.store, CliReachCfg, .root = has_root ? root : NULL, .address = address,
                                    .key = key, .err_msg = err_msg);
}

int cli_serve_command_answer(const CliServeIdCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    const int sending = g_cli_serve.sending;
    char sender[CLI_COMMS_KEY];
    memor.cpy(&(MemoriaCfg){.dst = sender, .src = g_cli_serve.sender, .bytes = sizeof(sender)});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    if (!sending)
    {
        mmgr_line_put(err_msg, "the server sends on no channel yet");
        return 0;
    }
    char channel[CLI_NAME_MAX_LEN];
    VerbaLinea named = {channel, sizeof(channel), 0ull};
    mmgr_line_put(&named, "answer/");
    mmgr_line_put(&named, args->id);
    (void)mmgr_line_finish(&named);
    static char s_body[1u << 16u];
    static CliLock s_body_lock = CLI_LOCK_INIT;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_body_lock);
    VerbaLinea body = {s_body, sizeof(s_body), 0ull};
    mmgr_line_put(&body, args->done ? "{\"done\":true,\"message\":" : "{\"done\":false,\"message\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &body, .text = args->answer_text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->answer_text, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&body, '}');
    unsigned long long delivered = 0ull;
    const int sent =
        EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = sender, .channel = channel,
                   .body = s_body, .size = body.at, .delivered = &delivered, .err_msg = err_msg);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_body_lock);
    return sent;
}

int cli_serve_root(const CliServeErrMsgRootCfg *args)
{
    if (!cli_serve_err_msg_root.root_now(args))
    {
        mmgr_line_put(args->err_msg, "no orior tree is open");
        return 0;
    }
    return 1;
}

int cli_serve_root_now(const CliServeErrMsgRootCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    const int has = g_cli_serve.has_root;
    memor.cpy(&(MemoriaCfg){.dst = args->root_out, .src = g_cli_serve.root, .bytes = CLI_PATH_MAX_LEN});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    return has;
}

void cli_serve_stop_all(void)
{
    cli_servers_stop_all();
    cli_debug_stop_all();
    cli_serve_kernels_stop();
    cli_serve_databases_close();
}

int cli_serve_root_set(const CliServeErrMsgPathOutCtx *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_err_msg_full_internal.tree_at, CliPathErrMsgFullCtx, .path = args->path, .full = full,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    const int moved = !g_cli_serve.has_root ||
                      (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_serve.root, .other = full, .cap = CLI_CELLS_POOL}));
    memor.cpy(&(MemoriaCfg){.dst = g_cli_serve.root, .src = full, .bytes = sizeof(full)});
    g_cli_serve.has_root = 1;
    const int command_line = g_cli_serve.has_command_line;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    EMBED_CALL(cli_toolchains_root.set_tree, CliToolchainsRootCfg, .root = full);
    EMBED_CALL(cli_plugins_root.set_tree, CliPluginsRootCfg, .root = full);
    // the folders mounted beside one tree are its own; another tree mounts its own; a server answers for the tree it
    // started in, another tree starting its own as its files open
    if (moved)
    {
        cli_serve_kernels_stop();
        cli_serve_databases_close();
        cli_unmounted_all();
        cli_servers_let_go();
        cli_tree_forgotten();
        // the pool made anew for the tree: its files' bytes over the fixed pool, where no cell is allocated across the
        // move
        (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg,
                         .bytes = EMBED_CALL(cli_path_root_internal.tree_bytes, CliPathRootCtx, .root = full) +
                                  CLI_CELLS_POOL);
        cli_serve_watch();
        EMBED_CALL(cli_files_root_paths.warm, CliFilesRootPathsCfg, .root = full);
        if (command_line)
        {
            char quiet[1024];
            VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
            (void)EMBED_CALL(cli_serve_err_msg_address.open_to_command_line, CliServeErrMsgAddressCfg,
                             .err_msg = &ignored);
        }
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)full,
               .length = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}));
    return 1;
}
