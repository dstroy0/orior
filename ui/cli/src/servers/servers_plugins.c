// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_plugins.c: the language servers script plugins bring. A plugin's `server` in its plugin.json names its
// `program`, found by the toolchain `toolchain` names, the `args` it starts with, `{folder}` in one and in the options
// standing for the folder the program was found in, the `languages` it serves, `*` every one, and, in `ids`, the
// protocol's name of a language where it is not orior's. The window starts one as it starts its plugin, at the tree's
// top, with the options the plugin gives, and stops it as it stops the plugin. It is handed every file of its languages
// the editor holds as it starts, and each the editor opens, changes and closes after; its diagnostics are kept as a
// checker's findings under the plugin's id; and what else it says goes to the window as the event `plugin-server`,
// `{plugin, method, params, request}`, `request` the id of a request held until the plugin answers it, null for a
// notification
#include "../plugins/plugins.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

// the most plugins whose servers run at once
#define CLI_SERVERS_PLUGINS 8u

// A plugin's server running: whether its place is taken, its plugin's id, the server, the languages it serves and the
// protocol's name of each, and where what it says goes
typedef struct
{
    int used;
    char id[CLI_NAME_LONGEST];
    CliLsp *lsp;
    char language[CLI_SERVERS_LANGUAGES][CLI_NAME_LONGEST];
    char named[CLI_SERVERS_LANGUAGES][CLI_NAME_LONGEST];
    unsigned int languages;
    CliServersEmit emit;
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
    if (!cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = read->id, .cap = CLI_NAME_LONGEST}))
    {
        return;
    }
    cli_json_given(&read->json);
    cli_json_begun(&read->json, NULL, 0u, NULL, 0ull);
    char quiet[256];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    read->root = cli_json_taken(&read->json, plugin->text, plugin->size, 0u, 0ull, &said);
}

// the place of the running server of plugin `id`, or CLI_SERVERS_PLUGINS where none runs. Under the plugins' lock
static unsigned int plugins_place(const char *id)
{
    unsigned int at = 0u;
    while ((at < CLI_SERVERS_PLUGINS) &&
           !(s_plugin[at].used &&
             cellul.eq(&(CatenaFinitaCfg){.src = s_plugin[at].id, .other = id, .cap = CLI_NAME_LONGEST})))
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
        if (cellul.eq(&(CatenaFinitaCfg){.src = plugin->language[at], .other = language, .cap = CLI_NAME_LONGEST}))
        {
            return plugin->named[at];
        }
        every = every || cellul.eq(&(CatenaFinitaCfg){.src = plugin->language[at], .other = "*", .cap = 2u});
    }
    return every ? language : NULL;
}

// a server's diagnostics of a file kept as its plugin's findings, and the file's diagnostics passed on whole
static void plugins_diagnostics(const CliPluginServer *plugin, const char *params, unsigned long long size)
{
    unsigned long long uri_at = 0ull;
    unsigned long long uri_length = 0ull;
    unsigned long long all_at = 0ull;
    unsigned long long all_length = 0ull;
    char uri[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (!cli_lsp_member(params, size, "uri", &uri_at, &uri_length) ||
        !cli_lsp_member(params, size, "diagnostics", &all_at, &all_length) ||
        !cli_lsp_string(params + uri_at, uri_length, uri, sizeof(uri)) ||
        !cli_lsp_path_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), path))
    {
        return;
    }
    CliLine found = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_line_growing(&found, all_length + CLI_PATH_LONGEST, "a plugin server's diagnostics");
    cli_line_text(&found, "[[");
    cli_json_quoted_lossy(&found, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_character(&found, ',');
    cli_line_counted(&found, params + all_at, all_length);
    cli_line_text(&found, "]]");
    if (cli_line_fits(&found))
    {
        cli_servers_keep_checked(plugin->id, found.out, found.at, 1, &plugin->emit);
    }
    cli_cell_given(found.out);
}

