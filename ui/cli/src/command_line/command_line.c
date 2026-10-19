// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line.c: what the command line's parts share: a line written to the output or the errors; the terminal's
// width; a text cut to it and broken into lines at it; a word quoted for the runner's shell words; the tree's jobs read
// and a job found by the word that names it; the values a run's words give a job; and a stage job's subject
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "command_line_internal.h"

// the most params, steps, arguments, list items and bytes of text the catalog the command line reads holds
#define CLI_LINE_PARAMS 16384u
#define CLI_LINE_STEPS 16384u
#define CLI_LINE_ARGS 65536u
#define CLI_LINE_LISTS 65536u
#define CLI_LINE_CATALOG_TEXT (4u << 20u)

static CliJob s_jobs[CLI_LINE_JOBS];
static CliParam s_params[CLI_LINE_PARAMS];
static CliStep s_steps[CLI_LINE_STEPS];
static CliArg s_args[CLI_LINE_ARGS];
static unsigned int s_lists[CLI_LINE_LISTS];
static char s_text[CLI_LINE_CATALOG_TEXT];
static CliCatalog s_catalog;

void cli_out_line(const char *text, unsigned long long size)
{
    query_terminal_written(0, text, size);
    query_terminal_written(0, "\n", 1ull);
}

void cli_err_line(const char *text, unsigned long long size)
{
    query_terminal_written(1, text, size);
    query_terminal_written(1, "\n", 1ull);
}

void cli_out(const char *text)
{
    cli_out_line(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_err(const char *text)
{
    cli_err_line(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

unsigned int cli_width(void)
{
    char columns[32];
    unsigned long long width = 0ull;
    if (query_environment_read("COLUMNS", columns, sizeof(columns)))
    {
        for (const char *at = columns; (*at >= '0') && (*at <= '9') && (width < 100000ull); at += 1)
        {
            width = (width * 10ull) + (unsigned long long)(*at - '0');
        }
    }
    // the width is under 100000 here, as checked as it was read
    return (width >= 40ull) ? (unsigned int)width : 100u;
}

void cli_cut(const char *text, unsigned long long size, unsigned long long most, CliLine *out)
{
    if (cli_text_letters(text, size) <= most)
    {
        cli_line_counted(out, text, size);
        return;
    }
    const unsigned long long kept = (most > 3ull) ? most - 3ull : 0ull;
    unsigned long long at = 0ull;
    for (unsigned long long letter = 0ull; letter < kept; letter += 1ull)
    {
        unsigned long long length = 0ull;
        (void)cli_text_letter(text, size, at, &length);
        at += (length > 0ull) ? length : 1ull;
    }
    cli_line_counted(out, text, cli_text_trimmed_end(text, at));
    cli_line_text(out, "...");
}

void cli_wrap(const char *text, unsigned long long size, const char *lead, unsigned long long most,
              void (*each)(void *context, const char *line, unsigned long long size), void *context)
{
    static char s_line[1u << 16u];
    const unsigned long long lead_size = cellul.len(&(CatenaFinitaCfg){.src = lead, .cap = CLI_CELLS_POOL});
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, lead);
    unsigned long long at = 0ull;
    unsigned long long word = 0ull;
    unsigned long long length = 0ull;
    while (cli_text_word(text, size, &at, &word, &length))
    {
        const int held = (line.at > lead_size);
        if (held && (line.at + 1ull + length > most))
        {
            each(context, s_line, line.at);
            line.at = lead_size;
        }
        cli_line_text(&line, (line.at > lead_size) ? " " : "");
        cli_line_counted(&line, text + word, length);
    }
    if (line.at > lead_size)
    {
        each(context, s_line, line.at);
    }
}

int cli_holds_word(const char *text, const char *word)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL});
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    for (size_t at = 0u; at + length <= size; at += 1u)
    {
        size_t same = 0u;
        while (same < length)
        {
            const char one = text[at + same];
            const char other = word[same];
            const char lower = ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one;
            const char lower_other = ((other >= 'A') && (other <= 'Z')) ? (char)(other - 'A' + 'a') : other;
            if (lower != lower_other)
            {
                break;
            }
            same += 1u;
        }
        if (same == length)
        {
            return 1;
        }
    }
    return 0;
}

void cli_quoted_word(const char *word, CliLine *out)
{
    cli_line_character(out, '"');
    for (const char *at = word; *at != '\0'; at += 1)
    {
        if ((*at == '\\') || (*at == '"'))
        {
            cli_line_character(out, '\\');
        }
        cli_line_character(out, *at);
    }
    cli_line_character(out, '"');
}

CliCatalog *cli_command_line_catalog(const char *root)
{
    cli_catalog_begun(&s_catalog, s_jobs, CLI_LINE_JOBS, s_params, CLI_LINE_PARAMS, s_steps, CLI_LINE_STEPS, s_args,
                      CLI_LINE_ARGS, s_lists, CLI_LINE_LISTS, s_text, CLI_LINE_CATALOG_TEXT);
    cli_catalog_read(root, &s_catalog);
    return &s_catalog;
}

