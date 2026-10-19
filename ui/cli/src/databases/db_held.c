// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_held.c: the rows of the last statement run that gave rows, held as they are read, each a JSON array on a line of
// a file of the connection's own in orior's temporary folder, the place of every 64th marked; a page of them written
// back out, read on from the run where they are not held yet
#include "db_internal.h"

#include <stdint.h>

// the rows held's pending bytes written to their file: 1, or 0 where they could not be
static int held_flushed(CliDbHeld *held)
{
    const int written =
        (held->pending_size == 0ull) || cli_file_appended(held->path, held->pending, held->pending_size, 0);
    held->size += held->pending_size;
    held->pending_size = 0ull;
    return written;
}

// a row written as a JSON array of its values, each a string or null
static void held_row_written(const CliDbValue *values, unsigned int count, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        if (values[at].null)
        {
            cli_line_text(out, "null");
        }
        else
        {
            cli_json_quoted_lossy(out, (const unsigned char *)values[at].text, values[at].size);
        }
    }
    cli_line_character(out, ']');
}

// whether a value reads as a whole number, as i64 parses one
static int held_integer(const char *text, unsigned long long size)
{
    unsigned long long at = ((size > 0u) && ((text[0] == '-') || (text[0] == '+'))) ? 1ull : 0ull;
    if ((at == size) || (size - at > 19u))
    {
        return 0;
    }
    for (; at < size; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9'))
        {
            return 0;
        }
    }
    return 1;
}

// whether a value reads as a real number, as f64 parses one: digits with a point and an exponent, or inf, infinity or
// NaN, any case
static int held_real(const char *text, unsigned long long size)
{
    unsigned long long at = ((size > 0u) && ((text[0] == '-') || (text[0] == '+'))) ? 1ull : 0ull;
    const unsigned long long rest = size - at;
    static const char *const s_words[3] = {"inf", "infinity", "nan"};
    for (unsigned int word = 0u; word < 3u; word += 1u)
    {
        int same = (rest == cellul.len(&(CatenaFinitaCfg){.src = s_words[word], .cap = CLI_CELLS_POOL}));
        for (unsigned long long one = 0ull; same && (one < rest); one += 1ull)
        {
            const char letter = text[at + one];
            same = (((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter) == s_words[word][one];
        }
        if (same)
        {
            return 1;
        }
    }
    unsigned long long digits = 0ull;
    while ((at < size) && (text[at] >= '0') && (text[at] <= '9'))
    {
        at += 1ull;
        digits += 1ull;
    }
    if ((at < size) && (text[at] == '.'))
    {
        at += 1ull;
        while ((at < size) && (text[at] >= '0') && (text[at] <= '9'))
        {
            at += 1ull;
            digits += 1ull;
        }
    }
    if ((digits > 0u) && (at < size) && ((text[at] == 'e') || (text[at] == 'E')))
    {
        at += ((at + 1u < size) && ((text[at + 1u] == '-') || (text[at + 1u] == '+'))) ? 2ull : 1ull;
        unsigned long long exponent = 0ull;
        while ((at < size) && (text[at] >= '0') && (text[at] <= '9'))
        {
            at += 1ull;
            exponent += 1ull;
        }
        digits = (exponent > 0u) ? digits : 0ull;
    }
    return (digits > 0u) && (at == size);
}

// The holding of a page of rows: the connection, where the page is written, and whether SQLite's columns' kinds are
// read from the values
typedef struct
{
    CliDbOpen *open;
    CliLine *out;
    unsigned long long written;
    int infer;
    int kinded[CLI_DB_COLUMNS];
} CliDbHolding;

// a row read held: written to its file's pending bytes, its place marked, written into the page where one is written,
// and its values' kinds read where SQLite's are
static int held_row(void *context, const CliDbValue *values, unsigned int count)
{
    CliDbHolding *const holding = (CliDbHolding *)context;
    CliDbHeld *const held = &holding->open->held;
    // the row written where the pending bytes end, the pending bytes written out first where it does not fit after
    // them, and held as its values' nulls where it does not fit the room at all, so that the rows after keep their
    // places
    CliLine line = {held->pending + held->pending_size, sizeof(held->pending) - held->pending_size, 0ull};
    held_row_written(values, count, &line);
    cli_line_character(&line, '\n');
    if (!cli_line_fits(&line) && (held->pending_size > 0u))
    {
        (void)held_flushed(held);
        line = (CliLine){held->pending, sizeof(held->pending), 0ull};
        held_row_written(values, count, &line);
        cli_line_character(&line, '\n');
    }
    if (!cli_line_fits(&line))
    {
        line.at = 0ull;
        cli_line_character(&line, '[');
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            cli_line_text(&line, (at > 0u) ? ",null" : "null");
        }
        cli_line_text(&line, "]\n");
    }
    if ((held->rows % CLI_DB_HELD_EVERY) == 0u)
    {
        const unsigned long long mark = held->rows / CLI_DB_HELD_EVERY;
        if (mark < CLI_DB_HELD_MARKS)
        {
            held->marks[mark] = held->size + held->pending_size;
        }
    }
    const char *const row = held->pending + held->pending_size;
    held->pending_size += line.at;
    held->rows += 1ull;
    if (holding->out != NULL)
    {
        cli_line_text(holding->out, (holding->written > 0u) ? "," : "");
        cli_line_counted(holding->out, row, line.at - 1u);
        holding->written += 1ull;
    }
    for (unsigned int at = 0u; holding->infer && (at < count) && (at < held->column_count); at += 1u)
    {
        if (holding->kinded[at] || values[at].null)
        {
            continue;
        }
        CliDbColumn *const column = &held->columns[at];
        const int integer = held_integer(values[at].text, values[at].size);
        const int real = !integer && held_real(values[at].text, values[at].size);
        memor.cpy(&(MemoriaCfg){.dst = column->kind,
                                .src = integer ? "integer"
                                       : real  ? "real"
                                               : "text",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = integer ? "integer"
                                                                              : real  ? "real"
                                                                                      : "text",
                                                                       .cap = CLI_CELLS_POOL}) +
                                         1u});
        column->numeric = integer || real;
        holding->kinded[at] = 1;
    }
    return 1;
}

