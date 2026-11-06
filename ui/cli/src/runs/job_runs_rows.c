// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// job_runs_rows.c: the Run pane's rows (job_runs.h). The rows stand in sections laid in order, each its first row,
// its rows, their kind, the item they read and the flags of its one row, in one cell sized to them and laid anew as
// the store changes: a group's, a job's, the chosen job's file and about, a param's, the choices of the param focused,
// a run's, the lines of the run shown and its end. A row is read by a binary search over the sections' starts, and its
// texts point into the catalog, the form and the runs' cells
#include "job_runs_internal.h"

typedef struct
{
    CliJobRunsSection *section;
    unsigned int at;
    unsigned int start;
    unsigned int count;
    unsigned int kind;
    unsigned int item;
    unsigned int flags;
    unsigned int row_at;
    unsigned int job;
    const char *text;
    unsigned long long length;
    const char *other;
    unsigned long long other_length;
} RowsCtx;

// the section `at` written where the sections are handed, from the row `start`, `count` rows of `kind` reading `item`
EMBED_INLINE void section_put(const RowsCtx *args)
{
    if (args->section != NULL)
    {
        args->section[args->at] = (CliJobRunsSection){args->start, args->count, args->kind, args->item, args->flags};
    }
}

// whether a run of the job `job` goes
EMBED_INLINE int job_live(const RowsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    for (unsigned int at = 0u; at < store->runs; at += 1u)
    {
        if (store->run[at].live && (store->run[at].job == args->job))
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
    const CliJobRunsRun *const run = &store->run[args->item];
    const int failed = !run->live && (run->stopped || (run->has_code && (run->code != 0LL)));
    const int ok = !run->live && !run->stopped && run->has_code && (run->code == 0LL);
    return (run->live ? CLI_JOB_RUN_LIVE : 0u) | (ok ? CLI_JOB_RUN_OK : 0u) | (failed ? CLI_JOB_RUN_FAILED : 0u) |
           ((run->run == store->shown) ? CLI_JOB_RUN_SHOWN : 0u);
}

// whether `kind` is one of the param kinds whose choices are the value itself
EMBED_INLINE int kind_chosen(const RowsCtx *args)
{
    return cellul.eq(
               &(CatenaFinitaCfg){.src = args->text, .other = CLI_JOB_RUNS_KIND_CHOICE, .cap = CLI_NAME_MAX_LEN}) ||
           cellul.eq(&(CatenaFinitaCfg){.src = args->text, .other = CLI_JOB_RUNS_KIND_MANY, .cap = CLI_NAME_MAX_LEN});
}

// the chosen job's sections laid from `start` into `section` from `at`, where the sections are handed: how many
EMBED_INLINE unsigned int chosen_laid(const RowsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    const CliJob *const job = &store->catalog->job[args->job];
    unsigned int at = args->at;
    unsigned int row = args->start;
    EMBED_CALL(section_put, RowsCtx, .section = args->section, .at = at, .start = row, .count = 1u,
               .kind = CLI_JOB_RUN_ABOUT, .item = args->job);
    at += 1u;
    row += 1u;
    for (unsigned int param = 0u; param < store->form_params; param += 1u)
    {
        const CliParam *const one = &store->catalog->param[job->param_first + param];
        const unsigned int flags =
            (one->required ? CLI_JOB_RUN_REQUIRED : 0u) | ((param == store->focus) ? CLI_JOB_RUN_FOCUSED : 0u);
        EMBED_CALL(section_put, RowsCtx, .section = args->section, .at = at, .start = row, .count = 1u,
                   .kind = CLI_JOB_RUN_PARAM, .item = param, .flags = flags);
        at += 1u;
        row += 1u;
        if ((param == store->focus) && (one->choices != 0u))
        {
            EMBED_CALL(section_put, RowsCtx, .section = args->section, .at = at, .start = row, .count = one->choices,
                       .kind = CLI_JOB_RUN_CHOICE, .item = param);
            at += 1u;
            row += one->choices;
        }
    }
    for (unsigned int run = 0u; run < store->runs; run += 1u)
    {
        const CliJobRunsRun *const one = &store->run[run];
        if (one->job != args->job)
        {
            continue;
        }
        EMBED_CALL(section_put, RowsCtx, .section = args->section, .at = at, .start = row, .count = 1u,
                   .kind = CLI_JOB_RUN_RUN, .item = run, .flags = EMBED_CALL(run_flags, RowsCtx, .item = run));
        at += 1u;
        row += 1u;
        if (one->run != store->shown)
        {
            continue;
        }
        if (one->lines != 0u)
        {
            EMBED_CALL(section_put, RowsCtx, .section = args->section, .at = at, .start = row, .count = one->lines,
                       .kind = CLI_JOB_RUN_LINE, .item = run);
            at += 1u;
            row += one->lines;
        }
        if (!one->live)
        {
            EMBED_CALL(section_put, RowsCtx, .section = args->section, .at = at, .start = row, .count = 1u,
                       .kind = CLI_JOB_RUN_END, .item = run, .flags = EMBED_CALL(run_flags, RowsCtx, .item = run));
            at += 1u;
        }
    }
    return at - args->at;
}

// every section laid in order into `section`, where it is handed: how many
EMBED_INLINE unsigned int sections_laid(const RowsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    const int filtered = store->filter.length != 0ULL;
    unsigned int at = 0u;
    unsigned int row = 0u;
    for (unsigned int group = 0u; group < store->groups; group += 1u)
    {
        const CliJobRunsGroup *const one = &store->group[group];
        if (filtered && (one->listed == 0u))
        {
            continue;
        }
        EMBED_CALL(section_put, RowsCtx, .section = args->section, .at = at, .start = row, .count = 1u,
                   .kind = CLI_JOB_RUN_GROUP, .item = group, .flags = one->open ? CLI_JOB_RUN_OPEN : 0u);
        at += 1u;
        row += 1u;
        for (unsigned int member = 0u; one->open && (member < one->count); member += 1u)
        {
            const unsigned int job = store->order[one->first + member];
            if (!store->passes[job])
            {
                continue;
            }
            const unsigned int flags = ((job == store->chosen) ? CLI_JOB_RUN_CHOSEN : 0u) |
                                       (EMBED_CALL(job_live, RowsCtx, .job = job) ? CLI_JOB_RUN_LIVE : 0u);
            EMBED_CALL(section_put, RowsCtx, .section = args->section, .at = at, .start = row, .count = 1u,
                       .kind = CLI_JOB_RUN_JOB, .item = job, .flags = flags);
            at += 1u;
            row += 1u;
            if (job != store->chosen)
            {
                continue;
            }
            const unsigned int laid =
                EMBED_CALL(chosen_laid, RowsCtx, .section = args->section, .at = at, .start = row, .job = job);
            // the rows the chosen job's sections take read off them as they are laid; a count of sections alone reads
            // no rows
            for (unsigned int each = 0u; each < laid; each += 1u)
            {
                row += (args->section != NULL) ? args->section[at + each].count : 0u;
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
        if (store->section[middle].start <= args->row_at)
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
    const unsigned int count = EMBED_CALL(sections_laid, RowsCtx, .section = NULL);
    if (count != store->sections)
    {
        g_cli_guard->persistent_buf_release(store->section, g_cli_job_runs_owner);
        store->section =
            (count != 0u) ? g_cli_guard->persistent_buf_alloc(count * sizeof(CliJobRunsSection), g_cli_job_runs_owner)
                          : NULL;
        store->sections = (store->section != NULL) ? count : 0u;
    }
    if (store->sections != 0u)
    {
        (void)EMBED_CALL(sections_laid, RowsCtx, .section = store->section);
    }
    const CliJobRunsSection *const last = (store->sections != 0u) ? &store->section[store->sections - 1u] : NULL;
    store->rows = (last != NULL) ? last->start + last->count : 0u;
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
    const CliJobRunsSection *const section = &store->section[EMBED_CALL(section_at, RowsCtx, .row_at = args->row_at)];
    const unsigned int offset = args->row_at - section->start;
    const CliCatalog *const catalog = store->catalog;
    CliJobRunRow *const row = args->row_out;
    *row = (CliJobRunRow){0};
    row->kind = section->kind;
    row->flags = section->flags;
    switch (section->kind)
    {
    case CLI_JOB_RUN_GROUP:
        row->text = store->group[section->item].name;
        row->second = store->group[section->item].listed_text;
        row->second_length = store->group[section->item].listed_length;
        row->number = store->group[section->item].listed;
        break;
    case CLI_JOB_RUN_JOB: {
        const CliJob *const job = &catalog->job[section->item];
        const char *const title =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->title);
        row->text = (title[0] != '\0') ? title
                                       : EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                    .catalog_read = catalog, .text_u32 = job->id);
        row->depth = 1u;
        break;
    }
    case CLI_JOB_RUN_ABOUT: {
        const CliJob *const job = &catalog->job[section->item];
        row->text =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->file);
        row->second =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->about);
        row->depth = 2u;
        break;
    }
    case CLI_JOB_RUN_PARAM:
    case CLI_JOB_RUN_CHOICE: {
        const CliParam *const param = &catalog->param[catalog->job[store->chosen].param_first + section->item];
        unsigned long long length = 0ULL;
        const char *const value =
            EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx, .param = section->item, .length_out = &length);
        if (section->kind == CLI_JOB_RUN_CHOICE)
        {
            row->text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                   .text_u32 = catalog->list[param->first + offset]);
            const unsigned long long choice_length =
                cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_JOB_RUNS_FIELD_MAX});
            row->flags |= EMBED_CALL(word_among, RowsCtx, .text = row->text, .length = choice_length, .other = value,
                                     .other_length = length)
                              ? CLI_JOB_RUN_CHOSEN
                              : 0u;
            row->depth = 3u;
            break;
        }
        row->text =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = param->key);
        row->second = value;
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
        const CliJobRunsRun *const run = &store->run[section->item];
        const unsigned int from = (section->kind == CLI_JOB_RUN_END) ? run->word : 0u;
        row->text = run->state + from;
        row->text_length = run->state_length - from;
        row->number = run->run;
        row->depth = (section->kind == CLI_JOB_RUN_END) ? 3u : 2u;
        break;
    }
    default: {
        const CliJobRunsRun *const run = &store->run[section->item];
        const CliJobRunsLine *const line = &run->line[offset];
        row->text = run->text + line->start;
        row->text_length = line->length;
        row->stream = line->stream;
        row->number = run->run;
        row->depth = 3u;
        break;
    }
    }
    // a text that is not a run's or a value's carries its end, its bytes counted to it
    if ((row->text != NULL) && (row->text_length == 0ULL) && (section->kind != CLI_JOB_RUN_LINE))
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
    const CliJobRunsSection *const section = &store->section[EMBED_CALL(section_at, RowsCtx, .row_at = args->row_at)];
    if (section->kind != CLI_JOB_RUN_LINE)
    {
        return 0;
    }
    const CliJobRunsRun *const run = &store->run[section->item];
    const CliJobRunsLine *const line = &run->line[args->row_at - section->start];
    *args->line_out =
        (CliJobRunLine){run->text + line->start, line->length,
                        (line->spans != 0u) ? run->span + line->span_first : NULL, line->spans, line->stream};
    return 1;
}

