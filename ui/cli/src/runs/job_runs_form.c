// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// job_runs_form.c: the Run pane's filter and form (job_runs.h). The filter is typed into its cell, and a byte a job, 1
// where its id, title or about contains the filter, is set anew as it changes, with each group's count of the jobs it
// lets are. The form of the job chosen is one cell taken at its max as the job is chosen: a start a param and one
// past the last, then the values' bytes one after another, CLI_JOB_RUNS_FIELD_MAX a param, the values after a changed
// one moved in place. A choice of many is a word among the value's words, a space between them
#include "job_runs_internal.h"

#include "../command_line/command_line_internal.h"

typedef struct
{
    unsigned int param;
    const char *bytes;
    unsigned long long length;
    const char *type;
    const char *text;
    unsigned int text_at;
    unsigned int *starts;
} FormCtx;

// the param `param` of the job chosen
EMBED_INLINE const CliParam *form_param(const FormCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    return &store->catalog->param[store->catalog->job[store->chosen].param_first + args->param];
}

// the text at `text_at` of the catalog listed
EMBED_INLINE const char *form_text(const FormCtx *args)
{
    return EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = g_cli_job_runs_store.catalog,
                      .text_u32 = args->text_at);
}

// the bytes the form's starts take: one a param and one past them
EMBED_INLINE unsigned long long form_starts_bytes(const FormCtx *args)
{
    (void)args;
    return (g_cli_job_runs_store.form_params + 1ULL) * sizeof(unsigned int);
}

// the form's starts read into `starts`
EMBED_INLINE void form_starts_read(const FormCtx *args)
{
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_job_runs_store.form,
                            .offset = 0ULL,
                            .dst = args->starts,
                            .bytes = EMBED_CALL(form_starts_bytes, FormCtx, .param = 0u)});
}

// the value of the param `param` the `length` bytes at `bytes`: the values after it moved in place by the change in
// its bytes, through a cell of the call's own, the value written and the starts after it moved
EMBED_INLINE void form_set(const FormCtx *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    if ((store->form == NULL) || (args->param >= store->form_params))
    {
        return;
    }
    const unsigned int params = store->form_params;
    unsigned int starts[CLI_JOB_RUNS_PARAMS_MAX + 1u];
    EMBED_CALL(form_starts_read, FormCtx, .starts = starts);
    const unsigned long long values_at = EMBED_CALL(form_starts_bytes, FormCtx, .param = 0u);
    const unsigned long long before = starts[args->param + 1u] - starts[args->param];
    const unsigned long long tail = starts[params] - starts[args->param + 1u];
    if (tail != 0ULL)
    {
        char *const moved = g_cli_guard->temporary_buf_alloc(tail, g_cli_job_runs_owner);
        if (moved == NULL)
        {
            return;
        }
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = store->form, .offset = values_at + starts[args->param + 1u], .dst = moved, .bytes = tail});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = store->form,
                                .offset = values_at + starts[args->param] + args->length,
                                .src = moved,
                                .bytes = tail});
        g_cli_guard->persistent_buf_release(moved, g_cli_job_runs_owner);
    }
    if (args->length != 0ULL)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = store->form, .offset = values_at + starts[args->param], .src = args->bytes, .bytes = args->length});
    }
    for (unsigned int at = args->param + 1u; at <= params; at += 1u)
    {
        // each value is bounded by CLI_JOB_RUNS_FIELD_MAX, the starts far inside an unsigned int
        starts[at] = (unsigned int)(starts[at] - before + args->length);
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = store->form, .offset = 0ULL, .src = starts, .bytes = values_at});
}

// whether `type` is the param type `text`
EMBED_INLINE int form_type_is(const FormCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->type, .other = args->text, .cap = CLI_NAME_MAX_LEN});
}

void cli_job_runs_form_clear(const CliJobRunsCtx *args)
{
    (void)args;
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    (void)g_cli_guard->movable_buf_release(store->form, g_cli_job_runs_owner);
    store->form = NULL;
    store->form_params = 0u;
    store->focus = CLI_JOB_RUNS_FILTER;
    if (store->chosen == CLI_CATALOG_NONE)
    {
        return;
    }
    // the form's cell at its max for the job's params, the pane's own bound on them: the starts and a value's bytes a
    // param
    const unsigned int listed = store->catalog->job[store->chosen].params;
    const unsigned int params = (listed <= CLI_JOB_RUNS_PARAMS_MAX) ? listed : CLI_JOB_RUNS_PARAMS_MAX;
    const unsigned long long bytes =
        ((params + 1ULL) * sizeof(unsigned int)) + ((unsigned long long)params * CLI_JOB_RUNS_FIELD_MAX);
    store->form = g_cli_guard->movable_buf_alloc(bytes, g_cli_job_runs_owner);
    if (store->form != NULL)
    {
        const unsigned int starts[CLI_JOB_RUNS_PARAMS_MAX + 1u] = {0u};
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = store->form, .offset = 0ULL, .src = starts, .bytes = (params + 1ULL) * sizeof(unsigned int)});
        store->form_params = params;
    }
}

