// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_export.c: the rows buffered, and then the rest the run gives, written to a file as CSV, TSV or JSON lines, the run
// read on to its end and not run again, and how many are written reported after each 4096 it reads on
#include "../servers/lsp.h"
#include "db_internal.h"

#include "../text/text.h"

// the bytes written to the file at a time, the most of the values of one buffered row read back, and the bytes a value
// of a JSON line is begun to be written in, doubling as it is
#define CLI_DB_EXPORT_ROOM (1u << 20u)
#define CLI_DB_EXPORT_ROW (1u << 20u)
#define CLI_DB_EXPORT_VALUE_FIRST 64u

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

// the pending bytes written to the file
static void export_flushed(CliDbExport *export)
{
    if ((export->pending_size > 0u) && !export->broke)
    {
        export->broke = !cli_file_appended(export->path, export->pending, export->pending_size, 0);
    }
    export->pending_size = 0ull;
}

// a text written to the pending bytes, written out first where it does not fit after them
static void export_put(CliDbExport *export, const char *text, unsigned long long size)
{
    if (export->pending_size + size > CLI_DB_EXPORT_ROOM)
    {
        export_flushed(export);
    }
    if (size > CLI_DB_EXPORT_ROOM)
    {
        export->broke = export->broke || !cli_file_appended(export->path, text, size, 0);
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = export->pending + export->pending_size, .src = text, .bytes = (size_t)size});
    export->pending_size += size;
}

// a value written into a line of CSV: as it is, or quoted with its quotes doubled where it contains a comma, a quote or
// a line's end
static void export_csv_value(CliDbExport *export, const CliDbValue *value)
{
    if (value->null)
    {
        return;
    }
    int quoted = 0;
    for (unsigned long long at = 0ull; (at < value->size) && !quoted; at += 1ull)
    {
        quoted = (cellul.chr(&(CatenaFinitaCfg){.src = ",\"\n\r", .cap = CLI_CELLS_POOL, .byte = value->text[at]}) !=
                  NULL) &&
                 (value->text[at] != '\0');
    }
    if (!quoted)
    {
        export_put(export, value->text, value->size);
        return;
    }
    export_put(export, "\"", 1ull);
    for (unsigned long long at = 0ull; at < value->size; at += 1ull)
    {
        export_put(export, (value->text[at] == '"') ? "\"\"" : value->text + at,
                   (value->text[at] == '"') ? 2ull : 1ull);
    }
    export_put(export, "\"", 1ull);
}

// a row written as a line of the export's format
static void export_line(CliDbExport *export, const CliDbValue *values, unsigned int count)
{
    char piece[CLI_DB_FIELD];
    if (cellul.eq(&(CatenaFinitaCfg){.src = export->format, .other = "json", .cap = CLI_CELLS_POOL}))
    {
        // an object of each column's name and value, the names in order and a name named twice retaining its last value
        export_put(export, "{", 1ull);
        for (unsigned int at = 0u; at < export->ordered; at += 1u)
        {
            const unsigned int column = export->order[at];
            const char *const name = export->open->results.columns[column].name;
            VerbaLinea line = {piece, sizeof(piece), 0ull};
            mmgr_line_put(&line, (at > 0u) ? "," : "");
            cli_json_quoted_lossy(&line, (const unsigned char *)name,
                                  cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
            mmgr_line_ch(&line, ':');
            export_put(export, line.out, line.at);
            if ((column >= count) || values[column].null)
            {
                export_put(export, "null", 4ull);
                continue;
            }
            VerbaLinea value;
            (void)cli_growing_line_made(&value, values[column].size + CLI_DB_EXPORT_VALUE_FIRST);
            cli_json_quoted_lossy(&value, (const unsigned char *)values[column].text, values[column].size);
            export->broke = export->broke || !mmgr_line_ok(&value);
            export_put(export, value.out, mmgr_line_ok(&value) ? value.at : 0ull);
            cli_cell_release(value.out, NULL);
        }
        export_put(export, "}\n", 2ull);
        return;
    }
    const int tabs = (cellul.eq(&(CatenaFinitaCfg){.src = export->format, .other = "tsv", .cap = CLI_CELLS_POOL}));
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        export_put(export, (at > 0u) ? (tabs ? "\t" : ",") : "", (at > 0u) ? 1ull : 0ull);
        if (!tabs)
        {
            export_csv_value(export, &values[at]);
            continue;
        }
        for (unsigned long long one = 0ull; !values[at].null && (one < values[at].size); one += 1ull)
        {
            const char letter = values[at].text[one];
            export_put(export, ((letter == '\t') || (letter == '\n')) ? " " : values[at].text + one, 1ull);
        }
    }
    export_put(export, "\n", 1ull);
}

