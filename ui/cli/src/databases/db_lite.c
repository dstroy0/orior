// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_lite.c: a SQLite database open in a process of the tree's Python, the shim stored in a file of orior's own
// temporary folder and run from it; a request written a line at a time and its answer read back a line, its rows read a
// few lines at a time, and an interrupt sent from another thread. Each process's place is a cell taken as it opens and
// given back as it closes, its place here empty while none is open in it
#include "../command_line/callback_cfg.h"
#include "../servers/lsp.h"
#include "../tests/test_runs.h"
#include "../text/text.h"
#include "db_internal.h"

static CliDbLite *s_lites[CLI_DB_LITES];

typedef struct
{
    CliDbLite *lite;
    const char *line;
    unsigned long long size;
    CliDbFailure *failure;
    const CliDbLite *lite_read;
    const char *name;
    unsigned long long *start;
    unsigned long long *length;
    const char *request;
    char *path;
    const char *text;
    unsigned long long at;
    unsigned long long length_value;
    void (*each)(const CliEachItemSizeCfg *args);
    void *context;
} DbLiteCtx;
// the line written to the process's input: 1, or 0 with the failure given
EMBED_INLINE int lite_write(const DbLiteCtx *args)
{
    unsigned long long size = args->size;
    CliDbFailure *failure = args->failure;
    const int written = EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx,
                                   .pipe_handle = args->lite->child.in, .bytes = args->line, .size = size) &&
                        EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx,
                                   .pipe_handle = args->lite->child.in, .bytes = "\n", .size = 1ULL);
    if (!written)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the connection broke: SQLite's process takes no more");
    }
    return written;
}

// the next line the process prints read into its capacity, without its newline: 1, or 0 with the failure given
EMBED_INLINE int lite_line(const DbLiteCtx *args)
{
    CliDbFailure *failure = args->failure;
    args->lite->size = 0ULL;
    int fits = 1;
    for (;;)
    {
        if (args->lite->start == args->lite->end)
        {
            const unsigned long long got =
                EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                           .pipe_handle = args->lite->child.out, .piece = (char *)args->lite->in,
                           .capacity = sizeof(args->lite->in));
            if (got == 0ULL)
            {
                EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                           .message = "the connection broke: SQLite's process ended");
                return 0;
            }
            args->lite->start = 0ULL;
            args->lite->end = got;
        }
        const unsigned char *const newline =
            memor.chr(&(MemoriaCfg){.src = args->lite->in + args->lite->start,
                                    .val = '\n',
                                    .bytes = (embed_word)(args->lite->end - args->lite->start)});
        const unsigned long long end =
            (newline != NULL) ? (unsigned long long)(newline - args->lite->in) : args->lite->end;
        const unsigned long long piece = end - args->lite->start;
        fits = fits && (args->lite->size + piece < sizeof(args->lite->line));
        if (fits)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->lite->line + args->lite->size,
                                    .src = args->lite->in + args->lite->start,
                                    .bytes = (embed_word)piece});
            args->lite->size += piece;
        }
        args->lite->start = (newline != NULL) ? end + 1u : end;
        if (newline != NULL)
        {
            break;
        }
    }
    if (!fits)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "SQLite's process printed more in one line than impero reads");
    }
    return fits;
}

// a member of the line the process printed: its start and length, 0 where it has none
EMBED_INLINE int lite_member(const DbLiteCtx *args)
{
    const char *name = args->name;
    unsigned long long *start = args->start;
    unsigned long long *length = args->length;
    return EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->lite_read->line,
                      .size_value = args->lite_read->size, .name = name, .start = start, .length_pointer = length);
}

