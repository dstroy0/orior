// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve.c: the tree open and everything that runs on it: where what the server tells goes, through the comms layer
// once the window listens, the server a part that speaks each event on the channel of its name and the window a part
// that listens on every channel, both parts of this process and the door taking neither key; the window opened to a
// command line run on its tree; the tree made the one open, the folders mounted beside one tree its own, another tree
// mounting its own, and a server answering for the tree it started in; and everything ended as the program ends
#include "../code/symbols.h"
#include "../comms/door.h"
#include "../debug/debug.h"
#include "../editor/files.h"
#include "../machine/reach.h"
#include "../toolchains/toolchains.h"
#include "serve_internal.h"

CliServeState g_cli_serve = {CLI_LOCK_INIT};

void cli_serve_begun(const char *root, const char *keeping)
{
    cli_lock_take(&g_cli_serve.lock);
    g_cli_serve.has_root = 0;
    if (root != NULL)
    {
        CliLine line = {g_cli_serve.root, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, root);
        g_cli_serve.has_root = (cli_line_closed(&line) != 0ull);
    }
    g_cli_serve.has_keeping = 0;
    if (keeping != NULL)
    {
        CliLine line = {g_cli_serve.keeping, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, keeping);
        g_cli_serve.has_keeping = (cli_line_closed(&line) != 0ull);
    }
    cli_lock_give(&g_cli_serve.lock);
    if (root != NULL)
    {
        cli_toolchains_set_tree(root);
    }
}

// a message the window hears on any channel handed to where what the server tells goes
static int serve_window_heard(void *context, const char *channel, const char *from, const char *body,
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

void cli_serve_tell_to(CliServeEmit emit, void *context)
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
    CliLine said = {quiet, sizeof(quiet), 0ull};
    // both are parts of this process alone, and the door takes neither key
    if (!cli_comms_register("orior", 0, 0u, NULL, 0u, s_every, 1u, server, &said) ||
        !cli_comms_register("window", 0, 0u, s_every, 1u, NULL, 0u, window, &said))
    {
        return;
    }
    unsigned int listener = 0u;
    if (!cli_comms_listen(window, "*", 0, serve_window_heard, NULL, &listener, &said))
    {
        return;
    }
    cli_lock_take(&g_cli_serve.lock);
    memor.cpy(&(MemoriaCfg){.dst = g_cli_serve.speaker, .src = server, .bytes = sizeof(server)});
    g_cli_serve.speaking = 1;
    cli_lock_give(&g_cli_serve.lock);
}

void cli_serve_emitted(const char *event, const char *body, unsigned long long size)
{
    cli_lock_take(&g_cli_serve.lock);
    const int speaking = g_cli_serve.speaking;
    char speaker[CLI_COMMS_KEY];
    memor.cpy(&(MemoriaCfg){.dst = speaker, .src = g_cli_serve.speaker, .bytes = sizeof(speaker)});
    const CliServeEmit emit = g_cli_serve.emit;
    void *const emit_context = g_cli_serve.emit_context;
    cli_lock_give(&g_cli_serve.lock);
    if (speaking)
    {
        char quiet[256];
        CliLine said = {quiet, sizeof(quiet), 0ull};
        unsigned long long heard = 0ull;
        (void)cli_comms_say(speaker, event, body, size, &heard, &said);
        return;
    }
    if (emit != NULL)
    {
        emit(emit_context, event, body, size);
    }
}

// what a server tells handed on as the server's own event
static void serve_servers_told(void *context, const char *event, const char *json, unsigned long long size)
{
    (void)context;
    cli_serve_emitted(event, json, size);
}

CliServersEmit cli_serve_emitter(const char *tree)
{
    CliServersEmit emit = {serve_servers_told, NULL, ""};
    CliLine line = {emit.tree, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, tree);
    (void)cli_line_closed(&line);
    return emit;
}

int cli_serve_open_door(char *address, CliLine *said)
{
    cli_lock_take(&g_cli_serve.lock);
    int open = g_cli_serve.has_door;
    if (!open)
    {
        open = cli_door_open(g_cli_serve.door, said);
        g_cli_serve.has_door = open;
    }
    if (open)
    {
        memor.cpy(&(MemoriaCfg){.dst = address, .src = g_cli_serve.door, .bytes = CLI_PATH_LONGEST});
    }
    cli_lock_give(&g_cli_serve.lock);
    return open;
}

