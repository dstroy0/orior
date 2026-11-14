// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// job_runs_rows.c: the Run pane's rows (job_runs.h). The rows stand in sections laid in order, each its first row,
// its rows, their type, the item they read and the flags of its one row, each a cell of the store's, laid anew as
// the store changes, the cells past those laid given back: a group's, a job's, the chosen job's file and about, a
// param's, the choices of the param focused, a run's, the lines of the run shown and its end. A row is read by a binary
// search over the sections' starts; its texts point into the catalog or into the rows' own buffers, which hold what was
// copied out of the store's cells for the row last read
#include "job_runs_internal.h"

typedef struct
{
    int write;
    unsigned int at;
    unsigned int start;
    unsigned int count;
    unsigned int type;
    unsigned int item;
    unsigned int flags;
    unsigned int row_at;
    unsigned int job;
    const char *text;
    unsigned long long length;
    const char *other;
    unsigned long long other_length;
} RowsCtx;

// the group, the run, the line's text and its spans, and the param's value of the row or line last read, each copied
// out of the store's cells: a reader reads them before the next row
static CliJobRunsGroup s_row_group;
static CliJobRunsRun s_row_run;
static char s_row_text[CLI_JOB_RUNS_LINE_MAX];
static char s_row_value[CLI_JOB_RUNS_FIELD_MAX];
static char s_line_text[CLI_JOB_RUNS_LINE_MAX];
static CliTerminalXtermSpan s_line_spans[CLI_JOB_RUNS_LINE_SPANS];

// the section `at` written where the sections are written, from the row `start`, `count` rows of `type` reading
// `item`
EMBED_INLINE void section_put(const RowsCtx *args)
{
    if (args->write && (args->at < CLI_JOB_RUNS_SECTIONS_MAX))
    {
        const CliJobRunsSection section = {args->start, args->count, args->type, args->item, args->flags};
        (void)EMBED_CALL(cli_job_runs_cells.section_write, CliJobRunsCellCfg, .at = args->at, .section = &section);
    }
}

// whether a run of the job `job` goes
EMBED_INLINE int job_live(const RowsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    for (unsigned int at = 0u; at < store->runs; at += 1u)
    {
        CliJobRunsRun run;
        EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = at, .run = &run);
        if (run.live && (run.job == args->job))
        {
            return 1;
        }
    }
    return 0;
}

// the flags of the run `item`: going, ended with 0, ended otherwise or stopped, and shown
EMBED_INLINE unsigned int run_flags(const RowsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    CliJobRunsRun run;
    EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = args->item, .run = &run);
    const int failed = !run.live && (run.stopped || (run.has_code && (run.code != 0LL)));
    const int ok = !run.live && !run.stopped && run.has_code && (run.code == 0LL);
    return (run.live ? CLI_JOB_RUN_LIVE : 0u) | (ok ? CLI_JOB_RUN_OK : 0u) | (failed ? CLI_JOB_RUN_FAILED : 0u) |
           ((run.run == store->shown) ? CLI_JOB_RUN_SHOWN : 0u);
}

// whether `type` is one of the param types whose choices are the value itself
EMBED_INLINE int type_chosen(const RowsCtx *args)
{
    return cellul.eq(
               &(CatenaFinitaCfg){.src = args->text, .other = CLI_JOB_RUNS_TYPE_CHOICE, .cap = CLI_NAME_MAX_LEN}) ||
           cellul.eq(&(CatenaFinitaCfg){.src = args->text, .other = CLI_JOB_RUNS_TYPE_MANY, .cap = CLI_NAME_MAX_LEN});
}

