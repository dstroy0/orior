// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_export.c: the rows buffered, and then the rest the run gives, written to a file as CSV, TSV or JSON lines, the run
// read on to its end and not run again, and how many are written reported after each 4096 it reads on
#include "../command_line/callback_cfg.h"
#include "../servers/lsp.h"
#include "db_internal.h"

#include "../text/text.h"

// The writing of an export: its connection, its file and format, the bytes not yet written to it, whether a write
// failed, how many rows it has written, the columns in the order a JSON line names them, each once, and a row's values;
// the values, the order and the bytes not yet written one cell, the values and the order `columns` long
typedef struct
{
    CliDbOpen *open;
    const char *path;
    const char *format;
    char *pending;
    unsigned long long pending_size;
    int broke;
    unsigned long long written;
    unsigned int *order;
    unsigned int ordered;
    CliDbValue *values;
    unsigned int columns;
} CliDbExport;
// the bytes written to the file at a time, the most of the values of one buffered row read back, and the bytes a value
// of a JSON line is written in
#define CLI_DB_EXPORT_CAPACITY (1u << 20u)
#define CLI_DB_EXPORT_ROW (1u << 20u)
#define CLI_DB_EXPORT_VALUE_MORE 64ULL

typedef struct
{
    CliDbExport *export;
    const char *text;
    unsigned long long size;
    const CliDbValue *value;
    const CliDbValue *values;
    unsigned int count;
} DbExportCtx;
// the bytes waiting written to the file
EMBED_INLINE void export_flush(const DbExportCtx *args)
{
    if ((args->export->pending_size > 0u) && !args->export->broke)
    {
        args->export->broke =
            !EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = args->export->path,
                        .bytes = args->export->pending, .size = args->export->pending_size, .fresh = 0);
    }
    args->export->pending_size = 0ULL;
}

// a text written after the bytes waiting, written out first where it does not fit after them
EMBED_INLINE void export_append(const DbExportCtx *args)
{
    unsigned long long size = args->size;
    if (args->export->pending_size + size > CLI_DB_EXPORT_CAPACITY)
    {
        export_flush(args);
    }
    if (size > CLI_DB_EXPORT_CAPACITY)
    {
        args->export->broke = args->export->broke ||
                              !EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx,
                                          .path = args->export->path, .bytes = args->text, .size = size, .fresh = 0);
        return;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = args->export->pending + args->export->pending_size, .src = args->text, .bytes = (embed_word)size});
    args->export->pending_size += size;
}

// a value written into a line of CSV: as it is, or quoted with its quotes doubled where it contains a comma, a quote or
// a line's end
EMBED_INLINE void export_csv_value(const DbExportCtx *args)
{
    if (args->value->null)
    {
        return;
    }
    int quoted = 0;
    for (unsigned long long at = 0ULL; (at < args->value->size) && !quoted; at += 1ULL)
    {
        quoted =
            (cellul.chr(&(CatenaFinitaCfg){.src = ",\"\n\r", .cap = CLI_POOL_BYTES, .byte = args->value->text[at]}) !=
             NULL) &&
            (args->value->text[at] != '\0');
    }
    if (!quoted)
    {
        EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = args->value->text,
                   .size = args->value->size);
        return;
    }
    EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = "\"", .size = 1ULL);
    for (unsigned long long at = 0ULL; at < args->value->size; at += 1ULL)
    {
        EMBED_CALL(export_append, DbExportCtx, .export = args->export,
                   .text = (args->value->text[at] == '"') ? "\"\"" : args->value->text + at,
                   .size = (args->value->text[at] == '"') ? 2ULL : 1ULL);
    }
    EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = "\"", .size = 1ULL);
}

