// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_run.c: a statement run on a connection: confirmed first where it writes every row of what it names, or where it
// writes on a connection that matters and confirms; then started on the query link, its first page of rows buffered and
// written, what it did, what the server noted, the transaction's state, the session's zone, the object it shapes, and
// how long it took
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
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
static void run_notice(const CliEachNoticeCfg *args)
{
    CliDbRunning *const running = (CliDbRunning *)args->context;
    mmgr_line_put(&running->notices_line, (running->notices_line.at > 1ULL) ? "," : "");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &running->notices_line,
               .text_bytes = (const unsigned char *)args->notice,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->notice, .cap = CLI_CELLS_POOL}));
}

// the statement started on the query link, and its first page of rows buffered and written, or what it did
static int run_work(const CliEachLinkFailureCfg *args)
{
    void *context = args->context;
    CliDbLink *link = args->link;
    CliDbFailure *failure = args->failure;
    CliDbRunning *const running = (CliDbRunning *)context;
    if (!EMBED_CALL(cli_db_link_failure_statement_internal.link_start, CliDbLinkFailureStatementCtx, .link = link,
                    .statement = running->statement, .size = running->size, .failure = failure))
    {
        return 0;
    }
    const CliDbHead *const head = EMBED_CALL(cli_db_internal.link_head, CliDbCtx, .link_read = link);
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
        if (!EMBED_CALL(cli_db_link_failure_done_internal.results_begin, CliDbLinkFailureDoneCtx, .open = running->open,
                        .link = link, .page = running->page, .out = running->out, .done = &done, .tag = running->tag,
                        .failure = failure))
        {
            return 0;
        }
    }
    running->has_tag = done;
    running->notices_line = (VerbaLinea){running->notices, sizeof(running->notices), 0ULL};
    mmgr_line_ch(&running->notices_line, '[');
    EMBED_CALL(cli_db_link_context_internal.link_notices, CliDbLinkContextCtx, .link = link, .each = run_notice,
               .context = running);
    mmgr_line_ch(&running->notices_line, ']');
    memor.cpy(&(MemoriaCfg){.dst = running->transaction,
                            .src = EMBED_CALL(cli_db_internal.link_transaction, CliDbCtx, .link_read = link),
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_db_internal.link_transaction,
                                                                                     CliDbCtx, .link_read = link),
                                                                   .cap = CLI_CELLS_POOL}) +
                                     1u});
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

static int run_on_count(const CliDbRowCfg *args)
{
    CliDbCounting *const counting = (CliDbCounting *)args->context;
    if (!counting->has_count && (args->count > 0u) && !args->values[0].null)
    {
        VerbaLinea line = {counting->count, sizeof(counting->count), 0ULL};
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line,
                   .text_bytes = (const unsigned char *)args->values[0].text, .length = args->values[0].size);
        mmgr_line_finish(&line);
        counting->has_count = 1;
    }
    return 1;
}

static int run_count_work(const CliEachLinkFailureCfg *args)
{
    void *context = args->context;
    CliDbLink *link = args->link;
    CliDbFailure *failure = args->failure;
    CliDbCounting *const counting = (CliDbCounting *)context;
    return EMBED_CALL(cli_db_link_failure_statement_internal.link_all, CliDbLinkFailureStatementCtx, .link = link,
                      .statement = counting->query, .size = counting->size, .each_row = run_on_count,
                      .context = counting, .failure = failure);
}

typedef struct
{
    CliDbOpen *open;
    const char *statement;
    unsigned long long size;
    VerbaLinea *out;
} DbRunCtx;
// whether running the statement confirms first, and why, written into `out`: a write that reaches every row of what it
// names, with their count, on every connection; and any statement that writes on a connection that matters and confirms
EMBED_INLINE int run_queries(const DbRunCtx *args)
{
    const char *statement = args->statement;
    VerbaLinea *out = args->out;
    const unsigned int dialect = EMBED_CALL(cli_sql.dialect, CliSqlCfg, .name = args->open->config.kind);
    char query[CLI_DB_FIELD];
    VerbaLinea counting_line = {query, sizeof(query), 0ULL};
    if (EMBED_CALL(cli_sql_length_dialect.unbounded, CliSqlLengthDialectCfg, .statement = statement,
                   .length = args->size, .dialect = dialect, .out = &counting_line))
    {
        CliDbCounting counting = {query, counting_line.at, "", 0};
        CliDbFailure failure;
        if (!EMBED_CALL(cli_db_failure_context_internal.with_meta, CliDbFailureContextCtx, .open = args->open,
                        .work = run_count_work, .context = &counting, .failure = &failure))
        {
            counting.has_count = 0;
        }
        mmgr_line_put(out, "{\"confirm\":{\"why\":\"every row\",\"count\":");
        if (counting.has_count)
        {
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                       .text_bytes = (const unsigned char *)counting.count,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = counting.count, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, args->open->config.matters ? ",\"matters\":true}}" : ",\"matters\":false}}");
        return 1;
    }
    if (args->open->config.matters &&
        (!cellul.eq(
            &(CatenaFinitaCfg){.src = args->open->config.guard, .other = "read-only", .cap = CLI_CELLS_POOL})) &&
        EMBED_CALL(cli_sql_length.changes, CliSqlLengthCfg, .statement = statement, .length = args->size,
                   .dialect = dialect))
    {
        mmgr_line_put(out, "{\"confirm\":{\"why\":\"matters\",\"matters\":true}}");
        return 1;
    }
    return 0;
}

