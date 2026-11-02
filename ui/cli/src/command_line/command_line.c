// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line.c: what the command line's parts share: a line written to the output or the errors; the terminal's
// width; a text cut to it and broken into lines at it; a word quoted for the runner's shell words; the tree's jobs read
// and a job found by the word that names it; the values a run's words give a job; and a stage job's subject
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "callback_cfg.h"
#include "command_line_internal.h"

static CliCatalog s_catalog;

void cli_out_line(const CliCommandLineTextSizeCtx *args)
{
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 0, .text = args->text,
               .length = args->size);
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 0, .text = "\n", .length = 1ull);
}

void cli_err_line(const CliCommandLineTextSizeCtx *args)
{
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = args->text,
               .length = args->size);
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = "\n", .length = 1ull);
}

void cli_out(const CliCommandLineCtx *args)
{
    const char *text = args->text;
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_err(const CliCommandLineCtx *args)
{
    const char *text = args->text;
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

unsigned int cli_width(void)
{
    char columns[32];
    unsigned long long width = 0ull;
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "COLUMNS",
                   .value_out = columns, .capacity = sizeof(columns)))
    {
        const unsigned long long read =
            EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = columns, .at = 0ull,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = columns, .cap = CLI_CELLS_POOL}), .end = NULL);
        width = (read < 100000ull) ? read : 100ull;
    }
    // the width is under 100000 here, as checked as it was read
    return (width >= 40ull) ? (unsigned int)width : 100u;
}

void cli_cut(const CliCommandLineTextSizeCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    if (EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = text, .size = size) <= args->most)
    {
        mmgr_line_put_n(args->out, text, size);
        return;
    }
    const unsigned long long displayed = (args->most > 3ull) ? args->most - 3ull : 0ull;
    unsigned long long at = 0ull;
    for (unsigned long long letter = 0ull; letter < displayed; letter += 1ull)
    {
        unsigned long long length = 0ull;
        (void)EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = size, .at = at, .length = &length);
        at += (length > 0ull) ? length : 1ull;
    }
    mmgr_line_put_n(args->out, text,
                    EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = text, .size = at));
    mmgr_line_put(args->out, "...");
}

void cli_wrap(const CliCommandLineTextCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    static char s_line[1u << 16u];
    const unsigned long long comment_prefix_size =
        cellul.len(&(CatenaFinitaCfg){.src = args->lead, .cap = CLI_CELLS_POOL});
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, args->lead);
    unsigned long long at = 0ull;
    unsigned long long word = 0ull;
    unsigned long long length = 0ull;
    while (EMBED_CALL(cli_text_size_length.word, CliTextSizeLengthCfg, .text = text, .size = size, .at_pointer = &at,
                      .word = &word, .length = &length))
    {
        const int started = (line.at > comment_prefix_size);
        if (started && (line.at + 1ull + length > args->most))
        {
            EMBED_CALL(args->each, CliEachLineSizeCfg, .context = args->context, .line = s_line, .size = line.at);
            line.at = comment_prefix_size;
        }
        mmgr_line_put(&line, (line.at > comment_prefix_size) ? " " : "");
        mmgr_line_put_n(&line, text + word, length);
    }
    if (line.at > comment_prefix_size)
    {
        EMBED_CALL(args->each, CliEachLineSizeCfg, .context = args->context, .line = s_line, .size = line.at);
    }
}

int cli_contains_word(const CliCommandLineCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_CELLS_POOL});
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    for (size_t at = 0u; at + length <= size; at += 1u)
    {
        size_t same = 0u;
        while (same < length)
        {
            const char one = args->text[at + same];
            const char other = args->word[same];
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

void cli_quoted_word(const CliCommandLineOutCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '"');
    EMBED_CALL(cli_text_size_set.append_mark, CliTextSizeSetCfg, .out = out, .text = args->word, .set = "\\\"",
               .mark = '\\');
    mmgr_line_ch(out, '"');
}

CliCatalog *cli_command_line_catalog(const CliCommandLineWordCtx *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_catalog_choice_table.free, CliCatalogChoiceCfg, .catalog = &s_catalog);
    EMBED_CALL(cli_catalog_cells.take, CliCatalogCellsCfg, .catalog = &s_catalog, .cells = &g_cli_line_catalog_cells,
               .jobs_max = CLI_LINE_JOBS);
    EMBED_CALL(cli_catalog_root.read, CliCatalogRootCfg, .root = root, .catalog = &s_catalog);
    return &s_catalog;
}