int cli_db_held_begun(CliDbOpen *open, CliDbLink *link, unsigned long long page, CliLine *out, int *done, char *tag,
                      CliDbFailure *failure)
{
    CliDbHeld *const held = &open->held;
    const CliDbHead *const head = cli_db_link_head(link);
    held->has_columns = 1;
    held->column_count = head->column_count;
    memor.cpy(&(MemoriaCfg){
        .dst = held->columns, .src = head->columns, .bytes = sizeof(held->columns[0]) * head->column_count});
    held->rows = 0ull;
    held->size = 0ull;
    held->pending_size = 0ull;
    held->has_done = 0;
    held->beyond = 0ull;
    if (held->path[0] == '\0')
    {
        char temporary[CLI_PATH_LONGEST];
        char name[64];
        CliLine named = {name, sizeof(name), 0ull};
        cli_line_text(&named, "orior-db-");
        cli_line_decimal(&named, cli_process_own(), 1u);
        cli_line_character(&named, '-');
        cli_line_decimal(&named, (unsigned long long)(uintptr_t)open % 100000ull, 1u);
        cli_line_text(&named, ".jsonl");
        cli_line_closed(&named);
        cli_temp_folder(temporary);
        cli_full(temporary, name, held->path);
    }
    (void)cli_file_appended(held->path, "", 0ull, 1);
    CliDbHolding holding = {open, out, 0ull, link->kind == CLI_DB_LINK_LITE, {0}};
    cli_line_character(out, '[');
    const int read = cli_db_link_rows(link, page, held_row, &holding, done, tag, failure);
    cli_line_character(out, ']');
    (void)held_flushed(held);
    held->has_done = read && *done;
    memor.cpy(&(MemoriaCfg){
        .dst = held->done,
        .src = held->has_done ? tag : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = held->has_done ? tag : "", .cap = CLI_CELLS_POOL}) + 1u});
    return read;
}

