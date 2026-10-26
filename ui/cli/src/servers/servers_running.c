// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_running.c: the servers running, each its toolchain's id, its program's name, its process and the files it
// has open; the toolchains whose servers serve a language; servers stopped without waiting, each instructed to end on a
// thread of the servers', or every one stopped and waited on as the app closes; every server and what was stored of the
// tree let go as another tree opens; and each server notified its settings changed, for it to request them again. A
// server taken out of the running ones takes the toolchains that failed with it, so one requested after this starts
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

// a server stopped and its reference given back, on a thread of the servers'
static void running_stopped(const void *job, unsigned long long size)
{
    CliServersStopping stopping = {NULL};
    memor.cpy(&(MemoriaCfg){
        .dst = &stopping, .src = job, .bytes = (sizeof(stopping) <= size) ? sizeof(stopping) : (size_t)size});
    cli_lsp_stopped(stopping.server);
    cli_lsp_unref(stopping.server);
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
// their count; with the taken's lock acquired
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
    VerbaLinea *const out = context;
    if ((out->at > 0ull) && (out->out[out->at - 1ull] != '['))
    {
        mmgr_line_ch(out, ',');
    }
    cli_json_quoted_lossy(out, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
}

unsigned int cli_servers_listed(CliServerListed *servers, unsigned int room)
{
    unsigned int count = 0u;
    cli_lock_take(&g_cli_servers.lock);
    for (unsigned int at = 0u; (at < CLI_SERVERS_MOST) && (count < room); at += 1u)
    {
        const CliServerRunning *const one = &g_cli_servers.running[at];
        if (!one->used)
        {
            continue;
        }
        const char *const program = (one->spec.program[0] != '\0') ? one->spec.program : one->spec.tool;
        VerbaLinea tool = {servers[count].tool, CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&tool, one->spec.tool);
        (void)mmgr_line_finish(&tool);
        VerbaLinea named = {servers[count].program, CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&named, program);
        (void)mmgr_line_finish(&named);
        // a process id fits the 32 bits the system gives it
        servers[count].pid = (unsigned int)cli_lsp_pid(one->lsp);
        count += 1u;
    }
    cli_lock_give(&g_cli_servers.lock);
    return count;
}

unsigned int cli_servers_tools_for(const char *const *languages, unsigned int count, char (*tools)[CLI_NAME_MAX_LEN],
                                   unsigned int room)
{
    static CliServerSpec s_spec;
    static CliLock s_spec_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_spec_lock);
    unsigned int found = 0u;
    for (unsigned int at = 0u; (at < count) && (found < room); at += 1u)
    {
        if (cli_servers_spec_for(languages[at], &s_spec))
        {
            VerbaLinea tool = {tools[found], CLI_NAME_MAX_LEN, 0ull};
            mmgr_line_put(&tool, s_spec.tool);
            (void)mmgr_line_finish(&tool);
            found += 1u;
        }
    }
    cli_lock_give(&s_spec_lock);
    return found;
}

void cli_servers_stop(const char *const *tools, unsigned int count, VerbaLinea *out)
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
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        cli_lsp_open_each(s_taken[at], running_file_written, out);
    }
    mmgr_line_ch(out, ']');
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
        cli_lsp_unref(s_taken[at]);
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
            cli_lsp_ref(s_taken[count]);
            count += 1u;
        }
    }
    cli_lock_give(&g_cli_servers.lock);
    static const char s_settings[] = "{\"settings\":null}";
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
        (void)cli_lsp_notified(s_taken[at], "workspace/didChangeConfiguration", s_settings, sizeof(s_settings) - 1u,
                               &ignored);
        cli_lsp_unref(s_taken[at]);
    }
    cli_lock_give(&s_taken_lock);
}
