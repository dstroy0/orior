// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_save.c: a server given the tree's slices verifies what is written and does nothing else: it is given slices
// and publishes their problems, and no request reaches it. It runs only while it is asked: each slice it is given is an
// ask, and a file the tree's check slices waits on it until the check lets the file go. A watcher lets it go once
// nothing waits on it, no slice owes its diagnostics, the tree's check is not running and it has been quiet for as long
// as its asks of the last minute earn it: CLI_SERVERS_QUIET_LEAST, and CLI_SERVERS_QUIET_EACH more for each ask,
// CLI_SERVERS_QUIET_MOST at most. It is ended with no file closed, so what it published stands. The next slice of a
// file it was given starts it again, the file opened with its stored slice and the editor's text, the language each
// sliced file opened as stored by its key
#include "../command_line/cells.h"
#include "servers_internal.h"

// how often the watcher looks, in milliseconds, and the most sliced files whose language is stored
#define CLI_SERVERS_SAVE_LOOK 500ull
#define CLI_SERVERS_SAVE_FILES 256u

// the languages the sliced files opened as, by their keys, under their lock; the lock a server is started again and a
// file opened in it under; and the watcher, whether it was started, and the lock it is started under
static CliKeyed s_languages = {"sliced languages", {NULL}, 0u, CLI_SERVERS_SAVE_FILES};
static CliLock s_languages_lock = CLI_LOCK_INIT;
static CliLock s_starting_lock = CLI_LOCK_INIT;
static CliThread s_watcher;
static int s_watching;
static CliLock s_watching_lock = CLI_LOCK_INIT;

typedef struct
{
    const CliLsp *server;
    const CliServerRunning *one;
    unsigned long long now;
    int waits;
    const char *path;
    char *language;
} ServersSaveCtx;
// the running place of `server` given the tree's slices, CLI_SERVERS_MOST where none is; with the servers' lock taken
EMBED_INLINE unsigned int save_place(const ServersSaveCtx *args)
{
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        const CliServerRunning *const one = &g_cli_servers.running[at];
        if (one->used && (one->lsp == args->server) && one->spec.slices)
        {
            return at;
        }
    }
    return CLI_SERVERS_MOST;
}

// how many asks of the running place `one` came within the last minute, `now` the time
EMBED_INLINE unsigned int save_asks_recent(const ServersSaveCtx *args)
{
    const unsigned int saved = (args->one->asks < CLI_SERVERS_ASKS) ? (unsigned int)args->one->asks : CLI_SERVERS_ASKS;
    unsigned int recent = 0u;
    for (unsigned int at = 0u; at < saved; at += 1u)
    {
        recent += ((args->now - args->one->asked[at]) <= CLI_SERVERS_ASKS_WITHIN) ? 1u : 0u;
    }
    return recent;
}

// the quiet the running place `one` is let go after, `now` the time
EMBED_INLINE unsigned long long save_quiet(const ServersSaveCtx *args)
{
    const unsigned long long earned =
        CLI_SERVERS_QUIET_LEAST + ((unsigned long long)save_asks_recent(args) * CLI_SERVERS_QUIET_EACH);
    return (earned < CLI_SERVERS_QUIET_MOST) ? earned : CLI_SERVERS_QUIET_MOST;
}

// the servers given the tree's slices that may be let go looked at every CLI_SERVERS_SAVE_LOOK, each let go once
// nothing waits on it and it has been quiet long enough
static void save_watch(void *context)
{
    (void)context;
    for (;;)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx,
                   .milliseconds = (unsigned int)CLI_SERVERS_SAVE_LOOK);
        // a slice owing its diagnostics, or the tree's check running, saves every one
        if (cli_servers_slices_owed() || cli_servers_checking())
        {
            continue;
        }
        CliLsp *going[CLI_SERVERS_MOST];
        unsigned int goes = 0u;
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers.lock);
        const unsigned long long now = cli_clock_milliseconds();
        for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
        {
            CliServerRunning *const one = &g_cli_servers.running[at];
            if (one->used && one->spec.slices && (one->waits == 0u) &&
                ((now - one->quiet_from) >= EMBED_CALL(save_quiet, ServersSaveCtx, .one = one, .now = now)))
            {
                one->used = 0;
                going[goes] = one->lsp;
                goes += 1u;
            }
        }
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers.lock);
        for (unsigned int at = 0u; at < goes; at += 1u)
        {
            EMBED_CALL(cli_servers_path_lsp_internal.stop_later, CliServersPathLspCtx, .lsp = going[at]);
        }
    }
}

// the watcher started, once
static void save_watcher_start(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_watching_lock);
    if (!s_watching)
    {
        s_watcher = (CliThread){save_watch, NULL};
        s_watching = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_watcher);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_watching_lock);
}

// an ask of `server` counted: its time put in its ring and its quiet begun again, and the waits on it moved by `waits`
EMBED_INLINE void save_request(const ServersSaveCtx *args)
{
    int waits = args->waits;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers.lock);
    const unsigned int at = save_place(args);
    if (at < CLI_SERVERS_MOST)
    {
        CliServerRunning *const one = &g_cli_servers.running[at];
        const unsigned long long now = cli_clock_milliseconds();
        if (waits >= 0)
        {
            one->asked[one->asks % CLI_SERVERS_ASKS] = now;
            one->asks += 1ull;
        }
        one->waits =
            (waits > 0) ? (one->waits + 1u) : (((waits < 0) && (one->waits > 0u)) ? (one->waits - 1u) : one->waits);
        one->quiet_from = now;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers.lock);
    if (at < CLI_SERVERS_MOST)
    {
        save_watcher_start();
    }
}