// the chosen job's sections laid from `start` from the section `at`, written where `write` is: how many
EMBED_INLINE unsigned int chosen_laid(const RowsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    const CliJob *const job = &store->catalog->job[args->job];
    unsigned int at = args->at;
    unsigned int row = args->start;
    EMBED_CALL(section_put, RowsCtx, .write = args->write, .at = at, .start = row, .count = 1u,
               .type = CLI_JOB_RUN_ABOUT, .item = args->job);
    at += 1u;
    row += 1u;
    for (unsigned int param = 0u; param < store->form_params; param += 1u)
    {
        const CliParam *const one = &store->catalog->param[job->param_first + param];
        const unsigned int flags =
            (one->required ? CLI_JOB_RUN_REQUIRED : 0u) | ((param == store->focus) ? CLI_JOB_RUN_FOCUSED : 0u);
        EMBED_CALL(section_put, RowsCtx, .write = args->write, .at = at, .start = row, .count = 1u,
                   .type = CLI_JOB_RUN_PARAM, .item = param, .flags = flags);
        at += 1u;
        row += 1u;
        if ((param == store->focus) && (one->choices != 0u))
        {
            EMBED_CALL(section_put, RowsCtx, .write = args->write, .at = at, .start = row, .count = one->choices,
                       .type = CLI_JOB_RUN_CHOICE, .item = param);
            at += 1u;
            row += one->choices;
        }
    }
    for (unsigned int run = 0u; run < store->runs; run += 1u)
    {
        CliJobRunsRun one;
        EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = run, .run = &one);
        if (one.job != args->job)
        {
            continue;
        }
        EMBED_CALL(section_put, RowsCtx, .write = args->write, .at = at, .start = row, .count = 1u,
                   .type = CLI_JOB_RUN_RUN, .item = run, .flags = EMBED_CALL(run_flags, RowsCtx, .item = run));
        at += 1u;
        row += 1u;
        if (one.run != store->shown)
        {
            continue;
        }
        // the lines in flight are at most CLI_LINES_MAX, inside an unsigned int
        const unsigned int lines = (unsigned int)anularis.seg_inflight(&(AnularisCfg){.ring = &one.rings->line});
        if (lines != 0u)
        {
            EMBED_CALL(section_put, RowsCtx, .write = args->write, .at = at, .start = row, .count = lines,
                       .type = CLI_JOB_RUN_LINE, .item = run);
            at += 1u;
            row += lines;
        }
        if (!one.live)
        {
            EMBED_CALL(section_put, RowsCtx, .write = args->write, .at = at, .start = row, .count = 1u,
                       .type = CLI_JOB_RUN_END, .item = run, .flags = EMBED_CALL(run_flags, RowsCtx, .item = run));
            at += 1u;
        }
    }
    return at - args->at;
}

// every section laid in order, written into the store's cell where `write` is: how many
EMBED_INLINE unsigned int sections_laid(const RowsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    const int filtered = store->filter.length != 0ULL;
    unsigned int at = 0u;
    unsigned int row = 0u;
    for (unsigned int group = 0u; group < store->groups; group += 1u)
    {
        CliJobRunsGroup one;
        EMBED_CALL(cli_job_runs_cells.group_read, CliJobRunsCellCfg, .at = group, .group = &one);
        if (filtered && (one.listed == 0u))
        {
            continue;
        }
        EMBED_CALL(section_put, RowsCtx, .write = args->write, .at = at, .start = row, .count = 1u,
                   .type = CLI_JOB_RUN_GROUP, .item = group, .flags = one.open ? CLI_JOB_RUN_OPEN : 0u);
        at += 1u;
        row += 1u;
        for (unsigned int member = 0u; one.open && (member < one.count); member += 1u)
        {
            unsigned int job = 0u;
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = g_cli_job_runs_store.order,
                                    .offset = (unsigned long long)(one.first + member) * sizeof(job),
                                    .dst = &job,
                                    .bytes = sizeof(job)});
            unsigned char passes_1 = 0u;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = g_cli_job_runs_store.passes, .offset = job, .dst = &passes_1, .bytes = sizeof(passes_1)});

            if (!passes_1)
            {
                continue;
            }
            const unsigned int flags = ((job == store->chosen) ? CLI_JOB_RUN_CHOSEN : 0u) |
                                       (EMBED_CALL(job_live, RowsCtx, .job = job) ? CLI_JOB_RUN_LIVE : 0u);
            EMBED_CALL(section_put, RowsCtx, .write = args->write, .at = at, .start = row, .count = 1u,
                       .type = CLI_JOB_RUN_JOB, .item = job, .flags = flags);
            at += 1u;
            row += 1u;
            if (job != store->chosen)
            {
                continue;
            }
            const unsigned int laid =
                EMBED_CALL(chosen_laid, RowsCtx, .write = args->write, .at = at, .start = row, .job = job);
            // the rows the chosen job's sections take read off them as they are laid; a count of sections alone reads
            // no rows
            for (unsigned int each = 0u; args->write && (each < laid) && (at + each < CLI_JOB_RUNS_SECTIONS_MAX);
                 each += 1u)
            {
                CliJobRunsSection section;
                EMBED_CALL(cli_job_runs_cells.section_read, CliJobRunsCellCfg, .at = (at + each), .section = &section);
                row += section.count;
            }
            at += laid;
        }
    }
    return at;
}

