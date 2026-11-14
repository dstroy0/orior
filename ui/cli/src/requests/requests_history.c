// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_history.c: each request's last answers stored in orior's own folder, under requests/, a folder for the file
// named by the SHA-256 of its path and a file for the request named by the SHA-256 of its key, the last answer first,
// ten of them, each body cut to 1 MiB; and the answers stored read back
#include "../command_line/pool.h"
#include "../text/digest.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "requests_internal.h"

// the most one answer stored takes, written, and the bytes the answers are buffered in before each piece is added to
// the file
#define CLI_REQUESTS_PIECE (16u << 20u)
#define CLI_REQUESTS_CAPACITY 16384u

// The file the answers are added to as their line fills: its path, and whether the next piece makes it anew
typedef struct
{
    const char *path;
    int fresh;
} CliStateWriting;

// a piece added to the file the answers are written in, the first made anew: 1, or 0 where it could not be
static int history_append(void *context, const char *bytes, embed_word size)
{
    CliStateWriting *const writing = context;
    const int put = EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = writing->path,
                               .bytes = bytes, .size = size, .fresh = writing->fresh);
    writing->fresh = 0;
    return put;
}

typedef struct
{
    const char *bytes;
    unsigned long long size;
    VerbaLinea *out;
    const char *root;
    const char *path;
    char *folder;
    VerbaLinea *err_msg;
    const char *folder_read;
    const char *key;
    unsigned long long key_size;
    char *file;
    char *writing;
    unsigned char byte;
    const char *file_read;
    const unsigned char **bytes_u8s;
    unsigned long long *size_pointer;
    unsigned long long *first;
    VerbaLinea *piece;
    const unsigned char *bytes_bytes;
    unsigned long long at;
} RequestsHistoryCtx;
// the first 16 hex digits of the SHA-256 of `size` bytes written into `out`
EMBED_INLINE void history_by_name(const RequestsHistoryCtx *args)
{
    const char *bytes = args->bytes;
    unsigned long long size = args->size;
    char digest[65];
    VerbaLinea digest_line = {digest, sizeof(digest), 0ULL};
    EMBED_CALL(cli_digest_bytes.sha256_text, CliDigestBytesCfg, .bytes = bytes, .size = size, .text = &digest_line);
    mmgr_line_put_n(args->out, digest, 16u);
}

// the folder in orior's own folder that stores the answers of the requests of the file at `path`, into `folder`, which
// is sized for CLI_PATH_MAX_LEN: 1, or 0 with why given. The path is named with the system's own separator, as Rust's
// PathBuf writes it
EMBED_INLINE int history_folder(const RequestsHistoryCtx *args)
{
    const char *root = args->root;
    char *folder = args->folder;
    VerbaLinea *err_msg = args->err_msg;
    char file[CLI_PATH_MAX_LEN];
    char home[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ULL};
    VerbaLinea line = {folder, CLI_PATH_MAX_LEN, 0ULL};
    const int known = EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = args->path,
                                 .joined = file, .err_msg = &none) &&
                      EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home);
    if (known)
    {
#if defined(_WIN32)
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = file,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_POOL_BYTES}), .from = '/', .to = '\\');
#endif
        mmgr_line_put(&line, home);
        mmgr_line_put(&line, "/requests/");
        EMBED_CALL(history_by_name, RequestsHistoryCtx, .bytes = file,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_POOL_BYTES}), .out = &line);
    }
    if (!known || (mmgr_line_finish(&line) == 0ULL))
    {
        mmgr_line_put(err_msg, "orior's own folder is not known");
        return 0;
    }
    return 1;
}

// the file in `folder` that stores the answers of the request named `key`, and the file it is written as before it
// takes that one's place, into `file` and `writing`, which each are sized for CLI_PATH_MAX_LEN: 1, or 0 where a path
// does not fit
EMBED_INLINE int history_file(const RequestsHistoryCtx *args)
{
    VerbaLinea line = {args->file, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->folder_read);
    mmgr_line_ch(&line, '/');
    EMBED_CALL(history_by_name, RequestsHistoryCtx, .bytes = args->key, .size = args->key_size, .out = &line);
    mmgr_line_put(&line, ".json");
    VerbaLinea writing_line = {args->writing, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put_n(&writing_line, args->file, line.at);
    mmgr_line_put(&writing_line, ".writing");
    return (mmgr_line_finish(&line) != 0ULL) && (mmgr_line_finish(&writing_line) != 0ULL);
}

// whether a byte is white as JSON has it
EMBED_INLINE int history_white(const RequestsHistoryCtx *args)
{
    return (args->byte == ' ') || (args->byte == '\t') || (args->byte == '\r') || (args->byte == '\n');
}

// the answers the file at `file` stores, an array of them, its bytes mapped into `*bytes`, `*size` of them, and where
// its first item may start into `*first`: 1, or 0 where there is none or it contains no array, nothing then mapped
EMBED_INLINE int history_read(const RequestsHistoryCtx *args)
{
    const unsigned char **bytes = args->bytes_u8s;
    unsigned long long *size = args->size_pointer;
    unsigned long long *first = args->first;
    *bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->file_read, .size_pointer = size,
                        .owner = g_cli_requests_owner);
    VerbaLinea none = {NULL, 0ULL, 0ULL};
    unsigned long long at = 0ULL;
    while ((*bytes != NULL) && (at < *size) && EMBED_CALL(history_white, RequestsHistoryCtx, .byte = (*bytes)[at]))
    {
        at += 1u;
    }
    if ((*bytes != NULL) && ((at == *size) || ((*bytes)[at] != '[') ||
                             !EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg,
                                         .text = (const char *)*bytes, .size = *size, .out = &none)))
    {
        g_cli_guard->persistent_buf_release((void *)(embed_word)(*bytes), (g_cli_requests_owner));
        *bytes = NULL;
    }
    *first = at + 1u;
    return *bytes != NULL;
}

