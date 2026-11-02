// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_plugins.c: the language servers script plugins bring. A plugin's `server` in its plugin.json names its
// `program`, found by the toolchain `toolchain` names, the `args` it starts with, `{folder}` in one and in the options
// standing for the folder the program was found in, the `languages` it serves, `*` every one, and, in `ids`, the
// protocol's name of a language where it is not orior's. The window starts one as it starts its plugin, at the tree's
// top, with the options the plugin gives, and stops it as it stops the plugin. It is handed every file of its languages
// the editor has open as it starts, and each the editor opens, changes and closes after; its diagnostics are stored as
// a checker's findings under the plugin's id; and what else it sends goes to the window as the event `plugin-server`,
// `{plugin, method, params, request}`, `request` the id of a request pending until the plugin answers it, null for a
// notification
#include "../command_line/callback_cfg.h"
#include "../plugins/plugins.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

// A plugin's server running: whether its place is taken, its plugin's id, the server, the languages it serves and the
// protocol's name of each, and where what it sends goes
typedef struct
{
    int used;
    char id[CLI_NAME_MAX_LEN];
    CliLsp *lsp;
    char language[CLI_SERVERS_LANGUAGES][CLI_NAME_MAX_LEN];
    char named[CLI_SERVERS_LANGUAGES][CLI_NAME_MAX_LEN];
    unsigned int languages;
    CliServersSink sink;
} CliPluginServer;
// What the editor does to a file, sent to each plugin server: the file, its language where it opens, its text and its
// size where it opens or changes, and whether it closes
typedef struct
{
    const char *path;
    const char *language;
    const char *text;
    unsigned long long size;
    int closes;
} CliPluginsChange;
// the most plugins whose servers run at once
#define CLI_SERVERS_PLUGINS 8u

static CliPluginServer s_plugin[CLI_SERVERS_PLUGINS];

// What a plugin's start reads: its id, its plugin.json read into a tree, and that plugin.json's root there
typedef struct
{
    const char *id;
    CliJson json;
    unsigned int root;
} CliPluginRead;

typedef struct
{
    const char *id;
    const CliPluginServer *plugin;
    const char *language;
    const char *params;
    unsigned long long size;
    const CliServersSink *sink;
    const char *method;
    const char *request;
    const CliJson *json;
    unsigned int server;
    CliPluginServer *plugin_out;
    const CliPluginsChange *change;
} ServersPluginsCtx;
// the place of the running server of plugin `id`, or CLI_SERVERS_PLUGINS where none runs. Under the plugins' lock
EMBED_INLINE unsigned int plugins_position(const ServersPluginsCtx *args)
{
    unsigned int at = 0u;
    while ((at < CLI_SERVERS_PLUGINS) &&
           !(s_plugin[at].used &&
             cellul.eq(&(CatenaFinitaCfg){.src = s_plugin[at].id, .other = args->id, .cap = CLI_NAME_MAX_LEN})))
    {
        at += 1u;
    }
    return at;
}

// the protocol's name of `language` for the plugin's server, its own where the server serves `*`, every language, and
// `ids` names it no other; NULL where it does not serve it
EMBED_INLINE const char *plugins_by_name(const ServersPluginsCtx *args)
{
    int every = 0;
    for (unsigned int at = 0u; at < args->plugin->languages; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = args->plugin->language[at], .other = args->language, .cap = CLI_NAME_MAX_LEN}))
        {
            return args->plugin->named[at];
        }
        every = every || cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->language[at], .other = "*", .cap = 2u});
    }
    return every ? args->language : NULL;
}

// A plugin server just started and the language each extension opens as, to hand it the files the editor has open
typedef struct
{
    CliPluginServer *plugin;
    CliLanguages languages;
} CliPluginHanding;

void cli_servers_plugin_stop(const CliServersRootSinkCfg *args)
{
    const unsigned int at = EMBED_CALL(plugins_position, ServersPluginsCtx, .id = args->id);
    CliLsp *const lsp = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].lsp : NULL;
    if (at < CLI_SERVERS_PLUGINS)
    {
        s_plugin[at].used = 0;
        s_plugin[at].lsp = NULL;
    }
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = lsp);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = lsp);
    }
}

// the server of plugin `id`, referenced, or NULL with why given where none runs
CliLsp *cli_servers_plugin_get(const CliServersErrMsgServerCtx *args)
{
    const unsigned int at = EMBED_CALL(plugins_position, ServersPluginsCtx, .id = args->id);
    CliLsp *const lsp = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].lsp : NULL;
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = lsp);
    }
    if (lsp == NULL)
    {
        mmgr_line_put(args->err_msg, "no server of the plugin ");
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " runs");
    }
    return lsp;
}

