// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_files.c: the files the editor has open: a file handed to its language's server, to the plugins' servers of
// its language and its text to the inspections and the checkers as it opens, each new text given, and the file let go
// as the editor closes it, the inspections and orior's own reading reading it again as the disk stores it. Where the
// tree's check is on, a file the editor closes is checked as the disk stores it before its server lets it go. A file
// the check open is the editor's once it opens, and a file its server has open already receives the diagnostics it last
// gave. A server whose spec slices is given the tree's slice for the view in place of each text (servers_slice.c)
#include "servers_internal.h"

// the most files the editor has open
#define CLI_SERVERS_TEXTS 4096u

CliKeyed g_cli_servers_texts = {"editor texts", {NULL}, 0u, CLI_SERVERS_TEXTS};
CliKeyed g_cli_servers_parses = {"parses", {NULL}, 0u, CLI_SERVERS_TEXTS};
CliLock g_cli_servers_texts_lock = CLI_LOCK_INIT;

// the files closed, one at a time, each read where it lies on the disk as the editor closes it
static CliLock s_disk_lock = CLI_LOCK_INIT;

// the text of `path` stored as the editor has it, by its key with its path
static void files_text_stored(const char *path, const char *text, unsigned long long size)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&g_cli_servers_texts_lock);
    (void)cli_keyed_put(&g_cli_servers_texts, key, path, text, size);
    cli_lock_give(&g_cli_servers_texts_lock);
}

// the tree the checkers check and where what is reported goes, where a file was opened: 1, or 0 where none was
static int files_checked_sink(char *root, CliServersSink *sink)
{
    cli_lock_take(&g_cli_servers.lock);
    const int has = g_cli_servers.has_sink && (g_cli_servers.checked_root[0] != '\0');
    if (has)
    {
        memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_servers.checked_root, .bytes = CLI_PATH_MAX_LEN});
        *sink = g_cli_servers.sink;
    }
    cli_lock_give(&g_cli_servers.lock);
    return has;
}

int cli_servers_open(const char *root, const char *path, const char *language, const char *text,
                     unsigned long long size, const CliServersSink *sink, VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_servers_inspect(root, path, text, size, sink);
    files_text_stored(path, text, size);
    cli_lock_take(&g_cli_servers.lock);
    g_cli_servers.sink = *sink;
    g_cli_servers.has_sink = 1;
    VerbaLinea checked = {g_cli_servers.checked_root, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&checked, root);
    (void)mmgr_line_finish(&checked);
    cli_lock_give(&g_cli_servers.lock);
    cli_servers_run_checkers(root, path, text, size, 1, sink);
    cli_servers_check_own(root, path, text, size, sink);
    const int plugged = cli_servers_plugins_opened(path, language, text, size);
    static CliServerSpec s_spec;
    const unsigned long long before = err_msg->at;
    CliLsp *const server = cli_servers_server(root, language, sink, &s_spec, err_msg);
    if (server == NULL)
    {
        // a file a plugin's server took is the editor's to report of its changes and its closing, its language's own
        // server or none
        if ((err_msg->at > before) && !plugged)
        {
            return 0;
        }
        mmgr_line_put(out, plugged ? "true" : "false");
        return 1;
    }
    char id[CLI_NAME_MAX_LEN];
    cli_servers_id_of(&s_spec, path, language, id);
    // a file the check has open is the editor's from here, and its clearing as it closes is received; one the check
    // gives in slices is the editor's to slice for its view, and its diagnostics are passed on as they come
    char uri[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    cli_servers_uri_of(path, uri);
    cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
    cli_lock_take(&g_cli_servers_check_open_lock);
    const int check_had = cli_servers_check_open(uri);
    cli_servers_check_open_set(uri, 0);
    cli_servers_let_go_cleared(key);
    cli_servers_gathering_ended(key);
    if (check_had)
    {
        cli_servers_slice_closed(path);
    }
    int done = 0;
    if (cli_lsp_has(server, path))
    {
        done = s_spec.slices ? cli_servers_slice_sent(server, root, path, NULL, text, size, err_msg)
                             : cli_lsp_changed(server, path, text, size, err_msg);
        // a tab opened again on a file the server has open receives the diagnostics it last gave
        if (done)
        {
            cli_servers_emit_whole(key, uri, sink);
        }
    }
    else
    {
        done = s_spec.slices ? cli_servers_slice_sent(server, root, path, id, text, size, err_msg)
                             : cli_lsp_opened(server, path, id, text, size, err_msg);
    }
    // a file given its server as a slice is opened again as its language in the server started after one let go
    if (done && s_spec.slices)
    {
        cli_servers_language_noted(path, language);
    }
    cli_lock_give(&g_cli_servers_check_open_lock);
    cli_lsp_unref(server);
    if (done)
    {
        mmgr_line_put(out, "true");
    }
    return done;
}

int cli_servers_change(const char *path, const char *text, unsigned long long size, VerbaLinea *err_msg)
{
    cli_servers_inspect_again(path, text, size);
    files_text_stored(path, text, size);
    char root[CLI_PATH_MAX_LEN];
    static CliServersSink s_sink;
    static CliLock s_sink_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_sink_lock);
    if (files_checked_sink(root, &s_sink))
    {
        cli_servers_run_checkers(root, path, text, size, 0, &s_sink);
        cli_servers_check_own(root, path, text, size, &s_sink);
    }
    cli_lock_give(&s_sink_lock);
    cli_servers_plugins_changed(path, text, size);
    cli_lock_take(&g_cli_servers_check_open_lock);
    CliLsp *const server = cli_servers_owner_started(path);
    const int done = (server == NULL) ||
                     (cli_servers_sliced(path) ? cli_servers_slice_sent(server, NULL, path, NULL, text, size, err_msg)
                                               : cli_lsp_changed(server, path, text, size, err_msg));
    cli_lock_give(&g_cli_servers_check_open_lock);
    if (server != NULL)
    {
        cli_lsp_unref(server);
    }
    return done;
}

