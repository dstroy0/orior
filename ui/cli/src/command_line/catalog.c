// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog.c: a catalog's jobs made, found, copied and written, and the values a job is run with
#include "catalog.h"
#include "../text/json_tree.h"

void cli_catalog_begin(const CliCatalogJobsMaxCfg *args)
{
    CliJob *jobs = args->jobs;
    unsigned int jobs_max = args->jobs_max;
    CliParam *params = args->params;
    unsigned int params_max = args->params_max;
    CliStep *steps = args->steps;
    unsigned int steps_max = args->steps_max;
    CliArg *arguments = args->request;
    unsigned int args_max = args->args_max;
    unsigned int *lists = args->lists;
    unsigned int lists_max = args->lists_max;
    char *text = args->text_out;
    unsigned int text_max = args->text_max;
    args->catalog->job = jobs;
    args->catalog->jobs = 0u;
    args->catalog->jobs_max = jobs_max;
    args->catalog->param = params;
    args->catalog->params = 0u;
    args->catalog->params_max = params_max;
    args->catalog->step = steps;
    args->catalog->steps = 0u;
    args->catalog->steps_max = steps_max;
    args->catalog->arg = arguments;
    args->catalog->args = 0u;
    args->catalog->args_max = args_max;
    args->catalog->list = lists;
    args->catalog->lists = 0u;
    args->catalog->lists_max = lists_max;
    args->catalog->text = text;
    args->catalog->used = 0u;
    args->catalog->text_max = text_max;
    args->catalog->full = 0;
    args->catalog->cells = NULL;
    // the text at 0 is the empty one
    if (text_max != 0u)
    {
        text[0] = '\0';
        args->catalog->used = 1u;
    }
}

// the most jobs, params, steps, arguments and list items, and the bytes of text, of a catalog read
#define CLI_CATALOG_LIST_MAX 16384u
#define CLI_CATALOG_TEXT_MAX (4u << 20u)

void cli_catalog_alloc(const CliCatalogCellsCfg *args)
{
    CellOwner *cells = args->cells;
    unsigned int jobs_max = args->jobs_max;
    EMBED_CALL(cli_catalog_jobs_max.begin, CliCatalogJobsMaxCfg, .catalog = args->catalog, .jobs = NULL, .jobs_max = 0u,
               .params = NULL, .params_max = 0u, .steps = NULL, .steps_max = 0u, .request = NULL, .args_max = 0u,
               .lists = NULL, .lists_max = 0u, .text_out = NULL, .text_max = 0u);
    const unsigned long long job_bytes = (unsigned long long)CLI_CATALOG_LIST_MAX * sizeof(CliJob);
    const unsigned long long param_bytes = (unsigned long long)CLI_CATALOG_LIST_MAX * sizeof(CliParam);
    const unsigned long long step_bytes = (unsigned long long)CLI_CATALOG_LIST_MAX * sizeof(CliStep);
    const unsigned long long arg_bytes = (unsigned long long)CLI_CATALOG_LIST_MAX * sizeof(CliArg);
    const unsigned long long list_bytes = (unsigned long long)CLI_CATALOG_LIST_MAX * sizeof(unsigned int);
    char *const cell = g_cli_cells->persistent_buf_alloc(
        job_bytes + param_bytes + step_bytes + arg_bytes + list_bytes + CLI_CATALOG_TEXT_MAX, cells);
    args->catalog->cells = cells;
    args->catalog->full = cell == NULL;
    if (cell == NULL)
    {
        return;
    }
    args->catalog->job = (CliJob *)(void *)cell;
    args->catalog->jobs_max = (jobs_max < CLI_CATALOG_LIST_MAX) ? jobs_max : CLI_CATALOG_LIST_MAX;
    args->catalog->param = (CliParam *)(void *)(cell + job_bytes);
    args->catalog->params_max = CLI_CATALOG_LIST_MAX;
    args->catalog->step = (CliStep *)(void *)(cell + job_bytes + param_bytes);
    args->catalog->steps_max = CLI_CATALOG_LIST_MAX;
    args->catalog->arg = (CliArg *)(void *)(cell + job_bytes + param_bytes + step_bytes);
    args->catalog->args_max = CLI_CATALOG_LIST_MAX;
    args->catalog->list = (unsigned int *)(void *)(cell + job_bytes + param_bytes + step_bytes + arg_bytes);
    args->catalog->lists_max = CLI_CATALOG_LIST_MAX;
    args->catalog->text = cell + job_bytes + param_bytes + step_bytes + arg_bytes + list_bytes;
    args->catalog->text_max = CLI_CATALOG_TEXT_MAX;
    // the text at 0 is the empty one
    args->catalog->text[0] = '\0';
    args->catalog->used = 1u;
}

