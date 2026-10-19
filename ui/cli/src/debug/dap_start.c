// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap_start.c: a debug adapter begun: a program started for the session, or one listening at an address connected to;
// one that runs in orior is begun by dap_within.c
#include "../machine/net.h"
#include "dap_internal.h"

// how long an adapter listening at an address has to take the connection, in milliseconds
#define CLI_DAP_CONNECTING 5000u

// `size` bytes written whole to the adapter program's input, or to the adapter's connection
static int start_piped(void *sink, const void *bytes, unsigned long long size)
{
    return cli_spawn_written(((CliDap *)sink)->spawned.in, bytes, size);
}

static int start_sent(void *sink, const void *bytes, unsigned long long size)
{
    return cli_socket_written(&((CliDap *)sink)->socket, bytes, size);
}

// a piece of what the adapter program writes, or of what comes over the adapter's connection, read
static unsigned long long start_piped_read(void *source, char *piece, unsigned long long room)
{
    return cli_spawn_read(((CliDap *)source)->spawned.out, piece, room);
}

static unsigned long long start_received(void *source, char *piece, unsigned long long room)
{
    const long long count = cli_socket_some(&((CliDap *)source)->socket, piece, room);
    return (count > 0ll) ? (unsigned long long)count : 0ull;
}

// an adapter's reader made to read from `read`
static void start_reader(CliDap *dap, unsigned long long (*read)(void *source, char *piece, unsigned long long room))
{
    dap->reader.read = read;
    dap->reader.source = dap;
    dap->reader.start = 0ull;
    dap->reader.end = 0ull;
}

CliDap *cli_dap_started(const char *program, const char *const *args, unsigned int count, const char *folder,
                        const char *path, int ends_program, CliDapHeard heard, void *context, CliLine *said)
{
    CliDap *const dap = cli_dap_place_taken();
    if (dap == NULL)
    {
        cli_line_text(said, "no more debug adapters are held at once");
        return NULL;
    }
    dap->kind = CLI_DAP_PROGRAM;
    dap->ends_program = ends_program;
    dap->heard = heard;
    dap->context = context;
    dap->words.count = 0u;
    dap->words.used = 0u;
    dap->words.full = 0;
    cli_words_put(&dap->words, program);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_words_put(&dap->words, args[at]);
    }
    memor.cpy(&(MemoriaCfg){.dst = dap->path,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    const CliSetting settings[1] = {{"PATH", dap->path}};
    if (!cli_spawn_fed(&dap->spawned, dap->words.word, folder, settings, 1u, 1, said))
    {
        cli_lock_take(&g_cli_daps_lock);
        dap->taken = 0;
        cli_lock_give(&g_cli_daps_lock);
        return NULL;
    }
    dap->input = (CliLspWriter){start_piped, dap};
    start_reader(dap, start_piped_read);
    cli_dap_readers_started(dap, 1);
    return dap;
}

CliDap *cli_dap_connected(const char *host, unsigned int port, int ends_program, CliDapHeard heard, void *context,
                          CliLine *said)
{
    char address[CLI_PATH_LONGEST];
    CliLine address_line = {address, sizeof(address), 0ull};
    cli_line_text(&address_line, host);
    cli_line_character(&address_line, ':');
    cli_line_decimal(&address_line, port, 1u);
    cli_line_closed(&address_line);
    if (!cli_net_reach(address, "a debug adapter", said))
    {
        return NULL;
    }
    CliDap *const dap = cli_dap_place_taken();
    if (dap == NULL)
    {
        cli_line_text(said, "no more debug adapters are held at once");
        return NULL;
    }
    if (!cli_socket_connected_within(&dap->socket, host, port, CLI_DAP_CONNECTING, said))
    {
        cli_lock_take(&g_cli_daps_lock);
        dap->taken = 0;
        cli_lock_give(&g_cli_daps_lock);
        return NULL;
    }
    dap->kind = CLI_DAP_SOCKET;
    dap->ends_program = ends_program;
    dap->heard = heard;
    dap->context = context;
    dap->input = (CliLspWriter){start_sent, dap};
    start_reader(dap, start_received);
    cli_dap_readers_started(dap, 0);
    return dap;
}