// the place of the section the row `row_at` stands in: the last whose first row is at most it
EMBED_INLINE unsigned int section_at(const RowsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    unsigned int low = 0u;
    unsigned int high = store->sections;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        CliJobRunsSection section;
        EMBED_CALL(cli_job_runs_cells.section_read, CliJobRunsCellCfg, .at = middle, .section = &section);
        if (section.start <= args->row_at)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// whether the `length` bytes at `text` stand as one of the words of the `other_length` bytes at `other`, a space
// between them
EMBED_INLINE int word_among(const RowsCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->other_length;)
    {
        unsigned long long end = at;
        while ((end < args->other_length) && (args->other[end] != ' '))
        {
            end += 1ULL;
        }
        if (((end - at) == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->other + at, .other = args->text, .bytes = args->length}) == 0))
        {
            return 1;
        }
        at = end + 1ULL;
    }
    return 0;
}

void cli_job_runs_sections_settle(const CliJobRunsCtx *args)
{
    (void)args;
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    const unsigned int was_rows = store->rows;
    const unsigned int was_sections = store->sections;
    const unsigned int count = EMBED_CALL(sections_laid, RowsCtx, .write = 0);
    store->sections = (count <= CLI_JOB_RUNS_SECTIONS_MAX) ? count : CLI_JOB_RUNS_SECTIONS_MAX;
    if (store->sections != 0u)
    {
        (void)EMBED_CALL(sections_laid, RowsCtx, .write = 1);
    }
    for (unsigned int at = store->sections; at < was_sections; at += 1u)
    {
        (void)g_cli_guard->movable_buf_release(store->section[at], g_cli_job_runs_owner);
        store->section[at] = NULL;
    }
    CliJobRunsSection last = {0};
    if (store->sections != 0u)
    {
        EMBED_CALL(cli_job_runs_cells.section_read, CliJobRunsCellCfg, .at = (store->sections - 1u), .section = &last);
    }
    store->rows = last.start + last.count;
    store->rows_before = (store->rows > was_rows) ? was_rows : store->rows;
}

unsigned int cli_job_runs_row_count(const CliJobRunsRowCfg *args)
{
    (void)args;
    return g_cli_job_runs_store.rows;
}

