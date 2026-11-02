// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_files.c: the files the editor has open: a file handed to its language's server, to the plugins' servers of
// its language and its text to the inspections and the checkers as it opens, each new text given, and the file let go
// as the editor closes it, the inspections and orior's own reading reading it again as the disk stores it. Where the
// tree's check is on, a file the editor closes is checked as the disk stores it before its server lets it go. A file
// the check open is the editor's once it opens, and a file its server has open already receives the diagnostics it last
// gave. A server whose spec slices is given the tree's slice for the view in place of each text (servers_slice.c)
#include "../command_line/cells.h"
#include "servers_internal.h"

// the most files the editor has open
#define CLI_SERVERS_TEXTS 4096u

CliKeyed g_cli_servers_texts = {"editor texts", {NULL}, 0u, CLI_SERVERS_TEXTS};
CliKeyed g_cli_servers_parses = {"parses", {NULL}, 0u, CLI_SERVERS_TEXTS};

// the files closed, one at a time, each read where it lies on the disk as the editor closes it

typedef struct
{
    const char *path;
    const char *text;
    unsigned long long size;
    char *root;
    CliServersSink *sink;
} ServersFilesCtx;
// the text of `path` stored as the editor has it, by its key with its path
EMBED_INLINE void files_text_store(const ServersFilesCtx *args)
{
    const char *path = args->path;
    const char *text = args->text;
    unsigned long long size = args->size;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = path, .key_out = key);
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_texts, .key = key,
                     .adjacent_value = path, .text = text, .size_value = size);
}

// the tree the checkers check and where what is reported goes, where a file was opened: 1, or 0 where none was
EMBED_INLINE int files_checked_sink(const ServersFilesCtx *args)
{
    CliServersSink *sink = args->sink;
    const int has = g_cli_servers.has_sink && (g_cli_servers.checked_root[0] != '\0');
    if (has)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->root, .src = g_cli_servers.checked_root, .bytes = CLI_PATH_MAX_LEN});
        *sink = g_cli_servers.sink;
    }
    return has;
}

