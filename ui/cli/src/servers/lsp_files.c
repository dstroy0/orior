// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_files.c: the files a language server has open, each by its URI with its version and the text it was last given,
// that text stored in a file of orior's own in the system's folder for passing files, so a file's whole text is stored
// on the disk and not in memory
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "lsp_internal.h"

// the most files every server has open at once, the places the table of them is taken with, and the longest URI of one
#define CLI_LSP_FILES 2048u
#define CLI_LSP_URI 2048u

// A file a server has open: whether its place is taken, the server, its URI in a movable cell of its bytes with their
// end, how many bytes, and its version
typedef struct
{
    int reserved;
    unsigned int server;
    const CarcerMovableEntry *uri;
    unsigned long long uri_length;
    unsigned long long version;
} CliLspFile;

// the files open, by place, records in a movable cell at their most, read and written a record at a time; a place
// retains its number while its file is open, the file its text is stored in named by it
static const CarcerMovableEntry *s_files;
static unsigned int s_files_capacity;

// the place of the record `at` in the files' cell
#define LSP_FILE_AT(at_) ((unsigned long long)(at_) * sizeof(CliLspFile))

typedef struct
{
    unsigned int place;
    char *path;
    const CliLsp *lsp;
    const char *uri;
    unsigned int at;
    const char *text;
    const CarcerMovableEntry *cell;
    unsigned long long cell_at;
    unsigned long long length;
    unsigned long long version;
    VerbaLinea *err_msg;
} LspFilesCtx;
// the bytes of a text in a movable cell written to its file at a time
#define LSP_FILES_WINDOW 4096u
// the file the text of the open file at `place` is stored in, into `path`, sized for CLI_PATH_MAX_LEN, its folder made
EMBED_INLINE void files_saved_path(const LspFilesCtx *args)
{
    char temporary[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char name[64];
    VerbaLinea name_line = {name, sizeof(name), 0ULL};
    mmgr_line_put(&name_line, "orior-lsp-");
    mmgr_line_decimal(&name_line, cli_process_own(), 1u);
    mmgr_line_finish(&name_line);
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = temporary);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = temporary, .relative = name, .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    name_line.at = 0ULL;
    mmgr_line_decimal(&name_line, args->place, 1u);
    mmgr_line_put(&name_line, ".txt");
    mmgr_line_finish(&name_line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = name, .joined = args->path);
}

// the place of the open file of `uri` the server has, under the files' lock: CLI_LSP_FILES where it has none
EMBED_INLINE unsigned int files_position(const LspFilesCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->uri, .cap = CLI_LSP_URI});
    for (unsigned int at = 0u; at < s_files_capacity; at += 1u)
    {
        CliLspFile file;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(at), .dst = &file, .bytes = sizeof(file)});
        if (file.reserved && (file.server == args->lsp->number) && (file.uri_length == length) &&
            (g_cli_guard->movable_buf_compare(
                 &(CarcerMovableCfg){.cell = file.uri, .offset = 0ULL, .src = args->uri, .bytes = length}) == 0))
        {
            return at;
        }
    }
    return CLI_LSP_FILES;
}

// the table taken at its most, every place free, under the files' lock: 1, or 0 where it stands taken or the cell is
// refused. A movable cell is not zeroed: the empty records are laid in the temporary tier and written whole
static int files_init(void)
{
    const unsigned long long bytes = LSP_FILE_AT(CLI_LSP_FILES);
    if (s_files_capacity != 0u)
    {
        return 0;
    }
    unsigned char *const empty = g_cli_guard->temporary_buf_alloc(bytes, g_cli_lsp_files_owner);
    const CarcerMovableEntry *const cell =
        (empty != NULL) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_lsp_files_owner) : NULL;
    if (cell == NULL)
    {
        g_cli_guard->persistent_buf_release(empty, g_cli_lsp_files_owner);
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = empty, .val = 0, .bytes = (embed_word)bytes});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = empty, .bytes = bytes});
    g_cli_guard->persistent_buf_release(empty, g_cli_lsp_files_owner);
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
    CliLspFile file;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(args->at), .dst = &file, .bytes = sizeof(file)});
    if (file.uri != NULL)
    {
        (void)g_cli_guard->movable_buf_release(file.uri, g_cli_lsp_files_owner);
    }
    file = (CliLspFile){0, 0u, NULL, 0ULL, 0ULL};
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(args->at), .src = &file, .bytes = sizeof(file)});
}

