// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_history.c: each statement run on a connection recorded in its history, a JSON line each in orior's own folder
// under databases/, by the account's hash, the newest 1000 of them; the history read back, newest first; and the
// statements of an editor's text found by their spans, counted in characters as the editor counts them
#include "../command_line/pool.h"
#include "../text/digest.h"
#include "../text/text.h"
#include "db_internal.h"
#include "sql.h"

typedef struct
{
    const CliDbOpen *open;
    char *path;
    const char *text;
    unsigned long long size;
} DbHistoryCtx;

// the connection's history file into `path`, sized for CLI_PATH_MAX_LEN, its folder made: 1, or 0 where orior_cli has
// no home folder
EMBED_INLINE int history_file(const DbHistoryCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char account[CLI_PATH_MAX_LEN];
    char hash[80];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "databases", .joined = folder);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        return 0;
    }
    VerbaLinea account_line = {account, sizeof(account), 0ULL};
    EMBED_CALL(cli_db_connection_out_kind.account, CliDbConnectionOutKindCfg, .connection = &args->open->config,
               .out = &account_line);
    VerbaLinea hash_line = {hash, sizeof(hash), 0ULL};
    EMBED_CALL(cli_digest_bytes.sha256_text, CliDigestBytesCfg, .bytes = account_line.out, .size = account_line.at,
               .text = &hash_line);
    hash_line.at = 16ULL;
    mmgr_line_put(&hash_line, ".jsonl");
    mmgr_line_finish(&hash_line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = hash, .joined = args->path);
    return 1;
}

void cli_db_remembered(const CliDbStatementCtx *args)
{
    static char s_entry[1u << 20u];
    char path[CLI_PATH_MAX_LEN];
    char next[CLI_PATH_MAX_LEN + 8u];
    if (!EMBED_CALL(history_file, DbHistoryCtx, .open = args->open, .path = path))
    {
        return;
    }
    VerbaLinea next_line = {next, sizeof(next), 0ULL};
    mmgr_line_put(&next_line, path);
    mmgr_line_put(&next_line, ".next");
    mmgr_line_finish(&next_line);
    VerbaLinea entry = {s_entry, sizeof(s_entry), 0ULL};
    mmgr_line_put(&entry, "{\"at\":");
    mmgr_line_decimal(&entry, cli_wall_milliseconds(), 1u);
    mmgr_line_put(&entry, ",\"sql\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &entry,
               .text_bytes = (const unsigned char *)args->statement, .length = args->size);
    mmgr_line_put(&entry, ",\"ms\":");
    mmgr_line_decimal(&entry, args->consumed, 1u);
    mmgr_line_put(&entry, args->failed ? ",\"failed\":" : ",\"result\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &entry,
               .text_bytes = (const unsigned char *)args->err_text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->err_text, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&entry, "}\n");
    // the history's newest lines but one retained, then the entry after them
    unsigned long long history_size = 0ULL;
    const unsigned char *const history = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                    .size_pointer = &history_size, .owner = g_cli_db_owner);
    // each newline before the last line's end starts one more line retained, counted back from the end
    unsigned long long start = 0ULL;
    unsigned long long end =
        ((history_size > 0u) && (history[history_size - 1u] == '\n')) ? history_size - 1u : history_size;
    unsigned int count = 0u;
    const char *newline = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = (const char *)history,
                                     .length_value = end, .byte = '\n');
    while ((newline != NULL) && (count < CLI_DB_HISTORY - 2u))
    {
        count += 1u;
        // a newline found lies in the bytes
        newline = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = (const char *)history,
                             .length_value = (unsigned long long)(newline - (const char *)history), .byte = '\n');
    }
    start = (newline != NULL) ? ((unsigned long long)(newline - (const char *)history) + 1ULL) : 0ULL;
    int written = mmgr_line_ok(&entry) && EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = next,
                                                     .bytes = "", .size = 0ULL, .fresh = 1);
    if (written && (start < history_size))
    {
        written = EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = next,
                             .bytes = history + start, .size = history_size - start, .fresh = 0);
        if (written && (history[history_size - 1u] != '\n'))
        {
            written = EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = next, .bytes = "\n",
                                 .size = 1ULL, .fresh = 0);
        }
    }
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)history, (g_cli_db_owner));
    written = written && EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = next,
                                    .bytes = entry.out, .size = entry.at, .fresh = 0);
    if (written)
    {
        (void)EMBED_CALL(cli_err_msg_from_internal.move, CliErrMsgFromCtx, .from_text = next, .to = path);
    }
}