unsigned long long cli_job_runs_form_value(const CliJobRunsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    if ((store->form == NULL) || (args->param >= store->form_params))
    {
        args->value_out[0] = '\0';
        return 0ULL;
    }
    unsigned int starts[CLI_JOB_RUNS_PARAMS_MAX + 1u];
    EMBED_CALL(form_starts_read, FormCtx, .starts = starts);
    const unsigned long long length = starts[args->param + 1u] - starts[args->param];
    if (length != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = store->form,
                                .offset = EMBED_CALL(form_starts_bytes, FormCtx, .param = 0u) + starts[args->param],
                                .dst = args->value_out,
                                .bytes = length});
    }
    args->value_out[length] = '\0';
    return length;
}

void cli_job_runs_filter_apply(const CliJobRunsCtx *args)
{
    (void)args;
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    const CliCatalog *const catalog = store->catalog;
    // the filter copied out of the typed text's cell
    char filter[CLI_JOB_RUNS_FIELD_MAX];
    (void)EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &store->filter, .dst = filter, .capacity = sizeof(filter));
    for (unsigned int group = 0u; group < store->groups; group += 1u)
    {
        CliJobRunsGroup one;
        EMBED_CALL(cli_job_runs_cells.group_read, CliJobRunsCellCfg, .at = group, .group = &one);
        one.listed = 0u;
        for (unsigned int member = 0u; member < one.count; member += 1u)
        {
            unsigned int place = 0u;
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = g_cli_job_runs_store.order,
                                    .offset = (unsigned long long)(one.first + member) * sizeof(place),
                                    .dst = &place,
                                    .bytes = sizeof(place)});
            const CliJob *const job = &catalog->job[place];
            const int passes =
                (filter[0] == '\0') ||
                EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx,
                           .text = EMBED_CALL(form_text, FormCtx, .text_at = job->id), .word_text = filter) ||
                EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx,
                           .text = EMBED_CALL(form_text, FormCtx, .text_at = job->title), .word_text = filter) ||
                EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx,
                           .text = EMBED_CALL(form_text, FormCtx, .text_at = job->about), .word_text = filter);
            // whether the job exists is 0 or 1, which a byte carries
            const unsigned char is = (unsigned char)passes;
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = store->passes, .offset = place, .src = &is, .bytes = sizeof(is)});
            one.listed += passes ? 1u : 0u;
        }
        VerbaLinea digits = {one.listed_text, sizeof(one.listed_text), 0ULL};
        mmgr_line_decimal(&digits, one.listed, 1u);
        // an unsigned int's digits are fewer than the array's
        one.listed_length = mmgr_line_ok(&digits) ? (unsigned int)digits.at : 0u;
        (void)EMBED_CALL(cli_job_runs_cells.group_write, CliJobRunsCellCfg, .at = group, .group = &one);
    }
    EMBED_CALL(cli_job_runs_internal.sections_settle, CliJobRunsCtx, .values = NULL);
}

void cli_job_runs_values_fill(const CliJobRunsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = args->values);
    char value[CLI_JOB_RUNS_FIELD_MAX];
    for (unsigned int param = 0u; param < store->form_params; param += 1u)
    {
        const CliParam *const one = EMBED_CALL(form_param, FormCtx, .param = param);
        const char *const key = EMBED_CALL(form_text, FormCtx, .text_at = one->key);
        const unsigned long long length =
            EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx, .param = param, .value_out = value);
        if (length == 0ULL)
        {
            const char *const fallback = EMBED_CALL(form_text, FormCtx, .text_at = one->fallback);
            if (fallback[0] != '\0')
            {
                EMBED_CALL(cli_catalog_key_values.values_append, CliCatalogKeyValuesCfg, .values_out = args->values,
                           .key = key, .value = fallback);
            }
            continue;
        }
        // each word of a choice of many a value of its own, any other value whole, each with its end
        const int many = EMBED_CALL(form_type_is, FormCtx, .type = one->type, .text = CLI_JOB_RUNS_TYPE_MANY);
        char word[CLI_JOB_RUNS_FIELD_MAX];
        for (unsigned long long at = 0ULL; at < length;)
        {
            unsigned long long end = at;
            while ((end < length) && (!many || (value[end] != ' ')))
            {
                end += 1ULL;
            }
            if (end > at)
            {
                memor.cpy(&(MemoriaCfg){.dst = word, .src = value + at, .bytes = end - at});
                word[end - at] = '\0';
                EMBED_CALL(cli_catalog_key_values.values_append, CliCatalogKeyValuesCfg, .values_out = args->values,
                           .key = key, .value = word);
            }
            at = end + 1ULL;
        }
    }
}

