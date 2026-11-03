// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_results.c: the rows of the last statement run that gave rows, buffered as they are read, each a JSON array on a
// line of a file of the connection's own in orior's temporary folder, the place of every 64th marked; a page of them
// written back out, read on from the run where they are not buffered yet
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "db_internal.h"

#include <stdint.h>

typedef struct
{
    CliDbResults *results;
    const CliDbValue *values;
    unsigned int count;
    VerbaLinea *out;
    const char *text;
    unsigned long long size;
} DbResultsCtx;

// the rows buffered's pending bytes written to their file: 1, or 0 where they could not be
EMBED_INLINE int results_flush(const DbResultsCtx *args)
{
    const int written = (args->results->pending_size == 0ULL) ||
                        EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = args->results->path,
                                   .bytes = args->results->pending, .size = args->results->pending_size, .fresh = 0);
    args->results->size += args->results->pending_size;
    args->results->pending_size = 0ULL;
    return written;
}

// a row written as a JSON array of its values, each a string or null
EMBED_INLINE void results_row_write(const DbResultsCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        if (args->values[at].null)
        {
            mmgr_line_put(out, "null");
        }
        else
        {
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                       .text_bytes = (const unsigned char *)args->values[at].text, .length = args->values[at].size);
        }
    }
    mmgr_line_ch(out, ']');
}

// whether a value reads as a whole number, as i64 parses one
EMBED_INLINE int results_integer(const DbResultsCtx *args)
{
    unsigned long long at = ((args->size > 0u) && ((args->text[0] == '-') || (args->text[0] == '+'))) ? 1ULL : 0ULL;
    if ((at == args->size) || (args->size - at > 19u))
    {
        return 0;
    }
    for (; at < args->size; at += 1ULL)
    {
        if ((args->text[at] < '0') || (args->text[at] > '9'))
        {
            return 0;
        }
    }
    return 1;
}

// whether a value reads as a real number, as f64 parses one: digits with a point and an exponent, or inf, infinity or
// NaN, any case
EMBED_INLINE int results_real(const DbResultsCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long at = ((size > 0u) && ((text[0] == '-') || (text[0] == '+'))) ? 1ULL : 0ULL;
    const unsigned long long rest = size - at;
    static const char *const s_words[3] = {"inf", "infinity", "nan"};
    for (unsigned int word_text = 0u; word_text < 3u; word_text += 1u)
    {
        int same = (rest == cellul.len(&(CatenaFinitaCfg){.src = s_words[word_text], .cap = CLI_CELLS_POOL}));
        unsigned long long one = 0ULL;
        while (same && (one < rest))
        {
            const char letter = text[at + one];
            same =
                (((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter) == s_words[word_text][one];
            one += 1ULL;
        }
        if (same)
        {
            return 1;
        }
    }
    unsigned long long digits = 0ULL;
    digits = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = at,
                        .size = size, .classes = CLI_TEXT_DIGITS, .set = "") -
             at;
    at += digits;
    if ((at < size) && (text[at] == '.'))
    {
        at += 1ULL;
        const unsigned long long more =
            EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = at,
                       .size = size, .classes = CLI_TEXT_DIGITS, .set = "") -
            at;
        at += more;
        digits += more;
    }
    if ((digits > 0u) && (at < size) && ((text[at] == 'e') || (text[at] == 'E')))
    {
        at += ((at + 1u < size) && ((text[at + 1u] == '-') || (text[at + 1u] == '+'))) ? 2ULL : 1ULL;
        unsigned long long exponent = 0ULL;
        const unsigned long long more =
            EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = at,
                       .size = size, .classes = CLI_TEXT_DIGITS, .set = "") -
            at;
        at += more;
        exponent += more;
        digits = (exponent > 0u) ? digits : 0ULL;
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
static int results_row(const CliDbRowCfg *args)
{
    const CliDbValue *values = args->values;
    unsigned int count = args->count;
    CliDbResultsReader *const reader = (CliDbResultsReader *)args->context;
    CliDbResults *const results = &reader->open->results;
    // the row written where the pending bytes end, the pending bytes written out first where it does not fit after
    // them, and buffered as its values' nulls where it does not fit the capacity at all, so that the rows after retain
    // their places
    VerbaLinea line = {results->pending + results->pending_size, sizeof(results->pending) - results->pending_size,
                       0ULL};
    EMBED_CALL(results_row_write, DbResultsCtx, .values = values, .count = count, .out = &line);
    mmgr_line_ch(&line, '\n');
    if (!mmgr_line_ok(&line) && (results->pending_size > 0u))
    {
        (void)EMBED_CALL(results_flush, DbResultsCtx, .results = results);
        line = (VerbaLinea){results->pending, sizeof(results->pending), 0ULL};
        EMBED_CALL(results_row_write, DbResultsCtx, .values = values, .count = count, .out = &line);
        mmgr_line_ch(&line, '\n');
    }
    if (!mmgr_line_ok(&line))
    {
        line.at = 0ULL;
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
    results->rows += 1ULL;
    if (reader->out != NULL)
    {
        mmgr_line_put(reader->out, (reader->written > 0u) ? "," : "");
        mmgr_line_put_n(reader->out, row, line.at - 1u);
        reader->written += 1ULL;
    }
    for (unsigned int at = 0u; reader->infer && (at < count) && (at < results->column_count); at += 1u)
    {
        if (reader->kinded[at] || values[at].null)
        {
            continue;
        }
        CliDbColumn *const column = &results->columns[at];
        const int integer = EMBED_CALL(results_integer, DbResultsCtx, .text = values[at].text, .size = values[at].size);
        const int real =
            !integer && EMBED_CALL(results_real, DbResultsCtx, .text = values[at].text, .size = values[at].size);
        const char *const other_kind = real ? "real" : "text";
        const char *const column_kind = integer ? "integer" : other_kind;
        memor.cpy(
            &(MemoriaCfg){.dst = column->kind,
                          .src = column_kind,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = column_kind, .cap = CLI_CELLS_POOL}) + 1u});
        column->numeric = integer || real;
        reader->kinded[at] = 1;
    }
    return 1;
}