int cli_servers_open(const CliServersRootCfg *args)
{
    const char *path = args->path;
    const char *text = args->text;
    const CliServersSink *sink = args->sink;
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(cli_servers_path_size_root_sink_internal.inspect, CliServersPathSizeRootSinkCtx, .root = args->root,
               .path = path, .text = text, .size = args->size, .sink = sink);
    EMBED_CALL(files_text_store, ServersFilesCtx, .path = path, .text = text, .size = args->size);
    g_cli_servers.sink = *sink;
    g_cli_servers.has_sink = 1;
    VerbaLinea checked = {g_cli_servers.checked_root, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&checked, args->root);
    (void)mmgr_line_finish(&checked);
    EMBED_CALL(cli_servers_path_size_root_sink_internal.run_checkers, CliServersPathSizeRootSinkCtx, .root = args->root,
               .path = path, .text = text, .size = args->size, .disk = 1, .sink = sink);
    EMBED_CALL(cli_servers_path_size_root_sink_internal.check_own, CliServersPathSizeRootSinkCtx, .root = args->root,
               .path = path, .text = text, .size = args->size, .sink = sink);
    const int plugged = EMBED_CALL(cli_servers_path_size_language_internal.plugins_open, CliServersPathSizeLanguageCtx,
                                   .path = path, .language = args->language, .text = text, .size = args->size);
    static CliServerSpec s_spec;
    const unsigned long long before = err_msg->at;
    CliLsp *const server = EMBED_CALL(cli_servers_language_internal.server, CliServersLanguageCtx, .root = args->root,
                                      .language = args->language, .sink = sink, .spec = &s_spec, .err_msg = err_msg);
    if (server == NULL)
    {
        // a file a plugin's server took is the editor's to report of its changes and its closing, its language's own
        // server or none
        if ((err_msg->at > before) && !plugged)
        {
            return 0;
        }
        *args->served = plugged;
        return 1;
    }
    char id[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_servers_path_language_internal.id_get, CliServersPathLanguageCtx, .spec_read = &s_spec, .path = path,
               .language = args->language, .id_out = id);
    // a file the check has open is the editor's from here, and its clearing as it closes is received; one the check
    // gives in slices is the editor's to slice for its view, and its diagnostics are passed on as they come
    char uri[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = path, .uri_out = uri);
    EMBED_CALL(cli_lsp_length.key_get, CliLspLengthCfg, .uri = uri,
               .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .key = key);
    const int check_had = EMBED_CALL(cli_servers_uri_internal.check_open, CliServersUriCtx, .uri = uri);
    EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = uri, .open = 0);
    EMBED_CALL(cli_servers_key_checker_internal.let_go_clear, CliServersKeyCheckerCtx, .key = key);
    EMBED_CALL(cli_servers_key_checker_internal.gathering_end, CliServersKeyCheckerCtx, .key = key);
    if (check_had)
    {
        EMBED_CALL(cli_servers_path_lsp_internal.slice_close, CliServersPathLspCtx, .path = path);
    }
    int done = 0;
    if (EMBED_CALL(cli_lsp_path_out.has, CliLspPathOutCfg, .lsp = server, .path = path))
    {
        done = s_spec.slices ? EMBED_CALL(cli_servers_path_size_root_internal.slice_send, CliServersPathSizeRootCtx,
                                          .server = server, .root = args->root, .path = path, .id = NULL, .text = text,
                                          .size = args->size, .err_msg = err_msg)
                             : EMBED_CALL(cli_lsp_err_msg_text.change, CliLspErrMsgTextCfg, .lsp = server, .path = path,
                                          .text = text, .length = args->size, .err_msg = err_msg);
        // a tab opened again on a file the server has open receives the diagnostics it last gave
        if (done)
        {
            EMBED_CALL(cli_servers_sink_key_internal.emit_file, CliServersSinkKeyCtx, .key = key, .uri = uri,
                       .sink = sink);
        }
    }
    else
    {
        done = s_spec.slices ? EMBED_CALL(cli_servers_path_size_root_internal.slice_send, CliServersPathSizeRootCtx,
                                          .server = server, .root = args->root, .path = path, .id = id, .text = text,
                                          .size = args->size, .err_msg = err_msg)
                             : EMBED_CALL(cli_lsp_err_msg_text.open, CliLspErrMsgTextCfg, .lsp = server, .path = path,
                                          .language_id = id, .text = text, .length = args->size, .err_msg = err_msg);
    }
    // a file given its server as a slice is opened again as its language in the server started after one let go
    if (done && s_spec.slices)
    {
        EMBED_CALL(cli_servers_path_language_internal.language_note, CliServersPathLanguageCtx, .path = path,
                   .language = args->language);
    }
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    *args->served = done;
    return done;
}

int cli_servers_change(const CliServersRootCfg *args)
{
    const char *path = args->path;
    const char *text = args->text;
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(cli_servers_path_size_language_internal.inspect_again, CliServersPathSizeLanguageCtx, .path = path,
               .text = text, .size = args->size);
    EMBED_CALL(files_text_store, ServersFilesCtx, .path = path, .text = text, .size = args->size);
    char root[CLI_PATH_MAX_LEN];
    static CliServersSink s_sink;
    if (EMBED_CALL(files_checked_sink, ServersFilesCtx, .root = root, .sink = &s_sink))
    {
        EMBED_CALL(cli_servers_path_size_root_sink_internal.run_checkers, CliServersPathSizeRootSinkCtx, .root = root,
                   .path = path, .text = text, .size = args->size, .disk = 0, .sink = &s_sink);
        EMBED_CALL(cli_servers_path_size_root_sink_internal.check_own, CliServersPathSizeRootSinkCtx, .root = root,
                   .path = path, .text = text, .size = args->size, .sink = &s_sink);
    }
    EMBED_CALL(cli_servers_path_size_language_internal.plugins_change, CliServersPathSizeLanguageCtx, .path = path,
               .text = text, .size = args->size);
    CliLsp *const server = EMBED_CALL(cli_servers_path_lsp_internal.owner_start, CliServersPathLspCtx, .path = path);
    const int done =
        (server == NULL) ||
        (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path)
             ? EMBED_CALL(cli_servers_path_size_root_internal.slice_send, CliServersPathSizeRootCtx, .server = server,
                          .root = NULL, .path = path, .id = NULL, .text = text, .size = args->size, .err_msg = err_msg)
             : EMBED_CALL(cli_lsp_err_msg_text.change, CliLspErrMsgTextCfg, .lsp = server, .path = path, .text = text,
                          .length = args->size, .err_msg = err_msg));
    if (server != NULL)
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    }
    return done;
}