unsigned int cli_job_named(const CliCatalog *catalog, const unsigned int *among, unsigned int among_count,
                           const char *name, CliLine *said)
{
    const unsigned int jobs = (among != NULL) ? among_count : catalog->jobs;
    for (unsigned int one = 0u; one < jobs; one += 1u)
    {
        const unsigned int at = (among != NULL) ? among[one] : one;
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_catalog_at(catalog, catalog->job[at].id), .other = name, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    unsigned int found = CLI_CATALOG_NONE;
    unsigned int count = 0u;
    static unsigned int s_many[CLI_LINE_JOBS];
    for (unsigned int one = 0u; one < jobs; one += 1u)
    {
        const unsigned int at = (among != NULL) ? among[one] : one;
        const char *const id = cli_catalog_at(catalog, catalog->job[at].id);
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL});
        const int tail =
            (size > length) && (id[size - length - 1u] == '/') &&
            (cellul.eq(&(CatenaFinitaCfg){.src = id + size - length, .other = name, .cap = CLI_CELLS_POOL}));
        if (tail || (cellul.eq(&(CatenaFinitaCfg){
                        .src = cli_catalog_at(catalog, catalog->job[at].title), .other = name, .cap = CLI_CELLS_POOL})))
        {
            found = at;
            s_many[count] = at;
            count += 1u;
        }
    }
    if (count == 1u)
    {
        return found;
    }
    if (count == 0u)
    {
        cli_line_text(said, "no job ");
        cli_line_text(said, name);
        cli_line_text(said, ": orior list names them");
        return CLI_CATALOG_NONE;
    }
    cli_line_text(said, name);
    cli_line_text(said, " names ");
    cli_line_decimal(said, count, 1u);
    cli_line_text(said, " jobs; give one of:");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(said, "\n  ");
        cli_line_text(said, cli_catalog_at(catalog, catalog->job[s_many[at]].id));
    }
    return CLI_CATALOG_NONE;
}

// whether the job at `job` takes the param `key`: 1, or 0 with what it takes said
static int values_known(const CliCatalog *catalog, unsigned int job, const char *key, CliLine *said)
{
    const CliJob *const one = &catalog->job[job];
    for (unsigned int at = 0u; at < one->params; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = cli_catalog_at(catalog, catalog->param[one->param_first + at].key),
                                         .other = key,
                                         .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    cli_line_text(said, cli_catalog_at(catalog, one->id));
    if (one->params == 0u)
    {
        cli_line_text(said, " takes no values");
        return 0;
    }
    cli_line_text(said, " takes no ");
    cli_line_text(said, key);
    cli_line_text(said, "; it takes ");
    for (unsigned int at = 0u; at < one->params; at += 1u)
    {
        cli_line_text(said, (at > 0u) ? ", " : "");
        cli_line_text(said, cli_catalog_at(catalog, catalog->param[one->param_first + at].key));
    }
    return 0;
}

int cli_job_values(const CliCatalog *catalog, unsigned int job, const char *const *words, unsigned int count,
                   CliValues *values, CliLine *said)
{
    cli_values_begun(values);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--", .cap = CLI_CELLS_POOL}))
        {
            if (!values_known(catalog, job, "arguments", said))
            {
                return 0;
            }
            static char s_free[1u << 16u];
            CliLine free = {s_free, sizeof(s_free), 0ull};
            for (unsigned int word = at + 1u; word < count; word += 1u)
            {
                cli_line_text(&free, (word > at + 1u) ? " " : "");
                cli_quoted_word(words[word], &free);
            }
            (void)cli_line_closed(&free);
            cli_values_put(values, "arguments", s_free);
            break;
        }
        const char *const equals = cellul.chr(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL, .byte = '='});
        if (equals == NULL)
        {
            cli_line_text(said, words[at]);
            cli_line_text(said, " is not key=value; words for the job go after --");
            return 0;
        }
        char key[CLI_NAME_LONGEST];
        const size_t size = (size_t)(equals - words[at]);
        if (size >= sizeof(key))
        {
            cli_line_text(said, words[at]);
            cli_line_text(said, " is not key=value; words for the job go after --");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = key, .src = words[at], .bytes = size});
        key[size] = '\0';
        if (!values_known(catalog, job, key, said))
        {
            return 0;
        }
        cli_values_put(values, key, equals + 1);
    }
    const CliJob *const one = &catalog->job[job];
    for (unsigned int at = 0u; at < one->params; at += 1u)
    {
        const CliParam *const param = &catalog->param[one->param_first + at];
        const char *const key = cli_catalog_at(catalog, param->key);
        const char *const fallback = cli_catalog_at(catalog, param->fallback);
        unsigned int given = 0u;
        (void)cli_values_of(values, key, &given);
        if ((fallback[0] != '\0') && (given == 0u))
        {
            cli_values_put(values, key, fallback);
        }
    }
    return 1;
}

void cli_job_subject(const CliCatalog *catalog, unsigned int job, char *subject)
{
    const char *const file = cli_catalog_at(catalog, catalog->job[job].file);
    const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL, .byte = '/'});
    const char *const from = (slash != NULL) ? slash + 1 : file;
    const char *const end =
        (slash != NULL) ? cellul.chr(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL, .byte = '/'}) : NULL;
    const size_t size =
        (end != NULL) ? (size_t)(end - from) : cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
    const size_t kept = (size < CLI_NAME_LONGEST) ? size : CLI_NAME_LONGEST - 1u;
    memor.cpy(&(MemoriaCfg){.dst = subject, .src = from, .bytes = kept});
    subject[kept] = '\0';
}

int cli_command_line_tree(const char *named, char *root, CliLine *said)
{
    const int found = (named != NULL) ? cli_tree_at(named, root, said) : cli_tree_found(root);
    if (!found && (named == NULL))
    {
        cli_line_text(said, "no orior tree here: run from inside one, or name one with --root");
    }
    if (found)
    {
        cli_toolchains_set_tree(root);
    }
    return found;
}
