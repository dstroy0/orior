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
static CliLock s_plugins_lock = CLI_LOCK_INIT;

// What a plugin's start reads: its id, its plugin.json read into a tree, and that plugin.json's root there
typedef struct
{
    const char *id;
    CliJson json;
    unsigned int root;
} CliPluginRead;

// the plugin of the id looked for read into the tree, a reader's standing in for one that comes with orior
static void plugins_find(const CliEachPluginCfg *args)
{
    CliPluginRead *const read = args->context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = read->id, .cap = CLI_NAME_MAX_LEN}))
    {
        return;
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &read->json);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &read->json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    read->root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                            .json = &read->json, .text = args->plugin->text, .size = args->plugin->size,
                            .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_cli_servers_cells);
}

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
EMBED_INLINE unsigned int plugins_place(const ServersPluginsCtx *args)
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

// a server's diagnostics of a file stored as its plugin's findings, and the file's diagnostics passed on whole
EMBED_INLINE void plugins_diagnostics(const ServersPluginsCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long uri_at = 0ull;
    unsigned long long uri_length = 0ull;
    unsigned long long all_at = 0ull;
    unsigned long long all_length = 0ull;
    char uri[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->params, .size_value = size, .name = "uri",
                    .start = &uri_at, .length_pointer = &uri_length) ||
        !EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->params, .size_value = size, .name = "diagnostics",
                    .start = &all_at, .length_pointer = &all_length) ||
        !EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = args->params + uri_at, .length = uri_length,
                    .out_room = uri, .room = sizeof(uri)) ||
        !EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri,
                    .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .path_out = path))
    {
        return;
    }
    const unsigned long long found_room = all_length + CLI_PATH_MAX_LEN;
    char *const found_cell = g_cli_cells->temporary_buf_alloc(found_room, &g_cli_servers_cells);
    VerbaLinea found = mmgr_line_over(found_cell, found_room);
    mmgr_line_put(&found, "[[");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &found,
               .text_bytes = (const unsigned char *)path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&found, ',');
    mmgr_line_put_n(&found, args->params + all_at, all_length);
    mmgr_line_put(&found, "]]");
    if (mmgr_line_ok(&found))
    {
        EMBED_CALL(cli_servers_size_internal.findings_store, CliServersSizeCtx, .id = args->plugin->id,
                   .found = found.out, .size = found.at, .always = 1, .sink = &args->plugin->sink);
    }
    g_cli_cells->persistent_buf_release(found.out, &g_cli_servers_cells);
}

// what the server of the plugin `id` sends, `method` with `size` bytes of `params`, passed to the window where `emit`
// sends it as `plugin-server`, `request` the id of the request awaiting an answer or null
EMBED_INLINE void plugins_msg_emit(const ServersPluginsCtx *args)
{
    const unsigned long long event_json_room = args->size + 256ull;
    char *const event_json_cell = g_cli_cells->temporary_buf_alloc(event_json_room, &g_cli_servers_cells);
    VerbaLinea event_json = mmgr_line_over(event_json_cell, event_json_room);
    mmgr_line_put(&event_json, "{\"plugin\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &event_json,
               .text_bytes = (const unsigned char *)args->id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_NAME_MAX_LEN}));
    mmgr_line_put(&event_json, ",\"method\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &event_json,
               .text_bytes = (const unsigned char *)args->method,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->method, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&event_json, ",\"params\":");
    mmgr_line_put_n(&event_json, args->params, args->size);
    mmgr_line_put(&event_json, ",\"request\":");
    mmgr_line_put(&event_json, args->request);
    mmgr_line_ch(&event_json, '}');
    if (mmgr_line_ok(&event_json))
    {
        EMBED_CALL(args->sink->emit, CliEachEventJsonSizeCfg, .context = args->sink->context, .event = "plugin-server",
                   .json = event_json.out, .size = event_json.at);
    }
    g_cli_cells->persistent_buf_release(event_json.out, &g_cli_servers_cells);
}

// what a plugin's server sends unprompted: its diagnostics stored, and all it sends passed to the window, a request
// waiting unanswered for the plugin to answer where the window receives it, and answered with null where it does not
static void plugins_recv(const CliLspRecvCfg *args)
{
    const char *method = args->method;
    const char *params = args->params;
    unsigned long long size = args->size;
    const CliPluginServer *const plugin = args->context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "textDocument/publishDiagnostics", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(plugins_diagnostics, ServersPluginsCtx, .plugin = plugin, .params = params, .size = size);
        // and received by the plugin as well, which lists them as its own findings
        if (plugin->sink.emit != NULL)
        {
            EMBED_CALL(plugins_msg_emit, ServersPluginsCtx, .id = plugin->id, .sink = &plugin->sink, .method = method,
                       .params = params, .size = size, .request = "null");
        }
        return;
    }
    if (plugin->sink.emit == NULL)
    {
        mmgr_line_put(args->answer, "null");
        return;
    }
    char request[64];
    const int deferred = EMBED_CALL(cli_lsp.answer_later, CliLspCfg, .id = request, .room = sizeof(request));
    EMBED_CALL(plugins_msg_emit, ServersPluginsCtx, .id = plugin->id, .sink = &plugin->sink, .method = method,
               .params = params, .size = size, .request = deferred ? request : "null");
}