void cli_job_runs_choice_take(const CliJobRunsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    const CliParam *const one = EMBED_CALL(form_param, FormCtx, .param = args->param);
    const char *const choice =
        EMBED_CALL(form_text, FormCtx, .text_at = store->catalog->list[one->first + args->choice]);
    const unsigned long long choice_length =
        cellul.len(&(CatenaFinitaCfg){.src = choice, .cap = CLI_JOB_RUNS_FIELD_MAX});
    char value[CLI_JOB_RUNS_FIELD_MAX];
    const unsigned long long length =
        EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx, .param = args->param, .value_out = value);
    if (!EMBED_CALL(form_type_is, FormCtx, .type = one->type, .text = CLI_JOB_RUNS_TYPE_MANY))
    {
        const int same = (length == choice_length) &&
                         (memor.cmp(&(MemoriaCfg){.src = value, .other = choice, .bytes = length}) == 0);
        EMBED_CALL(form_set, FormCtx, .param = args->param, .bytes = choice, .length = same ? 0ULL : choice_length);
        return;
    }
    // the value's words written again without the choice where it is among them, else with it after them
    char turned[CLI_JOB_RUNS_FIELD_MAX];
    VerbaLinea line = {turned, sizeof(turned), 0ULL};
    int found = 0;
    for (unsigned long long at = 0ULL; at < length;)
    {
        unsigned long long end = at;
        while ((end < length) && (value[end] != ' '))
        {
            end += 1ULL;
        }
        const int same = ((end - at) == choice_length) &&
                         (memor.cmp(&(MemoriaCfg){.src = value + at, .other = choice, .bytes = choice_length}) == 0);
        found = found || same;
        if (!same && (end > at))
        {
            mmgr_line_put(&line, (line.at != 0ULL) ? " " : "");
            mmgr_line_put_n(&line, value + at, end - at);
        }
        at = end + 1ULL;
    }
    if (!found)
    {
        mmgr_line_put(&line, (line.at != 0ULL) ? " " : "");
        mmgr_line_put_n(&line, choice, choice_length);
    }
    if (mmgr_line_ok(&line))
    {
        EMBED_CALL(form_set, FormCtx, .param = args->param, .bytes = turned, .length = line.at);
    }
}

void cli_job_runs_type(const CliJobRunsFieldCfg *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    if ((store->focus == CLI_JOB_RUNS_FILTER) || (store->focus >= store->form_params))
    {
        EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &store->filter, .text = args->bytes, .size = args->length,
                   .max = CLI_JOB_RUNS_FIELD_MAX, .owner = g_cli_job_runs_owner);
        EMBED_CALL(cli_job_runs_internal.filter_apply, CliJobRunsCtx, .values = NULL);
        return;
    }
    // the bytes typed up to a control character put after the value, the value at max CLI_JOB_RUNS_FIELD_MAX bytes
    // with its end
    char joined[CLI_JOB_RUNS_FIELD_MAX];
    const unsigned long long length =
        EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx, .param = store->focus, .value_out = joined);
    unsigned long long typed = 0ULL;
    // each byte read as the unsigned byte it is, a control character below 0x20
    while ((typed < args->length) && ((unsigned char)args->bytes[typed] >= 0x20u) &&
           (length + typed + 1ULL < CLI_JOB_RUNS_FIELD_MAX))
    {
        typed += 1ULL;
    }
    if (typed == 0ULL)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = joined + length, .src = args->bytes, .bytes = typed});
    EMBED_CALL(form_set, FormCtx, .param = store->focus, .bytes = joined, .length = length + typed);
}

int cli_job_runs_key(const CliJobRunsFieldCfg *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->key_name, .other = "Backspace", .cap = 16ULL}))
    {
        return 0;
    }
    if ((store->focus == CLI_JOB_RUNS_FILTER) || (store->focus >= store->form_params))
    {
        EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &store->filter, .owner = g_cli_job_runs_owner);
        EMBED_CALL(cli_job_runs_internal.filter_apply, CliJobRunsCtx, .values = NULL);
        return 1;
    }
    // the last character erased: its continuation bytes, each read as the unsigned byte it is, and its lead
    char value[CLI_JOB_RUNS_FIELD_MAX];
    const unsigned long long length =
        EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx, .param = store->focus, .value_out = value);
    unsigned long long left_length = length;
    while ((left_length != 0ULL) && (((unsigned char)value[left_length - 1ULL] & 0xc0u) == 0x80u))
    {
        left_length -= 1ULL;
    }
    left_length -= (left_length != 0ULL) ? 1ULL : 0ULL;
    if (left_length != length)
    {
        EMBED_CALL(form_set, FormCtx, .param = store->focus, .bytes = value, .length = left_length);
    }
    return 1;
}

unsigned long long cli_job_runs_filter(const CliJobRunsFieldCfg *args)
{
    return EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &g_cli_job_runs_store.filter, .dst = args->text_out,
                      .capacity = CLI_JOB_RUNS_FIELD_MAX);
}

int cli_job_runs_filter_focused(const CliJobRunsFieldCfg *args)
{
    (void)args;
    return g_cli_job_runs_store.focus == CLI_JOB_RUNS_FILTER;
}

void cli_job_runs_filter_focus(const CliJobRunsFieldCfg *args)
{
    (void)args;
    g_cli_job_runs_store.focus = CLI_JOB_RUNS_FILTER;
    EMBED_CALL(cli_job_runs_internal.sections_settle, CliJobRunsCtx, .values = NULL);
}