int cli_serve_open_to_command_line(CliLine *said)
{
    char address[CLI_PATH_LONGEST];
    if (!cli_serve_open_door(address, said))
    {
        return 0;
    }
    cli_lock_take(&g_cli_serve.lock);
    if (!g_cli_serve.has_command_line)
    {
        static const char *const s_listen[1] = {"answer/*"};
        static const char *const s_speak[1] = {"command"};
        g_cli_serve.has_command_line =
            cli_comms_register_kept("command line", s_listen, 1u, s_speak, 1u, g_cli_serve.command_line, said);
    }
    const int held = g_cli_serve.has_command_line;
    char key[CLI_COMMS_KEY];
    memor.cpy(&(MemoriaCfg){.dst = key, .src = g_cli_serve.command_line, .bytes = sizeof(key)});
    char root[CLI_PATH_LONGEST];
    const int has_root = g_cli_serve.has_root;
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_serve.root, .bytes = sizeof(root)});
    cli_lock_give(&g_cli_serve.lock);
    return held && cli_reach_keep(has_root ? root : NULL, address, key, said);
}

int cli_serve_command_answer(const char *id, int done, const char *said_text, CliLine *said)
{
    cli_lock_take(&g_cli_serve.lock);
    const int speaking = g_cli_serve.speaking;
    char speaker[CLI_COMMS_KEY];
    memor.cpy(&(MemoriaCfg){.dst = speaker, .src = g_cli_serve.speaker, .bytes = sizeof(speaker)});
    cli_lock_give(&g_cli_serve.lock);
    if (!speaking)
    {
        cli_line_text(said, "the server speaks on no channel yet");
        return 0;
    }
    char channel[CLI_NAME_LONGEST];
    CliLine named = {channel, sizeof(channel), 0ull};
    cli_line_text(&named, "answer/");
    cli_line_text(&named, id);
    (void)cli_line_closed(&named);
    static char s_body[1u << 16u];
    static CliLock s_body_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_body_lock);
    CliLine body = {s_body, sizeof(s_body), 0ull};
    cli_line_text(&body, done ? "{\"done\":true,\"said\":" : "{\"done\":false,\"said\":");
    cli_json_quoted(&body, said_text, cellul.len(&(CatenaFinitaCfg){.src = said_text, .cap = CLI_CELLS_POOL}));
    cli_line_character(&body, '}');
    unsigned long long heard = 0ull;
    const int spoken = cli_comms_say(speaker, channel, s_body, body.at, &heard, said);
    cli_lock_give(&s_body_lock);
    return spoken;
}

int cli_serve_root(char *root, CliLine *said)
{
    if (!cli_serve_root_now(root))
    {
        cli_line_text(said, "no orior tree is open");
        return 0;
    }
    return 1;
}

int cli_serve_root_now(char *root)
{
    cli_lock_take(&g_cli_serve.lock);
    const int has = g_cli_serve.has_root;
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_serve.root, .bytes = CLI_PATH_LONGEST});
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

int cli_serve_root_set(const char *path, CliLine *out, CliLine *said)
{
    char full[CLI_PATH_LONGEST];
    if (!cli_tree_at(path, full, said))
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
    // the folders mounted beside one tree are its own; another tree mounts its own; a server answers for the tree it
    // started in, another tree starting its own as its files open
    if (moved)
    {
        cli_serve_kernels_stopped();
        cli_serve_databases_closed();
        cli_unmounted_all();
        cli_servers_let_go();
        cli_symbols_forget();
        // the pool made anew for the tree: its files' bytes over the fixed pool, where no cell is held across the move
        (void)cli_cells_made(cli_tree_bytes(full) + CLI_CELLS_POOL);
        cli_serve_watch();
        cli_files_warm(full);
        if (command_line)
        {
            char quiet[1024];
            CliLine ignored = {quiet, sizeof(quiet), 0ull};
            (void)cli_serve_open_to_command_line(&ignored);
        }
    }
    cli_json_quoted_lossy(out, (const unsigned char *)full,
                          cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}));
    return 1;
}