// `request` sent and its answer read into the line: whether a transaction is open recorded, and an error given as the
// failure. 1, or 0 with the failure given
EMBED_INLINE int lite_query(const DbLiteCtx *args)
{
    unsigned long long size = args->size;
    CliDbFailure *failure = args->failure;
    if (!EMBED_CALL(lite_write, DbLiteCtx, .lite = args->lite, .line = args->request, .size = size,
                    .failure = failure) ||
        !EMBED_CALL(lite_line, DbLiteCtx, .lite = args->lite, .failure = failure))
    {
        return 0;
    }
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    if (!EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = args->lite->line,
                    .size = args->lite->size, .out = &(VerbaLinea){NULL, 0ULL, 0ULL}))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "SQLite's process printed what impero cannot read");
        return 0;
    }
    args->lite->in_transaction =
        EMBED_CALL(lite_member, DbLiteCtx, .lite_read = args->lite, .name = "tx", .start = &start, .length = &length) &&
        (length == 4u) &&
        (memor.cmp(&(MemoriaCfg){.src = args->lite->line + start, .other = "true", .bytes = 4u}) == 0);
    if (EMBED_CALL(lite_member, DbLiteCtx, .lite_read = args->lite, .name = "error", .start = &start,
                   .length = &length))
    {
        args->lite->read_state = 0;
        memor.set(&(MemoriaCfg){.dst = failure, .val = 0, .bytes = sizeof(*failure)});
        (void)EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = args->lite->line + start, .length = length,
                         .out_capacity = failure->message, .capacity = sizeof(failure->message));
        if (EMBED_CALL(lite_member, DbLiteCtx, .lite_read = args->lite, .name = "code", .start = &start,
                       .length = &length))
        {
            (void)EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = args->lite->line + start,
                             .length = length, .out_capacity = failure->code, .capacity = sizeof(failure->code));
        }
        return 0;
    }
    return 1;
}

// the shim written to orior's temporary folder, once, its path into `path`: 1, or 0 where it could not be
EMBED_INLINE int lite_shim(const DbLiteCtx *args)
{
    char *path = args->path;
    static int s_written;
    char temporary[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = temporary);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = temporary, .relative = "orior-sqlite", .joined = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "orior_sqlite.py", .joined = path);
    if (!s_written)
    {
        s_written =
            EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder) &&
            EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                       .bytes = g_cli_db_lite_shim,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_db_lite_shim, .cap = CLI_POOL_BYTES}));
    }
    const int written = s_written;
    return written;
}

CliDbLite *cli_db_lite_open(const CliDbFailureFileCtx *args)
{
    char python[CLI_PATH_MAX_LEN];
    char shim[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure,
                   .message = "no Python is found to read SQLite with");
        memor.cpy(&(MemoriaCfg){
            .dst = args->failure->hint,
            .src = "File, Toolchains finds Python, or takes its folder.",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "File, Toolchains finds Python, or takes its folder.",
                                                   .cap = CLI_POOL_BYTES}) +
                     1u});
        return NULL;
    }
    if (!EMBED_CALL(lite_shim, DbLiteCtx, .path = shim))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure,
                   .message = "orior's SQLite shim could not be written to the temporary folder");
        return NULL;
    }
    CliDbLite *lite = NULL;
    unsigned int entry_at = 0u;
    while ((entry_at < CLI_DB_LITES) && (s_lites[entry_at] != NULL))
    {
        entry_at += 1u;
    }
    // a database's record holds its child process, so the records are fixed outside the pool, one for each database
    // open at once, where they punch no hole in the persistent tier
    static CliDbLite s_places[CLI_DB_LITES];
    if (entry_at < CLI_DB_LITES)
    {
        lite = &s_places[entry_at];
        memor.set(&(MemoriaCfg){.dst = lite, .val = 0, .bytes = sizeof(*lite)});
        s_lites[entry_at] = lite;
    }
    if (lite == NULL)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure,
                   .message = (entry_at < CLI_DB_LITES) ? "no capacity is left to open a SQLite database in"
                                                        : "impero has as many SQLite databases open as it can");
        return NULL;
    }
    lite->start = 0ULL;
    lite->end = 0ULL;
    lite->in_transaction = 0;
    lite->read_state = 0;
    lite->count = 0ULL;
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = lite->path_text,
               .size = sizeof(lite->path_text));
    const CliSetting settings[2] = {{"PATH", lite->path_text}, {"PYTHONIOENCODING", "utf-8"}};
    const char *const words[5] = {python, "-I", "-u", shim, NULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &lite->child, .words = words,
                    .folder = NULL, .settings = settings, .count = 2u, .detached = 1, .err_msg = &err_msg))
    {
        char message[CLI_ERR_MSG_MAX_LEN + 16u];
        VerbaLinea line = {message, sizeof(message), 0ULL};
        mmgr_line_put(&line, "Python: ");
        mmgr_line_put_n(&line, err_msg.out, err_msg.at);
        mmgr_line_finish(&line);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure, .message = message);
        s_lites[entry_at] = NULL;
        return NULL;
    }
    char request[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea query = {request, sizeof(request), 0ULL};
    mmgr_line_put(&query, "{\"open\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &query,
               .text_bytes = (const unsigned char *)args->file,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&query, args->read_only ? ",\"readonly\":true}" : ",\"readonly\":false}");
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    if (!EMBED_CALL(lite_query, DbLiteCtx, .lite = lite, .request = query.out, .size = query.at,
                    .failure = args->failure))
    {
        EMBED_CALL(cli_db_err_msg_internal.lite_close, CliDbErrMsgCtx, .lite = lite);
        return NULL;
    }
    lite->version[0] = '\0';
    if (EMBED_CALL(lite_member, DbLiteCtx, .lite_read = lite, .name = "version", .start = &start, .length = &length))
    {
        (void)EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = lite->line + start, .length = length,
                         .out_capacity = lite->version, .capacity = sizeof(lite->version));
    }
    return lite;
}