// the languages a plugin's server serves read from `server` in `json` into `plugin`, each with the protocol's name
// `ids` gives it, its own where `ids` gives none
EMBED_INLINE void plugins_languages_read(const ServersPluginsCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int languages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = args->server, .name = "languages");
    const unsigned int ids = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = args->server, .name = "ids");
    args->plugin_out->languages = 0u;
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = languages, .kind = CFG_JSON_ARRAY)
                                 ? json->node[languages].first
                                 : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (args->plugin_out->languages < CLI_SERVERS_LANGUAGES); item = json->node[item].next)
    {
        const char *const language = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = json, .node = item, .length_pointer = NULL);
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = ids, .name = (language != NULL) ? language : ""),
                       .length_pointer = NULL);
        if ((language == NULL) ||
            (cellul.len(&(CatenaFinitaCfg){.src = language, .cap = CLI_NAME_MAX_LEN}) >= CLI_NAME_MAX_LEN))
        {
            continue;
        }
        VerbaLinea own = {args->plugin_out->language[args->plugin_out->languages], CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&own, language);
        (void)mmgr_line_finish(&own);
        VerbaLinea protocol = {args->plugin_out->named[args->plugin_out->languages], CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&protocol, (named != NULL) ? named : language);
        args->plugin_out->languages += (mmgr_line_finish(&protocol) != 0ull) ? 1u : 0u;
    }
}

// A plugin server just started and the language each extension opens as, to hand it the files the editor has open
typedef struct
{
    CliPluginServer *plugin;
    CliLanguages languages;
} CliPluginHanding;

// a file of the editor's handed to the plugin server just started where it serves the file's language
static void plugins_open_dispatch(const CliEachKeyBesideTextSizeCfg *args)
{
    const char *text = args->text;
    CliPluginHanding *const handing = args->context;
    CliPluginServer *const plugin = handing->plugin;
    char language[CLI_NAME_MAX_LEN];
    const char *const named =
        EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                   .languages_read = &handing->languages, .path = args->beside, .id_out = language)
            ? EMBED_CALL(plugins_by_name, ServersPluginsCtx, .plugin = plugin, .language = language)
            : NULL;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if ((named != NULL) &&
        !EMBED_CALL(cli_lsp_path_out.has, CliLspPathOutCfg, .lsp = plugin->lsp, .path = args->beside))
    {
        (void)EMBED_CALL(cli_lsp_err_msg_text.open, CliLspErrMsgTextCfg, .lsp = plugin->lsp, .path = args->beside,
                         .language_id = named, .text = text, .length = args->size, .err_msg = &err_msg);
    }
}