void cli_catalog_free(const CliCatalogChoiceCfg *args)
{
    CellOwner *const cells = args->catalog->cells;
    if (cells != NULL)
    {
        g_cli_cells->persistent_buf_release(args->catalog->job, cells);
    }
    EMBED_CALL(cli_catalog_jobs_max.begin, CliCatalogJobsMaxCfg, .catalog = args->catalog, .jobs = NULL, .jobs_max = 0u,
               .params = NULL, .params_max = 0u, .steps = NULL, .steps_max = 0u, .request = NULL, .args_max = 0u,
               .lists = NULL, .lists_max = 0u, .text_out = NULL, .text_max = 0u);
}

unsigned int cli_catalog_text(const CliCatalogTextProgramCfg *args)
{
    if (args->text == NULL)
    {
        return CLI_CATALOG_NONE;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((args->catalog->used + size) > args->catalog->text_max)
    {
        args->catalog->full = 1;
        return 0u;
    }
    const unsigned int at = args->catalog->used;
    memor.cpy(&(MemoriaCfg){.dst = args->catalog->text + at, .src = args->text, .bytes = size});
    // the texts are below the capacity handed, which an unsigned int counts
    args->catalog->used += (unsigned int)size;
    return at;
}

const char *cli_catalog_at(const CliCatalogTextCfg *args)
{
    return ((args->text_u32 == CLI_CATALOG_NONE) || (args->text_u32 >= args->catalog_read->used))
               ? ""
               : (args->catalog_read->text + args->text_u32);
}

unsigned int cli_catalog_job(const CliCatalogIdGroupCfg *args)
{
    const char *id = args->id;
    const char *group = args->group;
    const char *title = args->title;
    const char *file = args->file;
    const char *about = args->about;
    const char *opens = args->opens;
    if (args->catalog->jobs >= args->catalog->jobs_max)
    {
        args->catalog->full = 1;
        return CLI_CATALOG_NONE;
    }
    CliJob *const job = &args->catalog->job[args->catalog->jobs];
    job->id = EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog, .text = id);
    job->group = group;
    job->title =
        EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog, .text = title);
    job->file =
        EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog, .text = file);
    job->about =
        EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog, .text = about);
    job->param_first = args->catalog->params;
    job->params = 0u;
    job->opens = opens;
    job->step_first = args->catalog->steps;
    job->steps = 0u;
    args->catalog->jobs += 1u;
    return args->catalog->jobs - 1u;
}

void cli_catalog_param(const CliCatalogKeyKindCfg *args)
{
    if ((args->catalog->jobs == 0u) || (args->catalog->params >= args->catalog->params_max))
    {
        args->catalog->full = 1;
        return;
    }
    const CliParam param = {EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg,
                                       .catalog = args->catalog, .text = args->key),
                            args->kind,
                            args->catalog->lists,
                            0u,
                            0u,
                            args->required};
    args->catalog->param[args->catalog->params] = param;
    args->catalog->params += 1u;
    args->catalog->job[args->catalog->jobs - 1u].params += 1u;
}

typedef struct
{
    CliCatalog *catalog;
    unsigned int text;
    const CliCatalog *from;
    VerbaLinea *out;
    const char *text_text;
} CatalogCtx;
// a text put after the lists
EMBED_INLINE void cli_on_list(const CatalogCtx *args)
{
    if (args->catalog->lists >= args->catalog->lists_max)
    {
        args->catalog->full = 1;
        return;
    }
    args->catalog->list[args->catalog->lists] = args->text;
    args->catalog->lists += 1u;
}

// A choice of a choice's param is its default where it is the first
void cli_catalog_choice(const CliCatalogChoiceCfg *args)
{
    if (args->catalog->params == 0u)
    {
        args->catalog->full = 1;
        return;
    }
    CliParam *const param = &args->catalog->param[args->catalog->params - 1u];
    const unsigned int text = EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg,
                                         .catalog = args->catalog, .text = args->choice);
    if ((param->choices == 0u) &&
        (cellul.starts(&(CatenaFinitaCfg){.src = param->kind, .other = "choice", .cap = CLI_CELLS_POOL})))
    {
        param->fallback = text;
    }
    EMBED_CALL(cli_on_list, CatalogCtx, .catalog = args->catalog, .text = text);
    param->choices += 1u;
}

void cli_catalog_step(const CliCatalogTextProgramCfg *args)
{
    if ((args->catalog->jobs == 0u) || (args->catalog->steps >= args->catalog->steps_max))
    {
        args->catalog->full = 1;
        return;
    }
    const CliStep step = {args->program,
                          cli_catalog_text_program.text(args),
                          EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog,
                                     .text = args->tool_program),
                          args->system,
                          args->catalog->args,
                          0u};
    args->catalog->step[args->catalog->steps] = step;
    args->catalog->steps += 1u;
    args->catalog->job[args->catalog->jobs - 1u].steps += 1u;
}