// the place of the open file of `uri` the server has, taken for it where it has none, its version 1: CLI_LSP_FILES
// where no place is left, the URI is too long or its cell is refused
EMBED_INLINE unsigned int files_position_alloc(const LspFilesCtx *args)
{
    unsigned int place = files_position(args);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->uri, .cap = CLI_POOL_BYTES});
    if ((place < CLI_LSP_FILES) || (length >= CLI_LSP_URI))
    {
        return place;
    }
    unsigned int vacant = 0u;
    while (vacant < s_files_capacity)
    {
        CliLspFile file;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(vacant), .dst = &file, .bytes = sizeof(file)});
        if (!file.reserved)
        {
            break;
        }
        vacant += 1u;
    }
    const CarcerMovableEntry *const text_cell =
        ((vacant < s_files_capacity) || files_init())
            ? g_cli_guard->movable_buf_alloc(length + 1ULL, g_cli_lsp_files_owner)
            : NULL;
    if (text_cell == NULL)
    {
        return CLI_LSP_FILES;
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = text_cell, .offset = 0ULL, .src = args->uri, .bytes = length + 1ULL});
    const CliLspFile file = {1, args->lsp->number, text_cell, length, 1ULL};
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(vacant), .src = &file, .bytes = sizeof(file)});
    return vacant;
}

// the version of the open file at `place` set to `version`
EMBED_INLINE void files_version_set(const LspFilesCtx *args)
{
    CliLspFile file;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(args->place), .dst = &file, .bytes = sizeof(file)});
    file.version = args->version;
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(args->place), .src = &file, .bytes = sizeof(file)});
}

// the version of the open file at `place`
EMBED_INLINE unsigned long long files_version(const LspFilesCtx *args)
{
    CliLspFile file;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(args->place), .dst = &file, .bytes = sizeof(file)});
    return file.version;
}

