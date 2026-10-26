// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_keep.c: a server given the tree's slices verifies what is written and does nothing else: it is given slices
// and publishes their problems, and no request reaches it. It runs only while it is asked: each slice it is given is an
// ask, and a file the tree's check slices waits on it until the check lets the file go. A watcher lets it go once
// nothing waits on it, no slice owes its diagnostics, the tree's check is not running and it has been quiet for as long
// as its asks of the last minute earn it: CLI_SERVERS_QUIET_LEAST, and CLI_SERVERS_QUIET_EACH more for each ask,
// CLI_SERVERS_QUIET_MOST at most. It is ended with no file closed, so what it published stands. The next slice of a file
// it was given starts it again, the file opened with its stored slice and the editor's text, the language each sliced
// file opened as stored by its key
#include "../command_line/cells.h"
#include "servers_internal.h"

// how often the watcher looks, in milliseconds, and the most sliced files whose language is stored
#define CLI_SERVERS_KEEP_LOOK 500ull
#define CLI_SERVERS_KEEP_FILES 256u

// the languages the sliced files opened as, by their keys, under their lock; the lock a server is started again and a
// file opened in it under; and the watcher, whether it was started, and the lock it is started under
static CliKeyed s_languages = {"sliced languages", {NULL}, 0u, CLI_SERVERS_KEEP_FILES};
static CliLock s_languages_lock = CLI_LOCK_INIT;
static CliLock s_starting_lock = CLI_LOCK_INIT;
static CliThread s_watcher;
static int s_watching;
static CliLock s_watching_lock = CLI_LOCK_INIT;

// the running place of `server` given the tree's slices, CLI_SERVERS_MOST where none is; with the servers' lock taken
static unsigned int keep_place(const CliLsp *server)
{
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        const CliServerRunning *const one = &g_cli_servers.running[at];
        if (one->used && (one->lsp == server) && one->spec.slices)
        {
            return at;
        }
    }
    return CLI_SERVERS_MOST;
}

// how many asks of the running place `one` came within the last minute, `now` the time
static unsigned int keep_asks_recent(const CliServerRunning *one, unsigned long long now)
{
    const unsigned int held = (one->asks < CLI_SERVERS_ASKS) ? (unsigned int)one->asks : CLI_SERVERS_ASKS;
    unsigned int recent = 0u;
    for (unsigned int at = 0u; at < held; at += 1u)
    {
        recent += ((now - one->asked[at]) <= CLI_SERVERS_ASKS_WITHIN) ? 1u : 0u;
    }
    return recent;
}

// the quiet the running place `one` is let go after, `now` the time
static unsigned long long keep_quiet(const CliServerRunning *one, unsigned long long now)
{
    const unsigned long long earned =
        CLI_SERVERS_QUIET_LEAST + ((unsigned long long)keep_asks_recent(one, now) * CLI_SERVERS_QUIET_EACH);
    return (earned < CLI_SERVERS_QUIET_MOST) ? earned : CLI_SERVERS_QUIET_MOST;
}

// the servers given the tree's slices that may be let go looked at every CLI_SERVERS_KEEP_LOOK, each let go once
// nothing waits on it and it has been quiet long enough
static void keep_watched(void *context)
{
    (void)context;
    for (;;)
    {
        query_paused((unsigned int)CLI_SERVERS_KEEP_LOOK);
        // a slice owing its diagnostics, or the tree's check running, keeps every one
        if (cli_servers_slices_owed() || cli_servers_checking())
        {
            continue;
        }
        CliLsp *going[CLI_SERVERS_MOST];
        unsigned int goes = 0u;
        cli_lock_take(&g_cli_servers.lock);
        const unsigned long long now = cli_clock_milliseconds();
        for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
        {
            CliServerRunning *const one = &g_cli_servers.running[at];
            if (one->used && one->spec.slices && (one->waits == 0u) &&
                ((now - one->quiet_from) >= keep_quiet(one, now)))
            {
                one->used = 0;
                going[goes] = one->lsp;
                goes += 1u;
            }
        }
        cli_lock_give(&g_cli_servers.lock);
        for (unsigned int at = 0u; at < goes; at += 1u)
        {
            cli_servers_stop_later(going[at]);
        }
    }
}

// the watcher started, once
static void keep_watcher_started(void)
{
    cli_lock_take(&s_watching_lock);
    if (!s_watching)
    {
        s_watcher = (CliThread){keep_watched, NULL};
        s_watching = cli_thread_started(&s_watcher);
    }
    cli_lock_give(&s_watching_lock);
}