int cli_job_runs_row_read(const CliJobRunsRowCfg *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    if (args->row_at >= store->rows)
    {
        return 0;
    }
    CliJobRunsSection section;
    EMBED_CALL(cli_job_runs_cells.section_read, CliJobRunsCellCfg,
               .at = (EMBED_CALL(section_at, RowsCtx, .row_at = args->row_at)), .section = &section);
    const unsigned int offset = args->row_at - section.start;
    const CliCatalog *const catalog = store->catalog;
    CliJobRunRow *const row = args->row_out;
    *row = (CliJobRunRow){0};
    row->type = section.type;
    row->flags = section.flags;
    switch (section.type)
    {
    case CLI_JOB_RUN_GROUP:
        EMBED_CALL(cli_job_runs_cells.group_read, CliJobRunsCellCfg, .at = section.item, .group = &s_row_group);
        row->text = s_row_group.name;
        row->second = s_row_group.listed_text;
        row->second_length = s_row_group.listed_length;
        row->number = s_row_group.listed;
        break;
    case CLI_JOB_RUN_JOB: {
        const CliJob *const job = &catalog->job[section.item];
        const char *const title =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->title);
        row->text = (title[0] != '\0') ? title
                                       : EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                    .catalog_read = catalog, .text_u32 = job->id);
        row->depth = 1u;
        break;
    }
    case CLI_JOB_RUN_ABOUT: {
        const CliJob *const job = &catalog->job[section.item];
        row->text =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->file);
        row->second =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->about);
        row->depth = 2u;
        break;
    }
    case CLI_JOB_RUN_PARAM:
    case CLI_JOB_RUN_CHOICE: {
        const CliParam *const param = &catalog->param[catalog->job[store->chosen].param_first + section.item];
        const unsigned long long length = EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx,
                                                     .param = section.item, .value_out = s_row_value);
        if (section.type == CLI_JOB_RUN_CHOICE)
        {
            row->text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                   .text_u32 = catalog->list[param->first + offset]);
            const unsigned long long choice_length =
                cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_JOB_RUNS_FIELD_MAX});
            row->flags |= EMBED_CALL(word_among, RowsCtx, .text = row->text, .length = choice_length,
                                     .other = s_row_value, .other_length = length)
                              ? CLI_JOB_RUN_CHOSEN
                              : 0u;
            row->depth = 3u;
            break;
        }
        row->text =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = param->key);
        row->second = s_row_value;
        row->second_length = length;
        if (length == 0ULL)
        {
            row->second = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                     .text_u32 = param->fallback);
            row->second_length = cellul.len(&(CatenaFinitaCfg){.src = row->second, .cap = CLI_JOB_RUNS_FIELD_MAX});
            row->flags |= (row->second_length != 0ULL) ? CLI_JOB_RUN_DEFAULTED : 0u;
        }
        row->depth = 2u;
        break;
    }
    case CLI_JOB_RUN_RUN:
    case CLI_JOB_RUN_END: {
        EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = section.item, .run = &s_row_run);
        const unsigned int from = (section.type == CLI_JOB_RUN_END) ? s_row_run.word : 0u;
        row->text = s_row_run.state + from;
        row->text_length = s_row_run.state_length - from;
        row->number = s_row_run.run;
        row->depth = (section.type == CLI_JOB_RUN_END) ? 3u : 2u;
        break;
    }
    default: {
        EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = section.item, .run = &s_row_run);
        CliJobRunsRings *const rings = s_row_run.rings;
        // the line's record is the segment `offset` past the line ring's front
        size_t front = 0u;
        (void)anularis.seg_front(&(AnularisCfg){.ring = &rings->line, .out_index = &front});
        CliJobRunsLine line;
        memor.cpy(&(MemoriaCfg){.dst = &line,
                                .src = anularis.seg_at(&(AnularisCfg){
                                    .ring = &rings->line, .index = MMGR_RING_WRAP(front + offset, CLI_LINES_MAX)}),
                                .bytes = sizeof(line)});
        row->text = s_row_text;
        // the line's plain bytes out of the text ring, ahead of its tail by the bytes put before them less the bytes
        // consumed since, a difference that wraps right in the unsigned int
        const unsigned long long text_length =
            (line.length < CLI_JOB_RUNS_LINE_MAX) ? line.length : CLI_JOB_RUNS_LINE_MAX - 1ULL;
        anularis.peek(&(AnularisCfg){.ring = &rings->text,
                                     .offset = line.start - rings->text_taken,
                                     .dst = (embed_u8 *)(void *)s_row_text,
                                     .bytes = text_length});
        s_row_text[text_length] = '\0';
        row->text_length = text_length;
        row->stream = line.stream;
        row->number = s_row_run.run;
        row->depth = 3u;
        break;
    }
    }
    // a text that is not a run's or a value's carries its end, its bytes counted to it
    if ((row->text != NULL) && (row->text_length == 0ULL) && (section.type != CLI_JOB_RUN_LINE))
    {
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_LINES_TEXT});
    }
    if ((row->second != NULL) && (row->second_length == 0ULL))
    {
        row->second_length = cellul.len(&(CatenaFinitaCfg){.src = row->second, .cap = CLI_LINES_TEXT});
    }
    return 1;
}

