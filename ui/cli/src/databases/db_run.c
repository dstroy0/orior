// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_run.c: a statement run on a connection: confirmed first where it writes every row of what it names, or where it
// writes on a connection that matters and confirms; then started on the query link, its first page of rows buffered and
// written, what it did, what the server noted, the transaction's state, the session's zone, the object it shapes, and
// how long it took
#include "db_internal.h"
#include "sql.h"

// The running of one statement: its connection, the statement, the page of rows requested, where the result goes, its
// tag and whether it has one, whether the statement gave rows and how many were written, the notices the server gave
// written as a JSON array, and the transaction's state after it
typedef struct
{
    CliDbOpen *open;
    const char *statement;
    unsigned long long size;
    unsigned long long page;
    VerbaLinea *out;
    char tag[CLI_DB_TAG];
    int has_tag;
    int rows;
    char notices[CLI_DB_FIELD * 4u];
    VerbaLinea notices_line;
    char transaction[16];
} CliDbRunning;

// a notice the server gave written into the running's array
static void run_notice(void *context, const char *notice)
{
    CliDbRunning *const running = (CliDbRunning *)context;
    mmgr_line_put(&running->notices_line, (running->notices_line.at > 1ull) ? "," : "");
    cli_json_quoted_lossy(&running->notices_line, (const unsigned char *)notice,
                          cellul.len(&(CatenaFinitaCfg){.src = notice, .cap = CLI_CELLS_POOL}));
}

