// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_files.c: the files a language server has open, each by its URI with its version and the text it was last given,
// that text stored in a file of orior's own in the system's folder for passing files, so a file's whole text is stored
// on the disk and not in memory
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "lsp_internal.h"

// the most files every server has open at once, the places the table of them is taken with, and the longest URI of one
#define CLI_LSP_FILES 2048u
#define CLI_LSP_URI 2048u

// A file a server has open: whether its place is taken, the server, its URI in a cell of its length, and its version
typedef struct
{
    int reserved;
    unsigned int server;
    char *uri;
    unsigned long long version;
} CliLspFile;

// the files open, by place, in a cell at their most; a place retains its number while its file is open, the file its
// text is stored in named by it
static CliLspFile *s_files;
static unsigned int s_files_capacity;

typedef struct
{
    unsigned int place;
    char *path;
    const CliLsp *lsp;
    const char *uri;
    unsigned int at;
    const char *text;
    unsigned long long length;
    VerbaLinea *err_msg;
} LspFilesCtx;
// the file the text of the open file at `place` is stored in, into `path`, sized for CLI_PATH_MAX_LEN, its folder made
EMBED_INLINE void files_saved_path(const LspFilesCtx *args)
{
    char temporary[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char name[64];
    VerbaLinea name_line = {name, sizeof(name), 0ull};
    mmgr_line_put(&name_line, "orior-lsp-");
    mmgr_line_decimal(&name_line, cli_process_own(), 1u);
    mmgr_line_finish(&name_line);
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = temporary);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = temporary, .relative = name, .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    name_line.at = 0ull;
    mmgr_line_decimal(&name_line, args->place, 1u);
    mmgr_line_put(&name_line, ".txt");
    mmgr_line_finish(&name_line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = name, .joined = args->path);
}

// the place of the open file of `uri` the server has, under the files' lock: CLI_LSP_FILES where it has none
EMBED_INLINE unsigned int files_position(const LspFilesCtx *args)
{
    for (unsigned int at = 0u; at < s_files_capacity; at += 1u)
    {
        if (s_files[at].reserved && (s_files[at].server == args->lsp->number) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_files[at].uri, .other = args->uri, .cap = CLI_CELLS_POOL})))
        {
            return at;
        }
    }
    return CLI_LSP_FILES;
}

// the table taken at its most, every place free, under the files' lock: 1, or 0 where it stands taken or the cell is
// refused
static int files_init(void)
{
    CliLspFile *const cell = (s_files_capacity == 0u)
                                 ? g_cli_cells->persistent_buf_alloc(
                                       (unsigned long long)CLI_LSP_FILES * sizeof(CliLspFile), &g_cli_lsp_files_cells)
                                 : NULL;
    if (cell == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = cell, .val = 0, .bytes = (size_t)CLI_LSP_FILES * sizeof(CliLspFile)});
    s_files = cell;
    s_files_capacity = CLI_LSP_FILES;
    return 1;
}

// the place at `at` let go, under the files' lock, the file its text was stored in removed and its URI's cell given
// back
EMBED_INLINE void files_let_go(const LspFilesCtx *args)
{
    char saved_path[CLI_PATH_MAX_LEN];
    EMBED_CALL(files_saved_path, LspFilesCtx, .place = args->at, .path = saved_path);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = saved_path);
    g_cli_cells->persistent_buf_release(s_files[args->at].uri, &g_cli_lsp_files_cells);
    s_files[args->at].uri = NULL;
    s_files[args->at].reserved = 0;
}

// the place of the open file of `uri` the server has, taken for it where it has none, its version 1: CLI_LSP_FILES
// where no place is left, the URI is too long or its cell is refused
EMBED_INLINE unsigned int files_position_alloc(const LspFilesCtx *args)
{
    unsigned int place = files_position(args);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->uri, .cap = CLI_CELLS_POOL});
    if ((place < CLI_LSP_FILES) || (length >= CLI_LSP_URI))
    {
        return place;
    }
    unsigned int vacant = 0u;
    while ((vacant < s_files_capacity) && s_files[vacant].reserved)
    {
        vacant += 1u;
    }
    char *const text_cell = ((vacant < s_files_capacity) || files_init())
                                ? g_cli_cells->persistent_buf_alloc(length + 1ull, &g_cli_lsp_files_cells)
                                : NULL;
    if (text_cell == NULL)
    {
        return CLI_LSP_FILES;
    }
    memor.cpy(&(MemoriaCfg){.dst = text_cell, .src = args->uri, .bytes = length + 1u});
    s_files[vacant] = (CliLspFile){1, args->lsp->number, text_cell, 1ull};
    return vacant;
}

