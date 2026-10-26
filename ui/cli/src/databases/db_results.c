// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_results.c: the rows of the last statement run that gave rows, buffered as they are read, each a JSON array on a
// line of a file of the connection's own in orior's temporary folder, the place of every 64th marked; a page of them
// written back out, read on from the run where they are not buffered yet
#include "db_internal.h"

#include <stdint.h>

// the rows buffered's pending bytes written to their file: 1, or 0 where they could not be
static int results_flushed(CliDbResults *results)
{
    const int written =
        (results->pending_size == 0ull) || cli_file_appended(results->path, results->pending, results->pending_size, 0);
    results->size += results->pending_size;
    results->pending_size = 0ull;
    return written;
}

// a row written as a JSON array of its values, each a string or null
static void results_row_written(const CliDbValue *values, unsigned int count, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        if (values[at].null)
        {
            mmgr_line_put(out, "null");
        }
        else
        {
            cli_json_quoted_lossy(out, (const unsigned char *)values[at].text, values[at].size);
        }
    }
    mmgr_line_ch(out, ']');
}

// whether a value reads as a whole number, as i64 parses one
static int results_integer(const char *text, unsigned long long size)
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
static int results_real(const char *text, unsigned long long size)
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

// The buffering of a page of rows: the connection, where the page is written, and whether SQLite's columns' kinds are
// read from the values
typedef struct
{
    CliDbOpen *open;
    VerbaLinea *out;
    unsigned long long written;
    int infer;
    int kinded[CLI_DB_COLUMNS];
} CliDbResultsReader;

// a row read buffered: written to its file's pending bytes, its place marked, written into the page where one is
// written, and its values' kinds read where SQLite's are
static int results_row(void *context, const CliDbValue *values, unsigned int count)
{
    CliDbResultsReader *const reader = (CliDbResultsReader *)context;
    CliDbResults *const results = &reader->open->results;
    // the row written where the pending bytes end, the pending bytes written out first where it does not fit after
    // them, and buffered as its values' nulls where it does not fit the room at all, so that the rows after retain
    // their places
    VerbaLinea line = {results->pending + results->pending_size, sizeof(results->pending) - results->pending_size,
                       0ull};
    results_row_written(values, count, &line);
    mmgr_line_ch(&line, '\n');
    if (!mmgr_line_ok(&line) && (results->pending_size > 0u))
    {
        (void)results_flushed(results);
        line = (VerbaLinea){results->pending, sizeof(results->pending), 0ull};
        results_row_written(values, count, &line);
        mmgr_line_ch(&line, '\n');
    }
    if (!mmgr_line_ok(&line))
    {
        line.at = 0ull;
        mmgr_line_ch(&line, '[');
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            mmgr_line_put(&line, (at > 0u) ? ",null" : "null");
        }
        mmgr_line_put(&line, "]\n");
    }
    if ((results->rows % CLI_DB_RESULTS_EVERY) == 0u)
    {
        const unsigned long long mark = results->rows / CLI_DB_RESULTS_EVERY;
        if (mark < CLI_DB_RESULTS_MARKS)
        {
            results->marks[mark] = results->size + results->pending_size;
        }
    }
    const char *const row = results->pending + results->pending_size;
    results->pending_size += line.at;
    results->rows += 1ull;
    if (reader->out != NULL)
    {
        mmgr_line_put(reader->out, (reader->written > 0u) ? "," : "");
        mmgr_line_put_n(reader->out, row, line.at - 1u);
        reader->written += 1ull;
    }
    for (unsigned int at = 0u; reader->infer && (at < count) && (at < results->column_count); at += 1u)
    {
        if (reader->kinded[at] || values[at].null)
        {
            continue;
        }
        CliDbColumn *const column = &results->columns[at];
        const int integer = results_integer(values[at].text, values[at].size);
        const int real = !integer && results_real(values[at].text, values[at].size);
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
        reader->kinded[at] = 1;
    }
    return 1;
}

