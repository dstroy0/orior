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
static CliLsp *s_taken[CLI_SERVERS_MOST];

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
                            .bytes = (sizeof(stopping) <= args->size) ? sizeof(stopping) : (size_t)args->size});
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
static unsigned int running_take_all(void)
{
    unsigned int count = 0u;
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
    return count;
}

// a path a server has open written as a JSON string after those written
static void running_file_write(const CliEachPathTextLengthCfg *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    (void)text;
    (void)length;
    VerbaLinea *const out = args->context;
    if ((out->at > 0ull) && (out->out[out->at - 1ull] != '['))
    {
        mmgr_line_ch(out, ',');
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
}

unsigned int cli_servers_list(const CliServersCfg *args)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < CLI_SERVERS_MOST) && (count < args->room); at += 1u)
    {
        const CliServerRunning *const one = &g_cli_servers.running[at];
        if (!one->used)
        {
            continue;
        }
        const char *const program = (one->spec.program[0] != '\0') ? one->spec.program : one->spec.tool;
        VerbaLinea tool = {args->servers[count].tool, CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&tool, one->spec.tool);
        (void)mmgr_line_finish(&tool);
        VerbaLinea named = {args->servers[count].program, CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&named, program);
        (void)mmgr_line_finish(&named);
        // a process id fits the 32 bits the system gives it
        args->servers[count].pid =
            (unsigned int)EMBED_CALL(cli_lsp_size_kind.pid, CliLspSizeKindCfg, .lsp_read = one->lsp);
        count += 1u;
    }
    return count;
}

unsigned int cli_servers_tools_for(const CliServersRoomCfg *args)
{
    static CliServerSpec s_spec;
    unsigned int found = 0u;
    for (unsigned int at = 0u; (at < args->count) && (found < args->room); at += 1u)
    {
        if (EMBED_CALL(cli_servers_language_internal.spec_for, CliServersLanguageCtx, .language = args->languages[at],
                       .spec = &s_spec))
        {
            VerbaLinea tool = {args->tools[found], CLI_NAME_MAX_LEN, 0ull};
            mmgr_line_put(&tool, s_spec.tool);
            (void)mmgr_line_finish(&tool);
            found += 1u;
        }
    }
    return found;
}

void cli_servers_stop(const CliServersOutCfg *args)
{
    unsigned int taken = 0u;
    for (unsigned int tool = 0u; tool < args->count; tool += 1u)
    {
        for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
        {
            CliServerRunning *const one = &g_cli_servers.running[at];
            if (one->used && (cellul.eq(&(CatenaFinitaCfg){
                                 .src = one->spec.tool, .other = args->tools_letters[tool], .cap = CLI_CELLS_POOL})))
            {
                one->used = 0;
                s_taken[taken] = one->lsp;
                taken += 1u;
            }
        }
    }
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        EMBED_CALL(cli_lsp_context.open_each, CliLspContextCfg, .lsp = s_taken[at], .each = running_file_write,
                   .context = args->out);
    }
    mmgr_line_ch(args->out, ']');
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        EMBED_CALL(cli_servers_path_lsp_internal.stop_later, CliServersPathLspCtx, .lsp = s_taken[at]);
    }
}

void cli_servers_stop_all(void)
{
    const unsigned int taken = running_take_all();
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = s_taken[at]);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = s_taken[at]);
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
    const unsigned int taken = running_take_all();
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        EMBED_CALL(cli_servers_path_lsp_internal.stop_later, CliServersPathLspCtx, .lsp = s_taken[at]);
    }
}

void cli_servers_settings_change(void)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        if (g_cli_servers.running[at].used)
        {
            s_taken[count] = g_cli_servers.running[at].lsp;
            EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = s_taken[count]);
            count += 1u;
        }
    }
    static const char s_settings[] = "{\"settings\":null}";
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
        (void)EMBED_CALL(cli_lsp_err_msg_length.notify, CliLspErrMsgLengthCfg, .lsp = s_taken[at],
                         .method = "workspace/didChangeConfiguration", .params = s_settings,
                         .length = sizeof(s_settings) - 1u, .err_msg = &ignored);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = s_taken[at]);
    }
}
