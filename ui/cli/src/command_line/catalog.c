// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog.c: a catalog's jobs made, found, copied and written, and the values a job is run with
#include "catalog.h"
#include "../text/json_tree.h"

void cli_catalog_begun(CliCatalog *catalog, CliJob *jobs, unsigned int jobs_most, CliParam *params,
                       unsigned int params_most, CliStep *steps, unsigned int steps_most, CliArg *args,
                       unsigned int args_most, unsigned int *lists, unsigned int lists_most, char *text,
                       unsigned int text_most)
{
    catalog->job = jobs;
    catalog->jobs = 0u;
    catalog->jobs_most = jobs_most;
    catalog->param = params;
    catalog->params = 0u;
    catalog->params_most = params_most;
    catalog->step = steps;
    catalog->steps = 0u;
    catalog->steps_most = steps_most;
    catalog->arg = args;
    catalog->args = 0u;
    catalog->args_most = args_most;
    catalog->list = lists;
    catalog->lists = 0u;
    catalog->lists_most = lists_most;
    catalog->text = text;
    catalog->used = 0u;
    catalog->text_most = text_most;
    catalog->full = 0;
    // the text at 0 is the empty one
    if (text_most != 0u)
    {
        text[0] = '\0';
        catalog->used = 1u;
    }
}