int cli_db_results_begun(CliDbOpen *open, CliDbLink *link, unsigned long long page, VerbaLinea *out, int *done,
                         char *tag, CliDbFailure *failure)
{
    CliDbResults *const results = &open->results;
    const CliDbHead *const head = cli_db_link_head(link);
    results->has_columns = 1;
    results->column_count = head->column_count;
    memor.cpy(&(MemoriaCfg){
        .dst = results->columns, .src = head->columns, .bytes = sizeof(results->columns[0]) * head->column_count});
    results->rows = 0ull;
    results->size = 0ull;
    results->pending_size = 0ull;
    results->has_done = 0;
    results->beyond = 0ull;
    if (results->path[0] == '\0')
    {
        char temporary[CLI_PATH_MAX_LEN];
        char name[64];
        VerbaLinea named = {name, sizeof(name), 0ull};
        mmgr_line_put(&named, "orior-db-");
        mmgr_line_decimal(&named, cli_process_own(), 1u);
        mmgr_line_ch(&named, '-');
        mmgr_line_decimal(&named, (unsigned long long)(uintptr_t)open % 100000ull, 1u);
        mmgr_line_put(&named, ".jsonl");
        mmgr_line_finish(&named);
        cli_temp_folder(temporary);
        cli_full(temporary, name, results->path);
    }
    (void)cli_file_appended(results->path, "", 0ull, 1);
    CliDbResultsReader reader = {open, out, 0ull, link->kind == CLI_DB_LINK_LITE, {0}};
    mmgr_line_ch(out, '[');
    const int read = cli_db_link_rows(link, page, results_row, &reader, done, tag, failure);
    mmgr_line_ch(out, ']');
    (void)results_flushed(results);
    results->has_done = read && *done;
    memor.cpy(&(MemoriaCfg){
        .dst = results->done,
        .src = results->has_done ? tag : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = results->has_done ? tag : "", .cap = CLI_CELLS_POOL}) + 1u});
    return read;
}

// The reading on of a run's rows into those buffered: the connection, how many are wanted, and how the run ended
typedef struct
{
    CliDbOpen *open;
    unsigned long long want;
    int done;
    char tag[CLI_DB_TAG];
} CliDbReadOn;

static int results_read_on(void *context, CliDbLink *link, CliDbFailure *failure)
{
    CliDbReadOn *const reading = (CliDbReadOn *)context;
    CliDbResultsReader reader = {reading->open, NULL, 0ull, 0, {0}};
    return cli_db_link_rows(link, reading->want, results_row, &reader, &reading->done, reading->tag, failure);
}

// the lines of the buffered rows from `start` to `end` handed to `each`, read from their file from the mark before the
// first
void cli_db_results_lines(CliDbResults *results, unsigned long long start, unsigned long long end,
                          void (*each)(void *context, const char *line, unsigned long long size), void *context)
{
    (void)results_flushed(results);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = (start < end) ? cli_file_read(results->path, &size) : NULL;
    if (bytes == NULL)
    {
        return;
    }
    const unsigned long long mark_index = start / CLI_DB_RESULTS_EVERY;
    const unsigned long long mark = (mark_index < CLI_DB_RESULTS_MARKS) ? mark_index : CLI_DB_RESULTS_MARKS - 1u;
    unsigned long long row = mark * CLI_DB_RESULTS_EVERY;
    for (unsigned long long at = results->marks[mark]; (at < size) && (row < end);)
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
    cli_file_let_go(bytes, size);
}

// a buffered row's line written into the page after those before it
static void results_shown(void *context, const char *line, unsigned long long size)
{
    VerbaLinea *const out = (VerbaLinea *)context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    mmgr_line_put_n(out, line, size);
}

int cli_db_rows(CliDbOpen *open, unsigned long long from, unsigned long long count, VerbaLinea *out,
                CliDbFailure *failure)
{
    CliDbResults *const results = &open->results;
    cli_lock_take(&open->results_lock);
    while ((results->rows < from + count) && results->has_columns && !results->has_done)
    {
        const unsigned long long short_of = from + count - results->rows;
        CliDbReadOn reading = {open, (short_of > 200u) ? short_of : 200ull, 0, ""};
        if (!cli_db_with_query(open, results_read_on, &reading, failure))
        {
            (void)results_flushed(results);
            cli_lock_give(&open->results_lock);
            return 0;
        }
        (void)results_flushed(results);
        results->has_done = reading.done;
        memor.cpy(&(MemoriaCfg){
            .dst = results->done, .bytes = sizeof(results->done), .src = reading.done ? reading.tag : ""});
        if (!reading.done && (results->rows < from + count) && (reading.want > 0u))
        {
            // a run that gave fewer rows than requested and did not end gives no more now
            break;
        }
    }
    const unsigned long long end = (from + count < results->rows) ? from + count : results->rows;
    const unsigned long long start = (from < end) ? from : end;
    mmgr_line_put(out, "{\"rows\":[");
    cli_db_results_lines(results, start, end, results_shown, out);
    mmgr_line_put(out, "],\"read\":");
    mmgr_line_decimal(out, results->rows, 1u);
    mmgr_line_put(out, ",\"done\":");
    if (results->has_done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)results->done,
                              cellul.len(&(CatenaFinitaCfg){.src = results->done, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"beyond\":");
    mmgr_line_decimal(out, results->beyond, 1u);
    mmgr_line_ch(out, '}');
    cli_lock_give(&open->results_lock);
    return 1;
}
