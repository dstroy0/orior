// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// job_runs_form.c: the Run pane's filter and form (job_runs.h). The filter is typed into a cell sized to it, and a byte
// a job, 1 where its id, title or about contains the filter, is set anew as it changes, with each group's count of the
// jobs it lets stand. The form of the job chosen is one cell: a start a param and one past the last, then the values'
// bytes one after another, laid anew in a cell sized to them at each change. A choice of many is a word among the
// value's words, a space between them
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

// the form laid anew with the value of the param `param` the `length` bytes at `bytes`, in a cell sized to the values
EMBED_INLINE void form_set(const FormCtx *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    if (store->form == NULL)
    {
        return;
    }
    const unsigned int params = store->form_params;
    const unsigned int *const was = store->form;
    const char *const was_bytes = (const char *)(was + params + 1u);
    const unsigned long long before = was[args->param + 1u] - was[args->param];
    const unsigned long long total = was[params] - before + args->length;
    unsigned int *const form = g_cli_guard->persistent_buf_alloc(
        ((params + 1ULL) * sizeof(unsigned int)) + total + 1ULL, g_cli_job_runs_owner);
    if (form == NULL)
    {
        return;
    }
    char *const bytes = (char *)(form + params + 1u);
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < params; at += 1u)
    {
        // the form's bytes are bounded by CLI_JOB_RUNS_FIELD_MAX a param, far inside an unsigned int
        form[at] = (unsigned int)used;
        const char *const from = (at == args->param) ? args->bytes : (was_bytes + was[at]);
        const unsigned long long length = (at == args->param) ? args->length : (was[at + 1u] - was[at]);
        if (length != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = bytes + used, .src = from, .bytes = length});
        }
        used += length;
    }
    // the total is bounded as each value is
    form[params] = (unsigned int)used;
    g_cli_guard->persistent_buf_release(store->form, g_cli_job_runs_owner);
    store->form = form;
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
    g_cli_guard->persistent_buf_release(store->form, g_cli_job_runs_owner);
    store->form = NULL;
    store->form_params = 0u;
    store->focus = CLI_JOB_RUNS_FILTER;
    if (store->chosen == CLI_CATALOG_NONE)
    {
        return;
    }
    const unsigned int params = store->catalog->job[store->chosen].params;
    const unsigned long long bytes = (params + 1ULL) * sizeof(unsigned int) + 1ULL;
    store->form = g_cli_guard->persistent_buf_alloc(bytes, g_cli_job_runs_owner);
    if (store->form != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = store->form, .val = 0, .bytes = bytes});
        store->form_params = params;
    }
}

const char *cli_job_runs_form_value(const CliJobRunsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    if ((store->form == NULL) || (args->param >= store->form_params))
    {
        *args->length_out = 0ULL;
        return "";
    }
    *args->length_out = store->form[args->param + 1u] - store->form[args->param];
    return (const char *)(store->form + store->form_params + 1u) + store->form[args->param];
}

void cli_job_runs_filter_apply(const CliJobRunsCtx *args)
{
    (void)args;
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    const CliCatalog *const catalog = store->catalog;
    const char *const filter = (store->filter.bytes != NULL) ? store->filter.bytes : "";
    for (unsigned int group = 0u; group < store->groups; group += 1u)
    {
        CliJobRunsGroup *const one = &store->group[group];
        one->listed = 0u;
        for (unsigned int member = 0u; member < one->count; member += 1u)
        {
            const CliJob *const job = &catalog->job[store->order[one->first + member]];
            const int passes =
                (filter[0] == '\0') ||
                EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx,
                           .text = EMBED_CALL(form_text, FormCtx, .text_at = job->id), .word_text = filter) ||
                EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx,
                           .text = EMBED_CALL(form_text, FormCtx, .text_at = job->title), .word_text = filter) ||
                EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx,
                           .text = EMBED_CALL(form_text, FormCtx, .text_at = job->about), .word_text = filter);
            // whether the job stands is 0 or 1, which a byte carries
            store->passes[store->order[one->first + member]] = (unsigned char)passes;
            one->listed += passes ? 1u : 0u;
        }
        VerbaLinea digits = {one->listed_text, sizeof(one->listed_text), 0ULL};
        mmgr_line_decimal(&digits, one->listed, 1u);
        // an unsigned int's digits are fewer than the array's
        one->listed_length = mmgr_line_ok(&digits) ? (unsigned int)digits.at : 0u;
    }
    EMBED_CALL(cli_job_runs_internal.sections_settle, CliJobRunsCtx, .values = NULL);
}

void cli_job_runs_values_fill(const CliJobRunsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = args->values);
    for (unsigned int param = 0u; param < store->form_params; param += 1u)
    {
        const CliParam *const one = EMBED_CALL(form_param, FormCtx, .param = param);
        const char *const key = EMBED_CALL(form_text, FormCtx, .text_at = one->key);
        unsigned long long length = 0ULL;
        const char *const value =
            EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx, .param = param, .length_out = &length);
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
    unsigned long long length = 0ULL;
    const char *const value =
        EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx, .param = args->param, .length_out = &length);
    if (!EMBED_CALL(form_type_is, FormCtx, .type = one->type, .text = CLI_JOB_RUNS_TYPE_MANY))
    {
        const int same = (length == choice_length) &&
                         (memor.cmp(&(MemoriaCfg){.src = value, .other = choice, .bytes = length}) == 0);
        EMBED_CALL(form_set, FormCtx, .param = args->param, .bytes = choice, .length = same ? 0ULL : choice_length);
        return;
    }
    // the value's words written again without the choice where it stands among them, else with it after them
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
                   .most = CLI_JOB_RUNS_FIELD_MAX, .owner = g_cli_job_runs_owner);
        EMBED_CALL(cli_job_runs_internal.filter_apply, CliJobRunsCtx, .values = NULL);
        return;
    }
    // the bytes typed up to a control character put after the value, the value at most CLI_JOB_RUNS_FIELD_MAX bytes
    // with its end
    unsigned long long length = 0ULL;
    const char *const value =
        EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx, .param = store->focus, .length_out = &length);
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
    char joined[CLI_JOB_RUNS_FIELD_MAX];
    if (length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = joined, .src = value, .bytes = length});
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
    // the last letter erased: its continuation bytes, each read as the unsigned byte it is, and its lead
    unsigned long long length = 0ULL;
    const char *const value =
        EMBED_CALL(cli_job_runs_internal.form_value, CliJobRunsCtx, .param = store->focus, .length_out = &length);
    unsigned long long left_length = length;
    while ((left_length != 0ULL) && (((unsigned char)value[left_length - 1ULL] & 0xc0u) == 0x80u))
    {
        left_length -= 1ULL;
    }
    left_length -= (left_length != 0ULL) ? 1ULL : 0ULL;
    if (left_length != length)
    {
        char shorter[CLI_JOB_RUNS_FIELD_MAX];
        if (left_length != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = shorter, .src = value, .bytes = left_length});
        }
        EMBED_CALL(form_set, FormCtx, .param = store->focus, .bytes = shorter, .length = left_length);
    }
    return 1;
}

const char *cli_job_runs_filter(const CliJobRunsFieldCfg *args)
{
    *args->length_out = g_cli_job_runs_store.filter.length;
    return (g_cli_job_runs_store.filter.bytes != NULL) ? g_cli_job_runs_store.filter.bytes : "";
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