// a row written as a line of the export's format
static void export_line(const DbExportCtx *args)
{
    char piece[CLI_DB_FIELD];
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->export->format, .other = "json", .cap = CLI_POOL_BYTES}))
    {
        // an object of each column's name and value, the names in order and a name named twice retaining its last value
        EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = "{", .size = 1ULL);
        for (unsigned int at = 0u; at < args->export->ordered; at += 1u)
        {
            const unsigned int column = args->export->order[at];
            const char *const name = args->export->open->results.columns[column].name;
            VerbaLinea line = {piece, sizeof(piece), 0ULL};
            mmgr_line_put(&line, (at > 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &line,
                       .text_bytes = (const unsigned char *)name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
            mmgr_line_ch(&line, ':');
            EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = line.out, .size = line.at);
            if ((column >= args->count) || args->values[column].null)
            {
                EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = "null", .size = 4ULL);
                continue;
            }
            const unsigned long long value_capacity = (args->values[column].size * 6ULL) + CLI_DB_EXPORT_VALUE_MORE;
            char *const value_cell = g_cli_guard->temporary_buf_alloc(value_capacity, g_cli_db_owner);
            VerbaLinea value = mmgr_line_over(value_cell, value_capacity);
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &value,
                       .text_bytes = (const unsigned char *)args->values[column].text,
                       .length = args->values[column].size);
            args->export->broke = args->export->broke || !mmgr_line_ok(&value);
            EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = value.out,
                       .size = mmgr_line_ok(&value) ? value.at : 0ULL);
            g_cli_guard->persistent_buf_release(value.out, g_cli_db_owner);
        }
        EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = "}\n", .size = 2ULL);
        return;
    }
    const int tabs =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->export->format, .other = "tsv", .cap = CLI_POOL_BYTES}));
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const parting = tabs ? "\t" : ",";
        EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = (at > 0u) ? parting : "",
                   .size = (at > 0u) ? 1ULL : 0ULL);
        if (!tabs)
        {
            EMBED_CALL(export_csv_value, DbExportCtx, .export = args->export, .value = &args->values[at]);
            continue;
        }
        for (unsigned long long one = 0ULL; !args->values[at].null && (one < args->values[at].size); one += 1ULL)
        {
            const char letter = args->values[at].text[one];
            EMBED_CALL(export_append, DbExportCtx, .export = args->export,
                       .text = ((letter == '\t') || (letter == '\n')) ? " " : args->values[at].text + one,
                       .size = 1ULL);
        }
    }
    EMBED_CALL(export_append, DbExportCtx, .export = args->export, .text = "\n", .size = 1ULL);
}

// a buffered row's line, a JSON array of strings and nulls, read back into values and written
static void export_results_line(const CliEachLineSizeCfg *args)
{
    unsigned long long size = args->size;
    CliDbExport *const export = (CliDbExport *)args->context;
    // the values read back take no more bytes than the line writes them in, each string's quotes capacity for its
    // terminator
    const unsigned long long capacity = (size < CLI_DB_EXPORT_ROW) ? (size + 1ULL) : CLI_DB_EXPORT_ROW;
    char *const texts = g_cli_guard->temporary_buf_alloc(capacity, g_cli_db_owner);
    if (texts == NULL)
    {
        export->broke = 1;
        return;
    }
    unsigned int count = 0u;
    unsigned long long used = 0ULL;
    for (unsigned long long at = 1ULL; (at < size) && (count < export->columns);)
    {
        while ((at < size) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " ,", .cap = CLI_POOL_BYTES, .byte = args->line[at]}) != NULL))
        {
            at += 1ULL;
        }
        if ((at >= size) || (args->line[at] == ']'))
        {
            break;
        }
        if (args->line[at] == 'n')
        {
            export->values[count] = (CliDbValue){NULL, 0ULL, 1};
            count += 1u;
            at += 4ULL;
            continue;
        }
        unsigned long long end = at + 1ULL;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->line, .at = end,
                         .size = size, .set = "\"\\");
        while ((end < size) && (args->line[end] == '\\'))
        {
            end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->line,
                             .at = end + 2ULL, .size = size, .set = "\"\\");
        }
        char *const text = texts + used;
        if (!EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = args->line + at, .length = end + 1u - at,
                        .out_capacity = text, .capacity = capacity - used))
        {
            text[0] = '\0';
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES});
        export->values[count] = (CliDbValue){text, length, 0};
        used += length + 1ULL;
        count += 1u;
        at = end + 1ULL;
    }
    EMBED_CALL(export_line, DbExportCtx, .export = export, .values = export->values, .count = count);
    export->written += 1ULL;
    g_cli_guard->persistent_buf_release(texts, g_cli_db_owner);
}

// a row read on from the run written, and counted past those buffered
static int export_row(const CliDbRowCfg *args)
{
    const CliDbValue *values = args->values;
    unsigned int count = args->count;
    CliDbExport *const export = (CliDbExport *)args->context;
    EMBED_CALL(export_line, DbExportCtx, .export = export, .values = values, .count = count);
    export->written += 1ULL;
    return !export->broke;
}

// The reading on of an export: the export, and how the run ended
typedef struct
{
    CliDbExport *export;
    int done;
    char tag[CLI_DB_TAG];
} CliDbExportReadState;

static int export_read_on(const CliEachLinkFailureCfg *args)
{
    void *context = args->context;
    CliDbLink *link = args->link;
    CliDbFailure *failure = args->failure;
    CliDbExportReadState *const read_state = (CliDbExportReadState *)context;
    return EMBED_CALL(cli_db_link_failure_internal.link_rows, CliDbLinkFailureCtx, .link = link, .count = 4096ULL,
                      .each_row = export_row, .context = read_state->export, .done = &read_state->done,
                      .tag = read_state->tag, .failure = failure);
}