int cli_servers_close(const CliServersPathOutErrMsgFromToCfg *args)
{
    const char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = path, .key_out = key);
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_texts, .key = key);
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_parses, .key = key);
    unsigned long long read = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                   .size_pointer = &read, .owner = &g_cli_servers_cells);
    const char *const disk = (const char *)mapped;
    const int integer =
        (mapped != NULL) && EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = disk, .size = read);
    EMBED_CALL(cli_servers_path_size_language_internal.inspect_again, CliServersPathSizeLanguageCtx, .path = path,
               .text = integer ? disk : NULL, .size = integer ? read : 0ull);
    char root[CLI_PATH_MAX_LEN];
    static CliServersSink s_sink;
    if (integer && EMBED_CALL(files_checked_sink, ServersFilesCtx, .root = root, .sink = &s_sink))
    {
        EMBED_CALL(cli_servers_path_size_root_sink_internal.check_own, CliServersPathSizeRootSinkCtx, .root = root,
                   .path = path, .text = disk, .size = read, .sink = &s_sink);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_servers_cells);
    EMBED_CALL(cli_servers_path_lsp_internal.plugins_close, CliServersPathLspCtx, .path = path);
    // a file the editor closes is the check's to give its server, in slices of the disk's text where it slices; a
    // server let go is not started again to close it
    EMBED_CALL(cli_servers_path_lsp_internal.slice_close, CliServersPathLspCtx, .path = path);
    EMBED_CALL(cli_servers_path_lsp_internal.language_forgotten, CliServersPathLspCtx, .path = path);
    CliLsp *const server = EMBED_CALL(cli_servers_path_lsp_internal.owner_get, CliServersPathLspCtx, .path = path);
    if (server == NULL)
    {
        return 1;
    }
    const int done =
        EMBED_CALL(cli_servers_path_lsp_internal.check_allocs, CliServersPathLspCtx, .path = path, .lsp = server) ||
        EMBED_CALL(cli_lsp_err_msg_text.close, CliLspErrMsgTextCfg, .lsp = server, .path = path, .err_msg = err_msg);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    return done;
}

int cli_servers_text_get(const CliServersPathOutErrMsgFromToCfg *args)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    const char *adjacent = NULL;
    unsigned long long size = 0ull;
    const char *const text =
        EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_texts, .key = key,
                   .adjacent = &adjacent, .size = &size);
    if (text != NULL)
    {
        mmgr_line_put_n(args->out, text, size);
    }
    return text != NULL;
}

char *cli_servers_text_alloc(const CliServersPathSizeKindCfg *args)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    const char *adjacent = NULL;
    const char *const text =
        EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_texts, .key = key,
                   .adjacent = &adjacent, .size = args->size_pointer);
    char *const reserved =
        (text != NULL) ? g_cli_cells->persistent_buf_alloc(*args->size_pointer + 1ull, &g_cli_servers_cells) : NULL;
    if (reserved != NULL)
    {
        // a size the store stored fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = reserved, .src = text, .bytes = (size_t)*args->size_pointer});
        reserved[*args->size_pointer] = '\0';
    }
    return reserved;
}