void cli_catalog_arg(const CliCatalogKindCfg *args)
{
    if ((args->catalog->steps == 0u) || (args->catalog->args >= args->catalog->args_max))
    {
        args->catalog->full = 1;
        return;
    }
    const CliArg arg = {
        args->kind_u32,
        {EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog, .text = args->a),
         EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog, .text = args->b),
         EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog, .text = args->c),
         EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog,
                    .text = args->d)},
        args->separated,
        args->catalog->lists,
        0u};
    args->catalog->arg[args->catalog->args] = arg;
    args->catalog->args += 1u;
    args->catalog->step[args->catalog->steps - 1u].args += 1u;
}

void cli_catalog_word(const CliCatalogWordCfg *args)
{
    if (args->catalog->args == 0u)
    {
        args->catalog->full = 1;
        return;
    }
    EMBED_CALL(cli_on_list, CatalogCtx, .catalog = args->catalog,
               .text = EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->catalog,
                                  .text = args->word));
    args->catalog->arg[args->catalog->args - 1u].words += 1u;
}

void cli_catalog_words(const CliCatalogChoiceCfg *args)
{
    EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = args->catalog, .key = "arguments",
               .kind = "words", .required = 0);
}

void cli_catalog_trace(const CliCatalogChoiceCfg *args)
{
    EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = args->catalog, .key = CLI_TRACE,
               .kind = "choice", .required = 1);
    EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = args->catalog,
               .choice = CLI_TRACE_VERBOSE);
    EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = args->catalog,
               .choice = CLI_TRACE_QUIET);
}

unsigned int cli_catalog_find(const CliCatalogIdCfg *args)
{
    for (unsigned int at = 0u; at < args->catalog_read->jobs; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                           .catalog_read = args->catalog_read,
                                                           .text_u32 = args->catalog_read->job[at].id),
                                         .other = args->id,
                                         .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    return CLI_CATALOG_NONE;
}

// a text of `from` stored in `to`
EMBED_INLINE const char *cli_from(const CatalogCtx *args)
{
    unsigned int text = args->text;
    return (text == CLI_CATALOG_NONE)
               ? NULL
               : EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = args->from, .text_u32 = text);
}

unsigned int cli_catalog_copy(const CliCatalogFromCfg *args)
{
    const CliJob *const job = &args->from->job[args->index];
    const unsigned int made = EMBED_CALL(
        cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = args->to,
        .id = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = job->id), .group = job->group,
        .title = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = job->title),
        .file = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = job->file),
        .about = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = job->about), .opens = job->opens);
    for (unsigned int at = 0u; (made != CLI_CATALOG_NONE) && (at < job->params); at += 1u)
    {
        const CliParam *const param = &args->from->param[job->param_first + at];
        EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = args->to,
                   .key = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = param->key), .kind = param->kind,
                   .required = param->required);
        for (unsigned int each = 0u; each < param->choices; each += 1u)
        {
            EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = args->to,
                       .choice = EMBED_CALL(cli_from, CatalogCtx, .from = args->from,
                                            .text = args->from->list[param->first + each]));
        }
        if (args->to->params != 0u)
        {
            args->to->param[args->to->params - 1u].fallback =
                EMBED_CALL(cli_catalog_text_program.text, CliCatalogTextProgramCfg, .catalog = args->to,
                           .text = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = param->fallback));
        }
    }
    for (unsigned int at = 0u; (made != CLI_CATALOG_NONE) && (at < job->steps); at += 1u)
    {
        const CliStep *const step = &args->from->step[job->step_first + at];
        EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = args->to,
                   .program = step->program,
                   .text = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = step->text),
                   .tool_program = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = step->tool_program),
                   .system = step->system);
        for (unsigned int each = 0u; each < step->args; each += 1u)
        {
            const CliArg *const arg = &args->from->arg[step->first + each];
            EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->to, .kind_u32 = arg->kind,
                       .a = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = arg->text[0]),
                       .b = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = arg->text[1]),
                       .c = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = arg->text[2]),
                       .d = EMBED_CALL(cli_from, CatalogCtx, .from = args->from, .text = arg->text[3]),
                       .separated = arg->separated);
            for (unsigned int word = 0u; word < arg->words; word += 1u)
            {
                EMBED_CALL(cli_catalog_word_table.word, CliCatalogWordCfg, .catalog = args->to,
                           .word = EMBED_CALL(cli_from, CatalogCtx, .from = args->from,
                                              .text = args->from->list[arg->first + word]));
            }
        }
    }
    return args->to->full ? CLI_CATALOG_NONE : made;
}

