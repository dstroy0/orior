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
#include "../plugins/plugins.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

// the most plugins whose servers run at once
#define CLI_SERVERS_PLUGINS 8u

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
static void plugins_found(void *context, const CliPlugin *plugin)
{
    CliPluginRead *const read = context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = read->id, .cap = CLI_NAME_MAX_LEN}))
    {
        return;
    }
    cli_json_given(&read->json);
    cli_json_begun(&read->json, NULL, 0u, NULL, 0ull);
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    read->root = cli_json_taken(&read->json, plugin->text, plugin->size, 0u, 0ull, &err_msg);
}

// the place of the running server of plugin `id`, or CLI_SERVERS_PLUGINS where none runs. Under the plugins' lock
static unsigned int plugins_place(const char *id)
{
    unsigned int at = 0u;
    while ((at < CLI_SERVERS_PLUGINS) &&
           !(s_plugin[at].used &&
             cellul.eq(&(CatenaFinitaCfg){.src = s_plugin[at].id, .other = id, .cap = CLI_NAME_MAX_LEN})))
    {
        at += 1u;
    }
    return at;
}

// the protocol's name of `language` for the plugin's server, its own where the server serves `*`, every language, and
// `ids` names it no other; NULL where it does not serve it
static const char *plugins_named(const CliPluginServer *plugin, const char *language)
{
    int every = 0;
    for (unsigned int at = 0u; at < plugin->languages; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = plugin->language[at], .other = language, .cap = CLI_NAME_MAX_LEN}))
        {
            return plugin->named[at];
        }
        every = every || cellul.eq(&(CatenaFinitaCfg){.src = plugin->language[at], .other = "*", .cap = 2u});
    }
    return every ? language : NULL;
}

// a server's diagnostics of a file stored as its plugin's findings, and the file's diagnostics passed on whole
static void plugins_diagnostics(const CliPluginServer *plugin, const char *params, unsigned long long size)
{
    unsigned long long uri_at = 0ull;
    unsigned long long uri_length = 0ull;
    unsigned long long all_at = 0ull;
    unsigned long long all_length = 0ull;
    char uri[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!cli_lsp_member(params, size, "uri", &uri_at, &uri_length) ||
        !cli_lsp_member(params, size, "diagnostics", &all_at, &all_length) ||
        !cli_lsp_string(params + uri_at, uri_length, uri, sizeof(uri)) ||
        !cli_lsp_path_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), path))
    {
        return;
    }
    VerbaLinea found = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_growing_line_made(&found, all_length + CLI_PATH_MAX_LEN);
    mmgr_line_put(&found, "[[");
    cli_json_quoted_lossy(&found, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&found, ',');
    mmgr_line_put_n(&found, params + all_at, all_length);
    mmgr_line_put(&found, "]]");
    if (mmgr_line_ok(&found))
    {
        cli_servers_findings_stored(plugin->id, found.out, found.at, 1, &plugin->sink);
    }
    g_cli_cells->persistent_buf_release(found.out, NULL);
}

// what the server of the plugin `id` sends, `method` with `size` bytes of `params`, passed to the window where `emit`
// sends it as `plugin-server`, `request` the id of the request pending or null
static void plugins_msg_emitted(const char *id, const CliServersSink *sink, const char *method, const char *params,
                                unsigned long long size, const char *request)
{
    VerbaLinea event_json = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_growing_line_made(&event_json, size + 256ull);
    mmgr_line_put(&event_json, "{\"plugin\":");
    cli_json_quoted_lossy(&event_json, (const unsigned char *)id,
                          cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_NAME_MAX_LEN}));
    mmgr_line_put(&event_json, ",\"method\":");
    cli_json_quoted_lossy(&event_json, (const unsigned char *)method,
                          cellul.len(&(CatenaFinitaCfg){.src = method, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&event_json, ",\"params\":");
    mmgr_line_put_n(&event_json, params, size);
    mmgr_line_put(&event_json, ",\"request\":");
    mmgr_line_put(&event_json, request);
    mmgr_line_ch(&event_json, '}');
    if (mmgr_line_ok(&event_json))
    {
        sink->emit(sink->context, "plugin-server", event_json.out, event_json.at);
    }
    g_cli_cells->persistent_buf_release(event_json.out, NULL);
}

// what a plugin's server sends unprompted: its diagnostics stored, and all it sends passed to the window, a request
// pending unanswered for the plugin to answer where the window receives it, and answered with null where it does not
static void plugins_recv(void *context, const char *method, const char *params, unsigned long long size,
                         VerbaLinea *answer)
{
    const CliPluginServer *const plugin = context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "textDocument/publishDiagnostics", .cap = CLI_CELLS_POOL}))
    {
        plugins_diagnostics(plugin, params, size);
        // and received by the plugin as well, which lists them as its own findings
        if (plugin->sink.emit != NULL)
        {
            plugins_msg_emitted(plugin->id, &plugin->sink, method, params, size, "null");
        }
        return;
    }
    if (plugin->sink.emit == NULL)
    {
        mmgr_line_put(answer, "null");
        return;
    }
    char request[64];
    const int deferred = cli_lsp_answer_later(request, sizeof(request));
    plugins_msg_emitted(plugin->id, &plugin->sink, method, params, size, deferred ? request : "null");
}