unsigned int cli_catalog_text(CliCatalog *catalog, const char *text)
{
    if (text == NULL)
    {
        return CLI_CATALOG_NONE;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((catalog->used + size) > catalog->text_most)
    {
        catalog->full = 1;
        return 0u;
    }
    const unsigned int at = catalog->used;
    memor.cpy(&(MemoriaCfg){.dst = catalog->text + at, .src = text, .bytes = size});
    // the texts are below the room handed, which an unsigned int counts
    catalog->used += (unsigned int)size;
    return at;
}

const char *cli_catalog_at(const CliCatalog *catalog, unsigned int text)
{
    return ((text == CLI_CATALOG_NONE) || (text >= catalog->used)) ? "" : (catalog->text + text);
}

unsigned int cli_catalog_job(CliCatalog *catalog, const char *id, const char *group, const char *title,
                             const char *file, const char *about, const char *opens)
{
    if (catalog->jobs >= catalog->jobs_most)
    {
        catalog->full = 1;
        return CLI_CATALOG_NONE;
    }
    CliJob *const job = &catalog->job[catalog->jobs];
    job->id = cli_catalog_text(catalog, id);
    job->group = group;
    job->title = cli_catalog_text(catalog, title);
    job->file = cli_catalog_text(catalog, file);
    job->about = cli_catalog_text(catalog, about);
    job->param_first = catalog->params;
    job->params = 0u;
    job->opens = opens;
    job->step_first = catalog->steps;
    job->steps = 0u;
    catalog->jobs += 1u;
    return catalog->jobs - 1u;
}

void cli_catalog_param(CliCatalog *catalog, const char *key, const char *kind, int required)
{
    if ((catalog->jobs == 0u) || (catalog->params >= catalog->params_most))
    {
        catalog->full = 1;
        return;
    }
    const CliParam param = {cli_catalog_text(catalog, key), kind, catalog->lists, 0u, 0u, required};
    catalog->param[catalog->params] = param;
    catalog->params += 1u;
    catalog->job[catalog->jobs - 1u].params += 1u;
}

// a text put after the lists
static void cli_listed(CliCatalog *catalog, unsigned int text)
{
    if (catalog->lists >= catalog->lists_most)
    {
        catalog->full = 1;
        return;
    }
    catalog->list[catalog->lists] = text;
    catalog->lists += 1u;
}

// A choice of a choice's param is its default where it is the first
void cli_catalog_choice(CliCatalog *catalog, const char *choice)
{
    if (catalog->params == 0u)
    {
        catalog->full = 1;
        return;
    }
    CliParam *const param = &catalog->param[catalog->params - 1u];
    const unsigned int text = cli_catalog_text(catalog, choice);
    if ((param->choices == 0u) &&
        (cellul.starts(&(CatenaFinitaCfg){.src = param->kind, .other = "choice", .cap = CLI_CELLS_POOL})))
    {
        param->fallback = text;
    }
    cli_listed(catalog, text);
    param->choices += 1u;
}

void cli_catalog_step(CliCatalog *catalog, unsigned int program, const char *text, const char *tool_program, int system)
{
    if ((catalog->jobs == 0u) || (catalog->steps >= catalog->steps_most))
    {
        catalog->full = 1;
        return;
    }
    const CliStep step = {
        program, cli_catalog_text(catalog, text), cli_catalog_text(catalog, tool_program), system, catalog->args, 0u};
    catalog->step[catalog->steps] = step;
    catalog->steps += 1u;
    catalog->job[catalog->jobs - 1u].steps += 1u;
}

void cli_catalog_arg(CliCatalog *catalog, unsigned int kind, const char *a, const char *b, const char *c, const char *d,
                     int separated)
{
    if ((catalog->steps == 0u) || (catalog->args >= catalog->args_most))
    {
        catalog->full = 1;
        return;
    }
    const CliArg arg = {kind,
                        {cli_catalog_text(catalog, a), cli_catalog_text(catalog, b), cli_catalog_text(catalog, c),
                         cli_catalog_text(catalog, d)},
                        separated,
                        catalog->lists,
                        0u};
    catalog->arg[catalog->args] = arg;
    catalog->args += 1u;
    catalog->step[catalog->steps - 1u].args += 1u;
}

void cli_catalog_word(CliCatalog *catalog, const char *word)
{
    if (catalog->args == 0u)
    {
        catalog->full = 1;
        return;
    }
    cli_listed(catalog, cli_catalog_text(catalog, word));
    catalog->arg[catalog->args - 1u].words += 1u;
}

void cli_catalog_words(CliCatalog *catalog)
{
    cli_catalog_param(catalog, "arguments", "words", 0);
}

void cli_catalog_trace(CliCatalog *catalog)
{
    cli_catalog_param(catalog, CLI_TRACE, "choice", 1);
    cli_catalog_choice(catalog, CLI_TRACE_VERBOSE);
    cli_catalog_choice(catalog, CLI_TRACE_QUIET);
}

unsigned int cli_catalog_find(const CliCatalog *catalog, const char *id)
{
    for (unsigned int at = 0u; at < catalog->jobs; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_catalog_at(catalog, catalog->job[at].id), .other = id, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    return CLI_CATALOG_NONE;
}

// a text of `from` kept in `to`
static const char *cli_from(const CliCatalog *from, unsigned int text)
{
    return (text == CLI_CATALOG_NONE) ? NULL : cli_catalog_at(from, text);
}

unsigned int cli_catalog_copy(const CliCatalog *from, unsigned int index, CliCatalog *to)
{
    const CliJob *const job = &from->job[index];
    const unsigned int made = cli_catalog_job(to, cli_from(from, job->id), job->group, cli_from(from, job->title),
                                              cli_from(from, job->file), cli_from(from, job->about), job->opens);
    for (unsigned int at = 0u; (made != CLI_CATALOG_NONE) && (at < job->params); at += 1u)
    {
        const CliParam *const param = &from->param[job->param_first + at];
        cli_catalog_param(to, cli_from(from, param->key), param->kind, param->required);
        for (unsigned int each = 0u; each < param->choices; each += 1u)
        {
            cli_catalog_choice(to, cli_from(from, from->list[param->first + each]));
        }
        if (to->params != 0u)
        {
            to->param[to->params - 1u].fallback = cli_catalog_text(to, cli_from(from, param->fallback));
        }
    }
    for (unsigned int at = 0u; (made != CLI_CATALOG_NONE) && (at < job->steps); at += 1u)
    {
        const CliStep *const step = &from->step[job->step_first + at];
        cli_catalog_step(to, step->program, cli_from(from, step->text), cli_from(from, step->tool_program),
                         step->system);
        for (unsigned int each = 0u; each < step->args; each += 1u)
        {
            const CliArg *const arg = &from->arg[step->first + each];
            cli_catalog_arg(to, arg->kind, cli_from(from, arg->text[0]), cli_from(from, arg->text[1]),
                            cli_from(from, arg->text[2]), cli_from(from, arg->text[3]), arg->separated);
            for (unsigned int word = 0u; word < arg->words; word += 1u)
            {
                cli_catalog_word(to, cli_from(from, from->list[arg->first + word]));
            }
        }
    }
    return to->full ? CLI_CATALOG_NONE : made;
}

// the bytes a text of `from` takes kept again, its 0 among them, none where it is none
static unsigned long long cli_kept_size(const CliCatalog *from, unsigned int text)
{
    return (text == CLI_CATALOG_NONE)
               ? 0ull
               : (cellul.len(&(CatenaFinitaCfg){.src = cli_catalog_at(from, text), .cap = CLI_CELLS_POOL}) + 1ull);
}

void cli_catalog_job_extent(const CliCatalog *from, unsigned int index, CliCatalogExtent *extent)
{
    const CliJob *const job = &from->job[index];
    *extent = (CliCatalogExtent){job->params, job->steps, 0u, 0u, 16ull};
    extent->text += cli_kept_size(from, job->id) + cli_kept_size(from, job->title) + cli_kept_size(from, job->file) +
                    cli_kept_size(from, job->about);
    for (unsigned int at = 0u; at < job->params; at += 1u)
    {
        const CliParam *const param = &from->param[job->param_first + at];
        extent->text += cli_kept_size(from, param->key) + cli_kept_size(from, param->fallback);
        extent->lists += param->choices;
        for (unsigned int each = 0u; each < param->choices; each += 1u)
        {
            extent->text += cli_kept_size(from, from->list[param->first + each]);
        }
    }
    for (unsigned int at = 0u; at < job->steps; at += 1u)
    {
        const CliStep *const step = &from->step[job->step_first + at];
        extent->text += cli_kept_size(from, step->text) + cli_kept_size(from, step->tool_program);
        extent->args += step->args;
        for (unsigned int each = 0u; each < step->args; each += 1u)
        {
            const CliArg *const arg = &from->arg[step->first + each];
            for (unsigned int part = 0u; part < 4u; part += 1u)
            {
                extent->text += cli_kept_size(from, arg->text[part]);
            }
            extent->lists += arg->words;
            for (unsigned int word = 0u; word < arg->words; word += 1u)
            {
                extent->text += cli_kept_size(from, from->list[arg->first + word]);
            }
        }
    }
}

// a text written as a JSON string
static void cli_quoted(CliLine *out, const char *text)
{
    cli_json_quoted(out, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_catalog_job_written(const CliCatalog *catalog, unsigned int index, CliLine *out)
{
    const CliJob *const job = &catalog->job[index];
    cli_line_text(out, "{\"id\":");
    cli_quoted(out, cli_catalog_at(catalog, job->id));
    cli_line_text(out, ",\"group\":");
    cli_quoted(out, job->group);
    cli_line_text(out, ",\"title\":");
    cli_quoted(out, cli_catalog_at(catalog, job->title));
    cli_line_text(out, ",\"file\":");
    cli_quoted(out, cli_catalog_at(catalog, job->file));
    cli_line_text(out, ",\"about\":");
    cli_quoted(out, cli_catalog_at(catalog, job->about));
    cli_line_text(out, ",\"params\":[");
    for (unsigned int at = 0u; at < job->params; at += 1u)
    {
        const CliParam *const param = &catalog->param[job->param_first + at];
        cli_line_text(out, (at != 0u) ? ",{\"key\":" : "{\"key\":");
        cli_quoted(out, cli_catalog_at(catalog, param->key));
        cli_line_text(out, ",\"kind\":");
        cli_quoted(out, param->kind);
        cli_line_text(out, ",\"choices\":[");
        for (unsigned int each = 0u; each < param->choices; each += 1u)
        {
            cli_line_text(out, (each != 0u) ? "," : "");
            cli_quoted(out, cli_catalog_at(catalog, catalog->list[param->first + each]));
        }
        cli_line_text(out, "],\"default\":");
        cli_quoted(out, cli_catalog_at(catalog, param->fallback));
        cli_line_text(out, param->required ? ",\"required\":true}" : ",\"required\":false}");
    }
    cli_line_text(out, "],\"opens\":");
    cli_quoted(out, job->opens);
    cli_line_character(out, '}');
}

void cli_values_begun(CliValues *values)
{
    values->keys = 0u;
    values->items = 0u;
    values->used = 0u;
}

void cli_values_put(CliValues *values, const char *key, const char *value)
{
    unsigned int at = 0u;
    while ((at < values->keys) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = values->key[at], .other = key, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1ull;
    if (((at == values->keys) && (values->keys >= CLI_VALUES_KEYS)) || (values->items >= CLI_VALUES_ITEMS) ||
        ((values->used + size) > CLI_VALUES_TEXT))
    {
        return;
    }
    if (at == values->keys)
    {
        memor.cpy(&(MemoriaCfg){.dst = values->key[at],
                                .src = key,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
        values->first[at] = values->items;
        values->count[at] = 0u;
        values->keys += 1u;
    }
    // a key's values stand together: those of the keys after it are moved one along
    const unsigned int place = values->first[at] + values->count[at];
    for (unsigned int item = values->items; item > place; item -= 1u)
    {
        values->item[item] = values->item[item - 1u];
    }
    for (unsigned int other = 0u; other < values->keys; other += 1u)
    {
        values->first[other] += ((other != at) && (values->first[other] >= place)) ? 1u : 0u;
    }
    memor.cpy(&(MemoriaCfg){.dst = values->text + values->used, .src = value, .bytes = size});
    values->item[place] = values->used;
    // the texts are below CLI_VALUES_TEXT
    values->used += (unsigned int)size;
    values->items += 1u;
    values->count[at] += 1u;
}

const unsigned int *cli_values_of(const CliValues *values, const char *key, unsigned int *count)
{
    for (unsigned int at = 0u; at < values->keys; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = values->key[at], .other = key, .cap = CLI_CELLS_POOL}))
        {
            *count = values->count[at];
            return values->item + values->first[at];
        }
    }
    *count = 0u;
    return NULL;
}

const char *cli_values_first(const CliValues *values, const char *key)
{
    unsigned int count = 0u;
    const unsigned int *const items = cli_values_of(values, key, &count);
    return ((items != NULL) && (count != 0u)) ? (values->text + items[0]) : NULL;
}

const char *cli_values_at(const CliValues *values, unsigned int text)
{
    return values->text + text;
}