void cli_servers_ask_begin(const CliServersCtx *args)
{
    EMBED_CALL(save_request, ServersSaveCtx, .server = args->server_read, .waits = 1);
}

void cli_servers_ask_end(const CliServersCtx *args)
{
    EMBED_CALL(save_request, ServersSaveCtx, .server = args->server_read, .waits = -1);
}

void cli_servers_ask_note(const CliServersCtx *args)
{
    EMBED_CALL(save_request, ServersSaveCtx, .server = args->server_read, .waits = 0);
}

void cli_servers_language_note(const CliServersPathLanguageCtx *args)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_languages_lock);
    (void)EMBED_CALL(cli_keyed_key_beside.put, CliKeyedKeyBesideCfg, .keyed_out = &s_languages, .key = key,
                     .beside_value = args->language, .text = "", .size_value = 0ull);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_languages_lock);
}

void cli_servers_language_forgotten(const CliServersPathLspCtx *args)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_languages_lock);
    EMBED_CALL(cli_keyed_key_beside.remove, CliKeyedKeyBesideCfg, .keyed_out = &s_languages, .key = key);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_languages_lock);
}

// the language the sliced file at `path` opened as into `language`, sized for CLI_NAME_MAX_LEN: 1, or 0 where none is
// stored
EMBED_INLINE int save_language_get(const ServersSaveCtx *args)
{
    const char *path = args->path;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = path, .key_out = key);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_languages_lock);
    const char *named = NULL;
    unsigned long long size = 0ull;
    const int found = (EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = &s_languages, .key = key,
                                  .beside = &named, .size = &size) != NULL) &&
                      (named != NULL);
    VerbaLinea line = {args->language, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, found ? named : "");
    const int whole = mmgr_line_finish(&line) != 0ull;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_languages_lock);
    return found && whole;
}

CliLsp *cli_servers_owner_start(const CliServersPathLspCtx *args)
{
    const char *path = args->path;
    CliLsp *const running = EMBED_CALL(cli_servers_path_lsp_internal.owner_get, CliServersPathLspCtx, .path = path);
    if ((running != NULL) || !EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path))
    {
        return running;
    }
    char language[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(save_language_get, ServersSaveCtx, .path = path, .language = language))
    {
        return NULL;
    }
    // two asks of the file at once start it and open it once
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_starting_lock);
    CliLsp *server = EMBED_CALL(cli_servers_path_lsp_internal.owner_get, CliServersPathLspCtx, .path = path);
    if (server == NULL)
    {
        char root[CLI_PATH_MAX_LEN];
        CliServersSink sink;
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers.lock);
        memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_servers.root, .bytes = sizeof(root)});
        const int has_sink = g_cli_servers.has_sink;
        sink = g_cli_servers.sink;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers.lock);
        CliServerSpec *const spec =
            has_sink ? g_cli_cells->temporary_buf_alloc(sizeof(CliServerSpec), &g_cli_servers_cells) : NULL;
        char why[1024];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        server = (spec != NULL) ? EMBED_CALL(cli_servers_language_internal.server, CliServersLanguageCtx, .root = root,
                                             .language = language, .sink = &sink, .spec = spec, .err_msg = &err_msg)
                                : NULL;
        unsigned long long size = 0ull;
        char *const text = (server != NULL) ? EMBED_CALL(cli_servers_path_size_kind.text_take,
                                                         CliServersPathSizeKindCfg, .path = path, .size_pointer = &size)
                                            : NULL;
        char id[CLI_NAME_MAX_LEN];
        if (text != NULL)
        {
            EMBED_CALL(cli_servers_path_language_internal.id_get, CliServersPathLanguageCtx, .spec_read = spec,
                       .path = path, .language = language, .id_out = id);
        }
        // a server that could not be given the file gives no answer about it
        if ((server != NULL) &&
            ((text == NULL) ||
             !EMBED_CALL(cli_servers_path_size_root_internal.slice_send, CliServersPathSizeRootCtx, .server = server,
                         .root = root, .path = path, .id = id, .text = text, .size = size, .err_msg = &err_msg)))
        {
            EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
            server = NULL;
        }
        g_cli_cells->persistent_buf_release(text, &g_cli_servers_cells);
        g_cli_cells->persistent_buf_release(spec, &g_cli_servers_cells);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_starting_lock);
    return server;
}

CliLsp *cli_servers_owner_answering(const CliServersPathLspCtx *args)
{
    if (cli_servers_path_lsp_internal.slice(args))
    {
        return NULL;
    }
    CliLsp *const server = cli_servers_path_lsp_internal.owner_get(args);
    if ((server != NULL) && EMBED_CALL(cli_servers_internal.slicing, CliServersCtx, .server_read = server))
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
        return NULL;
    }
    return server;
}