// what the server of the plugin `id` says, `method` with `size` bytes of `params`, passed to the window where `emit`
// sends it as `plugin-server`, `request` the id of the request held or null
static void plugins_said(const char *id, const CliServersEmit *emit, const char *method, const char *params,
                         unsigned long long size, const char *request)
{
    CliLine told = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_line_growing(&told, size + 256ull, "what a plugin's server says");
    cli_line_text(&told, "{\"plugin\":");
    cli_json_quoted_lossy(&told, (const unsigned char *)id,
                          cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_NAME_LONGEST}));
    cli_line_text(&told, ",\"method\":");
    cli_json_quoted_lossy(&told, (const unsigned char *)method,
                          cellul.len(&(CatenaFinitaCfg){.src = method, .cap = CLI_CELLS_POOL}));
    cli_line_text(&told, ",\"params\":");
    cli_line_counted(&told, params, size);
    cli_line_text(&told, ",\"request\":");
    cli_line_text(&told, request);
    cli_line_character(&told, '}');
    if (cli_line_fits(&told))
    {
        emit->told(emit->context, "plugin-server", told.out, told.at);
    }
    cli_cell_given(told.out);
}

// what a plugin's server says on its own: its diagnostics kept, and all it says passed to the window, a request held
// unanswered for the plugin to answer where the window hears it, and answered with null where it does not
static void plugins_heard(void *context, const char *method, const char *params, unsigned long long size,
                          CliLine *answer)
{
    const CliPluginServer *const plugin = context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "textDocument/publishDiagnostics", .cap = CLI_CELLS_POOL}))
    {
        plugins_diagnostics(plugin, params, size);
        // and heard by the plugin as well, which lists them as its own findings
        if (plugin->emit.told != NULL)
        {
            plugins_said(plugin->id, &plugin->emit, method, params, size, "null");
        }
        return;
    }
    if (plugin->emit.told == NULL)
    {
        cli_line_text(answer, "null");
        return;
    }
    char request[64];
    const int asked = cli_lsp_answer_later(request, sizeof(request));
    plugins_said(plugin->id, &plugin->emit, method, params, size, asked ? request : "null");
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
            (cellul.len(&(CatenaFinitaCfg){.src = language, .cap = CLI_NAME_LONGEST}) >= CLI_NAME_LONGEST))
        {
            continue;
        }
        CliLine own = {plugin->language[plugin->languages], CLI_NAME_LONGEST, 0ull};
        cli_line_text(&own, language);
        (void)cli_line_closed(&own);
        CliLine protocol = {plugin->named[plugin->languages], CLI_NAME_LONGEST, 0ull};
        cli_line_text(&protocol, (named != NULL) ? named : language);
        plugin->languages += (cli_line_closed(&protocol) != 0ull) ? 1u : 0u;
    }
}

// A plugin server just started and the language each extension opens as, to hand it the files the editor holds
typedef struct
{
    CliPluginServer *plugin;
    CliLanguages languages;
} CliPluginHanding;

// a file of the editor's handed to the plugin server just started where it serves the file's language
static void plugins_held_handed(void *context, const char *key, const char *beside, const char *text,
                                unsigned long long size)
{
    (void)key;
    CliPluginHanding *const handing = context;
    CliPluginServer *const plugin = handing->plugin;
    char language[CLI_NAME_LONGEST];
    const char *const named =
        cli_plugins_language_for(&handing->languages, beside, language) ? plugins_named(plugin, language) : NULL;
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    if ((named != NULL) && !cli_lsp_has(plugin->lsp, beside))
    {
        (void)cli_lsp_opened(plugin->lsp, beside, named, text, size, &said);
    }
}

