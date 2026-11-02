// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_tunnel.c: a tunnel to a database's server through ssh: the system's ssh, or the one ORIOR_SSH names, with the
// reader's config, keys and agent, forwarding a port of this machine's loopback, waited for until the port answers
#include "../runs/runner.h"
#include "../toolchains/toolchains.h"
#include "db_internal.h"

int cli_db_tunnel_open(const CliDbHostCtx *args)
{
    unsigned int port = args->port;
    static CliWords s_words;
    static char s_path[CLI_PATH_TEXT];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned int local = 0u;
    memor.set(&(MemoriaCfg){.dst = args->tunnel, .val = 0, .bytes = sizeof(*args->tunnel)});
    if (!EMBED_CALL(cli_err_msg_port_internal.port_free, CliErrMsgPortCtx, .port_pointer = &local, .err_msg = &err_msg))
    {
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure, .message = why);
        return 0;
    }
    char forward[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea forward_line = {forward, sizeof(forward), 0ull};
    mmgr_line_put(&forward_line, "127.0.0.1:");
    mmgr_line_decimal(&forward_line, local, 1u);
    mmgr_line_ch(&forward_line, ':');
    mmgr_line_put(&forward_line, args->host);
    mmgr_line_ch(&forward_line, ':');
    mmgr_line_decimal(&forward_line, port, 1u);
    mmgr_line_finish(&forward_line);
    const char *const words[11] = {"-N",
                                   "-L",
                                   forward,
                                   "-o",
                                   "ExitOnForwardFailure=yes",
                                   "-o",
                                   "BatchMode=yes",
                                   "-o",
                                   "ServerAliveInterval=15",
                                   "-o",
                                   "ServerAliveCountMax=3"};
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    if (!EMBED_CALL(cli_runner_variable.program_words, CliRunnerVariableCfg, .variable = "ORIOR_SSH",
                    .words = &s_words))
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = "ssh");
    }
    for (unsigned int at = 0u; at < 11u; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = words[at]);
    }
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->through);
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path, .size = sizeof(s_path));
    const CliSetting settings[1] = {{"PATH", s_path}};
    const int started = EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &args->tunnel->child,
                                   .words = s_words.word, .folder = NULL, .settings = settings, .count = 1u,
                                   .detached = 1, .err_msg = &err_msg);
    if (!started)
    {
        char message[CLI_ERR_MSG_MAX_LEN + 64u];
        VerbaLinea line = {message, sizeof(message), 0ull};
        mmgr_line_put(&line, "ssh, which the tunnel goes through: ");
        mmgr_line_put_n(&line, err_msg.out, err_msg.at);
        mmgr_line_finish(&line);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure, .message = message);
        return 0;
    }
    const unsigned long long until = cli_clock_milliseconds() + CLI_DB_WAIT;
    for (;;)
    {
        long long code = 0ll;
        if (EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx, .spawned = &args->tunnel->child,
                       .code = &code) != 0)
        {
            // what ssh printed to its errors as it ended, trimmed
            char errors[CLI_DB_FIELD];
            unsigned long long used = 0ull;
            for (unsigned long long got = 1ull; (got > 0ull) && (used < sizeof(errors));)
            {
                got = EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                                 .pipe_handle = args->tunnel->child.err, .piece = errors + used,
                                 .room = sizeof(errors) - used);
                used += got;
            }
            while ((used > 0u) &&
                   (cellul.chr(&(CatenaFinitaCfg){
                        .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = errors[used - 1u]}) != NULL))
            {
                used -= 1u;
            }
            EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &args->tunnel->child);
            memor.set(&(MemoriaCfg){.dst = args->failure, .val = 0, .bytes = sizeof(*args->failure)});
            VerbaLinea message = {args->failure->message, sizeof(args->failure->message), 0ull};
            mmgr_line_put(&message, "ssh ");
            mmgr_line_put(&message, args->through);
            mmgr_line_put(&message, " ended: ");
            unsigned long long start = 0ull;
            while ((start < used) && (cellul.chr(&(CatenaFinitaCfg){
                                          .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = errors[start]}) != NULL))
            {
                start += 1u;
            }
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &message,
                       .text_bytes = (const unsigned char *)errors + start, .length = used - start);
            mmgr_line_finish(&message);
            memor.cpy(&(MemoriaCfg){
                .dst = args->failure->hint,
                .src = "The tunnel signs in with the keys ssh's agent has and the reader's ~/.ssh/config, "
                       "as the terminal's ssh does; a machine that prompts for a password needs a key put on "
                       "it.",
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){
                        .src = "The tunnel signs in with the keys ssh's agent has and the reader's ~/.ssh/config, "
                               "as the terminal's ssh does; a machine that prompts for a password needs a key put on "
                               "it.",
                        .cap = CLI_CELLS_POOL}) +
                    1u});
            return 0;
        }
        CliSocket probe;
        if (EMBED_CALL(cli_err_msg_internal.socket_connected_within, CliErrMsgCtx, .sock_out = &probe,
                       .host = "127.0.0.1", .port = local, .milliseconds = 200u, .err_msg = &err_msg))
        {
            EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &probe);
            args->tunnel->port = local;
            args->tunnel->open = 1;
            return 1;
        }
        err_msg.at = 0ull;
        if (cli_clock_milliseconds() > until)
        {
            EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &args->tunnel->child);
            (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &args->tunnel->child);
            EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &args->tunnel->child);
            char message[CLI_NAME_MAX_LEN + 64u];
            VerbaLinea line = {message, sizeof(message), 0ull};
            mmgr_line_put(&line, "ssh ");
            mmgr_line_put(&line, args->through);
            mmgr_line_put(&line, " made no tunnel in ");
            mmgr_line_decimal(&line, CLI_DB_WAIT / 1000ull, 1u);
            mmgr_line_put(&line, " s");
            mmgr_line_finish(&line);
            EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure, .message = message);
            return 0;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 100u);
    }
}

int cli_db_tunnel_alive(const CliDbHostCtx *args)
{
    long long code = 0ll;
    return args->tunnel->open && (EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx,
                                             .spawned = &args->tunnel->child, .code = &code) == 0);
}

void cli_db_tunnel_close(const CliDbHostCtx *args)
{
    if (args->tunnel->open)
    {
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &args->tunnel->child);
        (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &args->tunnel->child);
        EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &args->tunnel->child);
    }
    memor.set(&(MemoriaCfg){.dst = args->tunnel, .val = 0, .bytes = sizeof(*args->tunnel)});
}