int cli_db_results_begin(const CliDbLinkFailureDoneCtx *args)
{
    CliDbResults *const results = &args->open->results;
    const CliDbHead *const head = EMBED_CALL(cli_db_internal.link_head, CliDbCtx, .link_read = args->link);
    results->has_columns = 1;
    results->column_count = head->column_count;
    memor.cpy(&(MemoriaCfg){
        .dst = results->columns, .src = head->columns, .bytes = sizeof(results->columns[0]) * head->column_count});
    results->rows = 0ULL;
    results->size = 0ULL;
    results->pending_size = 0ULL;
    results->has_done = 0;
    results->beyond = 0ULL;
    if (results->path[0] == '\0')
    {
        char temporary[CLI_PATH_MAX_LEN];
        char name[64];
        VerbaLinea named = {name, sizeof(name), 0ULL};
        mmgr_line_put(&named, "orior-db-");
        mmgr_line_decimal(&named, cli_process_own(), 1u);
        mmgr_line_ch(&named, '-');
        mmgr_line_decimal(&named, (unsigned long long)(uintptr_t)args->open % 100000ULL, 1u);
        mmgr_line_put(&named, ".jsonl");
        mmgr_line_finish(&named);
        EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = temporary);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = temporary, .relative = name, .joined = results->path);
    }
    (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = results->path, .bytes = "",
                     .size = 0ULL, .fresh = 1);
    CliDbResultsReader reader = {args->open, args->out, 0ULL, args->link->kind == CLI_DB_LINK_LITE, {0}};
    mmgr_line_ch(args->out, '[');
    const int read = EMBED_CALL(cli_db_link_failure_internal.link_rows, CliDbLinkFailureCtx, .link = args->link,
                                .count = args->page, .each_row = results_row, .context = &reader, .done = args->done,
                                .tag = args->tag, .failure = args->failure);
    mmgr_line_ch(args->out, ']');
    (void)EMBED_CALL(results_flush, DbResultsCtx, .results = results);
    results->has_done = read && *args->done;
    memor.cpy(&(MemoriaCfg){
        .dst = results->done,
        .src = results->has_done ? args->tag : "",
        .bytes =
            cellul.len(&(CatenaFinitaCfg){.src = results->has_done ? args->tag : "", .cap = CLI_CELLS_POOL}) + 1u});
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

