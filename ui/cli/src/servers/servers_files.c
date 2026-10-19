// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_files.c: the files the editor holds: a file handed to its language's server and its text to the inspections
// and the checkers as it opens, each new text given, and the file let go as the editor closes it, the inspections and
// orior's own reading reading it again as the disk holds it. Where the tree's check is on, a file the editor closes is
// checked as the disk holds it before its server lets it go. A file the check held is the editor's once it opens, and
// a file its server holds already hears the diagnostics it last gave
#include "servers_internal.h"

// the most files the editor holds
#define CLI_SERVERS_TEXTS 4096u

CliKeyed g_cli_servers_texts = {"editor texts", {NULL}, 0u, CLI_SERVERS_TEXTS};
CliKeyed g_cli_servers_parses = {"parses", {NULL}, 0u, CLI_SERVERS_TEXTS};
CliLock g_cli_servers_texts_lock = CLI_LOCK_INIT;

// the files closed, one at a time, each read where it lies on the disk as the editor closes it
static CliLock s_disk_lock = CLI_LOCK_INIT;

// the text of `path` kept as the editor holds it, by its key with its path
static void files_text_kept(const char *path, const char *text, unsigned long long size)
{
    char key[CLI_PATH_LONGEST];
    cli_servers_key_of(path, key);
    cli_lock_take(&g_cli_servers_texts_lock);
    (void)cli_keyed_put(&g_cli_servers_texts, key, path, text, size);
    cli_lock_give(&g_cli_servers_texts_lock);
}

// the tree the checkers check and where what is said goes, where a file was opened: 1, or 0 where none was
static int files_told(char *root, CliServersEmit *told)
{
    cli_lock_take(&g_cli_servers.lock);
    const int has = g_cli_servers.has_told && (g_cli_servers.checked_root[0] != '\0');
    if (has)
    {
        memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_servers.checked_root, .bytes = CLI_PATH_LONGEST});
        *told = g_cli_servers.told;
    }
    cli_lock_give(&g_cli_servers.lock);
    return has;
}

int cli_servers_open(const char *root, const char *path, const char *language, const char *text,
                     unsigned long long size, const CliServersEmit *emit, CliLine *out, CliLine *said)
{
    cli_servers_inspect(root, path, text, size, emit);
    files_text_kept(path, text, size);
    cli_lock_take(&g_cli_servers.lock);
    g_cli_servers.told = *emit;
    g_cli_servers.has_told = 1;
    CliLine checked = {g_cli_servers.checked_root, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&checked, root);
    (void)cli_line_closed(&checked);
    cli_lock_give(&g_cli_servers.lock);
    cli_servers_run_checkers(root, path, text, size, 1, emit);
    cli_servers_check_own(root, path, text, size, emit);
    static CliServerSpec s_spec;
    const unsigned long long before = said->at;
    CliLsp *const server = cli_servers_server(root, language, emit, &s_spec, said);
    if (server == NULL)
    {
        if (said->at > before)
        {
            return 0;
        }
        cli_line_text(out, "false");
        return 1;
    }
    char id[CLI_NAME_LONGEST];
    cli_servers_id_of(&s_spec, path, language, id);
    // a file the check holds is the editor's from here, and its clearing as it closes is heard
    char uri[CLI_PATH_LONGEST];
    char key[CLI_PATH_LONGEST];
    cli_servers_uri_of(path, uri);
    cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
    cli_lock_take(&g_cli_servers_held_lock);
    cli_servers_hold(uri, 0);
    cli_servers_heard_opened(key);
    int done = 0;
    if (cli_lsp_has(server, path))
    {
        done = cli_lsp_changed(server, path, text, size, said);
        // a tab opened again on a file the server holds hears the diagnostics it last gave
        if (done)
        {
            cli_servers_emit_whole(key, uri, emit);
        }
    }
    else
    {
        done = cli_lsp_opened(server, path, id, text, size, said);
    }
    cli_lock_give(&g_cli_servers_held_lock);
    cli_lsp_given(server);
    if (done)
    {
        cli_line_text(out, "true");
    }
    return done;
}

int cli_servers_change(const char *path, const char *text, unsigned long long size, CliLine *said)
{
    cli_servers_inspect_again(path, text, size);
    files_text_kept(path, text, size);
    char root[CLI_PATH_LONGEST];
    static CliServersEmit s_told;
    static CliLock s_told_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_told_lock);
    if (files_told(root, &s_told))
    {
        cli_servers_run_checkers(root, path, text, size, 0, &s_told);
        cli_servers_check_own(root, path, text, size, &s_told);
    }
    cli_lock_give(&s_told_lock);
    cli_lock_take(&g_cli_servers_held_lock);
    CliLsp *const server = cli_servers_holding(path);
    const int done = (server == NULL) || cli_lsp_changed(server, path, text, size, said);
    cli_lock_give(&g_cli_servers_held_lock);
    if (server != NULL)
    {
        cli_lsp_given(server);
    }
    return done;
}

int cli_servers_close(const char *path, CliLine *said)
{
    char key[CLI_PATH_LONGEST];
    cli_servers_key_of(path, key);
    cli_lock_take(&g_cli_servers_texts_lock);
    cli_keyed_remove(&g_cli_servers_texts, key);
    cli_keyed_remove(&g_cli_servers_parses, key);
    cli_lock_give(&g_cli_servers_texts_lock);
    cli_lock_take(&s_disk_lock);
    unsigned long long read = 0ull;
    const unsigned char *const mapped = qry_member_file_mapped(path, &read);
    const char *const disk = (const char *)mapped;
    const int whole = (mapped != NULL) && cli_utf8_whole(disk, read);
    cli_servers_inspect_again(path, whole ? disk : NULL, whole ? read : 0ull);
    char root[CLI_PATH_LONGEST];
    static CliServersEmit s_told;
    if (whole && files_told(root, &s_told))
    {
        cli_servers_check_own(root, path, disk, read, &s_told);
    }
    if (mapped != NULL)
    {
        qry_member_file_unmapped(mapped, read);
    }
    cli_lock_give(&s_disk_lock);
    CliLsp *const server = cli_servers_holding(path);
    if (server == NULL)
    {
        return 1;
    }
    cli_lock_take(&g_cli_servers_held_lock);
    const int done = cli_servers_check_takes(path, server) || cli_lsp_closed(server, path, said);
    cli_lock_give(&g_cli_servers_held_lock);
    cli_lsp_given(server);
    return done;
}

int cli_servers_text_of(const char *path, CliLine *out)
{
    char key[CLI_PATH_LONGEST];
    cli_servers_key_of(path, key);
    cli_lock_take(&g_cli_servers_texts_lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const text = cli_keyed_get(&g_cli_servers_texts, key, &beside, &size);
    if (text != NULL)
    {
        cli_line_counted(out, text, size);
    }
    cli_lock_give(&g_cli_servers_texts_lock);
    return text != NULL;
}

char *cli_servers_text_taken(const char *path, unsigned long long *size)
{
    char key[CLI_PATH_LONGEST];
    cli_servers_key_of(path, key);
    cli_lock_take(&g_cli_servers_texts_lock);
    const char *beside = NULL;
    const char *const text = cli_keyed_get(&g_cli_servers_texts, key, &beside, size);
    char *const taken = (text != NULL) ? cli_cell_taken(*size + 1ull, "a file's text as the editor holds it") : NULL;
    if (taken != NULL)
    {
        // a size the keeper held fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = taken, .src = text, .bytes = (size_t)*size});
        taken[*size] = '\0';
    }
    cli_lock_give(&g_cli_servers_texts_lock);
    return taken;
}