// the text of an open file stored: 1, or 0 with why given
EMBED_INLINE int files_text_save(const LspFilesCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(files_saved_path, LspFilesCtx, .place = args->place, .path = path);
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = args->text,
                    .size = args->length))
    {
        mmgr_line_put(args->err_msg, path);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

int cli_lsp_open(const CliLspErrMsgTextCfg *args)
{
    char uri[CLI_LSP_URI];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = args->path, .uri_out = uri,
               .capacity = sizeof(uri));
    const unsigned int place = EMBED_CALL(files_position_alloc, LspFilesCtx, .lsp = args->lsp, .uri = uri);
    if (place < CLI_LSP_FILES)
    {
        s_files[place].version = 1ull;
    }
    const int saved =
        (place < CLI_LSP_FILES) && EMBED_CALL(files_text_save, LspFilesCtx, .place = place, .text = args->text,
                                              .length = args->length, .err_msg = args->err_msg);
    if (place == CLI_LSP_FILES)
    {
        mmgr_line_put(args->err_msg, "no more files can be open at once");
        return 0;
    }
    if (!saved)
    {
        return 0;
    }
    const CliLspPart parts[7] = {
        CLI_LSP_RAW("{\"textDocument\":{\"uri\":\""),
        {uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), 1},
        CLI_LSP_RAW("\",\"languageId\":\""),
        {args->language_id, cellul.len(&(CatenaFinitaCfg){.src = args->language_id, .cap = CLI_CELLS_POOL}), 1},
        CLI_LSP_RAW("\",\"version\":1,\"text\":\""),
        {args->text, args->length, 1},
        CLI_LSP_RAW("\"}}")};
    const CliLspPart head = CLI_LSP_RAW("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":");
    const CliLspPart tail = CLI_LSP_RAW("}");
    CliLspPart message[9];
    message[0] = head;
    memor.cpy(&(MemoriaCfg){.dst = message + 1, .src = parts, .bytes = sizeof(parts)});
    message[8] = tail;
    return EMBED_CALL(cli_lsp_writer_lock.send_parts, CliLspWriterLockCfg, .writer = &args->lsp->input,
                      .parts = message, .count = 9u, .err_msg = args->err_msg);
}

int cli_lsp_change(const CliLspErrMsgTextCfg *args)
{
    char uri[CLI_LSP_URI];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = args->path, .uri_out = uri,
               .capacity = sizeof(uri));
    const unsigned int place = EMBED_CALL(files_position_alloc, LspFilesCtx, .lsp = args->lsp, .uri = uri);
    unsigned long long version = 0ull;
    if (place < CLI_LSP_FILES)
    {
        s_files[place].version += 1ull;
        version = s_files[place].version;
    }
    const int saved =
        (place < CLI_LSP_FILES) && EMBED_CALL(files_text_save, LspFilesCtx, .place = place, .text = args->text,
                                              .length = args->length, .err_msg = args->err_msg);
    if (place == CLI_LSP_FILES)
    {
        mmgr_line_put(args->err_msg, "no more files can be open at once");
        return 0;
    }
    if (!saved)
    {
        return 0;
    }
    char number[24];
    VerbaLinea number_line = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&number_line, version, 1u);
    const CliLspPart message[7] = {
        CLI_LSP_RAW(
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\""),
        {uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), 1},
        CLI_LSP_RAW("\",\"version\":"),
        {number, number_line.at, 0},
        CLI_LSP_RAW("},\"contentChanges\":[{\"text\":\""),
        {args->text, args->length, 1},
        CLI_LSP_RAW("\"}]}}")};
    return EMBED_CALL(cli_lsp_writer_lock.send_parts, CliLspWriterLockCfg, .writer = &args->lsp->input,
                      .parts = message, .count = 7u, .err_msg = args->err_msg);
}