int cli_servers_plugin_start(const char *root, const char *id, const char *options, unsigned long long length,
                             const char *const *first, unsigned int firsts, const CliServersEmit *emit, CliLine *said)
{
    cli_servers_plugin_stop(id);
    CliPluginRead read = {id, {0}, CLI_JSON_NONE};
    cli_json_begun(&read.json, NULL, 0u, NULL, 0ull);
    cli_plugins_each(plugins_found, &read);
    const unsigned int server = cli_json_member(&read.json, read.root, "server");
    char program[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    CliWords *const words = cli_cell_taken(sizeof(*words), "a plugin server's words");
    char *const path = cli_cell_taken(CLI_PATH_TEXT, "a plugin server's PATH");
    int started = 0;
    if (!cli_json_is(&read.json, server, CFG_JSON_OBJECT))
    {
        cli_line_text(said, "the plugin brings no server");
    }
    else if ((words != NULL) && (path != NULL) && cli_servers_plugin_program(&read.json, server, program, folder, said))
    {
        *words = (CliWords){{NULL}, 0u, {0}, 0u, 0};
        for (unsigned int at = 0u; at < firsts; at += 1u)
        {
            cli_words_put(words, first[at]);
        }
        cli_servers_plugin_args(&read.json, server, folder, words);
        // the options with each `{folder}` in them the folder the program was found in, as the arguments have it
        const unsigned long long room = length + (CLI_PATH_LONGEST * 16ull);
        char *const given = cli_cell_taken(room, "a plugin server's options");
        CliLine written = {given, room, 0ull};
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
            CliLine named = {plugin->id, CLI_NAME_LONGEST, 0ull};
            cli_line_text(&named, id);
            (void)cli_line_closed(&named);
            plugins_languages_read(&read.json, server, plugin);
            plugin->emit = *emit;
            plugin->lsp = (words->full || (given == NULL) || !cli_line_fits(&written))
                              ? NULL
                              : cli_lsp_started(program, words->word, words->count, root, given, written.at, setting,
                                                1u, plugins_heard, plugin, said);
            plugin->used = plugin->lsp != NULL;
            started = plugin->used;
        }
        else
        {
            cli_line_text(said, "as many plugin servers run as orior_cli runs at once");
        }
        cli_lock_give(&s_plugins_lock);
        if (started)
        {
            CliPluginHanding handing = {plugin, {NULL, 0u, 0u}};
            cli_plugins_languages(&handing.languages);
            cli_lock_take(&g_cli_servers_texts_lock);
            cli_keyed_each(&g_cli_servers_texts, plugins_held_handed, &handing);
            cli_lock_give(&g_cli_servers_texts_lock);
            cli_plugins_languages_given(&handing.languages);
        }
        cli_cell_given(given);
    }
    cli_cell_given(path);
    cli_cell_given(words);
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
        cli_lsp_given(lsp);
    }
}

// the server of plugin `id`, held, or NULL with why said where none runs
CliLsp *cli_servers_plugin_of(const char *id, CliLine *said)
{
    cli_lock_take(&s_plugins_lock);
    const unsigned int at = plugins_place(id);
    CliLsp *const lsp = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].lsp : NULL;
    if (lsp != NULL)
    {
        cli_lsp_held(lsp);
    }
    cli_lock_give(&s_plugins_lock);
    if (lsp == NULL)
    {
        cli_line_text(said, "no server of the plugin ");
        cli_line_text(said, id);
        cli_line_text(said, " runs");
    }
    return lsp;
}

int cli_servers_plugin_request(const char *id, const char *method, const char *params, unsigned long long length,
                               unsigned long long wait, CliLine *result, CliLine *said)
{
    CliLsp *const lsp = cli_servers_plugin_of(id, said);
    const int done = (lsp != NULL) && cli_lsp_requested(lsp, method, params, length, wait, result, said);
    if (lsp != NULL)
    {
        cli_lsp_given(lsp);
    }
    return done;
}

int cli_servers_plugin_notify(const char *id, const char *method, const char *params, unsigned long long length,
                              CliLine *said)
{
    CliLsp *const lsp = cli_servers_plugin_of(id, said);
    const int done = (lsp != NULL) && cli_lsp_notified(lsp, method, params, length, said);
    if (lsp != NULL)
    {
        cli_lsp_given(lsp);
    }
    return done;
}

