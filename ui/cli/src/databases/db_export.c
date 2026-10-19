// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_export.c: the rows held, and then the rest the run gives, written to a file as CSV, TSV or JSON lines, the run
// read on to its end and not run again, and how many are written told after each 4096 it reads on
#include "../servers/lsp.h"
#include "db_internal.h"

#include "../text/text.h"

// the bytes written to the file at a time, and of the values of one held row read back
#define CLI_DB_EXPORT_ROOM (1u << 20u)
#define CLI_DB_EXPORT_ROW (1u << 20u)

// The writing of an export: its connection, its file and format, the bytes not yet written to it, whether a write
// failed, how many rows it has written, where it is told so, the columns in the order a JSON line names them, each
// once, and the room a held row's values are read back in
typedef struct
{
    CliDbOpen *open;
    const char *path;
    const char *format;
    char pending[CLI_DB_EXPORT_ROOM];
    unsigned long long pending_size;
    int broke;
    unsigned long long written;
    unsigned int order[CLI_DB_COLUMNS];
    unsigned int ordered;
    char text[CLI_DB_EXPORT_ROW];
    CliDbValue values[CLI_DB_COLUMNS];
} CliDbExport;

static CliDbExport s_export;
static CliLock s_export_lock = CLI_LOCK_INIT;

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
    if (export->pending_size + size > sizeof(export->pending))
    {
        export_flushed(export);
    }
    if (size > sizeof(export->pending))
    {
        export->broke = export->broke || !cli_file_appended(export->path, text, size, 0);
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = export->pending + export->pending_size, .src = text, .bytes = (size_t)size});
    export->pending_size += size;
}

// a value written into a line of CSV: as it is, or quoted with its quotes doubled where it holds a comma, a quote or a
// line's end
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
        // an object of each column's name and value, the names in order and a name named twice keeping its last value
        export_put(export, "{", 1ull);
        for (unsigned int at = 0u; at < export->ordered; at += 1u)
        {
            const unsigned int column = export->order[at];
            const char *const name = export->open->held.columns[column].name;
            CliLine line = {piece, sizeof(piece), 0ull};
            cli_line_text(&line, (at > 0u) ? "," : "");
            cli_json_quoted_lossy(&line, (const unsigned char *)name,
                                  cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
            cli_line_character(&line, ':');
            export_put(export, line.out, line.at);
            if ((column >= count) || values[column].null)
            {
                export_put(export, "null", 4ull);
                continue;
            }
            static char s_value[CLI_DB_EXPORT_ROW * 2u];
            CliLine value = {s_value, sizeof(s_value), 0ull};
            cli_json_quoted_lossy(&value, (const unsigned char *)values[column].text, values[column].size);
            export_put(export, value.out, value.at);
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

// a held row's line, a JSON array of strings and nulls, read back into values and written
static void export_held_line(void *context, const char *line, unsigned long long size)
{
    CliDbExport *const export = (CliDbExport *)context;
    unsigned int count = 0u;
    unsigned long long used = 0ull;
    for (unsigned long long at = 1ull; (at < size) && (count < CLI_DB_COLUMNS);)
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
        char *const text = export->text + used;
        if (!cli_lsp_string(line + at, end + 1u - at, text, sizeof(export->text) - used))
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
}

// a row read on from the run written, and counted past those held
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
    const CliDbHeld *const held = &export->open->held;
    unsigned int count = 0u;
    for (unsigned int column = 0u; column < held->column_count; column += 1u)
    {
        unsigned int at = count;
        while ((at > 0u) &&
               (cli_text_order(held->columns[export->order[at - 1u]].name, held->columns[column].name) > 0))
        {
            at -= 1u;
        }
        if ((at > 0u) && (cellul.eq(&(CatenaFinitaCfg){.src = held->columns[export->order[at - 1u]].name,
                                                       .other = held->columns[column].name,
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
                  void (*told)(void *context, unsigned long long written), void *context, CliLine *out,
                  CliDbFailure *failure)
{
    CliDbHeld *const held = &open->held;
    cli_lock_take(&open->held_lock);
    if (!held->has_columns)
    {
        cli_lock_give(&open->held_lock);
        cli_db_failure_said(failure, "no statement has given rows to export");
        return 0;
    }
    cli_lock_take(&s_export_lock);
    CliDbExport *const export = &s_export;
    export->open = open;
    export->path = path;
    export->format = format;
    export->pending_size = 0ull;
    export->broke = !cli_file_appended(path, "", 0ull, 1);
    export->written = 0ull;
    export_ordered(export);
    if (!export->broke && (!cellul.eq(&(CatenaFinitaCfg){.src = format, .other = "json", .cap = CLI_CELLS_POOL})))
    {
        for (unsigned int at = 0u; at < held->column_count; at += 1u)
        {
            export->values[at] =
                (CliDbValue){held->columns[at].name,
                             cellul.len(&(CatenaFinitaCfg){.src = held->columns[at].name, .cap = CLI_CELLS_POOL}), 0};
        }
        export_line(export, export->values, held->column_count);
    }
    if (!export->broke)
    {
        cli_db_held_lines(held, 0ull, held->rows, export_held_line, export);
    }
    int read = 1;
    while (read && !export->broke && !held->has_done)
    {
        CliDbExportReading reading = {export, 0, ""};
        const unsigned long long before = export->written;
        read = cli_db_with_query(open, export_read_on, &reading, failure);
        held->beyond += export->written - before;
        held->has_done = read && reading.done;
        memor.cpy(
            &(MemoriaCfg){.dst = held->done, .bytes = sizeof(held->done), .src = held->has_done ? reading.tag : ""});
        if (read && (told != NULL))
        {
            told(context, export->written);
        }
    }
    export_flushed(export);
    const int broke = export->broke;
    const unsigned long long written = export->written;
    cli_lock_give(&s_export_lock);
    cli_lock_give(&open->held_lock);
    if (!read)
    {
        return 0;
    }
    if (broke)
    {
        char said[CLI_PATH_LONGEST + 64u];
        CliLine line = {said, sizeof(said), 0ull};
        cli_line_text(&line, path);
        cli_line_text(&line, ": the file could not be written");
        cli_line_closed(&line);
        cli_db_failure_said(failure, said);
        return 0;
    }
    cli_line_text(out, "{\"written\":");
    cli_line_decimal(out, written, 1u);
    cli_line_text(out, ",\"path\":");
    cli_json_quoted_lossy(out, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_character(out, '}');
    return 1;
}