int cli_db_run(const CliDbOutFailureCfg *args)
{
    // the statement trimmed of its spaces, then of the semicolons at its end, then of the spaces before them
    unsigned long long start = 0ULL;
    unsigned long long end = args->size;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = args->statement[start]}) != NULL))
    {
        start += 1ULL;
    }
    while ((end > start) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = args->statement[end - 1u]}) != NULL))
    {
        end -= 1ULL;
    }
    end = (start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                              .text = args->statement + start, .at = end - start, .set = ";"));
    while ((end > start) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = args->statement[end - 1u]}) != NULL))
    {
        end -= 1ULL;
    }
    if (start == end)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure,
                   .message = "there is no statement to run");
        return 0;
    }
    const char *const text = args->statement + start;
    const unsigned long long length = end - start;
    if (!args->answered &&
        EMBED_CALL(run_queries, DbRunCtx, .open = args->open, .statement = text, .size = length, .out = args->out))
    {
        return 1;
    }
    CliDbRunning running_capacity;
    CliDbRunning *const running = &running_capacity;
    const unsigned long long from = args->out->at;
    *running = (CliDbRunning){args->open, text, length, args->page, args->out, "", 0, 0, "", {NULL, 0ULL, 0ULL}, ""};
    const unsigned long long started = cli_clock_milliseconds();
    mmgr_line_ch(args->out, '{');
    // the rows buffered are taken before the query link, as every reader of them takes the two, and retained through
    // the run
    const int run_result =
        EMBED_CALL(cli_db_failure_context_internal.with_query, CliDbFailureContextCtx, .open = args->open,
                   .work = run_work, .context = running, .failure = args->failure);
    const unsigned long long consumed = cli_clock_milliseconds() - started;
    char tag_buf[CLI_DB_TAG];
    VerbaLinea tag_line = {tag_buf, sizeof(tag_buf), 0ULL};
    if (run_result && running->has_tag)
    {
        mmgr_line_put(&tag_line, running->tag);
    }
    else if (run_result)
    {
        mmgr_line_decimal(&tag_line, args->open->results.rows, 1u);
        mmgr_line_put(&tag_line, "+ rows");
    }
    else
    {
        EMBED_CALL(cli_db_wire_out.db_failure_write, CliDbWireOutCfg, .failure_read = args->failure, .out = &tag_line);
    }
    mmgr_line_finish(&tag_line);
    EMBED_CALL(cli_db_statement_internal.remembered, CliDbStatementCtx, .open = args->open, .statement = text,
               .size = length, .consumed = consumed, .err_text = tag_buf, .failed = !run_result);
    if (!run_result)
    {
        args->out->at = from;
        return 0;
    }
    const unsigned int dialect = EMBED_CALL(cli_sql.dialect, CliSqlCfg, .name = args->open->config.kind);
    const unsigned int does =
        EMBED_CALL(cli_sql_length.does, CliSqlLengthCfg, .statement = text, .length = length, .dialect = dialect);
    CliShaped shaped;
    const int has_shaped =
        (does == CLI_DOES_SHAPES) && EMBED_CALL(cli_sql_length.shape, CliSqlLengthCfg, .statement = text,
                                                .length = length, .dialect = dialect, .shaped = &shaped);
    if (has_shaped || (does == CLI_DOES_OTHER))
    {
        EMBED_CALL(cli_db_failure_context_internal.zone_read, CliDbFailureContextCtx, .open = args->open);
    }
    mmgr_line_put(args->out, ",\"columns\":");
    if (running->rows)
    {
        mmgr_line_ch(args->out, '[');
        for (unsigned int at = 0u; at < args->open->results.column_count; at += 1u)
        {
            const CliDbColumn *const column = &args->open->results.columns[at];
            mmgr_line_put(args->out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)column->name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = column->name, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(args->out, ",\"kind\":");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)column->kind,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = column->kind, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(args->out, column->numeric ? ",\"numeric\":true}" : ",\"numeric\":false}");
        }
        mmgr_line_ch(args->out, ']');
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"done\":");
    if (running->has_tag)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)running->tag,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = running->tag, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"notices\":");
    mmgr_line_put_n(args->out, running->notices_line.out, running->notices_line.at);
    mmgr_line_put(args->out, ",\"transaction\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = running->transaction,
               .length = cellul.len(&(CatenaFinitaCfg){.src = running->transaction, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"zone\":");
    char zone[CLI_NAME_MAX_LEN];
    VerbaLinea zone_line = {zone, sizeof(zone), 0ULL};
    EMBED_CALL(cli_db_out_count.zone, CliDbOutCountCfg, .open = args->open, .out = &zone_line);
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)zone_line.out, .length = zone_line.at);
    mmgr_line_put(args->out, ",\"shaped\":");
    if (has_shaped)
    {
        mmgr_line_put(args->out, "{\"verb\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)shaped.verb,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = shaped.verb, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"kind\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)shaped.kind,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = shaped.kind, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"name\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)shaped.name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = shaped.name, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"on\":");
        if (shaped.has_on)
        {
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)shaped.on,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = shaped.on, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            mmgr_line_put(args->out, "null");
        }
        mmgr_line_ch(args->out, '}');
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"ms\":");
    mmgr_line_decimal(args->out, consumed, 1u);
    mmgr_line_ch(args->out, '}');
    return 1;
}