// an ask of `server` counted: its time put in its ring and its quiet begun again, and the waits on it moved by `waits`
static void keep_asked(const CliLsp *server, int waits)
{
    cli_lock_take(&g_cli_servers.lock);
    const unsigned int at = keep_place(server);
    if (at < CLI_SERVERS_MOST)
    {
        CliServerRunning *const one = &g_cli_servers.running[at];
        const unsigned long long now = cli_clock_milliseconds();
        if (waits >= 0)
        {
            one->asked[one->asks % CLI_SERVERS_ASKS] = now;
            one->asks += 1ull;
        }
        one->waits = (waits > 0) ? (one->waits + 1u) : (((waits < 0) && (one->waits > 0u)) ? (one->waits - 1u)
                                                                                            : one->waits);
        one->quiet_from = now;
    }
    cli_lock_give(&g_cli_servers.lock);
    if (at < CLI_SERVERS_MOST)
    {
        keep_watcher_started();
    }
}

void cli_servers_ask_begun(const CliLsp *server)
{
    keep_asked(server, 1);
}

void cli_servers_ask_ended(const CliLsp *server)
{
    keep_asked(server, -1);
}

void cli_servers_ask_noted(const CliLsp *server)
{
    keep_asked(server, 0);
}

void cli_servers_language_noted(const char *path, const char *language)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_languages_lock);
    (void)cli_keyed_put(&s_languages, key, language, "", 0ull);
    cli_lock_give(&s_languages_lock);
}

void cli_servers_language_forgotten(const char *path)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_languages_lock);
    cli_keyed_remove(&s_languages, key);
    cli_lock_give(&s_languages_lock);
}

// the language the sliced file at `path` opened as into `language`, sized for CLI_NAME_MAX_LEN: 1, or 0 where none is
// stored
static int keep_language_of(const char *path, char *language)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_languages_lock);
    const char *named = NULL;
    unsigned long long size = 0ull;
    const int found = (cli_keyed_get(&s_languages, key, &named, &size) != NULL) && (named != NULL);
    VerbaLinea line = {language, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, found ? named : "");
    const int whole = mmgr_line_finish(&line) != 0ull;
    cli_lock_give(&s_languages_lock);
    return found && whole;
}

CliLsp *cli_servers_owner_started(const char *path)
{
    CliLsp *const running = cli_servers_owner_of(path);
    if ((running != NULL) || !cli_servers_sliced(path))
    {
        return running;
    }
    char language[CLI_NAME_MAX_LEN];
    if (!keep_language_of(path, language))
    {
        return NULL;
    }
    // two asks of the file at once start it and open it once
    cli_lock_take(&s_starting_lock);
    CliLsp *server = cli_servers_owner_of(path);
    if (server == NULL)
    {
        char root[CLI_PATH_MAX_LEN];
        CliServersSink sink;
        cli_lock_take(&g_cli_servers.lock);
        memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_servers.root, .bytes = sizeof(root)});
        const int has_sink = g_cli_servers.has_sink;
        sink = g_cli_servers.sink;
        cli_lock_give(&g_cli_servers.lock);
        CliServerSpec *const spec = has_sink ? g_cli_cells->persistent_buf_alloc(sizeof(CliServerSpec), NULL) : NULL;
        char why[1024];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        server = (spec != NULL) ? cli_servers_server(root, language, &sink, spec, &err_msg) : NULL;
        unsigned long long size = 0ull;
        char *const text = (server != NULL) ? cli_servers_text_taken(path, &size) : NULL;
        char id[CLI_NAME_MAX_LEN];
        if (text != NULL)
        {
            cli_servers_id_of(spec, path, language, id);
        }
        // a server that could not be given the file answers none about it
        if ((server != NULL) && ((text == NULL) || !cli_servers_slice_sent(server, root, path, id, text, size, &err_msg)))
        {
            cli_lsp_unref(server);
            server = NULL;
        }
        g_cli_cells->persistent_buf_release(text, NULL);
        g_cli_cells->persistent_buf_release(spec, NULL);
    }
    cli_lock_give(&s_starting_lock);
    return server;
}

CliLsp *cli_servers_owner_answering(const char *path)
{
    if (cli_servers_sliced(path))
    {
        return NULL;
    }
    CliLsp *const server = cli_servers_owner_of(path);
    if ((server != NULL) && cli_servers_slicing(server))
    {
        cli_lsp_unref(server);
        return NULL;
    }
    return server;
}