// the columns put in the order a JSON line names them, by their names' bytes, a name named twice once at its last
EMBED_INLINE void export_order(const DbExportCtx *args)
{
    const CliDbResults *const results = &args->export->open->results;
    unsigned int count = 0u;
    for (unsigned int column = 0u; column < results->column_count; column += 1u)
    {
        unsigned int at = count;
        while ((at > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                        .one = results->columns[args->export->order[at - 1u]].name,
                                        .other_pointer = results->columns[column].name) > 0))
        {
            at -= 1u;
        }
        if ((at > 0u) && (cellul.eq(&(CatenaFinitaCfg){.src = results->columns[args->export->order[at - 1u]].name,
                                                       .other = results->columns[column].name,
                                                       .cap = CLI_POOL_BYTES})))
        {
            args->export->order[at - 1u] = column;
            continue;
        }
        memor.move_up(&(MemoriaCfg){.dst = &args->export->order[at + 1u],
                                    .src = &args->export->order[at],
                                    .bytes = (embed_word)(count - at) * sizeof(args->export->order[0])});
        args->export->order[at] = column;
        count += 1u;
    }
    args->export->ordered = count;
}

int cli_db_export(const CliDbOutCfg *args)
{
    const char *path = args->path;
    VerbaLinea *out = args->out;
    CliDbFailure *failure = args->failure;
    CliDbResults *const results = &args->open->results;
    if (!results->has_columns)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "no statement has given rows to export");
        return 0;
    }
    // a row's values, the columns' order, then the bytes not yet written, each list on its type's boundary
    const unsigned int columns = (results->column_count != 0u) ? results->column_count : 1u;
    const unsigned long long values_bytes = columns * sizeof(CliDbValue);
    const unsigned long long order_bytes = columns * sizeof(unsigned int);
    char *const cell =
        g_cli_guard->temporary_buf_alloc(values_bytes + order_bytes + CLI_DB_EXPORT_CAPACITY, g_cli_db_owner);
    if (cell == NULL)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the cells cannot meet an export");
        return 0;
    }
    CliDbExport results_export = {args->open,
                                  path,
                                  args->format,
                                  cell + values_bytes + order_bytes,
                                  0ULL,
                                  !EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path,
                                              .bytes = "", .size = 0ULL, .fresh = 1),
                                  0ULL,
                                  (unsigned int *)(void *)(cell + values_bytes),
                                  0u,
                                  (CliDbValue *)(void *)cell,
                                  columns};
    CliDbExport *const export = &results_export;
    EMBED_CALL(export_order, DbExportCtx, .export = export);
    if (!export->broke && (!cellul.eq(&(CatenaFinitaCfg){.src = args->format, .other = "json", .cap = CLI_POOL_BYTES})))
    {
        for (unsigned int at = 0u; at < results->column_count; at += 1u)
        {
            export->values[at] = (CliDbValue){
                results->columns[at].name,
                cellul.len(&(CatenaFinitaCfg){.src = results->columns[at].name, .cap = CLI_POOL_BYTES}), 0};
        }
        EMBED_CALL(export_line, DbExportCtx, .export = export, .values = export->values,
                   .count = results->column_count);
    }
    if (!export->broke)
    {
        EMBED_CALL(cli_db_context_internal.results_lines, CliDbContextCtx, .results = results, .start = 0ULL,
                   .end = results->rows, .each_callback = export_results_line, .context = export);
    }
    int read = 1;
    while (read && !export->broke && !results->has_done)
    {
        CliDbExportReadState read_state = {export, 0, ""};
        const unsigned long long before = export->written;
        read = EMBED_CALL(cli_db_failure_context_internal.with_query, CliDbFailureContextCtx, .open = args->open,
                          .work = export_read_on, .context = &read_state, .failure = failure);
        results->beyond += export->written - before;
        results->has_done = read && read_state.done;
        memor.cpy(&(MemoriaCfg){
            .dst = results->done, .bytes = sizeof(results->done), .src = results->has_done ? read_state.tag : ""});
        if (read && (args->progress != NULL))
        {
            EMBED_CALL(args->progress, CliEachWrittenCfg, .context = args->context, .written = export->written);
        }
    }
    EMBED_CALL(export_flush, DbExportCtx, .export = export);
    const int broke = export->broke;
    const unsigned long long written = export->written;
    g_cli_guard->persistent_buf_release(cell, g_cli_db_owner);
    if (!read)
    {
        return 0;
    }
    if (broke)
    {
        char msg_buf[CLI_PATH_MAX_LEN + 64u];
        VerbaLinea line = {msg_buf, sizeof(msg_buf), 0ULL};
        mmgr_line_put(&line, path);
        mmgr_line_put(&line, ": the file could not be written");
        mmgr_line_finish(&line);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = msg_buf);
        return 0;
    }
    mmgr_line_put(out, "{\"written\":");
    mmgr_line_decimal(out, written, 1u);
    mmgr_line_put(out, ",\"path\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}));
    mmgr_line_ch(out, '}');
    return 1;
}