void cli_db_history(const CliDbOutCountCfg *args)
{
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(history_file, DbHistoryCtx, .open = args->open, .path = path)
                                           ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                        .size_pointer = &size, .owner = g_cli_db_owner)
                                           : NULL;
    mmgr_line_ch(args->out, '[');
    unsigned int written = 0u;
    // the lines from the last back, each that reads as JSON
    for (unsigned long long end = size; (bytes != NULL) && (end > 0u);)
    {
        unsigned long long start = end;
        start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                           .at = start, .set = "\n");
        VerbaLinea none = {NULL, 0ULL, 0ULL};
        if ((end > start) && EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg,
                                        .text = (const char *)bytes + start, .size = end - start, .out = &none))
        {
            mmgr_line_put(args->out, (written > 0u) ? "," : "");
            mmgr_line_put_n(args->out, (const char *)bytes + start, end - start);
            written += 1u;
        }
        end = (start > 0u) ? start - 1u : 0u;
    }
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_db_owner));
    mmgr_line_ch(args->out, ']');
}

// the characters of the first `size` bytes of a text, each counted once at its first byte
EMBED_INLINE unsigned long long history_characters(const DbHistoryCtx *args)
{
    unsigned long long count = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        count += (((unsigned char)args->text[at] & 0xc0u) != 0x80u) ? 1ULL : 0ULL;
    }
    return count;
}

void cli_db_statement_at(const CliDbOutSizeCfg *args)
{
    // the byte the character at `offset` starts at, the text's end where it is past its last
    unsigned long long byte = 0ULL;
    for (unsigned long long seen = 0ULL; byte < args->size; byte += 1ULL)
    {
        if (((unsigned char)args->text[byte] & 0xc0u) != 0x80u)
        {
            if (seen == args->offset)
            {
                break;
            }
            seen += 1ULL;
        }
    }
    CliSqlSpan span;
    if (!EMBED_CALL(cli_sql_text.statement_at, CliSqlTextCfg, .text = args->text, .length = args->size,
                    .at_value = byte, .dialect = EMBED_CALL(cli_sql.dialect, CliSqlCfg, .name = args->kind),
                    .span = &span))
    {
        mmgr_line_put(args->out, "null");
        return;
    }
    mmgr_line_ch(args->out, '[');
    mmgr_line_decimal(args->out, EMBED_CALL(history_characters, DbHistoryCtx, .text = args->text, .size = span.start),
                      1u);
    mmgr_line_ch(args->out, ',');
    mmgr_line_decimal(args->out, EMBED_CALL(history_characters, DbHistoryCtx, .text = args->text, .size = span.end),
                      1u);
    mmgr_line_ch(args->out, ']');
}

// The spans of a text's statements being written: the text and where they go
typedef struct
{
    const char *text;
    VerbaLinea *out;
    unsigned int written;
} CliDbSpans;

static void history_span(const CliEachSpanCfg *args)
{
    CliDbSpans *const spans = (CliDbSpans *)args->context;
    mmgr_line_put(spans->out, (spans->written > 0u) ? ",[" : "[");
    mmgr_line_decimal(spans->out,
                      EMBED_CALL(history_characters, DbHistoryCtx, .text = spans->text, .size = args->span.start), 1u);
    mmgr_line_ch(spans->out, ',');
    mmgr_line_decimal(spans->out,
                      EMBED_CALL(history_characters, DbHistoryCtx, .text = spans->text, .size = args->span.end), 1u);
    mmgr_line_ch(spans->out, ']');
    spans->written += 1u;
}

void cli_db_statements(const CliDbOutSizeCfg *args)
{
    CliDbSpans spans = {args->text, args->out, 0u};
    mmgr_line_ch(args->out, '[');
    EMBED_CALL(cli_sql_text_length.statements, CliSqlTextLengthCfg, .text = args->text, .length = args->size,
               .dialect = EMBED_CALL(cli_sql.dialect, CliSqlCfg, .name = args->kind), .each = history_span,
               .context = &spans);
    mmgr_line_ch(args->out, ']');
}