// the text of an open file stored: 1, or 0 with why given
EMBED_INLINE int files_text_save(const LspFilesCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(files_saved_path, LspFilesCtx, .place = args->place, .path = path);
    int written = 1;
    if (args->cell == NULL)
    {
        written = EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                             .bytes = args->text, .size = args->length);
    }
    else
    {
        // a text in a movable cell goes to the file a window at a time, read out of the cell as it goes
        written = EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = "",
                             .size = 0ULL, .fresh = 1);
        for (unsigned long long at = 0ULL; written && (at < args->length);)
        {
            char window[LSP_FILES_WINDOW];
            const unsigned long long left = args->length - at;
            const unsigned long long bytes = (left < sizeof(window)) ? left : sizeof(window);
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->cell,
                                                                    .offset = (embed_word)(args->cell_at + at),
                                                                    .dst = window,
                                                                    .bytes = (embed_word)bytes});
            written = EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = window,
                                 .size = bytes, .fresh = 0);
            at += bytes;
        }
    }
    if (!written)
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
        EMBED_CALL(files_version_set, LspFilesCtx, .place = place, .version = 1ULL);
    }
    const int saved = (place < CLI_LSP_FILES) &&
                      EMBED_CALL(files_text_save, LspFilesCtx, .place = place, .text = args->text, .cell = args->cell,
                                 .cell_at = args->at, .length = args->length, .err_msg = args->err_msg);
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
        {uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_POOL_BYTES}), 1, NULL, 0ULL},
        CLI_LSP_RAW("\",\"languageId\":\""),
        {args->language_id, cellul.len(&(CatenaFinitaCfg){.src = args->language_id, .cap = CLI_POOL_BYTES}), 1, NULL,
         0ULL},
        CLI_LSP_RAW("\",\"version\":1,\"text\":\""),
        {args->text, args->length, 1, args->cell, args->at},
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
    unsigned long long version = 0ULL;
    if (place < CLI_LSP_FILES)
    {
        version = EMBED_CALL(files_version, LspFilesCtx, .place = place) + 1ULL;
        EMBED_CALL(files_version_set, LspFilesCtx, .place = place, .version = version);
    }
    const int saved = (place < CLI_LSP_FILES) &&
                      EMBED_CALL(files_text_save, LspFilesCtx, .place = place, .text = args->text, .cell = args->cell,
                                 .cell_at = args->at, .length = args->length, .err_msg = args->err_msg);
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
    VerbaLinea number_line = {number, sizeof(number), 0ULL};
    mmgr_line_decimal(&number_line, version, 1u);
    const CliLspPart message[7] = {
        CLI_LSP_RAW(
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\""),
        {uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_POOL_BYTES}), 1, NULL, 0ULL},
        CLI_LSP_RAW("\",\"version\":"),
        {number, number_line.at, 0, NULL, 0ULL},
        CLI_LSP_RAW("},\"contentChanges\":[{\"text\":\""),
        {args->text, args->length, 1, args->cell, args->at},
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
    *size = 0ULL;
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
        unsigned long long size = 0ULL;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = saved_path,
                                                      .size_pointer = &size, .owner = g_cli_lsp_files_owner);
        mmgr_line_put_n(args->out, (bytes != NULL) ? (const char *)bytes : "", size);
        g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_lsp_files_owner));
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
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = saved_path,
                                                  .size_pointer = &size, .owner = g_cli_lsp_files_owner);
    const int changed =
        EMBED_CALL(cli_lsp_err_msg_text.change, CliLspErrMsgTextCfg, .lsp = args->lsp, .path = args->path,
                   .text = (bytes != NULL) ? (const char *)bytes : "", .length = size, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_lsp_files_owner));
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
        {uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_POOL_BYTES}), 1},
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
    const unsigned long long version =
        (place < CLI_LSP_FILES) ? EMBED_CALL(files_version, LspFilesCtx, .place = place) : 0ULL;
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
    unsigned long long open = 0ULL;
    for (unsigned int at = 0u; at < s_files_capacity; at += 1u)
    {
        CliLspFile file;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(at), .dst = &file, .bytes = sizeof(file)});
        open += (file.reserved && (file.server == args->lsp->number)) ? 1ULL : 0ULL;
    }
    char *const uris = (open > 0ULL) ? g_cli_guard->temporary_buf_alloc(open * CLI_LSP_URI, args->owner) : NULL;
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; (uris != NULL) && (at < s_files_capacity); at += 1u)
    {
        CliLspFile file;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(at), .dst = &file, .bytes = sizeof(file)});
        if (file.reserved && (file.server == args->lsp->number))
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = file.uri, .offset = 0ULL, .dst = uris + used, .bytes = file.uri_length + 1ULL});
            used += file.uri_length + 1ULL;
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
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < s_files_capacity; at += 1u)
    {
        CliLspFile file;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(at), .dst = &file, .bytes = sizeof(file)});
        if (file.reserved && (file.server == args->lsp->number))
        {
            count += 1u;
            bytes += file.uri_length + 1ULL;
        }
    }
    unsigned int *const positions = g_cli_guard->temporary_buf_alloc(
        ((unsigned long long)count + 1ULL) * sizeof(unsigned int), g_cli_lsp_files_owner);
    char *const uris = g_cli_guard->temporary_buf_alloc(bytes + 1ULL, g_cli_lsp_files_owner);
    count = ((positions != NULL) && (uris != NULL)) ? count : 0u;
    unsigned int copied = 0u;
    unsigned long long used = 0ULL;
    {
        unsigned int at = 0u;
        while ((copied < count) && (at < s_files_capacity))
        {
            CliLspFile file;
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(at), .dst = &file, .bytes = sizeof(file)});
            if (file.reserved && (file.server == args->lsp->number))
            {
                positions[copied] = at;
                (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                    .cell = file.uri, .offset = 0ULL, .dst = uris + used, .bytes = file.uri_length + 1ULL});
                used += file.uri_length + 1ULL;
                copied += 1u;
            }
            at += 1u;
        }
    }
    const char *uri = uris;
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_LSP_URI});
        const char *const this_uri = uri;
        uri += length + 1ULL;
        char path[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = this_uri, .length = length,
                        .path_out = path))
        {
            continue;
        }
        char saved_path[CLI_PATH_MAX_LEN];
        EMBED_CALL(files_saved_path, LspFilesCtx, .place = positions[one], .path = saved_path);
        unsigned long long size = 0ULL;
        const unsigned char *const inner_bytes =
            EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = saved_path, .size_pointer = &size,
                       .owner = g_cli_lsp_files_owner);
        EMBED_CALL(args->each, CliEachPathTextLengthCfg, .context = args->context, .path = path,
                   .text = (inner_bytes != NULL) ? (const char *)inner_bytes : "",
                   .length = (inner_bytes != NULL) ? size : 0ULL);
        g_cli_guard->persistent_buf_release((void *)(embed_word)inner_bytes, (g_cli_lsp_files_owner));
    }
    g_cli_guard->persistent_buf_release(uris, g_cli_lsp_files_owner);
    g_cli_guard->persistent_buf_release(positions, g_cli_lsp_files_owner);
}

void cli_lsp_files_let_go(const CliLspTypeCtx *args)
{
    for (unsigned int at = 0u; at < s_files_capacity; at += 1u)
    {
        CliLspFile file;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_files, .offset = LSP_FILE_AT(at), .dst = &file, .bytes = sizeof(file)});
        if (file.reserved && (file.server == args->lsp_read->number))
        {
            EMBED_CALL(files_let_go, LspFilesCtx, .at = at);
        }
    }
}