int cli_job_runs_line_read(const CliJobRunsRowCfg *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    if (args->row_at >= store->rows)
    {
        return 0;
    }
    CliJobRunsSection section;
    EMBED_CALL(cli_job_runs_cells.section_read, CliJobRunsCellCfg,
               .at = (EMBED_CALL(section_at, RowsCtx, .row_at = args->row_at)), .section = &section);
    if (section.type != CLI_JOB_RUN_LINE)
    {
        return 0;
    }
    CliJobRunsRun run;
    EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = section.item, .run = &run);
    CliJobRunsRings *const rings = run.rings;
    // the line's record is the segment `row_at - start` past the line ring's front
    size_t front = 0u;
    (void)anularis.seg_front(&(AnularisCfg){.ring = &rings->line, .out_index = &front});
    CliJobRunsLine line;
    memor.cpy(&(MemoriaCfg){
        .dst = &line,
        .src = anularis.seg_at(&(AnularisCfg){
            .ring = &rings->line, .index = MMGR_RING_WRAP(front + (args->row_at - section.start), CLI_LINES_MAX)}),
        .bytes = sizeof(line)});
    // the line's plain bytes out of the text ring, ahead of its tail by the bytes put before them less the bytes
    // consumed since, a difference that wraps right in the unsigned int; and its spans, one segment each from its first
    const unsigned long long length =
        (line.length < CLI_JOB_RUNS_LINE_MAX) ? line.length : CLI_JOB_RUNS_LINE_MAX - 1ULL;
    anularis.peek(&(AnularisCfg){.ring = &rings->text,
                                 .offset = line.start - rings->text_taken,
                                 .dst = (embed_u8 *)(void *)s_line_text,
                                 .bytes = length});
    s_line_text[length] = '\0';
    const unsigned int spans = (line.spans < CLI_JOB_RUNS_LINE_SPANS) ? line.spans : CLI_JOB_RUNS_LINE_SPANS;
    for (unsigned int at = 0u; at < spans; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = &s_line_spans[at],
            .src = anularis.seg_at(&(AnularisCfg){
                .ring = &rings->span, .index = MMGR_RING_WRAP(line.span_first + at, CLI_JOB_RUNS_SPANS_MAX)}),
            .bytes = sizeof(*s_line_spans)});
    }
    *args->line_out = (CliJobRunLine){s_line_text, length, (spans != 0u) ? s_line_spans : NULL, spans, line.stream};
    return 1;
}

int cli_job_runs_row_press(const CliJobRunsRowCfg *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    if (args->row_at >= store->rows)
    {
        return 0;
    }
    CliJobRunsSection section;
    EMBED_CALL(cli_job_runs_cells.section_read, CliJobRunsCellCfg,
               .at = (EMBED_CALL(section_at, RowsCtx, .row_at = args->row_at)), .section = &section);
    const unsigned int item = section.item;
    switch (section.type)
    {
    case CLI_JOB_RUN_GROUP: {
        CliJobRunsGroup group;
        EMBED_CALL(cli_job_runs_cells.group_read, CliJobRunsCellCfg, .at = item, .group = &group);
        group.open = !group.open;
        (void)EMBED_CALL(cli_job_runs_cells.group_write, CliJobRunsCellCfg, .at = item, .group = &group);
        break;
    }
    case CLI_JOB_RUN_JOB: {
        // the job chosen, its id with its end, or let go where it was; the run shown its newest
        store->chosen = (store->chosen == item) ? CLI_CATALOG_NONE : item;
        const char *const id =
            (store->chosen != CLI_CATALOG_NONE)
                ? EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = store->catalog,
                             .text_u32 = store->catalog->job[item].id)
                : NULL;
        const unsigned long long length =
            (id != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_PATH_MAX_LEN - 1ULL}) : 0ULL;
        if (length != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = store->chosen_id, .src = id, .bytes = length});
        }
        store->chosen_id[length] = '\0';
        store->shown = 0ULL;
        for (unsigned int at = 0u; at < store->runs; at += 1u)
        {
            CliJobRunsRun run;
            EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = at, .run = &run);
            store->shown = (run.job == store->chosen) ? run.run : store->shown;
        }
        EMBED_CALL(cli_job_runs_internal.form_clear, CliJobRunsCtx, .values = NULL);
        break;
    }
    case CLI_JOB_RUN_PARAM: {
        store->focus = item;
        EMBED_CALL(cli_job_runs_internal.sections_settle, CliJobRunsCtx, .values = NULL);
        const CliParam *const param = &store->catalog->param[store->catalog->job[store->chosen].param_first + item];
        return !EMBED_CALL(type_chosen, RowsCtx, .text = param->type);
    }
    case CLI_JOB_RUN_CHOICE:
        EMBED_CALL(cli_job_runs_internal.choice_take, CliJobRunsCtx, .param = item,
                   .choice = args->row_at - section.start);
        return 0;
    case CLI_JOB_RUN_RUN: {
        CliJobRunsRun run;
        EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = item, .run = &run);
        store->shown = run.run;
        break;
    }
    default:
        return 0;
    }
    EMBED_CALL(cli_job_runs_internal.sections_settle, CliJobRunsCtx, .values = NULL);
    return 0;
}

unsigned int cli_job_runs_follow(const CliJobRunsScrollCfg *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    const int stood_last = (args->scroll_top + args->capacity) >= store->rows_before;
    return (stood_last && (store->rows > args->capacity)) ? store->rows - args->capacity : args->scroll_top;
}