int cli_servers_close(const char *path, VerbaLinea *err_msg)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&g_cli_servers_texts_lock);
    cli_keyed_remove(&g_cli_servers_texts, key);
    cli_keyed_remove(&g_cli_servers_parses, key);
    cli_lock_give(&g_cli_servers_texts_lock);
    cli_lock_take(&s_disk_lock);
    unsigned long long read = 0ull;
    const unsigned char *const mapped = cli_file_read(path, &read);
    const char *const disk = (const char *)mapped;
    const int whole = (mapped != NULL) && cli_utf8_whole(disk, read);
    cli_servers_inspect_again(path, whole ? disk : NULL, whole ? read : 0ull);
    char root[CLI_PATH_MAX_LEN];
    static CliServersSink s_sink;
    if (whole && files_checked_sink(root, &s_sink))
    {
        cli_servers_check_own(root, path, disk, read, &s_sink);
    }
    if (mapped != NULL)
    {
        cli_file_let_go(mapped, read);
    }
    cli_lock_give(&s_disk_lock);
    cli_servers_plugins_closed(path);
    // a file the editor closes is the check's to give its server, in slices of the disk's text where it slices; a
    // server let go is not started again to close it
    cli_servers_slice_closed(path);
    cli_servers_language_forgotten(path);
    CliLsp *const server = cli_servers_owner_of(path);
    if (server == NULL)
    {
        return 1;
    }
    cli_lock_take(&g_cli_servers_check_open_lock);
    const int done = cli_servers_check_takes(path, server) || cli_lsp_closed(server, path, err_msg);
    cli_lock_give(&g_cli_servers_check_open_lock);
    cli_lsp_unref(server);
    return done;
}

int cli_servers_text_of(const char *path, VerbaLinea *out)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&g_cli_servers_texts_lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const text = cli_keyed_get(&g_cli_servers_texts, key, &beside, &size);
    if (text != NULL)
    {
        mmgr_line_put_n(out, text, size);
    }
    cli_lock_give(&g_cli_servers_texts_lock);
    return text != NULL;
}

char *cli_servers_text_taken(const char *path, unsigned long long *size)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&g_cli_servers_texts_lock);
    const char *beside = NULL;
    const char *const text = cli_keyed_get(&g_cli_servers_texts, key, &beside, size);
    char *const taken = (text != NULL) ? g_cli_cells->persistent_buf_alloc(*size + 1ull, &g_cli_servers_cells) : NULL;
    if (taken != NULL)
    {
        // a size the keeper stored fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = taken, .src = text, .bytes = (size_t)*size});
        taken[*size] = '\0';
    }
    cli_lock_give(&g_cli_servers_texts_lock);
    return taken;
}
