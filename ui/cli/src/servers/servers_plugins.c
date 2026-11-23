// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_plugins.c: the language servers script plugins bring. A plugin's `server` in its plugin.json names its
// `program`, found by the toolchain `toolchain` names, the `args` it starts with, `{folder}` in one and in the options
// located for the folder the program was found in, the `languages` it serves, `*` every one, and, in `ids`, the
// protocol's name of a language where it is not orior's. The window starts one as it starts its plugin, at the tree's
// top, with the options the plugin gives, and stops it as it stops the plugin. It is passed every file of its languages
// the editor has open as it starts, and each the editor opens, changes and closes after; its diagnostics are stored as
// a checker's findings under the plugin's id; and what else it sends goes to the window as the event `plugin-server`,
// `{plugin, method, params, request}`, `request` the id of a request pending until the plugin answers it, null for a
// notification
#include "../command_line/callback_cfg.h"
#include "../machine/overseer.h"
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
// the max plugins whose servers run at once
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
} CliPluginHanding;

// What a plugin's server sends unprompted. Its diagnostics go to the servers' own receiver, and the servers store them.
// Each other message goes to the window as the event `plugin-server`. A request's answer waits until the plugin
// answers it through cli_servers_plugin_answer
static void plugins_recv(const CliLspRecvCfg *args)
{
    CliPluginServer *const plugin = (CliPluginServer *)args->context;
    if (cellul.eq(
            &(CatenaFinitaCfg){.src = args->method, .other = "textDocument/publishDiagnostics", .cap = CLI_POOL_BYTES}))
    {
        cli_servers_recv(&(CliLspRecvCfg){.context = &plugin->sink,
                                          .method = args->method,
                                          .params = args->params,
                                          .size = args->size,
                                          .answer = args->answer});
        return;
    }
    char request[64];
    const int later = EMBED_CALL(cli_lsp.answer_later, CliLspCfg, .id = request, .capacity = sizeof(request));
    const unsigned long long capacity = args->size + 512ULL;
    char *const text = g_cli_guard->temporary_buf_alloc(capacity, g_cli_servers_owner);
    VerbaLinea event = mmgr_line_over(text, (text != NULL) ? capacity : 0ULL);
    mmgr_line_put(&event, "{\"plugin\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &event,
               .text_bytes = (const unsigned char *)plugin->id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = plugin->id, .cap = CLI_NAME_MAX_LEN}));
    mmgr_line_put(&event, ",\"method\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &event,
               .text_bytes = (const unsigned char *)args->method,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->method, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&event, ",\"params\":");
    mmgr_line_put_n(&event, args->params, args->size);
    mmgr_line_put(&event, ",\"request\":");
    mmgr_line_put(&event, later ? request : "null");
    mmgr_line_put(&event, "}");
    if ((plugin->sink.emit != NULL) && mmgr_line_ok(&event))
    {
        EMBED_CALL(plugin->sink.emit, CliEachEventJsonSizeCfg, .context = plugin->sink.context,
                   .event = "plugin-server", .json = text, .size = event.at);
    }
    g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
}

// What one start of a plugin's server holds: the start's own arguments, the place that it takes, and whether its
// server started
typedef struct
{
    const CliServersRootSinkCfg *start;
    unsigned int at;
    int started;
} PluginsStarting;

// the languages of the plugin's `server` read into its place: each name, and its name in the protocol from `ids`
static void plugins_languages_read(const CliJson *json, unsigned int server, CliPluginServer *plugin)
{
    const unsigned int languages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = server, .name = "languages");
    const unsigned int ids = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = server, .name = "ids");
    plugin->languages = 0u;
    unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                   .node = languages, .type = CFG_JSON_ARRAY)
                            ? json->node[languages].first
                            : CLI_JSON_NONE;
    while ((item != CLI_JSON_NONE) && (plugin->languages < CLI_SERVERS_LANGUAGES))
    {
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = item, .length_pointer = NULL);
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = (name != NULL) ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                           .json_read = json, .object = ids, .name = name)
                                              : CLI_JSON_NONE,
                       .length_pointer = NULL);
        if (name != NULL)
        {
            (void)cellul.copy(
                &(CatenaFinitaCfg){.dst = plugin->language[plugin->languages], .src = name, .cap = CLI_NAME_MAX_LEN});
            (void)cellul.copy(&(CatenaFinitaCfg){.dst = plugin->named[plugin->languages],
                                                 .src = (named != NULL) ? named : name,
                                                 .cap = CLI_NAME_MAX_LEN});
            plugin->languages += 1u;
        }
        item = json->node[item].next;
    }
}