int cli_servers_plugin_start(const CliServersRootSinkCfg *args)
{
    const char *root = args->root;
    const char *options = args->options;
    unsigned long long length = args->length;
    const CliServersSink *sink = args->sink;
    VerbaLinea *err_msg = args->err_msg;
    cli_servers_root_sink.plugin_stop(args);
    CliPluginRead read = {args->id, {0}, CLI_JSON_NONE};
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &read.json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = plugins_find, .context = &read);
    const unsigned int server = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = &read.json, .object = read.root, .name = "server");
    char program[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    CliWords *const words = g_cli_cells->temporary_buf_alloc(sizeof(*words), &g_cli_servers_cells);
    char *const path = g_cli_cells->temporary_buf_alloc(CLI_PATH_TEXT, &g_cli_servers_cells);
    int started = 0;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &read.json, .node = server,
                    .kind = CFG_JSON_OBJECT))
    {
        mmgr_line_put(err_msg, "the plugin brings no server");
    }
    else if ((words != NULL) && (path != NULL) &&
             EMBED_CALL(cli_servers_err_msg_json_internal.plugin_program, CliServersErrMsgJsonCtx, .json = &read.json,
                        .server_u32 = server, .program = program, .folder_out = folder, .err_msg = err_msg))
    {
        *words = (CliWords){{NULL}, 0u, {0}, 0u, 0};
        for (unsigned int at = 0u; at < args->firsts; at += 1u)
        {
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                       .word = args->first_letters[at]);
        }
        EMBED_CALL(cli_servers_json_server_internal.plugin_args, CliServersJsonServerCtx, .json = &read.json,
                   .server_u32 = server, .folder = folder, .words = words);
        // the options with each `{folder}` in them the folder the program was found in, as the arguments have it
        const unsigned long long room = length + (CLI_PATH_MAX_LEN * 16ull);
        char *const given = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
        VerbaLinea written = mmgr_line_over(given, room);
        EMBED_CALL(cli_servers_text_internal.plugin_folder_put, CliServersTextCtx, .written = &written, .text = options,
                   .length = length, .folder = folder, .escaped = 1);
        EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = path, .size = CLI_PATH_TEXT);
        const CliSetting setting[1] = {{"PATH", path}};
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_plugins_lock);
        unsigned int at = 0u;
        while ((at < CLI_SERVERS_PLUGINS) && s_plugin[at].used)
        {
            at += 1u;
        }
        CliPluginServer *const plugin = (at < CLI_SERVERS_PLUGINS) ? &s_plugin[at] : NULL;
        if (plugin != NULL)
        {
            VerbaLinea named = {plugin->id, CLI_NAME_MAX_LEN, 0ull};
            mmgr_line_put(&named, args->id);
            (void)mmgr_line_finish(&named);
            EMBED_CALL(plugins_languages_read, ServersPluginsCtx, .json = &read.json, .server = server,
                       .plugin_out = plugin);
            plugin->sink = *sink;
            plugin->lsp =
                (words->full || !mmgr_line_ok(&written))
                    ? NULL
                    : EMBED_CALL(cli_lsp_err_msg.start, CliLspErrMsgCfg, .program = program, .request = words->word,
                                 .count = words->count, .root = root, .options = given, .length = written.at,
                                 .settings = setting, .setting_count = 1u, .recv = plugins_recv, .context = plugin,
                                 .err_msg = err_msg);
            plugin->used = plugin->lsp != NULL;
            started = plugin->used;
        }
        else
        {
            mmgr_line_put(err_msg, "as many plugin servers run as orior_cli runs at once");
        }
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_plugins_lock);
        if (started)
        {
            CliPluginHanding handing = {plugin, {NULL, 0u, 0u}};
            EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &handing.languages);
            EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_texts_lock);
            EMBED_CALL(cli_keyed_each_table.each, CliKeyedEachCfg, .keyed = &g_cli_servers_texts,
                       .each = plugins_open_dispatch, .context = &handing);
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_texts_lock);
            EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &handing.languages);
        }
        g_cli_cells->persistent_buf_release(given, &g_cli_servers_cells);
    }
    g_cli_cells->persistent_buf_release(path, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(words, &g_cli_servers_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &read.json);
    return started;
}

void cli_servers_plugin_stop(const CliServersRootSinkCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_plugins_lock);
    const unsigned int at = EMBED_CALL(plugins_place, ServersPluginsCtx, .id = args->id);
    CliLsp *const lsp = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].lsp : NULL;
    if (at < CLI_SERVERS_PLUGINS)
    {
        s_plugin[at].used = 0;
        s_plugin[at].lsp = NULL;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_plugins_lock);
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = lsp);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = lsp);
    }
}

// the server of plugin `id`, referenced, or NULL with why given where none runs
CliLsp *cli_servers_plugin_get(const CliServersErrMsgServerCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_plugins_lock);
    const unsigned int at = EMBED_CALL(plugins_place, ServersPluginsCtx, .id = args->id);
    CliLsp *const lsp = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].lsp : NULL;
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = lsp);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_plugins_lock);
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

int cli_servers_plugin_answer(const CliServersErrMsgIdCfg *args)
{
    const char *id = args->id;
    const char *result = args->result_text;
    VerbaLinea *err_msg = args->err_msg;
    CliLsp *const lsp = EMBED_CALL(cli_servers_err_msg_server_internal.plugin_get, CliServersErrMsgServerCtx, .id = id,
                                   .err_msg = err_msg);
    const int done = (lsp != NULL) &&
                     EMBED_CALL(cli_lsp_size_err_msg.answer, CliLspSizeErrMsgCfg, .lsp = lsp, .id_read = args->request,
                                .result = result, .size_value = args->length, .err_msg = err_msg);
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_plugins_lock);
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
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_plugins_lock);
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_plugins_lock);
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
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_plugins_lock);
    return found;
}

void cli_servers_plugin_command(const CliServersSizeCommandCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_plugins_lock);
    const unsigned int at = EMBED_CALL(plugins_place, ServersPluginsCtx, .id = args->id);
    const CliServersSink sink = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].sink : (CliServersSink){NULL, NULL, ""};
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_plugins_lock);
    if (sink.emit != NULL)
    {
        EMBED_CALL(plugins_msg_emit, ServersPluginsCtx, .id = args->id, .sink = &sink, .method = "orior/command",
                   .params = args->command, .size = args->size, .request = "null");
    }
}