// the bytes a text of `from` takes stored again, its 0 among them, none where it is none
EMBED_INLINE unsigned long long cli_stored_size(const CatalogCtx *args)
{
    unsigned int text = args->text;
    return (text == CLI_CATALOG_NONE)
               ? 0ull
               : (cellul.len(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                                  .catalog_read = args->from, .text_u32 = text),
                                                .cap = CLI_CELLS_POOL}) +
                  1ull);
}

void cli_catalog_job_extent(const CliCatalogFromIndexCfg *args)
{
    CliCatalogExtent *extent = args->extent;
    const CliJob *const job = &args->from->job[args->index];
    *extent = (CliCatalogExtent){job->params, job->steps, 0u, 0u, 16ull};
    extent->text += EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from, .text = job->id) +
                    EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from, .text = job->title) +
                    EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from, .text = job->file) +
                    EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from, .text = job->about);
    for (unsigned int at = 0u; at < job->params; at += 1u)
    {
        const CliParam *const param = &args->from->param[job->param_first + at];
        extent->text += EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from, .text = param->key) +
                        EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from, .text = param->fallback);
        extent->lists += param->choices;
        for (unsigned int each = 0u; each < param->choices; each += 1u)
        {
            extent->text += EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from,
                                       .text = args->from->list[param->first + each]);
        }
    }
    for (unsigned int at = 0u; at < job->steps; at += 1u)
    {
        const CliStep *const step = &args->from->step[job->step_first + at];
        extent->text += EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from, .text = step->text) +
                        EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from, .text = step->tool_program);
        extent->args += step->args;
        for (unsigned int each = 0u; each < step->args; each += 1u)
        {
            const CliArg *const arg = &args->from->arg[step->first + each];
            for (unsigned int part = 0u; part < 4u; part += 1u)
            {
                extent->text += EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from, .text = arg->text[part]);
            }
            extent->lists += arg->words;
            for (unsigned int word = 0u; word < arg->words; word += 1u)
            {
                extent->text += EMBED_CALL(cli_stored_size, CatalogCtx, .from = args->from,
                                           .text = args->from->list[arg->first + word]);
            }
        }
    }
}

void cli_values_begin(const CliCatalogKeyValuesCfg *args)
{
    args->values_out->keys = 0u;
    args->values_out->items = 0u;
    args->values_out->used = 0u;
}

void cli_values_append(const CliCatalogKeyValuesCfg *args)
{
    unsigned int at = 0u;
    while (
        (at < args->values_out->keys) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->values_out->key[at], .other = args->key, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_CELLS_POOL}) + 1ull;
    if (((at == args->values_out->keys) && (args->values_out->keys >= CLI_VALUES_KEYS)) ||
        (args->values_out->items >= CLI_VALUES_ITEMS) || ((args->values_out->used + size) > CLI_VALUES_TEXT))
    {
        return;
    }
    if (at == args->values_out->keys)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->values_out->key[at],
                                .src = args->key,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}) + 1u});
        args->values_out->first[at] = args->values_out->items;
        args->values_out->count[at] = 0u;
        args->values_out->keys += 1u;
    }
    // a key's values stand together: those of the keys after it are moved one along
    const unsigned int place = args->values_out->first[at] + args->values_out->count[at];
    for (unsigned int item = args->values_out->items; item > place; item -= 1u)
    {
        args->values_out->item[item] = args->values_out->item[item - 1u];
    }
    for (unsigned int other = 0u; other < args->values_out->keys; other += 1u)
    {
        args->values_out->first[other] += ((other != at) && (args->values_out->first[other] >= place)) ? 1u : 0u;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->values_out->text + args->values_out->used, .src = args->value, .bytes = size});
    args->values_out->item[place] = args->values_out->used;
    // the texts are below CLI_VALUES_TEXT
    args->values_out->used += (unsigned int)size;
    args->values_out->items += 1u;
    args->values_out->count[at] += 1u;
}

const unsigned int *cli_values_get(const CliCatalogKeyCfg *args)
{
    unsigned int *count = args->count;
    for (unsigned int at = 0u; at < args->values->keys; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->values->key[at], .other = args->key, .cap = CLI_CELLS_POOL}))
        {
            *count = args->values->count[at];
            return args->values->item + args->values->first[at];
        }
    }
    *count = 0u;
    return NULL;
}

const char *cli_values_first(const CliCatalogKeyCfg *args)
{
    unsigned int count = 0u;
    const unsigned int *const items = EMBED_CALL(cli_catalog_key.values_get, CliCatalogKeyCfg, .values = args->values,
                                                 .key = args->key, .count = &count);
    return ((items != NULL) && (count != 0u)) ? (args->values->text + items[0]) : NULL;
}

const char *cli_values_at(const CliCatalogTextValuesCfg *args)
{
    return args->values->text + args->text_u32;
}