// The plugin's server started from its kept plugin.json, as cli_plugins_settings_of passes the plugin. Its toolchain
// finds its program. Its first words come before the words of its `args`. The options take `{folder}` as
// the program's folder
static void plugins_started(const CliEachPluginCfg *args)
{
    PluginsStarting *const starting = (PluginsStarting *)args->context;
    const CliServersRootSinkCfg *const start = starting->start;
    const CliJson *const json = args->plugin->json;
    const unsigned int server = EMBED_CALL(cli_plugins_setting_table.setting, CliPluginsSettingCfg, .json = json,
                                           .node = args->plugin->root, .key = "server");
    char program[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    if (server == CLI_JSON_NONE)
    {
        mmgr_line_put(start->err_msg, "the plugin's plugin.json names no server");
        return;
    }
    if (!EMBED_CALL(cli_servers_err_msg_json_internal.plugin_program, CliServersErrMsgJsonCtx,
                    .err_msg = start->err_msg, .json = json, .server_u32 = server, .program = program,
                    .folder_out = folder))
    {
        return;
    }
    CliWords *const words = (CliWords *)g_cli_guard->temporary_buf_alloc(sizeof(CliWords), g_cli_servers_owner);
    const unsigned long long capacity = (start->length * 2ULL) + CLI_PATH_MAX_LEN + 16ULL;
    char *const options = g_cli_guard->temporary_buf_alloc(capacity, g_cli_servers_owner);
    if ((words == NULL) || (options == NULL))
    {
        mmgr_line_put(start->err_msg, "no room to start the plugin's server");
        g_cli_guard->persistent_buf_release(words, g_cli_servers_owner);
        g_cli_guard->persistent_buf_release(options, g_cli_servers_owner);
        return;
    }
    words->count = 0u;
    words->used = 0u;
    words->full = 0;
    for (unsigned int at = 0u; at < start->firsts; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                   .word_text = start->first_characters[at]);
    }
    EMBED_CALL(cli_servers_json_server_internal.plugin_args, CliServersJsonServerCtx, .json = json,
               .server_u32 = server, .folder = folder, .words = words);
    VerbaLinea optioned = mmgr_line_over(options, capacity);
    EMBED_CALL(cli_servers_text_internal.plugin_folder_append, CliServersTextCtx, .written = &optioned,
               .text = (start->options != NULL) ? start->options : "null",
               .length = (start->options != NULL) ? start->length : 4ULL, .folder = folder, .escaped = 1);
    CliPluginServer *const plugin = &s_plugin[starting->at];
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = plugin->id, .src = start->id, .cap = CLI_NAME_MAX_LEN});
    plugins_languages_read(json, server, plugin);
    plugin->sink = (start->sink != NULL) ? *start->sink : (CliServersSink){0};
    CliLsp *const lsp =
        (!words->full && mmgr_line_ok(&optioned))
            ? EMBED_CALL(cli_lsp_err_msg.start, CliLspErrMsgCfg, .err_msg = start->err_msg, .program = program,
                         .request = words->word_text, .count = words->count, .root = start->root, .options = options,
                         .length = optioned.at, .recv = plugins_recv, .context = plugin, .overseen = start->id,
                         .overseen_role = CLI_OVERSEER_SERVER)
            : NULL;
    plugin->lsp = lsp;
    plugin->used = (lsp != NULL);
    starting->started = (lsp != NULL);
    g_cli_guard->persistent_buf_release(words, g_cli_servers_owner);
    g_cli_guard->persistent_buf_release(options, g_cli_servers_owner);
}

int cli_servers_plugin_start(const CliServersRootSinkCfg *args)
{
    if (EMBED_CALL(plugins_position, ServersPluginsCtx, .id = args->id) < CLI_SERVERS_PLUGINS)
    {
        return 1;
    }
    unsigned int at = 0u;
    while ((at < CLI_SERVERS_PLUGINS) && s_plugin[at].used)
    {
        at += 1u;
    }
    if (at == CLI_SERVERS_PLUGINS)
    {
        mmgr_line_put(args->err_msg, "the servers of too many plugins run");
        return 0;
    }
    PluginsStarting starting = {args, at, 0};
    (void)EMBED_CALL(cli_plugins_settings_of_table.of, CliPluginsSettingsOfCfg, .id = args->id, .each = plugins_started,
                     .context = &starting);
    return starting.started;
}

int cli_servers_plugin_answer(const CliServersErrMsgIdCfg *args)
{
    CliLsp *const lsp = EMBED_CALL(cli_servers_err_msg_server_internal.plugin_get, CliServersErrMsgServerCtx,
                                   .id = args->id, .err_msg = args->err_msg);
    const int done =
        (lsp != NULL) && EMBED_CALL(cli_lsp_answered_table.answered, CliLspAnsweredCfg, .lsp = lsp, .id = args->request,
                                    .result = args->result_text, .size = args->length, .err_msg = args->err_msg);
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = lsp);
    }
    return done;
}

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
    CliLspAnswer answer = {NULL, NULL, 0ULL};
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
    g_cli_guard->persistent_buf_release(answer.message, g_cli_servers_owner);
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
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
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
    const CliPluginsChange change = {args->path, NULL, "", 0ULL, 1};
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