// the languages a plugin's server serves read from `server` in `json` into `plugin`, each with the protocol's name
// `ids` gives it, its own where `ids` gives none
static void plugins_languages_read(const CliJson *json, unsigned int server, CliPluginServer *plugin)
{
    const unsigned int languages = cli_json_member(json, server, "languages");
    const unsigned int ids = cli_json_member(json, server, "ids");
    plugin->languages = 0u;
    for (unsigned int item = cli_json_is(json, languages, CFG_JSON_ARRAY) ? json->node[languages].first : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (plugin->languages < CLI_SERVERS_LANGUAGES); item = json->node[item].next)
    {
        const char *const language = cli_json_string(json, item, NULL);
        const char *const named =
            cli_json_string(json, cli_json_member(json, ids, (language != NULL) ? language : ""), NULL);
        if ((language == NULL) ||
            (cellul.len(&(CatenaFinitaCfg){.src = language, .cap = CLI_NAME_MAX_LEN}) >= CLI_NAME_MAX_LEN))
        {
            continue;
        }
        VerbaLinea own = {plugin->language[plugin->languages], CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&own, language);
        (void)mmgr_line_finish(&own);
        VerbaLinea protocol = {plugin->named[plugin->languages], CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&protocol, (named != NULL) ? named : language);
        plugin->languages += (mmgr_line_finish(&protocol) != 0ull) ? 1u : 0u;
    }
}

// A plugin server just started and the language each extension opens as, to hand it the files the editor has open
typedef struct
{
    CliPluginServer *plugin;
    CliLanguages languages;
} CliPluginHanding;

// a file of the editor's handed to the plugin server just started where it serves the file's language
static void plugins_open_handed(void *context, const char *key, const char *beside, const char *text,
                                unsigned long long size)
{
    (void)key;
    CliPluginHanding *const handing = context;
    CliPluginServer *const plugin = handing->plugin;
    char language[CLI_NAME_MAX_LEN];
    const char *const named =
        cli_plugins_language_for(&handing->languages, beside, language) ? plugins_named(plugin, language) : NULL;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if ((named != NULL) && !cli_lsp_has(plugin->lsp, beside))
    {
        (void)cli_lsp_opened(plugin->lsp, beside, named, text, size, &err_msg);
    }
}