// the statement started on the query link, and its first page of rows buffered and written, or what it did
static int run_work(void *context, CliDbLink *link, CliDbFailure *failure)
{
    CliDbRunning *const running = (CliDbRunning *)context;
    if (!cli_db_link_start(link, running->statement, running->size, failure))
    {
        return 0;
    }
    const CliDbHead *const head = cli_db_link_head(link);
    running->rows = head->rows;
    int done = 1;
    if (!head->rows)
    {
        memor.cpy(&(MemoriaCfg){.dst = running->tag,
                                .src = head->tag,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = head->tag, .cap = CLI_CELLS_POOL}) + 1u});
        mmgr_line_put(running->out, "\"rows\":[]");
    }
    else
    {
        mmgr_line_put(running->out, "\"rows\":");
        if (!cli_db_results_begun(running->open, link, running->page, running->out, &done, running->tag, failure))
        {
            return 0;
        }
    }
    running->has_tag = done;
    running->notices_line = (VerbaLinea){running->notices, sizeof(running->notices), 0ull};
    mmgr_line_ch(&running->notices_line, '[');
    cli_db_link_notices(link, run_notice, running);
    mmgr_line_ch(&running->notices_line, ']');
    memor.cpy(&(MemoriaCfg){
        .dst = running->transaction,
        .src = cli_db_link_transaction(link),
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = cli_db_link_transaction(link), .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// The counting of the rows a write would reach: the query, and the count the first row's first value gives
typedef struct
{
    const char *query;
    unsigned long long size;
    char count[64];
    int has_count;
} CliDbCounting;

static int run_counted(void *context, const CliDbValue *values, unsigned int count)
{
    CliDbCounting *const counting = (CliDbCounting *)context;
    if (!counting->has_count && (count > 0u) && !values[0].null)
    {
        VerbaLinea line = {counting->count, sizeof(counting->count), 0ull};
        cli_lossy(&line, (const unsigned char *)values[0].text, values[0].size);
        mmgr_line_finish(&line);
        counting->has_count = 1;
    }
    return 1;
}

static int run_count_work(void *context, CliDbLink *link, CliDbFailure *failure)
{
    CliDbCounting *const counting = (CliDbCounting *)context;
    return cli_db_link_all(link, counting->query, counting->size, run_counted, counting, failure);
}

// whether running the statement confirms first, and why, written into `out`: a write that reaches every row of what it
// names, with their count, on every connection; and any statement that writes on a connection that matters and confirms
static int run_queries(CliDbOpen *open, const char *statement, unsigned long long size, VerbaLinea *out)
{
    const unsigned int dialect = cli_sql_dialect(open->config.kind);
    char query[CLI_DB_FIELD];
    VerbaLinea counting_line = {query, sizeof(query), 0ull};
    if (cli_sql_unbounded(statement, size, dialect, &counting_line))
    {
        CliDbCounting counting = {query, counting_line.at, "", 0};
        CliDbFailure failure;
        if (!cli_db_with_meta(open, run_count_work, &counting, &failure))
        {
            counting.has_count = 0;
        }
        mmgr_line_put(out, "{\"confirm\":{\"why\":\"every row\",\"count\":");
        if (counting.has_count)
        {
            cli_json_quoted_lossy(out, (const unsigned char *)counting.count,
                                  cellul.len(&(CatenaFinitaCfg){.src = counting.count, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, open->config.matters ? ",\"matters\":true}}" : ",\"matters\":false}}");
        return 1;
    }
    if (open->config.matters &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = open->config.guard, .other = "read-only", .cap = CLI_CELLS_POOL})) &&
        cli_sql_changes(statement, size, dialect))
    {
        mmgr_line_put(out, "{\"confirm\":{\"why\":\"matters\",\"matters\":true}}");
        return 1;
    }
    return 0;
}

int cli_db_run(CliDbOpen *open, const char *statement, unsigned long long size, int answered, unsigned long long page,
               VerbaLinea *out, CliDbFailure *failure)
{
    // the statement trimmed of its spaces, then of the semicolons at its end, then of the spaces before them
    unsigned long long start = 0ull;
    unsigned long long end = size;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = statement[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = statement[end - 1u]}) != NULL))
    {
        end -= 1ull;
    }
    while ((end > start) && (statement[end - 1u] == ';'))
    {
        end -= 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = statement[end - 1u]}) != NULL))
    {
        end -= 1ull;
    }
    if (start == end)
    {
        cli_db_failure_set(failure, "there is no statement to run");
        return 0;
    }
    const char *const text = statement + start;
    const unsigned long long length = end - start;
    if (!answered && run_queries(open, text, length, out))
    {
        return 1;
    }
    CliDbRunning running_room;
    CliDbRunning *const running = &running_room;
    const unsigned long long from = out->at;
    *running = (CliDbRunning){open, text, length, page, out, "", 0, 0, "", {NULL, 0ull, 0ull}, ""};
    const unsigned long long started = cli_clock_milliseconds();
    mmgr_line_ch(out, '{');
    // the rows buffered are taken before the query link, as every reader of them takes the two, and retained through
    // the run
    cli_lock_take(&open->results_lock);
    const int ran = cli_db_with_query(open, run_work, running, failure);
    const unsigned long long took = cli_clock_milliseconds() - started;
    char tag_buf[CLI_DB_TAG];
    VerbaLinea tag_line = {tag_buf, sizeof(tag_buf), 0ull};
    if (ran && running->has_tag)
    {
        mmgr_line_put(&tag_line, running->tag);
    }
    else if (ran)
    {
        mmgr_line_decimal(&tag_line, open->results.rows, 1u);
        mmgr_line_put(&tag_line, "+ rows");
    }
    else
    {
        cli_db_failure_written(failure, &tag_line);
    }
    mmgr_line_finish(&tag_line);
    cli_db_remembered(open, text, length, took, tag_buf, !ran);
    if (!ran)
    {
        cli_lock_give(&open->results_lock);
        out->at = from;
        return 0;
    }
    const unsigned int dialect = cli_sql_dialect(open->config.kind);
    const unsigned int does = cli_sql_does(text, length, dialect);
    CliShaped shaped;
    const int has_shaped = (does == CLI_DOES_SHAPES) && cli_sql_shaped(text, length, dialect, &shaped);
    if (has_shaped || (does == CLI_DOES_OTHER))
    {
        cli_db_zone_read(open);
    }
    mmgr_line_put(out, ",\"columns\":");
    if (running->rows)
    {
        mmgr_line_ch(out, '[');
        for (unsigned int at = 0u; at < open->results.column_count; at += 1u)
        {
            const CliDbColumn *const column = &open->results.columns[at];
            mmgr_line_put(out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
            cli_json_quoted_lossy(out, (const unsigned char *)column->name,
                                  cellul.len(&(CatenaFinitaCfg){.src = column->name, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(out, ",\"kind\":");
            cli_json_quoted_lossy(out, (const unsigned char *)column->kind,
                                  cellul.len(&(CatenaFinitaCfg){.src = column->kind, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(out, column->numeric ? ",\"numeric\":true}" : ",\"numeric\":false}");
        }
        mmgr_line_ch(out, ']');
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"done\":");
    if (running->has_tag)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)running->tag,
                              cellul.len(&(CatenaFinitaCfg){.src = running->tag, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"notices\":");
    mmgr_line_put_n(out, running->notices_line.out, running->notices_line.at);
    mmgr_line_put(out, ",\"transaction\":");
    cli_json_quoted(out, running->transaction,
                    cellul.len(&(CatenaFinitaCfg){.src = running->transaction, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"zone\":");
    char zone[CLI_NAME_MAX_LEN];
    VerbaLinea zone_line = {zone, sizeof(zone), 0ull};
    cli_db_zone(open, &zone_line);
    cli_json_quoted_lossy(out, (const unsigned char *)zone_line.out, zone_line.at);
    mmgr_line_put(out, ",\"shaped\":");
    if (has_shaped)
    {
        mmgr_line_put(out, "{\"verb\":");
        cli_json_quoted_lossy(out, (const unsigned char *)shaped.verb,
                              cellul.len(&(CatenaFinitaCfg){.src = shaped.verb, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"kind\":");
        cli_json_quoted_lossy(out, (const unsigned char *)shaped.kind,
                              cellul.len(&(CatenaFinitaCfg){.src = shaped.kind, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)shaped.name,
                              cellul.len(&(CatenaFinitaCfg){.src = shaped.name, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"on\":");
        if (shaped.has_on)
        {
            cli_json_quoted_lossy(out, (const unsigned char *)shaped.on,
                                  cellul.len(&(CatenaFinitaCfg){.src = shaped.on, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_ch(out, '}');
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"ms\":");
    mmgr_line_decimal(out, took, 1u);
    mmgr_line_ch(out, '}');
    cli_lock_give(&open->results_lock);
    return 1;
}
