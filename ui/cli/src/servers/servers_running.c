// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_running.c: the servers running, each its toolchain's id, its program's name, its process and the files it
// has open; the toolchains whose servers serve a language; servers stopped without waiting, each told to end on a
// thread of the servers', or every one stopped and waited on as the app closes; every server and what was kept of the
// tree let go as another tree opens; and each server told its settings changed, for it to ask for them again. A
// server taken out of the running ones takes the toolchains that failed with it, so one asked for after this starts
// afresh
#include "servers_internal.h"

// the servers taken out of the running ones, under the servers' lock and then their own
static CliLsp *s_taken[CLI_SERVERS_MOST];
static CliLock s_taken_lock = CLI_LOCK_INIT;

// A server to stop
typedef struct
{
    CliLsp *server;
} CliServersStopping;

// a server stopped and its hold given back, on a thread of the servers'
static void running_stopped(const void *job, unsigned long long size)
{
    CliServersStopping stopping = {NULL};
    memor.cpy(&(MemoriaCfg){
        .dst = &stopping, .src = job, .bytes = (sizeof(stopping) <= size) ? sizeof(stopping) : (size_t)size});
    cli_lsp_stopped(stopping.server);
    cli_lsp_given(stopping.server);
}

void cli_servers_stop_later(CliLsp *lsp)
{
    const CliServersStopping stopping = {lsp};
    if (!cli_servers_later(running_stopped, &stopping, sizeof(stopping)))
    {
        running_stopped(&stopping, sizeof(stopping));
    }
}

// every server taken out of the running ones into the taken, and the toolchains that failed and ended forgotten:
// their count; with the taken's lock held
static unsigned int running_take_all(void)
{
    unsigned int count = 0u;
    cli_lock_take(&g_cli_servers.lock);
    g_cli_servers.failed = 0u;
    g_cli_servers.restarted = 0u;
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        if (g_cli_servers.running[at].used)
        {
            g_cli_servers.running[at].used = 0;
            s_taken[count] = g_cli_servers.running[at].lsp;
            count += 1u;
        }
    }
    cli_lock_give(&g_cli_servers.lock);
    return count;
}

// a path a server has open written as a JSON string after those written
static void running_file_written(void *context, const char *path, const char *text, unsigned long long length)
{
    (void)text;
    (void)length;
    CliLine *const out = context;
    if ((out->at > 0ull) && (out->out[out->at - 1ull] != '['))
    {
        cli_line_character(out, ',');
    }
    cli_json_quoted_lossy(out, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
}

void cli_servers_running(CliLine *out)
{
    cli_lock_take(&g_cli_servers.lock);
    cli_line_character(out, '[');
    int written = 0;
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        const CliServerRunning *const one = &g_cli_servers.running[at];
        if (!one->used)
        {
            continue;
        }
        cli_line_text(out, written ? ",{\"tool\":" : "{\"tool\":");
        cli_json_quoted(out, one->spec.tool,
                        cellul.len(&(CatenaFinitaCfg){.src = one->spec.tool, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"program\":");
        const char *const program = (one->spec.program[0] != '\0') ? one->spec.program : one->spec.tool;
        cli_json_quoted(out, program, cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"pid\":");
        cli_line_decimal(out, cli_lsp_pid(one->lsp), 1u);
        cli_line_text(out, ",\"files\":[");
        cli_lsp_held_each(one->lsp, running_file_written, out);
        cli_line_text(out, "]}");
        written = 1;
    }
    cli_line_character(out, ']');
    cli_lock_give(&g_cli_servers.lock);
}

void cli_servers_tools_for(const char *const *languages, unsigned int count, CliLine *out)
{
    static CliServerSpec s_spec;
    static CliLock s_spec_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_spec_lock);
    cli_line_character(out, '[');
    int written = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cli_servers_spec_for(languages[at], &s_spec))
        {
            cli_line_text(out, written ? "," : "");
            cli_json_quoted(out, s_spec.tool,
                            cellul.len(&(CatenaFinitaCfg){.src = s_spec.tool, .cap = CLI_CELLS_POOL}));
            written = 1;
        }
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_spec_lock);
}

void cli_servers_stop(const char *const *tools, unsigned int count, CliLine *out)
{
    cli_lock_take(&s_taken_lock);
    unsigned int taken = 0u;
    cli_lock_take(&g_cli_servers.lock);
    for (unsigned int tool = 0u; tool < count; tool += 1u)
    {
        for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
        {
            CliServerRunning *const one = &g_cli_servers.running[at];
            if (one->used &&
                (cellul.eq(&(CatenaFinitaCfg){.src = one->spec.tool, .other = tools[tool], .cap = CLI_CELLS_POOL})))
            {
                one->used = 0;
                s_taken[taken] = one->lsp;
                taken += 1u;
            }
        }
    }
    cli_lock_give(&g_cli_servers.lock);
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        cli_lsp_held_each(s_taken[at], running_file_written, out);
    }
    cli_line_character(out, ']');
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        cli_servers_stop_later(s_taken[at]);
    }
    cli_lock_give(&s_taken_lock);
}

void cli_servers_stop_all(void)
{
    cli_lock_take(&s_taken_lock);
    const unsigned int taken = running_take_all();
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        cli_lsp_stopped(s_taken[at]);
        cli_lsp_given(s_taken[at]);
    }
    cli_lock_give(&s_taken_lock);
}

void cli_servers_let_go(void)
{
    cli_servers_check_let_go();
    cli_lock_take(&g_cli_servers_published_lock);
    cli_keyed_clear(&g_cli_servers_published);
    cli_lock_give(&g_cli_servers_published_lock);
    cli_lock_take(&g_cli_servers_texts_lock);
    cli_keyed_clear(&g_cli_servers_texts);
    cli_keyed_clear(&g_cli_servers_parses);
    cli_lock_give(&g_cli_servers_texts_lock);
    cli_lock_take(&g_cli_servers_checks_lock);
    cli_keyed_clear(&g_cli_servers_inspected);
    cli_lock_give(&g_cli_servers_checks_lock);
    cli_servers_inspect_let_go();
    cli_lock_take(&s_taken_lock);
    const unsigned int taken = running_take_all();
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        cli_servers_stop_later(s_taken[at]);
    }
    cli_lock_give(&s_taken_lock);
}

void cli_servers_settings_changed(void)
{
    cli_lock_take(&s_taken_lock);
    unsigned int count = 0u;
    cli_lock_take(&g_cli_servers.lock);
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        if (g_cli_servers.running[at].used)
        {
            s_taken[count] = g_cli_servers.running[at].lsp;
            cli_lsp_held(s_taken[count]);
            count += 1u;
        }
    }
    cli_lock_give(&g_cli_servers.lock);
    static const char s_settings[] = "{\"settings\":null}";
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char quiet[256];
        CliLine ignored = {quiet, sizeof(quiet), 0ull};
        (void)cli_lsp_notified(s_taken[at], "workspace/didChangeConfiguration", s_settings, sizeof(s_settings) - 1u,
                               &ignored);
        cli_lsp_given(s_taken[at]);
    }
    cli_lock_give(&s_taken_lock);
}