void cli_db_lite_close(const CliDbErrMsgCtx *args)
{
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &args->lite->child);
    EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &args->lite->child);
    (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &args->lite->child);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &args->lite->child);
    for (unsigned int entry_at = 0u; entry_at < CLI_DB_LITES; entry_at += 1u)
    {
        s_lites[entry_at] = (s_lites[entry_at] == args->lite) ? NULL : s_lites[entry_at];
    }
}

int cli_db_lite_interrupt(const CliDbErrMsgCtx *args)
{
    static const char s_interrupt[] = "{\"interrupt\":true}\n";
    const int written =
        EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = args->lite->child.in,
                   .bytes = s_interrupt, .size = sizeof(s_interrupt) - 1u);
    if (!written)
    {
        mmgr_line_put(args->err_msg, "SQLite's process takes no more");
    }
    return written;
}

// where the JSON value at `at` of `size` bytes of `text` ends: past it, or `size` where it runs on
EMBED_INLINE unsigned long long lite_value_end(const DbLiteCtx *args)
{
    unsigned long long at = args->at;
    unsigned int depth = 0u;
    int quoted = 0;
    for (; at < args->size; at += 1ULL)
    {
        const char one = args->text[at];
        if (quoted)
        {
            at += (one == '\\') ? 1ULL : 0ULL;
            quoted = (one != '"');
            if (!quoted && (depth == 0u))
            {
                return at + 1ULL;
            }
            continue;
        }
        if (one == '"')
        {
            quoted = 1;
        }
        else if ((one == '[') || (one == '{'))
        {
            depth += 1u;
        }
        else if ((one == ']') || (one == '}'))
        {
            // a closing bracket past a value at the top ends it before the bracket, and one that closes the value's
            // own after the bracket
            if (depth == 0u)
            {
                return at;
            }
            depth -= 1u;
            if (depth == 0u)
            {
                return at + 1ULL;
            }
        }
        else if ((depth == 0u) &&
                 ((one == ',') ||
                  (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_POOL_BYTES, .byte = one}) != NULL)))
        {
            return at;
        }
    }
    return args->size;
}