int cli_servers_plugin_start(const char *root, const char *id, const char *options, unsigned long long length,
                             const char *const *first, unsigned int firsts, const CliServersSink *sink,
                             VerbaLinea *err_msg)
{
    cli_servers_plugin_stop(id);
    CliPluginRead read = {id, {0}, CLI_JSON_NONE};
    cli_json_begun(&read.json, NULL, 0u, NULL, 0ull);
    cli_plugins_each(plugins_found, &read);
    const unsigned int server = cli_json_member(&read.json, read.root, "server");
    char program[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    CliWords *const words = g_cli_cells->persistent_buf_alloc(sizeof(*words), NULL);
    char *const path = g_cli_cells->persistent_buf_alloc(CLI_PATH_TEXT, NULL);
    int started = 0;
    if (!cli_json_is(&read.json, server, CFG_JSON_OBJECT))
    {
        mmgr_line_put(err_msg, "the plugin brings no server");
    }
    else if ((words != NULL) && (path != NULL) &&
             cli_servers_plugin_program(&read.json, server, program, folder, err_msg))
    {
        *words = (CliWords){{NULL}, 0u, {0}, 0u, 0};
        for (unsigned int at = 0u; at < firsts; at += 1u)
        {
            cli_words_put(words, first[at]);
        }
        cli_servers_plugin_args(&read.json, server, folder, words);
        // the options with each `{folder}` in them the folder the program was found in, as the arguments have it
        const unsigned long long room = length + (CLI_PATH_MAX_LEN * 16ull);
        char *const given = g_cli_cells->persistent_buf_alloc(room, NULL);
        VerbaLinea written = {given, room, 0ull};
        cli_servers_plugin_folder_put(&written, (given != NULL) ? options : "", (given != NULL) ? length : 0ull, folder,
                                      1);
        cli_toolchains_run_path(path, CLI_PATH_TEXT);
        const CliSetting setting[1] = {{"PATH", path}};
        cli_lock_take(&s_plugins_lock);
        unsigned int at = 0u;
        while ((at < CLI_SERVERS_PLUGINS) && s_plugin[at].used)
        {
            at += 1u;
        }
        CliPluginServer *const plugin = (at < CLI_SERVERS_PLUGINS) ? &s_plugin[at] : NULL;
        if (plugin != NULL)
        {
            VerbaLinea named = {plugin->id, CLI_NAME_MAX_LEN, 0ull};
            mmgr_line_put(&named, id);
            (void)mmgr_line_finish(&named);
            plugins_languages_read(&read.json, server, plugin);
            plugin->sink = *sink;
            plugin->lsp = (words->full || (given == NULL) || !mmgr_line_ok(&written))
                              ? NULL
                              : cli_lsp_started(program, words->word, words->count, root, given, written.at, setting,
                                                1u, plugins_recv, plugin, err_msg);
            plugin->used = plugin->lsp != NULL;
            started = plugin->used;
        }
        else
        {
            mmgr_line_put(err_msg, "as many plugin servers run as orior_cli runs at once");
        }
        cli_lock_give(&s_plugins_lock);
        if (started)
        {
            CliPluginHanding handing = {plugin, {NULL, 0u, 0u}};
            cli_plugins_languages(&handing.languages);
            cli_lock_take(&g_cli_servers_texts_lock);
            cli_keyed_each(&g_cli_servers_texts, plugins_open_handed, &handing);
            cli_lock_give(&g_cli_servers_texts_lock);
            cli_plugins_languages_given(&handing.languages);
        }
        g_cli_cells->persistent_buf_release(given, NULL);
    }
    g_cli_cells->persistent_buf_release(path, NULL);
    g_cli_cells->persistent_buf_release(words, NULL);
    cli_json_given(&read.json);
    return started;
}

void cli_servers_plugin_stop(const char *id)
{
    cli_lock_take(&s_plugins_lock);
    const unsigned int at = plugins_place(id);
    CliLsp *const lsp = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].lsp : NULL;
    if (at < CLI_SERVERS_PLUGINS)
    {
        s_plugin[at].used = 0;
        s_plugin[at].lsp = NULL;
    }
    cli_lock_give(&s_plugins_lock);
    if (lsp != NULL)
    {
        cli_lsp_stopped(lsp);
        cli_lsp_unref(lsp);
    }
}

// the server of plugin `id`, referenced, or NULL with why given where none runs
CliLsp *cli_servers_plugin_of(const char *id, VerbaLinea *err_msg)
{
    cli_lock_take(&s_plugins_lock);
    const unsigned int at = plugins_place(id);
    CliLsp *const lsp = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].lsp : NULL;
    if (lsp != NULL)
    {
        cli_lsp_ref(lsp);
    }
    cli_lock_give(&s_plugins_lock);
    if (lsp == NULL)
    {
        mmgr_line_put(err_msg, "no server of the plugin ");
        mmgr_line_put(err_msg, id);
        mmgr_line_put(err_msg, " runs");
    }
    return lsp;
}

int cli_servers_plugin_request(const char *id, const char *method, const char *params, unsigned long long length,
                               unsigned long long wait, VerbaLinea *result, VerbaLinea *err_msg)
{
    CliLsp *const lsp = cli_servers_plugin_of(id, err_msg);
    const int done = (lsp != NULL) && cli_lsp_requested(lsp, method, params, length, wait, result, err_msg);
    if (lsp != NULL)
    {
        cli_lsp_unref(lsp);
    }
    return done;
}