// The reading on of a run's rows into those held: the connection, how many are wanted, and how the run ended
typedef struct
{
    CliDbOpen *open;
    unsigned long long want;
    int done;
    char tag[CLI_DB_TAG];
} CliDbReadOn;

static int held_read_on(void *context, CliDbLink *link, CliDbFailure *failure)
{
    CliDbReadOn *const reading = (CliDbReadOn *)context;
    CliDbHolding holding = {reading->open, NULL, 0ull, 0, {0}};
    return cli_db_link_rows(link, reading->want, held_row, &holding, &reading->done, reading->tag, failure);
}

// the lines of the held rows from `start` to `end` handed to `each`, read from their file from the mark before the
// first
void cli_db_held_lines(CliDbHeld *held, unsigned long long start, unsigned long long end,
                       void (*each)(void *context, const char *line, unsigned long long size), void *context)
{
    (void)held_flushed(held);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = (start < end) ? qry_member_file_mapped(held->path, &size) : NULL;
    if (bytes == NULL)
    {
        return;
    }
    const unsigned long long mark_index = start / CLI_DB_HELD_EVERY;
    const unsigned long long mark = (mark_index < CLI_DB_HELD_MARKS) ? mark_index : CLI_DB_HELD_MARKS - 1u;
    unsigned long long row = mark * CLI_DB_HELD_EVERY;
    for (unsigned long long at = held->marks[mark]; (at < size) && (row < end);)
    {
        const unsigned char *const newline =
            memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = (size_t)(size - at)});
        const unsigned long long line_end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
        if (row >= start)
        {
            each(context, (const char *)bytes + at, line_end - at);
        }
        row += 1ull;
        at = line_end + 1ull;
    }
    qry_member_file_unmapped(bytes, size);
}

// a held row's line written into the page after those before it
static void held_shown(void *context, const char *line, unsigned long long size)
{
    CliLine *const out = (CliLine *)context;
    cli_line_text(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    cli_line_counted(out, line, size);
}

int cli_db_rows(CliDbOpen *open, unsigned long long from, unsigned long long count, CliLine *out, CliDbFailure *failure)
{
    CliDbHeld *const held = &open->held;
    cli_lock_take(&open->held_lock);
    while ((held->rows < from + count) && held->has_columns && !held->has_done)
    {
        const unsigned long long short_of = from + count - held->rows;
        CliDbReadOn reading = {open, (short_of > 200u) ? short_of : 200ull, 0, ""};
        if (!cli_db_with_query(open, held_read_on, &reading, failure))
        {
            (void)held_flushed(held);
            cli_lock_give(&open->held_lock);
            return 0;
        }
        (void)held_flushed(held);
        held->has_done = reading.done;
        memor.cpy(
            &(MemoriaCfg){.dst = held->done, .bytes = sizeof(held->done), .src = reading.done ? reading.tag : ""});
        if (!reading.done && (held->rows < from + count) && (reading.want > 0u))
        {
            // a run that gave fewer rows than asked and did not end gives no more now
            break;
        }
    }
    const unsigned long long end = (from + count < held->rows) ? from + count : held->rows;
    const unsigned long long start = (from < end) ? from : end;
    cli_line_text(out, "{\"rows\":[");
    cli_db_held_lines(held, start, end, held_shown, out);
    cli_line_text(out, "],\"read\":");
    cli_line_decimal(out, held->rows, 1u);
    cli_line_text(out, ",\"done\":");
    if (held->has_done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)held->done,
                              cellul.len(&(CatenaFinitaCfg){.src = held->done, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"beyond\":");
    cli_line_decimal(out, held->beyond, 1u);
    cli_line_character(out, '}');
    cli_lock_give(&open->held_lock);
    return 1;
}