// a buffered row's line, a JSON array of strings and nulls, read back into values and written
static void export_results_line(void *context, const char *line, unsigned long long size)
{
    CliDbExport *const export = (CliDbExport *)context;
    // the values read back take no more bytes than the line writes them in, each string's quotes room for its
    // terminator
    const unsigned long long room = (size < CLI_DB_EXPORT_ROW) ? (size + 1ull) : CLI_DB_EXPORT_ROW;
    char *const texts = cli_lane_alloc(room, &g_cli_db_cells);
    if (texts == NULL)
    {
        export->broke = 1;
        return;
    }
    unsigned int count = 0u;
    unsigned long long used = 0ull;
    for (unsigned long long at = 1ull; (at < size) && (count < export->columns);)
    {
        while ((at < size) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " ,", .cap = CLI_CELLS_POOL, .byte = line[at]}) != NULL))
        {
            at += 1ull;
        }
        if ((at >= size) || (line[at] == ']'))
        {
            break;
        }
        if (line[at] == 'n')
        {
            export->values[count] = (CliDbValue){NULL, 0ull, 1};
            count += 1u;
            at += 4ull;
            continue;
        }
        unsigned long long end = at + 1ull;
        while ((end < size) && (line[end] != '"'))
        {
            end += (line[end] == '\\') ? 2ull : 1ull;
        }
        char *const text = texts + used;
        if (!cli_lsp_string(line + at, end + 1u - at, text, room - used))
        {
            text[0] = '\0';
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
        export->values[count] = (CliDbValue){text, length, 0};
        used += length + 1ull;
        count += 1u;
        at = end + 1ull;
    }
    export_line(export, export->values, count);
    export->written += 1ull;
    cli_cell_release(texts, &g_cli_db_cells);
}

// a row read on from the run written, and counted past those buffered
static int export_row(void *context, const CliDbValue *values, unsigned int count)
{
    CliDbExport *const export = (CliDbExport *)context;
    export_line(export, values, count);
    export->written += 1ull;
    return !export->broke;
}

// The reading on of an export: the export, and how the run ended
typedef struct
{
    CliDbExport *export;
    int done;
    char tag[CLI_DB_TAG];
} CliDbExportReading;

static int export_read_on(void *context, CliDbLink *link, CliDbFailure *failure)
{
    CliDbExportReading *const reading = (CliDbExportReading *)context;
    return cli_db_link_rows(link, 4096ull, export_row, reading->export, &reading->done, reading->tag, failure);
}

// the columns put in the order a JSON line names them, by their names' bytes, a name named twice once at its last
static void export_ordered(CliDbExport *export)
{
    const CliDbResults *const results = &export->open->results;
    unsigned int count = 0u;
    for (unsigned int column = 0u; column < results->column_count; column += 1u)
    {
        unsigned int at = count;
        while ((at > 0u) &&
               (cli_text_order(results->columns[export->order[at - 1u]].name, results->columns[column].name) > 0))
        {
            at -= 1u;
        }
        if ((at > 0u) && (cellul.eq(&(CatenaFinitaCfg){.src = results->columns[export->order[at - 1u]].name,
                                                       .other = results->columns[column].name,
                                                       .cap = CLI_CELLS_POOL})))
        {
            export->order[at - 1u] = column;
            continue;
        }
        memor.move_up(&(MemoriaCfg){.dst = &export->order[at + 1u],
                                    .src = &export->order[at],
                                    .bytes = (size_t)(count - at) * sizeof(export->order[0])});
        export->order[at] = column;
        count += 1u;
    }
    export->ordered = count;
}