static int results_read_on(const CliEachLinkFailureCfg *args)
{
    void *context = args->context;
    CliDbLink *link = args->link;
    CliDbFailure *failure = args->failure;
    CliDbReadOn *const read_state = (CliDbReadOn *)context;
    CliDbResultsReader reader = {read_state->open, NULL, 0ULL, 0, {0}};
    return EMBED_CALL(cli_db_link_failure_internal.link_rows, CliDbLinkFailureCtx, .link = link,
                      .count = read_state->want, .each_row = results_row, .context = &reader, .done = &read_state->done,
                      .tag = read_state->tag, .failure = failure);
}

// the lines of the buffered rows from `start` to `end` handed to `each`, read from their file from the mark before the
// first
void cli_db_results_lines(const CliDbContextCtx *args)
{
    CliDbResults *results = args->results;
    (void)EMBED_CALL(results_flush, DbResultsCtx, .results = results);
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = (args->start < args->end)
                                           ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = results->path,
                                                        .size_pointer = &size, .owner = &g_cli_db_cells)
                                           : NULL;
    if (bytes == NULL)
    {
        return;
    }
    const unsigned long long mark_index = args->start / CLI_DB_RESULTS_EVERY;
    const unsigned long long mark = (mark_index < CLI_DB_RESULTS_MARKS) ? mark_index : CLI_DB_RESULTS_MARKS - 1u;
    unsigned long long row = mark * CLI_DB_RESULTS_EVERY;
    for (unsigned long long at = results->marks[mark]; (at < size) && (row < args->end);)
    {
        const unsigned long long line_end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = (const char *)bytes, .at = at, .size = size).stop;
        if (row >= args->start)
        {
            EMBED_CALL(args->each_callback, CliEachLineSizeCfg, .context = args->context,
                       .line = (const char *)bytes + at, .size = line_end - at);
        }
        row += 1ULL;
        at = line_end + 1ULL;
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_db_cells);
}

// a buffered row's line written into the page after those before it
static void results_show(const CliEachLineSizeCfg *args)
{
    VerbaLinea *const out = (VerbaLinea *)args->context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    mmgr_line_put_n(out, args->line, args->size);
}

int cli_db_rows(const CliDbOutFailureOpenCfg *args)
{
    CliDbResults *const results = &args->open->results;
    while ((results->rows < args->from + args->count_u64) && results->has_columns && !results->has_done)
    {
        const unsigned long long short_of = args->from + args->count_u64 - results->rows;
        CliDbReadOn read_state = {args->open, (short_of > 200u) ? short_of : 200ULL, 0, ""};
        if (!EMBED_CALL(cli_db_failure_context_internal.with_query, CliDbFailureContextCtx, .open = args->open,
                        .work = results_read_on, .context = &read_state, .failure = args->failure))
        {
            (void)EMBED_CALL(results_flush, DbResultsCtx, .results = results);
            return 0;
        }
        (void)EMBED_CALL(results_flush, DbResultsCtx, .results = results);
        results->has_done = read_state.done;
        memor.cpy(&(MemoriaCfg){
            .dst = results->done, .bytes = sizeof(results->done), .src = read_state.done ? read_state.tag : ""});
        if (!read_state.done && (results->rows < args->from + args->count_u64) && (read_state.want > 0u))
        {
            // a run that gave fewer rows than requested and did not end gives no more now
            break;
        }
    }
    const unsigned long long end =
        (args->from + args->count_u64 < results->rows) ? args->from + args->count_u64 : results->rows;
    const unsigned long long start = (args->from < end) ? args->from : end;
    mmgr_line_put(args->out, "{\"rows\":[");
    EMBED_CALL(cli_db_context_internal.results_lines, CliDbContextCtx, .results = results, .start = start, .end = end,
               .each_callback = results_show, .context = args->out);
    mmgr_line_put(args->out, "],\"read\":");
    mmgr_line_decimal(args->out, results->rows, 1u);
    mmgr_line_put(args->out, ",\"done\":");
    if (results->has_done)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)results->done,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = results->done, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"beyond\":");
    mmgr_line_decimal(args->out, results->beyond, 1u);
    mmgr_line_ch(args->out, '}');
    return 1;
}