int cli_lsp_text_size(const CliLspSizePathCfg *args)
{
    unsigned long long *size = args->size;
    char uri[CLI_LSP_URI];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = args->path, .uri_out = uri,
               .capacity = sizeof(uri));
    const unsigned int place = EMBED_CALL(files_position, LspFilesCtx, .lsp = args->lsp, .uri = uri);
    char saved_path[CLI_PATH_MAX_LEN];
    if (place < CLI_LSP_FILES)
    {
        EMBED_CALL(files_saved_path, LspFilesCtx, .place = place, .path = saved_path);
    }
    *size = 0ull;
    return (place < CLI_LSP_FILES) &&
           EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = saved_path, .size_pointer = size);
}

int cli_lsp_text(const CliLspPathOutCfg *args)
{
    char uri[CLI_LSP_URI];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = args->path, .uri_out = uri,
               .capacity = sizeof(uri));
    const unsigned int place = EMBED_CALL(files_position, LspFilesCtx, .lsp = args->lsp, .uri = uri);
    int read = 0;
    if (place < CLI_LSP_FILES)
    {
        char saved_path[CLI_PATH_MAX_LEN];
        EMBED_CALL(files_saved_path, LspFilesCtx, .place = place, .path = saved_path);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = saved_path,
                                                      .size_pointer = &size, .owner = &g_cli_lsp_files_cells);
        mmgr_line_put_n(args->out, (bytes != NULL) ? (const char *)bytes : "", size);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes,
                   .owner = &g_cli_lsp_files_cells);
        read = 1;
    }
    return read;
}

int cli_lsp_read_again(const CliLspErrMsgTextCfg *args)
{
    char uri[CLI_LSP_URI];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = args->path, .uri_out = uri,
               .capacity = sizeof(uri));
    const unsigned int place = EMBED_CALL(files_position, LspFilesCtx, .lsp = args->lsp, .uri = uri);
    char saved_path[CLI_PATH_MAX_LEN];
    if (place < CLI_LSP_FILES)
    {
        EMBED_CALL(files_saved_path, LspFilesCtx, .place = place, .path = saved_path);
    }
    if (place == CLI_LSP_FILES)
    {
        return 1;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = saved_path,
                                                  .size_pointer = &size, .owner = &g_cli_lsp_files_cells);
    const int changed =
        EMBED_CALL(cli_lsp_err_msg_text.change, CliLspErrMsgTextCfg, .lsp = args->lsp, .path = args->path,
                   .text = (bytes != NULL) ? (const char *)bytes : "", .length = size, .err_msg = args->err_msg);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_lsp_files_cells);
    return changed;
}

int cli_lsp_close(const CliLspErrMsgTextCfg *args)
{
    char uri[CLI_LSP_URI];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = args->path, .uri_out = uri,
               .capacity = sizeof(uri));
    const unsigned int place = EMBED_CALL(files_position, LspFilesCtx, .lsp = args->lsp, .uri = uri);
    if (place < CLI_LSP_FILES)
    {
        EMBED_CALL(files_let_go, LspFilesCtx, .at = place);
    }
    const CliLspPart message[3] = {
        CLI_LSP_RAW(
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\",\"params\":{\"textDocument\":{\"uri\":\""),
        {uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), 1},
        CLI_LSP_RAW("\"}}}")};
    return EMBED_CALL(cli_lsp_writer_lock.send_parts, CliLspWriterLockCfg, .writer = &args->lsp->input,
                      .parts = message, .count = 3u, .err_msg = args->err_msg);
}

unsigned long long cli_lsp_version(const CliLspPathOutCfg *args)
{
    char uri[CLI_LSP_URI];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = args->path, .uri_out = uri,
               .capacity = sizeof(uri));
    const unsigned int place = EMBED_CALL(files_position, LspFilesCtx, .lsp = args->lsp, .uri = uri);
    const unsigned long long version = (place < CLI_LSP_FILES) ? s_files[place].version : 0ull;
    return version;
}