int cli_servers_plugin_answer(const char *id, const char *request, const char *result, unsigned long long length,
                              CliLine *said)
{
    CliLsp *const lsp = cli_servers_plugin_of(id, said);
    const int done = (lsp != NULL) && cli_lsp_answer(lsp, request, result, length, said);
    if (lsp != NULL)
    {
        cli_lsp_given(lsp);
    }
    return done;
}

// What the editor does to a file, told to each plugin server: the file, its language where it opens, its text and its
// size where it opens or changes, and whether it closes
typedef struct
{
    const char *path;
    const char *language;
    const char *text;
    unsigned long long size;
    int closes;
} CliPluginsTold;

// what the editor did to a file told to each plugin server that serves it or holds it: how many were told
static unsigned int plugins_told(const CliPluginsTold *told)
{
    CliLsp *held[CLI_SERVERS_PLUGINS];
    const char *named[CLI_SERVERS_PLUGINS];
    unsigned int count = 0u;
    cli_lock_take(&s_plugins_lock);
    for (unsigned int at = 0u; at < CLI_SERVERS_PLUGINS; at += 1u)
    {
        const char *const name =
            (s_plugin[at].used && (told->language != NULL)) ? plugins_named(&s_plugin[at], told->language) : NULL;
        if (s_plugin[at].used && ((name != NULL) || cli_lsp_has(s_plugin[at].lsp, told->path)))
        {
            cli_lsp_held(s_plugin[at].lsp);
            held[count] = s_plugin[at].lsp;
            named[count] = name;
            count += 1u;
        }
    }
    cli_lock_give(&s_plugins_lock);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const int has = cli_lsp_has(held[at], told->path);
        if (told->closes && has)
        {
            (void)cli_lsp_closed(held[at], told->path, &said);
        }
        else if (!told->closes && has)
        {
            (void)cli_lsp_changed(held[at], told->path, told->text, told->size, &said);
        }
        else if (!told->closes && (named[at] != NULL))
        {
            (void)cli_lsp_opened(held[at], told->path, named[at], told->text, told->size, &said);
        }
        cli_lsp_given(held[at]);
    }
    return count;
}

int cli_servers_plugins_opened(const char *path, const char *language, const char *text, unsigned long long size)
{
    const CliPluginsTold told = {path, language, text, size, 0};
    return plugins_told(&told) > 0u;
}

void cli_servers_plugins_changed(const char *path, const char *text, unsigned long long size)
{
    const CliPluginsTold told = {path, NULL, text, size, 0};
    (void)plugins_told(&told);
}

void cli_servers_plugins_closed(const char *path)
{
    const CliPluginsTold told = {path, NULL, "", 0ull, 1};
    (void)plugins_told(&told);
}

CliLsp *cli_servers_plugin_holding(unsigned int *at, const char *path, char *id)
{
    CliLsp *found = NULL;
    cli_lock_take(&s_plugins_lock);
    while ((found == NULL) && (*at < CLI_SERVERS_PLUGINS))
    {
        const CliPluginServer *const plugin = &s_plugin[*at];
        *at += 1u;
        if (plugin->used && cli_lsp_has(plugin->lsp, path))
        {
            cli_lsp_held(plugin->lsp);
            found = plugin->lsp;
            memor.cpy(&(MemoriaCfg){.dst = id, .src = plugin->id, .bytes = CLI_NAME_LONGEST});
        }
    }
    cli_lock_give(&s_plugins_lock);
    return found;
}

void cli_servers_plugin_command(const char *id, const char *command, unsigned long long size)
{
    cli_lock_take(&s_plugins_lock);
    const unsigned int at = plugins_place(id);
    const CliServersEmit emit = (at < CLI_SERVERS_PLUGINS) ? s_plugin[at].emit : (CliServersEmit){NULL, NULL, ""};
    cli_lock_give(&s_plugins_lock);
    if (emit.told != NULL)
    {
        plugins_said(id, &emit, "orior/command", command, size, "null");
    }
}
