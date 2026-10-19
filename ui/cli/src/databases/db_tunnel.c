// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_tunnel.c: a tunnel to a database's server through ssh: the system's ssh, or the one ORIOR_SSH names, with the
// reader's config, keys and agent, forwarding a port of this machine's loopback, waited for until the port answers
#include "../runs/runner.h"
#include "../toolchains/toolchains.h"
#include "db_internal.h"

int cli_db_tunnel_open(CliDbTunnel *tunnel, const char *through, const char *host, unsigned int port,
                       CliDbFailure *failure)
{
    static CliWords s_words;
    static char s_path[CLI_PATH_TEXT];
    static CliLock s_lock = CLI_LOCK_INIT;
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned int local = 0u;
    memor.set(&(MemoriaCfg){.dst = tunnel, .val = 0, .bytes = sizeof(*tunnel)});
    if (!cli_port_free(&local, &said))
    {
        cli_line_closed(&said);
        cli_db_failure_said(failure, why);
        return 0;
    }
    char forward[CLI_NAME_LONGEST * 2u];
    CliLine forward_line = {forward, sizeof(forward), 0ull};
    cli_line_text(&forward_line, "127.0.0.1:");
    cli_line_decimal(&forward_line, local, 1u);
    cli_line_character(&forward_line, ':');
    cli_line_text(&forward_line, host);
    cli_line_character(&forward_line, ':');
    cli_line_decimal(&forward_line, port, 1u);
    cli_line_closed(&forward_line);
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
    cli_lock_take(&s_lock);
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    if (!cli_runner_program_words("ORIOR_SSH", &s_words))
    {
        cli_words_put(&s_words, "ssh");
    }
    for (unsigned int at = 0u; at < 11u; at += 1u)
    {
        cli_words_put(&s_words, words[at]);
    }
    cli_words_put(&s_words, through);
    cli_toolchains_run_path(s_path, sizeof(s_path));
    const CliSetting settings[1] = {{"PATH", s_path}};
    const int started = cli_spawn(&tunnel->child, s_words.word, NULL, settings, 1u, 1, &said);
    cli_lock_give(&s_lock);
    if (!started)
    {
        char message[CLI_SAID_LONGEST + 64u];
        CliLine line = {message, sizeof(message), 0ull};
        cli_line_text(&line, "ssh, which the tunnel goes through: ");
        cli_line_counted(&line, said.out, said.at);
        cli_line_closed(&line);
        cli_db_failure_said(failure, message);
        return 0;
    }
    const unsigned long long until = cli_clock_milliseconds() + CLI_DB_WAIT;
    for (;;)
    {
        long long code = 0ll;
        if (cli_spawn_poll(&tunnel->child, &code) != 0)
        {
            // what ssh said to its errors as it ended, trimmed
            char errors[CLI_DB_FIELD];
            unsigned long long used = 0ull;
            for (unsigned long long got = 1ull; (got > 0ull) && (used < sizeof(errors));)
            {
                got = cli_spawn_read(tunnel->child.err, errors + used, sizeof(errors) - used);
                used += got;
            }
            while ((used > 0u) &&
                   (cellul.chr(&(CatenaFinitaCfg){
                        .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = errors[used - 1u]}) != NULL))
            {
                used -= 1u;
            }
            cli_spawn_closed(&tunnel->child);
            memor.set(&(MemoriaCfg){.dst = failure, .val = 0, .bytes = sizeof(*failure)});
            CliLine message = {failure->message, sizeof(failure->message), 0ull};
            cli_line_text(&message, "ssh ");
            cli_line_text(&message, through);
            cli_line_text(&message, " ended: ");
            unsigned long long start = 0ull;
            while ((start < used) && (cellul.chr(&(CatenaFinitaCfg){
                                          .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = errors[start]}) != NULL))
            {
                start += 1u;
            }
            cli_lossy(&message, (const unsigned char *)errors + start, used - start);
            cli_line_closed(&message);
            memor.cpy(&(MemoriaCfg){
                .dst = failure->hint,
                .src = "The tunnel signs in with the keys ssh's agent holds and the reader's ~/.ssh/config, "
                       "as the terminal's ssh does; a machine that asks for a password needs a key put on "
                       "it.",
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){
                        .src = "The tunnel signs in with the keys ssh's agent holds and the reader's ~/.ssh/config, "
                               "as the terminal's ssh does; a machine that asks for a password needs a key put on "
                               "it.",
                        .cap = CLI_CELLS_POOL}) +
                    1u});
            return 0;
        }
        CliSocket probe;
        if (cli_socket_connected_within(&probe, "127.0.0.1", local, 200u, &said))
        {
            cli_socket_closed(&probe);
            tunnel->port = local;
            tunnel->open = 1;
            return 1;
        }
        said.at = 0ull;
        if (cli_clock_milliseconds() > until)
        {
            cli_spawn_killed(&tunnel->child);
            (void)cli_spawn_wait(&tunnel->child);
            cli_spawn_closed(&tunnel->child);
            char message[CLI_NAME_LONGEST + 64u];
            CliLine line = {message, sizeof(message), 0ull};
            cli_line_text(&line, "ssh ");
            cli_line_text(&line, through);
            cli_line_text(&line, " made no tunnel in ");
            cli_line_decimal(&line, CLI_DB_WAIT / 1000ull, 1u);
            cli_line_text(&line, " s");
            cli_line_closed(&line);
            cli_db_failure_said(failure, message);
            return 0;
        }
        query_paused(100u);
    }
}

int cli_db_tunnel_alive(CliDbTunnel *tunnel)
{
    long long code = 0ll;
    return tunnel->open && (cli_spawn_poll(&tunnel->child, &code) == 0);
}

void cli_db_tunnel_closed(CliDbTunnel *tunnel)
{
    if (tunnel->open)
    {
        cli_spawn_killed(&tunnel->child);
        (void)cli_spawn_wait(&tunnel->child);
        cli_spawn_closed(&tunnel->child);
    }
    memor.set(&(MemoriaCfg){.dst = tunnel, .val = 0, .bytes = sizeof(*tunnel)});
}