int cli_servers_plugin_notify(const char *id, const char *method, const char *params, unsigned long long length,
                              VerbaLinea *err_msg)
{
    CliLsp *const lsp = cli_servers_plugin_of(id, err_msg);
    const int done = (lsp != NULL) && cli_lsp_notified(lsp, method, params, length, err_msg);
    if (lsp != NULL)
    {
        cli_lsp_unref(lsp);
    }
    return done;
}

int cli_servers_plugin_answer(const char *id, const char *request, const char *result, unsigned long long length,
                              VerbaLinea *err_msg)
{
    CliLsp *const lsp = cli_servers_plugin_of(id, err_msg);
    const int done = (lsp != NULL) && cli_lsp_answer(lsp, request, result, length, err_msg);
    if (lsp != NULL)
    {
        cli_lsp_unref(lsp);
    }
    return done;
}

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

// what the editor did to a file sent to each plugin server that serves it or has it open: how many were notified
static unsigned int plugins_change_sent(const CliPluginsChange *change)
{
    CliLsp *owners[CLI_SERVERS_PLUGINS];
    const char *named[CLI_SERVERS_PLUGINS];
    unsigned int count = 0u;
    cli_lock_take(&s_plugins_lock);
    for (unsigned int at = 0u; at < CLI_SERVERS_PLUGINS; at += 1u)
    {
        const char *const name =
            (s_plugin[at].used && (change->language != NULL)) ? plugins_named(&s_plugin[at], change->language) : NULL;
        if (s_plugin[at].used && ((name != NULL) || cli_lsp_has(s_plugin[at].lsp, change->path)))
        {
            cli_lsp_ref(s_plugin[at].lsp);
            owners[count] = s_plugin[at].lsp;
            named[count] = name;
            count += 1u;
        }
    }
    cli_lock_give(&s_plugins_lock);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const int has = cli_lsp_has(owners[at], change->path);
        if (change->closes && has)
        {
            (void)cli_lsp_closed(owners[at], change->path, &err_msg);
        }
        else if (!change->closes && has)
        {
            (void)cli_lsp_changed(owners[at], change->path, change->text, change->size, &err_msg);
        }
        else if (!change->closes && (named[at] != NULL))
        {
            (void)cli_lsp_opened(owners[at], change->path, named[at], change->text, change->size, &err_msg);
        }
        cli_lsp_unref(owners[at]);
    }
    return count;
}

int cli_servers_plugins_opened(const char *path, const char *language, const char *text, unsigned long long size)
{
    const CliPluginsChange change = {path, language, text, size, 0};
    return plugins_change_sent(&change) > 0u;
}

void cli_servers_plugins_changed(const char *path, const char *text, unsigned long long size)
{
    const CliPluginsChange change = {path, NULL, text, size, 0};
    (void)plugins_change_sent(&change);
}

void cli_servers_plugins_closed(const char *path)
{
    const CliPluginsChange change = {path, NULL, "", 0ull, 1};
    (void)plugins_change_sent(&change);
}

CliLsp *cli_servers_plugin_owner_of(unsigned int *at, const char *path, char *id)
{
    CliLsp *found = NULL;
    cli_lock_take(&s_plugins_lock);
    while ((found == NULL) && (*at < CLI_SERVERS_PLUGINS))
    {
        const CliPluginServer *const plugin = &s_plugin[*at];
        *at += 1u;
        if (plugin->used && cli_lsp_has(plugin->lsp, path))
        {
            cli_lsp_ref(plugin->lsp);
            found = plugin->lsp;
            memor.cpy(&(MemoriaCfg){.dst = id, .src = plugin->id, .bytes = CLI_NAME_MAX_LEN});
        }
    }
    cli_lock_give(&s_plugins_lock);
    return found;
}

void cli_servers_plugin_command(const char *id, const char *command, unsigned long long size)
{
    cli_lock_take(&s_plugins_lock);
    const unsigned int at = plugins_place(id);
    const CliServersSink sink = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].sink : (CliServersSink){NULL, NULL, ""};
    cli_lock_give(&s_plugins_lock);
    if (sink.emit != NULL)
    {
        plugins_msg_emitted(id, &sink, "orior/command", command, size, "null");
    }
}