// the items of the JSON array of `length` bytes at `text`, each passed to `each` by its start and length
EMBED_INLINE void lite_items(const DbLiteCtx *args)
{
    const char *text = args->text;
    unsigned long long at = 0ULL;
    at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = at,
                    .size = args->length_value, .set = "[");
    for (at += 1ULL; at < args->length_value;)
    {
        while ((at < args->length_value) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n,", .cap = CLI_POOL_BYTES, .byte = text[at]}) != NULL))
        {
            at += 1ULL;
        }
        if ((at >= args->length_value) || (text[at] == ']'))
        {
            return;
        }
        const unsigned long long end =
            EMBED_CALL(lite_value_end, DbLiteCtx, .text = text, .size = args->length_value, .at = at);
        EMBED_CALL(args->each, CliEachItemSizeCfg, .context = args->context, .item = text + at, .size = end - at);
        at = (end > at) ? end : at + 1ULL;
    }
}

// a column's name stored in the head
static void lite_column(const CliEachItemSizeCfg *args)
{
    CliDbLite *const lite = (CliDbLite *)args->context;
    if (lite->head.column_count < CLI_DB_COLUMNS)
    {
        CliDbColumn *const column = &lite->head.columns[lite->head.column_count];
        column->name[0] = '\0';
        (void)EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = args->item, .length = args->size,
                         .out_capacity = column->name, .capacity = sizeof(column->name));
        column->type[0] = '\0';
        column->numeric = 0;
        lite->head.column_count += 1u;
    }
}

int cli_db_lite_start(const CliDbFailureStatementCtx *args)
{
    char tag[CLI_DB_TAG];
    for (int done = !args->lite->read_state; !done;)
    {
        if (!EMBED_CALL(cli_db_failure_internal.lite_rows, CliDbFailureCtx, .lite = args->lite, .count = 4096ULL,
                        .each_row = NULL, .context = NULL, .done = &done, .tag = tag, .failure = args->failure))
        {
            return 0;
        }
    }
    // the request a cell of the statement's quoted size, six bytes at max for each of its bytes, and no more than a
    // line SQLite's process reads
    const unsigned long long quoted = (6ULL * args->size) + 16ULL;
    const unsigned long long capacity = (quoted < CLI_DB_LITE_LINE) ? quoted : CLI_DB_LITE_LINE;
    char *const request = g_cli_guard->temporary_buf_alloc(capacity, g_cli_db_owner);
    VerbaLinea query = mmgr_line_over(request, capacity);
    mmgr_line_put(&query, "{\"start\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &query,
               .text_bytes = (const unsigned char *)args->statement, .length = args->size);
    mmgr_line_ch(&query, '}');
    const int fits = mmgr_line_ok(&query);
    const int answered = fits && EMBED_CALL(lite_query, DbLiteCtx, .lite = args->lite, .request = query.out,
                                            .size = query.at, .failure = args->failure);
    g_cli_guard->persistent_buf_release(request, g_cli_db_owner);
    if (!fits)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure,
                   .message = (request != NULL) ? "the statement is longer than impero sends SQLite's process"
                                                : "no capacity is left to send the statement in");
    }
    if (!answered)
    {
        return 0;
    }
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    args->lite->head.column_count = 0u;
    if (EMBED_CALL(lite_member, DbLiteCtx, .lite_read = args->lite, .name = "done", .start = &start,
                   .length = &length) &&
        (args->lite->line[start] == '"'))
    {
        args->lite->head.rows = 0;
        args->lite->head.tag[0] = '\0';
        (void)EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = args->lite->line + start, .length = length,
                         .out_capacity = args->lite->head.tag, .capacity = sizeof(args->lite->head.tag));
        return 1;
    }
    args->lite->read_state = 1;
    args->lite->count = 0ULL;
    args->lite->head.rows = 1;
    if (EMBED_CALL(lite_member, DbLiteCtx, .lite_read = args->lite, .name = "columns", .start = &start,
                   .length = &length))
    {
        EMBED_CALL(lite_items, DbLiteCtx, .text = args->lite->line + start, .length_value = length, .each = lite_column,
                   .context = args->lite);
    }
    return 1;
}

// The reading of one line's rows: the process, the text its values are decoded into so far, how many values a row has
// so far, where its rows go, and whether they are wanted still
typedef struct
{
    CliDbLite *lite;
    unsigned long long used;
    unsigned int values;
    CliDbRowFn each_row;
    void *context;
    int going;
    unsigned long long rows;
} CliDbLiteRead;