// the answers stored before, from `at` of `size` bytes, the first CLI_REQUESTS_HISTORY_MAX - 1 of them written into
// `piece`, each after a comma: 1, or 0 where one could not be
EMBED_INLINE int history_before(const RequestsHistoryCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long at = args->at;
    for (unsigned int entry = 1u; entry < CLI_REQUESTS_HISTORY_MAX; entry += 1u)
    {
        const unsigned long long end = EMBED_CALL(cli_json_tree_text_size.json_value_end, CliJsonTreeTextSizeCfg,
                                                  .text = (const char *)args->bytes_bytes, .size = size, .at = at);
        if (end == at)
        {
            return 1;
        }
        mmgr_line_ch(args->piece, ',');
        if (!EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg,
                        .text = (const char *)args->bytes_bytes + at, .size = end - at, .out = args->piece) ||
            !mmgr_line_ok(args->piece))
        {
            return 0;
        }
        at = end;
        while ((at < size) && EMBED_CALL(history_white, RequestsHistoryCtx, .byte = args->bytes_bytes[at]))
        {
            at += 1u;
        }
        at += ((at < size) && (args->bytes_bytes[at] == ',')) ? 1u : 0u;
    }
    return 1;
}

int cli_requests_history_save(const CliRequestsErrMsgCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char writing[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(history_folder, RequestsHistoryCtx, .root = args->root, .path = args->path, .folder = folder,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(history_file, RequestsHistoryCtx, .folder_read = folder, .key = args->key_text,
                    .key_size = args->key_size, .file = file, .writing = writing))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": its path is too long");
        return 0;
    }
    const int made = EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    unsigned long long first = 0ULL;
    const int before = made && EMBED_CALL(history_read, RequestsHistoryCtx, .file_read = file, .bytes_u8s = &bytes,
                                          .size_pointer = &size, .first = &first);
    // the answers written into a capacity of their own, each piece added to the file as the capacity fills
    char capacity[CLI_REQUESTS_CAPACITY];
    CliStateWriting target = {writing, 1};
    VerbaLinea piece = {capacity, sizeof(capacity), 0ULL, history_append, &target, 0ULL};
    int done = made;
    if (done)
    {
        mmgr_line_ch(&piece, '[');
        EMBED_CALL(cli_requests_out_internal.answer_write, CliRequestsOutCtx, .answer = args->answer,
                   .sent = args->sent, .most = CLI_REQUESTS_HISTORY_BODY_MAX, .out = &piece);
        done = mmgr_line_ok(&piece) && ((piece.flushed + piece.at) <= CLI_REQUESTS_PIECE) &&
               (!before || EMBED_CALL(history_before, RequestsHistoryCtx, .piece = &piece, .bytes_bytes = bytes,
                                      .size = size, .at = first));
    }
    if (before)
    {
        g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_requests_owner));
    }
    mmgr_line_ch(&piece, ']');
    done = done && mmgr_line_flush(&piece) &&
           EMBED_CALL(cli_err_msg_from_internal.move, CliErrMsgFromCtx, .from_text = writing, .to = file);
    if (!done)
    {
        mmgr_line_put(args->err_msg, made ? file : folder);
        mmgr_line_put(args->err_msg, ": it could not be written");
    }
    return done;
}

int cli_requests_answers(const CliRequestsTextCfg *args)
{
    char key[CLI_REQUESTS_ADDRESS + CLI_REQUESTS_NAME];
    VerbaLinea key_line = {key, sizeof(key), 0ULL};
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char writing[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_requests_text_table.key_at, CliRequestsTextCfg, .text = args->text, .size = args->size,
                    .line = args->line, .out = &key_line, .err_msg = args->err_msg) ||
        !EMBED_CALL(history_folder, RequestsHistoryCtx, .root = args->root, .path = args->path, .folder = folder,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!mmgr_line_ok(&key_line) || !EMBED_CALL(history_file, RequestsHistoryCtx, .folder_read = folder, .key = key,
                                                .key_size = key_line.at, .file = file, .writing = writing))
    {
        mmgr_line_put(args->err_msg, "the request is larger than impero supports");
        return 0;
    }
    mmgr_line_put(args->out, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = key,
               .length = key_line.at);
    mmgr_line_put(args->out, ",\"answers\":");
    const unsigned char *bytes = NULL;
    unsigned long long byte_count = 0ULL;
    unsigned long long first = 0ULL;
    if (EMBED_CALL(history_read, RequestsHistoryCtx, .file_read = file, .bytes_u8s = &bytes,
                   .size_pointer = &byte_count, .first = &first))
    {
        (void)EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = (const char *)bytes,
                         .size = byte_count, .out = args->out);
        g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_requests_owner));
    }
    else
    {
        mmgr_line_put(args->out, "[]");
    }
    mmgr_line_ch(args->out, '}');
    return 1;
}