int cli_db_export(CliDbOpen *open, const char *path, const char *format,
                  void (*progress)(void *context, unsigned long long written), void *context, VerbaLinea *out,
                  CliDbFailure *failure)
{
    CliDbResults *const results = &open->results;
    cli_lock_take(&open->results_lock);
    if (!results->has_columns)
    {
        cli_lock_give(&open->results_lock);
        cli_db_failure_set(failure, "no statement has given rows to export");
        return 0;
    }
    // a row's values, the columns' order, then the bytes not yet written, each list on its type's boundary
    const unsigned int columns = (results->column_count != 0u) ? results->column_count : 1u;
    const unsigned long long values_bytes = columns * sizeof(CliDbValue);
    const unsigned long long order_bytes = columns * sizeof(unsigned int);
    char *const cell =
        cli_lane_alloc(values_bytes + order_bytes + CLI_DB_EXPORT_ROOM, &g_cli_db_cells);
    if (cell == NULL)
    {
        cli_lock_give(&open->results_lock);
        cli_db_failure_set(failure, "the cells cannot meet an export");
        return 0;
    }
    CliDbExport results_export = {open,   path,
                                  format, cell + values_bytes + order_bytes,
                                  0ull,   !cli_file_appended(path, "", 0ull, 1),
                                  0ull,   (unsigned int *)(void *)(cell + values_bytes),
                                  0u,     (CliDbValue *)(void *)cell,
                                  columns};
    CliDbExport *const export = &results_export;
    export_ordered(export);
    if (!export->broke && (!cellul.eq(&(CatenaFinitaCfg){.src = format, .other = "json", .cap = CLI_CELLS_POOL})))
    {
        for (unsigned int at = 0u; at < results->column_count; at += 1u)
        {
            export->values[at] = (CliDbValue){
                results->columns[at].name,
                cellul.len(&(CatenaFinitaCfg){.src = results->columns[at].name, .cap = CLI_CELLS_POOL}), 0};
        }
        export_line(export, export->values, results->column_count);
    }
    if (!export->broke)
    {
        cli_db_results_lines(results, 0ull, results->rows, export_results_line, export);
    }
    int read = 1;
    while (read && !export->broke && !results->has_done)
    {
        CliDbExportReading reading = {export, 0, ""};
        const unsigned long long before = export->written;
        read = cli_db_with_query(open, export_read_on, &reading, failure);
        results->beyond += export->written - before;
        results->has_done = read && reading.done;
        memor.cpy(&(MemoriaCfg){
            .dst = results->done, .bytes = sizeof(results->done), .src = results->has_done ? reading.tag : ""});
        if (read && (progress != NULL))
        {
            progress(context, export->written);
        }
    }
    export_flushed(export);
    const int broke = export->broke;
    const unsigned long long written = export->written;
    cli_cell_release(cell, &g_cli_db_cells);
    cli_lock_give(&open->results_lock);
    if (!read)
    {
        return 0;
    }
    if (broke)
    {
        char msg_buf[CLI_PATH_MAX_LEN + 64u];
        VerbaLinea line = {msg_buf, sizeof(msg_buf), 0ull};
        mmgr_line_put(&line, path);
        mmgr_line_put(&line, ": the file could not be written");
        mmgr_line_finish(&line);
        cli_db_failure_set(failure, msg_buf);
        return 0;
    }
    mmgr_line_put(out, "{\"written\":");
    mmgr_line_decimal(out, written, 1u);
    mmgr_line_put(out, ",\"path\":");
    cli_json_quoted_lossy(out, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, '}');
    return 1;
}