// a value of a row decoded into the line's text
static void lite_value(const CliEachItemSizeCfg *args)
{
    CliDbLiteRead *const read = (CliDbLiteRead *)args->context;
    CliDbLite *const lite = read->lite;
    if (read->values == CLI_DB_COLUMNS)
    {
        return;
    }
    CliDbValue *const value = &lite->row[read->values];
    read->values += 1u;
    if ((args->size == 4u) && (memor.cmp(&(MemoriaCfg){.src = args->item, .other = "null", .bytes = 4u}) == 0))
    {
        *value = (CliDbValue){NULL, 0ULL, 1};
        return;
    }
    char *const text = lite->text + read->used;
    if (!EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = args->item, .length = args->size,
                    .out_capacity = text, .capacity = sizeof(lite->text) - read->used))
    {
        *value = (CliDbValue){"", 0ULL, 0};
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES});
    *value = (CliDbValue){text, length, 0};
    read->used += length + 1ULL;
}

// a row of the line read and passed on
static void lite_row(const CliEachItemSizeCfg *args)
{
    void *context = args->context;
    CliDbLiteRead *const read = (CliDbLiteRead *)context;
    read->values = 0u;
    read->used = 0ULL;
    EMBED_CALL(lite_items, DbLiteCtx, .text = args->item, .length_value = args->size, .each = lite_value,
               .context = read);
    read->rows += 1ULL;
    if (read->going && (read->each_row != NULL))
    {
        read->going = EMBED_CALL(read->each_row, CliDbRowCfg, .context = read->context, .values = read->lite->row,
                                 .count = read->values);
    }
}

int cli_db_lite_rows(const CliDbFailureCtx *args)
{
    unsigned long long count = args->count;
    int *done = args->done;
    *done = 0;
    args->tag[0] = '\0';
    if (!args->lite->read_state)
    {
        *done = 1;
        return 1;
    }
    CliDbLiteRead read = {args->lite, 0ULL, 0u, args->each_row, args->context, 1, 0ULL};
    while (read.going && (read.rows < count))
    {
        const unsigned long long left = count - read.rows;
        char request[64];
        VerbaLinea query = {request, sizeof(request), 0ULL};
        mmgr_line_put(&query, "{\"rows\":");
        mmgr_line_decimal(&query, (left < CLI_DB_LITE_ROWS) ? left : CLI_DB_LITE_ROWS, 1u);
        mmgr_line_ch(&query, '}');
        if (!EMBED_CALL(lite_query, DbLiteCtx, .lite = args->lite, .request = query.out, .size = query.at,
                        .failure = args->failure))
        {
            return 0;
        }
        unsigned long long start = 0ULL;
        unsigned long long length = 0ULL;
        const unsigned long long before = read.rows;
        if (EMBED_CALL(lite_member, DbLiteCtx, .lite_read = args->lite, .name = "rows", .start = &start,
                       .length = &length))
        {
            EMBED_CALL(lite_items, DbLiteCtx, .text = args->lite->line + start, .length_value = length,
                       .each = lite_row, .context = &read);
        }
        args->lite->count += read.rows - before;
        const int ended =
            !EMBED_CALL(lite_member, DbLiteCtx, .lite_read = args->lite, .name = "done", .start = &start,
                        .length = &length) ||
            ((length == 4u) &&
             (memor.cmp(&(MemoriaCfg){.src = args->lite->line + start, .other = "true", .bytes = 4u}) == 0));
        if (ended)
        {
            args->lite->read_state = 0;
            VerbaLinea out = {args->tag, CLI_DB_TAG, 0ULL};
            mmgr_line_decimal(&out, args->lite->count, 1u);
            mmgr_line_put(&out, (args->lite->count == 1ULL) ? " row read" : " rows read");
            mmgr_line_finish(&out);
            *done = 1;
            return 1;
        }
    }
    return 1;
}