unsigned int cli_job_by_name(const CliCommandLineCatalogAmongCtx *args)
{
    const CliCatalog *catalog = args->catalog;
    const unsigned int jobs = (args->among != NULL) ? args->among_count : catalog->jobs;
    for (unsigned int one = 0u; one < jobs; one += 1u)
    {
        const unsigned int at = (args->among != NULL) ? args->among[one] : one;
        if (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                           .catalog_read = catalog, .text_u32 = catalog->job[at].id),
                                         .other = args->name,
                                         .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    unsigned int found = CLI_CATALOG_NONE;
    unsigned int count = 0u;
    static unsigned int s_many[CLI_LINE_JOBS];
    for (unsigned int one = 0u; one < jobs; one += 1u)
    {
        const unsigned int at = (args->among != NULL) ? args->among[one] : one;
        const char *const id = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                          .text_u32 = catalog->job[at].id);
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL});
        const int tail =
            (size > length) && (id[size - length - 1u] == '/') &&
            (cellul.eq(&(CatenaFinitaCfg){.src = id + size - length, .other = args->name, .cap = CLI_CELLS_POOL}));
        if (tail || (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                                    .catalog_read = catalog,
                                                                    .text_u32 = catalog->job[at].title),
                                                  .other = args->name,
                                                  .cap = CLI_CELLS_POOL})))
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
        mmgr_line_put(args->err_msg, "no job ");
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, ": orior_cli list names them");
        return CLI_CATALOG_NONE;
    }
    mmgr_line_put(args->err_msg, args->name);
    mmgr_line_put(args->err_msg, " names ");
    mmgr_line_decimal(args->err_msg, count, 1u);
    mmgr_line_put(args->err_msg, " jobs; give one of:");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(args->err_msg, "\n  ");
        mmgr_line_put(args->err_msg, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                                .text_u32 = catalog->job[s_many[at]].id));
    }
    return CLI_CATALOG_NONE;
}

typedef struct
{
    const CliCatalog *catalog;
    unsigned int job;
    const char *key;
    VerbaLinea *err_msg;
} CommandLineCtx;
// whether the job at `job` takes the param `key`: 1, or 0 with what it takes given
EMBED_INLINE int values_know(const CommandLineCtx *args)
{
    const CliCatalog *catalog = args->catalog;
    const CliJob *const one = &catalog->job[args->job];
    for (unsigned int at = 0u; at < one->params; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                           .catalog_read = catalog,
                                                           .text_u32 = catalog->param[one->param_first + at].key),
                                         .other = args->key,
                                         .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    mmgr_line_put(args->err_msg, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                            .text_u32 = one->id));
    if (one->params == 0u)
    {
        mmgr_line_put(args->err_msg, " takes no values");
        return 0;
    }
    mmgr_line_put(args->err_msg, " takes no ");
    mmgr_line_put(args->err_msg, args->key);
    mmgr_line_put(args->err_msg, "; it takes ");
    for (unsigned int at = 0u; at < one->params; at += 1u)
    {
        mmgr_line_put(args->err_msg, (at > 0u) ? ", " : "");
        mmgr_line_put(args->err_msg, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                                .text_u32 = catalog->param[one->param_first + at].key));
    }
    return 0;
}

