// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_running.c: the servers running, each its toolchain's id, its program's name, its process and the files it
// has open; the toolchains whose servers serve a language; servers stopped without waiting, each instructed to end on a
// thread of the servers', or every one stopped and waited on as the app closes; every server and what was stored of the
// tree let go as another tree opens; and each server notified its settings changed, for it to request them again. A
// server taken out of the running ones takes the toolchains that failed with it, so one requested after this starts
// afresh
#include "../command_line/callback_cfg.h"
#include "servers_internal.h"

// the servers taken out of the running ones, under the servers' lock and then their own
static CliLsp *s_allocated[CLI_SERVERS_MAX];

// A server to stop
typedef struct
{
    CliLsp *server;
} CliServersStopping;

// a server stopped and its reference given back, on a thread of the servers'
static void running_stop(const CliServersJobCfg *args)
{
    CliServersStopping stopping = {NULL};
    memor.cpy(&(MemoriaCfg){.dst = &stopping,
                            .src = args->job,
                            .bytes = (sizeof(stopping) <= args->size) ? sizeof(stopping) : (embed_word)args->size});
    EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = stopping.server);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = stopping.server);
}

void cli_servers_stop_later(const CliServersPathLspCtx *args)
{
    const CliServersStopping stopping = {args->lsp};
    if (!EMBED_CALL(cli_servers_run_internal.later, CliServersRunCtx, .run = running_stop, .job = &stopping,
                    .size = sizeof(stopping)))
    {
        EMBED_CALL(running_stop, CliServersJobCfg, .job = &stopping, .size = sizeof(stopping));
    }
}

// every server taken out of the running ones into the taken, and the toolchains that failed and ended forgotten:
// their count; with the taken's lock acquired
static unsigned int running_alloc_all(void)
{
    unsigned int count = 0u;
    g_cli_servers.failed = 0u;
    g_cli_servers.restarted = 0u;
    for (unsigned int at = 0u; at < CLI_SERVERS_MAX; at += 1u)
    {
        if (g_cli_servers.running[at].used)
        {
            g_cli_servers.running[at].used = 0;
            s_allocated[count] = g_cli_servers.running[at].lsp;
            count += 1u;
        }
    }
    return count;
}

unsigned int cli_servers_list(const CliServersCfg *args)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < CLI_SERVERS_MAX) && (count < args->capacity); at += 1u)
    {
        const CliServerRunning *const one = &g_cli_servers.running[at];
        if (!one->used)
        {
            continue;
        }
        const char *const program = (one->spec.program[0] != '\0') ? one->spec.program : one->spec.tool;
        VerbaLinea tool = {args->servers[count].tool, CLI_NAME_MAX_LEN, 0ULL};
        mmgr_line_put(&tool, one->spec.tool);
        (void)mmgr_line_finish(&tool);
        VerbaLinea named = {args->servers[count].program, CLI_NAME_MAX_LEN, 0ULL};
        mmgr_line_put(&named, program);
        (void)mmgr_line_finish(&named);
        // a process id fits the 32 bits the system gives it
        args->servers[count].pid =
            (unsigned int)EMBED_CALL(cli_lsp_size_type.pid, CliLspSizeTypeCfg, .lsp_read = one->lsp);
        count += 1u;
    }
    return count;
}

unsigned int cli_servers_tools_for(const CliServersCapacityCfg *args)
{
    static CliServerSpec s_spec;
    unsigned int found = 0u;
    for (unsigned int at = 0u; (at < args->count) && (found < args->capacity); at += 1u)
    {
        if (EMBED_CALL(cli_servers_language_internal.spec_for, CliServersLanguageCtx, .language = args->languages[at],
                       .spec = &s_spec))
        {
            VerbaLinea tool = {args->tools[found], CLI_NAME_MAX_LEN, 0ULL};
            mmgr_line_put(&tool, s_spec.tool);
            (void)mmgr_line_finish(&tool);
            found += 1u;
        }
    }
    return found;
}

void cli_servers_stop_all(void)
{
    const unsigned int reserved = running_alloc_all();
    for (unsigned int at = 0u; at < reserved; at += 1u)
    {
        EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = s_allocated[at]);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = s_allocated[at]);
    }
}

void cli_servers_let_go(void)
{
    cli_servers_check_let_go();
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &g_cli_servers_published);
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &g_cli_servers_texts);
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &g_cli_servers_parses);
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &g_cli_servers_inspected);
    cli_servers_inspect_let_go();
    const unsigned int reserved = running_alloc_all();
    for (unsigned int at = 0u; at < reserved; at += 1u)
    {
        EMBED_CALL(cli_servers_path_lsp_internal.stop_later, CliServersPathLspCtx, .lsp = s_allocated[at]);
    }
}