int cli_job_runs_row_press(const CliJobRunsRowCfg *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    if (args->row_at >= store->rows)
    {
        return 0;
    }
    const CliJobRunsSection *const section = &store->section[EMBED_CALL(section_at, RowsCtx, .row_at = args->row_at)];
    const unsigned int item = section->item;
    switch (section->kind)
    {
    case CLI_JOB_RUN_GROUP:
        store->group[item].open = !store->group[item].open;
        break;
    case CLI_JOB_RUN_JOB: {
        // the job chosen, its id in a cell of its own with its end, or let go where it was; the run shown its newest
        g_cli_guard->persistent_buf_release(store->chosen_id, g_cli_job_runs_owner);
        store->chosen_id = NULL;
        store->chosen = (store->chosen == item) ? CLI_CATALOG_NONE : item;
        const char *const id =
            (store->chosen != CLI_CATALOG_NONE)
                ? EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = store->catalog,
                             .text_u32 = store->catalog->job[item].id)
                : NULL;
        const unsigned long long length =
            (id != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_PATH_MAX_LEN}) : 0ULL;
        store->chosen_id = (id != NULL) ? g_cli_guard->persistent_buf_alloc(length + 1ULL, g_cli_job_runs_owner) : NULL;
        if (store->chosen_id != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = store->chosen_id, .src = id, .bytes = length + 1ULL});
        }
        store->shown = 0ULL;
        for (unsigned int at = 0u; at < store->runs; at += 1u)
        {
            store->shown = (store->run[at].job == store->chosen) ? store->run[at].run : store->shown;
        }
        EMBED_CALL(cli_job_runs_internal.form_clear, CliJobRunsCtx, .values = NULL);
        break;
    }
    case CLI_JOB_RUN_PARAM: {
        store->focus = item;
        EMBED_CALL(cli_job_runs_internal.sections_settle, CliJobRunsCtx, .values = NULL);
        const CliParam *const param = &store->catalog->param[store->catalog->job[store->chosen].param_first + item];
        return !EMBED_CALL(kind_chosen, RowsCtx, .text = param->kind);
    }
    case CLI_JOB_RUN_CHOICE:
        EMBED_CALL(cli_job_runs_internal.choice_take, CliJobRunsCtx, .param = item,
                   .choice = args->row_at - section->start);
        return 0;
    case CLI_JOB_RUN_RUN:
        store->shown = store->run[item].run;
        break;
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