int cli_job_values(const CliCommandLineCatalogCtx *args)
{
    const CliCatalog *catalog = args->catalog;
    unsigned int count = args->count;
    CliValues *values = args->values;
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = values);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--", .cap = CLI_CELLS_POOL}))
        {
            if (!EMBED_CALL(values_know, CommandLineCtx, .catalog = catalog, .job = args->job, .key = "arguments",
                            .err_msg = args->err_msg))
            {
                return 0;
            }
            static char s_free[1u << 16u];
            VerbaLinea free = {s_free, sizeof(s_free), 0ull};
            for (unsigned int word = at + 1u; word < count; word += 1u)
            {
                mmgr_line_put(&free, (word > at + 1u) ? " " : "");
                EMBED_CALL(cli_command_line_out_internal.quoted_word, CliCommandLineOutCtx, .word = args->words[word],
                           .out = &free);
            }
            (void)mmgr_line_finish(&free);
            EMBED_CALL(cli_catalog_key_values.values_append, CliCatalogKeyValuesCfg, .values_out = values,
                       .key = "arguments", .value = s_free);
            break;
        }
        const char *const equals =
            cellul.chr(&(CatenaFinitaCfg){.src = args->words[at], .cap = CLI_CELLS_POOL, .byte = '='});
        if (equals == NULL)
        {
            mmgr_line_put(args->err_msg, args->words[at]);
            mmgr_line_put(args->err_msg, " is not key=value; words for the job go after --");
            return 0;
        }
        char key[CLI_NAME_MAX_LEN];
        const size_t size = (size_t)(equals - args->words[at]);
        if (size >= sizeof(key))
        {
            mmgr_line_put(args->err_msg, args->words[at]);
            mmgr_line_put(args->err_msg, " is not key=value; words for the job go after --");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = key, .src = args->words[at], .bytes = size});
        key[size] = '\0';
        if (!EMBED_CALL(values_know, CommandLineCtx, .catalog = catalog, .job = args->job, .key = key,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_catalog_key_values.values_append, CliCatalogKeyValuesCfg, .values_out = values, .key = key,
                   .value = equals + 1);
    }
    const CliJob *const one = &catalog->job[args->job];
    for (unsigned int at = 0u; at < one->params; at += 1u)
    {
        const CliParam *const param = &catalog->param[one->param_first + at];
        const char *const key =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = param->key);
        const char *const fallback = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                                .text_u32 = param->fallback);
        unsigned int supplied = 0u;
        (void)EMBED_CALL(cli_catalog_key.values_get, CliCatalogKeyCfg, .values = values, .key = key,
                         .count = &supplied);
        if ((fallback[0] != '\0') && (supplied == 0u))
        {
            EMBED_CALL(cli_catalog_key_values.values_append, CliCatalogKeyValuesCfg, .values_out = values, .key = key,
                       .value = fallback);
        }
    }
    return 1;
}

void cli_job_subject(const CliCommandLineCatalogJobCtx *args)
{
    const CliCatalog *catalog = args->catalog;
    const char *const file = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                        .text_u32 = catalog->job[args->job].file);
    const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL, .byte = '/'});
    const char *const from = (slash != NULL) ? slash + 1 : file;
    const char *const end =
        (slash != NULL) ? cellul.chr(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL, .byte = '/'}) : NULL;
    const size_t size =
        (end != NULL) ? (size_t)(end - from) : cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
    const size_t copied_len = (size < CLI_NAME_MAX_LEN) ? size : CLI_NAME_MAX_LEN - 1u;
    memor.cpy(&(MemoriaCfg){.dst = args->subject, .src = from, .bytes = copied_len});
    args->subject[copied_len] = '\0';
}

int cli_command_line_tree(const CliCommandLineNamedCfg *args)
{
    char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    const int found = (args->named != NULL) ? EMBED_CALL(cli_path_err_msg_full_internal.tree_at, CliPathErrMsgFullCtx,
                                                         .path = args->named, .full = root, .err_msg = err_msg)
                                            : EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = root);
    if (!found && (args->named == NULL))
    {
        mmgr_line_put(err_msg, "no orior tree here: run from inside one, or name one with --root");
    }
    if (found)
    {
        EMBED_CALL(cli_toolchains_root.set_tree, CliToolchainsRootCfg, .root = root);
    }
    return found;
}