int cli_servers_plugin_request(const CliServersErrMsgCfg *args)
{
    const char *method = args->method;
    const char *params = args->params;
    unsigned long long length = args->length;
    unsigned long long wait = args->wait;
    VerbaLinea *err_msg = args->err_msg;
    CliLsp *const lsp = EMBED_CALL(cli_servers_err_msg_server_internal.plugin_get, CliServersErrMsgServerCtx,
                                   .id = args->id, .err_msg = err_msg);
    CliLspAnswer answer = {NULL, NULL, 0ull};
    const int done = (lsp != NULL) && EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = lsp,
                                                 .method = method, .params = params, .length = length, .wait = wait,
                                                 .answer = &answer, .err_msg = err_msg);
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = lsp);
    }
    if (done)
    {
        (void)mmgr_line_put_n(args->result, answer.text, answer.size);
    }
    g_cli_cells->persistent_buf_release(answer.message, &g_cli_servers_cells);
    return done;
}

int cli_servers_plugin_notify(const CliServersErrMsgCfg *args)
{
    const char *method = args->method;
    const char *params = args->params;
    unsigned long long length = args->length;
    VerbaLinea *err_msg = args->err_msg;
    CliLsp *const lsp = EMBED_CALL(cli_servers_err_msg_server_internal.plugin_get, CliServersErrMsgServerCtx,
                                   .id = args->id, .err_msg = err_msg);
    const int done =
        (lsp != NULL) && EMBED_CALL(cli_lsp_err_msg_length.notify, CliLspErrMsgLengthCfg, .lsp = lsp, .method = method,
                                    .params = params, .length = length, .err_msg = err_msg);
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = lsp);
    }
    return done;
}

// what the editor did to a file sent to each plugin server that serves it or has it open: how many were notified
static unsigned int plugins_change_send(const ServersPluginsCtx *args)
{
    CliLsp *owners[CLI_SERVERS_PLUGINS];
    const char *named[CLI_SERVERS_PLUGINS];
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < CLI_SERVERS_PLUGINS; at += 1u)
    {
        const char *const name = (s_plugin[at].used && (args->change->language != NULL))
                                     ? EMBED_CALL(plugins_by_name, ServersPluginsCtx, .plugin = &s_plugin[at],
                                                  .language = args->change->language)
                                     : NULL;
        if (s_plugin[at].used && ((name != NULL) || EMBED_CALL(cli_lsp_path_out.has, CliLspPathOutCfg,
                                                               .lsp = s_plugin[at].lsp, .path = args->change->path)))
        {
            EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = s_plugin[at].lsp);
            owners[count] = s_plugin[at].lsp;
            named[count] = name;
            count += 1u;
        }
    }
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const int has =
            EMBED_CALL(cli_lsp_path_out.has, CliLspPathOutCfg, .lsp = owners[at], .path = args->change->path);
        if (args->change->closes && has)
        {
            (void)EMBED_CALL(cli_lsp_err_msg_text.close, CliLspErrMsgTextCfg, .lsp = owners[at],
                             .path = args->change->path, .err_msg = &err_msg);
        }
        else if (!args->change->closes && has)
        {
            (void)EMBED_CALL(cli_lsp_err_msg_text.change, CliLspErrMsgTextCfg, .lsp = owners[at],
                             .path = args->change->path, .text = args->change->text, .length = args->change->size,
                             .err_msg = &err_msg);
        }
        else if (!args->change->closes && (named[at] != NULL))
        {
            (void)EMBED_CALL(cli_lsp_err_msg_text.open, CliLspErrMsgTextCfg, .lsp = owners[at],
                             .path = args->change->path, .language_id = named[at], .text = args->change->text,
                             .length = args->change->size, .err_msg = &err_msg);
        }
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = owners[at]);
    }
    return count;
}

int cli_servers_plugins_open(const CliServersPathSizeLanguageCtx *args)
{
    const CliPluginsChange change = {args->path, args->language, args->text, args->size, 0};
    return EMBED_CALL(plugins_change_send, ServersPluginsCtx, .change = &change) > 0u;
}

void cli_servers_plugins_change(const CliServersPathSizeLanguageCtx *args)
{
    const CliPluginsChange change = {args->path, NULL, args->text, args->size, 0};
    (void)EMBED_CALL(plugins_change_send, ServersPluginsCtx, .change = &change);
}

void cli_servers_plugins_close(const CliServersPathLspCtx *args)
{
    const CliPluginsChange change = {args->path, NULL, "", 0ull, 1};
    (void)EMBED_CALL(plugins_change_send, ServersPluginsCtx, .change = &change);
}

CliLsp *cli_servers_plugin_owner_get(const CliServersPathIdCtx *args)
{
    unsigned int *at = args->at_pointer;
    const char *path = args->path;
    CliLsp *found = NULL;
    while ((found == NULL) && (*at < CLI_SERVERS_PLUGINS))
    {
        const CliPluginServer *const plugin = &s_plugin[*at];
        *at += 1u;
        if (plugin->used && EMBED_CALL(cli_lsp_path_out.has, CliLspPathOutCfg, .lsp = plugin->lsp, .path = path))
        {
            EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = plugin->lsp);
            found = plugin->lsp;
            memor.cpy(&(MemoriaCfg){.dst = args->id_out, .src = plugin->id, .bytes = CLI_NAME_MAX_LEN});
        }
    }
    return found;
}