int cli_lsp_has(const CliLspPathOutCfg *args)
{
    char uri[CLI_LSP_URI];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = args->path, .uri_out = uri,
               .capacity = sizeof(uri));
    const int has = (EMBED_CALL(files_position, LspFilesCtx, .lsp = args->lsp, .uri = uri) < CLI_LSP_FILES);
    return has;
}

char *cli_lsp_open_uris_alloc(const CliLspSizeCfg *args)
{
    unsigned long long *size = args->size;
    // each file the server has open holds a URI of at most CLI_LSP_URI bytes
    unsigned long long open = 0ull;
    for (unsigned int at = 0u; at < s_files_capacity; at += 1u)
    {
        open += (s_files[at].reserved && (s_files[at].server == args->lsp->number)) ? 1ull : 0ull;
    }
    char *const uris = (open > 0ull) ? g_cli_cells->temporary_buf_alloc(open * CLI_LSP_URI, args->owner) : NULL;
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; (uris != NULL) && (at < s_files_capacity); at += 1u)
    {
        if (s_files[at].reserved && (s_files[at].server == args->lsp->number))
        {
            const unsigned long long length =
                cellul.len(&(CatenaFinitaCfg){.src = s_files[at].uri, .cap = CLI_LSP_URI}) + 1ull;
            memor.cpy(&(MemoriaCfg){.dst = uris + used, .src = s_files[at].uri, .bytes = (size_t)length});
            used += length;
        }
    }
    *size = used;
    return uris;
}

void cli_lsp_open_each(const CliLspContextCfg *args)
{
    // the server's files are copied as they stand under the lock into cells of their count and their URIs' bytes, and
    // each is handed on with the lock given back, so that `each` may open it in another server
    unsigned int count = 0u;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < s_files_capacity; at += 1u)
    {
        if (s_files[at].reserved && (s_files[at].server == args->lsp->number))
        {
            count += 1u;
            bytes += cellul.len(&(CatenaFinitaCfg){.src = s_files[at].uri, .cap = CLI_LSP_URI}) + 1ull;
        }
    }
    unsigned int *const positions = g_cli_cells->temporary_buf_alloc(
        ((unsigned long long)count + 1ull) * sizeof(unsigned int), &g_cli_lsp_files_cells);
    char *const uris = g_cli_cells->persistent_buf_alloc(bytes + 1ull, &g_cli_lsp_files_cells);
    count = ((positions != NULL) && (uris != NULL)) ? count : 0u;
    unsigned int copied = 0u;
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; (copied < count) && (at < s_files_capacity); at += 1u)
    {
        if (s_files[at].reserved && (s_files[at].server == args->lsp->number))
        {
            const unsigned long long length =
                cellul.len(&(CatenaFinitaCfg){.src = s_files[at].uri, .cap = CLI_LSP_URI}) + 1ull;
            positions[copied] = at;
            memor.cpy(&(MemoriaCfg){.dst = uris + used, .src = s_files[at].uri, .bytes = (size_t)length});
            used += length;
            copied += 1u;
        }
    }
    const char *uri = uris;
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_LSP_URI});
        const char *const this_uri = uri;
        uri += length + 1ull;
        char path[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = this_uri, .length = length,
                        .path_out = path))
        {
            continue;
        }
        char saved_path[CLI_PATH_MAX_LEN];
        EMBED_CALL(files_saved_path, LspFilesCtx, .place = positions[one], .path = saved_path);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = saved_path,
                                                      .size_pointer = &size, .owner = &g_cli_lsp_files_cells);
        EMBED_CALL(args->each, CliEachPathTextLengthCfg, .context = args->context, .path = path,
                   .text = (bytes != NULL) ? (const char *)bytes : "", .length = (bytes != NULL) ? size : 0ull);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes,
                   .owner = &g_cli_lsp_files_cells);
    }
    g_cli_cells->persistent_buf_release(uris, &g_cli_lsp_files_cells);
    g_cli_cells->persistent_buf_release(positions, &g_cli_lsp_files_cells);
}

void cli_lsp_files_let_go(const CliLspKindCtx *args)
{
    for (unsigned int at = 0u; at < s_files_capacity; at += 1u)
    {
        if (s_files[at].reserved && (s_files[at].server == args->lsp_read->number))
        {
            EMBED_CALL(files_let_go, LspFilesCtx, .at = at);
        }
    }
}
