// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_command.c: a step's arguments made from the values given, its program found, its environment set, and the
// line the window shows for it, each setting leading it as a shell writes it
#include "../code/git.h"
#include "../command_line/callback_cfg.h"
#include "../programs/executables.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "runner.h"

typedef struct
{
    const CliValues *values;
    const char *key;
    const char *value;
    char *out;
    const CliCatalog *catalog;
    const CliStep *step;
    CliWords *words;
    VerbaLinea *err_msg;
    CliCommand *command;
    VerbaLinea *set;
    const char *name;
    const char *root;
    VerbaLinea *moved;
    VerbaLinea *displayed;
    const char *word_text;
    unsigned long long length;
    int *first;
    int separated;
    const CliArg *arg;
    char *displayed_out;
} RunnerCommandCtx;

// the first value of `key` that is not empty, NULL where it has none
EMBED_INLINE const char *cli_first(const RunnerCommandCtx *args)
{
    const CliValues *values = args->values;
    const char *key = args->key;
    const char *const first = EMBED_CALL(cli_catalog_key.values_first, CliCatalogKeyCfg, .values = values, .key = key);
    return ((first != NULL) && (first[0] != '\0')) ? first : NULL;
}

// A path given to a bash script, written with forward slashes, which Git's bash reads on Windows and every other bash
// reads already, into `out`, sized for CLI_PATH_MAX_LEN * 4
EMBED_INLINE const char *cli_for_bash(const RunnerCommandCtx *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->out,
                            .src = args->value,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_POOL_BYTES}) + 1u});
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->out,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->out, .cap = CLI_POOL_BYTES}), .from = '\\',
               .to = '/');
#endif
    return args->out;
}

// for each value given of `key` that is not empty, the word `word_text` and the value put into `words`
EMBED_INLINE void cli_arg_each(const RunnerCommandCtx *args)
{
    unsigned int count = 0u;
    const unsigned int *const items = EMBED_CALL(cli_catalog_key.values_get, CliCatalogKeyCfg, .values = args->values,
                                                 .key = args->key, .count = &count);
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        const char *const item = EMBED_CALL(cli_catalog_text_values.values_at, CliCatalogTextValuesCfg,
                                            .values = args->values, .text_u32 = items[each]);
        if (item[0] != '\0')
        {
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                       .word_text = args->word_text);
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                       .word_text = item);
        }
    }
}

// the first value given of `key` split into words as a shell splits them, each put into `words`, a `--` before them
// where `separated`: 1, or 0 with why given where the value does not split
EMBED_INLINE int cli_arg_words(const RunnerCommandCtx *args)
{
    static CliWords s_split;
    s_split.count = 0u;
    s_split.used = 0u;
    s_split.full = 0;
    const char *const text = EMBED_CALL(cli_first, RunnerCommandCtx, .values = args->values, .key = args->key);
    if (!EMBED_CALL(cli_runner_text.shell_words, CliRunnerTextCfg, .text = (text != NULL) ? text : "",
                    .words = &s_split, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (args->separated && (s_split.count != 0u))
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "--");
    }
    for (unsigned int each = 0u; each < s_split.count; each += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = s_split.word_text[each]);
    }
    return 1;
}

// the words `arg` lists put into `words` where the first value given of `key` is `value`
EMBED_INLINE void cli_arg_when(const RunnerCommandCtx *args)
{
    const char *const given = EMBED_CALL(cli_first, RunnerCommandCtx, .values = args->values, .key = args->key);
    if ((given == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = given, .other = args->value, .cap = CLI_POOL_BYTES}))
    {
        return;
    }
    for (unsigned int each = 0u; each < args->arg->words; each += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = args->catalog,
                                           .text_u32 = args->catalog->list[args->arg->first + each]));
    }
}

// the word `value` put into `words`, each {key} in it filled with the first value given of that key, the keys taken
// in turn
EMBED_INLINE void cli_arg_format(const RunnerCommandCtx *args)
{
    const CliValues *const values = args->values;
    static char s_filled[2][CLI_PATH_MAX_LEN * 4u];
    unsigned int now = 0u;
    memor.cpy(&(MemoriaCfg){.dst = s_filled[now],
                            .src = args->value,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_POOL_BYTES}) + 1u});
    for (unsigned int key = 0u; key < values->keys; key += 1u)
    {
        char marker[CLI_NAME_MAX_LEN + 2u];
        VerbaLinea named = {marker, sizeof(marker), 0ULL};
        mmgr_line_ch(&named, '{');
        mmgr_line_put(&named, values->key[key]);
        mmgr_line_ch(&named, '}');
        const unsigned long long marker_length = mmgr_line_finish(&named);
        const char *const supplied = (values->count[key] != 0u)
                                         ? EMBED_CALL(cli_catalog_text_values.values_at, CliCatalogTextValuesCfg,
                                                      .values = values, .text_u32 = values->item[values->first[key]])
                                         : "";
        VerbaLinea next = {s_filled[1u - now], CLI_PATH_MAX_LEN * 4u, 0ULL};
        const char *rest = s_filled[now];
        for (const char *found = cellul.find(
                 &(CatenaFinitaCfg){.src = rest, .cap = CLI_POOL_BYTES, .other = marker, .other_cap = CLI_POOL_BYTES});
             found != NULL; found = cellul.find(&(CatenaFinitaCfg){
                                .src = rest, .cap = CLI_POOL_BYTES, .other = marker, .other_cap = CLI_POOL_BYTES}))
        {
            mmgr_line_put_n(&next, rest, (unsigned long long)(found - rest));
            mmgr_line_put(&next, supplied);
            rest = found + marker_length;
        }
        mmgr_line_put(&next, rest);
        mmgr_line_finish(&next);
        now = 1u - now;
    }
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = s_filled[now]);
}

// The words of a step's arguments put into `words`: 1, or 0 with why given
EMBED_INLINE int cli_step_arguments(const RunnerCommandCtx *args)
{
    const CliCatalog *catalog = args->catalog;
    const CliValues *values = args->values;
    CliWords *words = args->words;
    VerbaLinea *err_msg = args->err_msg;
    for (unsigned int at = 0u; at < args->step->args; at += 1u)
    {
        const CliArg *const arg = &catalog->arg[args->step->first + at];
        const char *const a =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = arg->text[0]);
        const char *const b =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = arg->text[1]);
        const char *const c =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = arg->text[2]);
        switch (arg->type)
        {
        case CLI_ARG_LIT:
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                       .word_text = a);
            break;
        case CLI_ARG_VALUE:
            if (EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = a) != NULL)
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word_text = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = a));
            }
            break;
        case CLI_ARG_FLAG:
            if (EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b) != NULL)
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word_text = a);
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word_text = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b));
            }
            break;
        case CLI_ARG_EACH:
            EMBED_CALL(cli_arg_each, RunnerCommandCtx, .values = values, .words = words, .key = b, .word_text = a);
            break;
        case CLI_ARG_WORDS:
            if (!EMBED_CALL(cli_arg_words, RunnerCommandCtx, .values = values, .words = words, .key = a,
                            .separated = arg->separated, .err_msg = err_msg))
            {
                return 0;
            }
            break;
        case CLI_ARG_AROUND:
            if (EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b) != NULL)
            {
                char word_text[CLI_PATH_MAX_LEN * 4u];
                VerbaLinea line = {word_text, sizeof(word_text), 0ULL};
                mmgr_line_put(&line, a);
                mmgr_line_put(&line, EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b));
                mmgr_line_put(&line, c);
                mmgr_line_finish(&line);
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word_text = word_text);
            }
            break;
        case CLI_ARG_WHEN:
            EMBED_CALL(cli_arg_when, RunnerCommandCtx, .catalog = catalog, .values = values, .words = words, .arg = arg,
                       .key = a, .value = b);
            break;
        case CLI_ARG_FORMAT:
            EMBED_CALL(cli_arg_format, RunnerCommandCtx, .values = values, .words = words, .value = a);
            break;
        case CLI_ARG_FLAG_BUT:
            if ((EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b) != NULL) &&
                (!cellul.eq(
                    &(CatenaFinitaCfg){.src = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b),
                                       .other = c,
                                       .cap = CLI_POOL_BYTES})))
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word_text = a);
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word_text = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b));
            }
            break;
        default:
            break;
        }
    }
    return 1;
}

// A setting put on the command and leading its line as a shell writes it
EMBED_INLINE void cli_setting(const RunnerCommandCtx *args)
{
    CliCommand *command = args->command;
    const char *name = args->name;
    const char *value = args->value;
    EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command, .name = name,
               .value = value);
    mmgr_line_put(args->set, (args->set->at != 0ULL) ? "\n" : "");
    mmgr_line_put(args->set, name);
    mmgr_line_ch(args->set, '=');
    mmgr_line_put(args->set, value);
}

// The settings a cross build's type gives, put on the command
typedef struct
{
    CliCommand *command;
    VerbaLinea *set;
} CliCrossed;

static void cli_cross_each(const CliEachNameValueCfg *args)
{
    const char *name = args->name;
    const char *value = args->value;
    CliCrossed *const crossed = (CliCrossed *)args->context;
    EMBED_CALL(cli_setting, RunnerCommandCtx, .command = crossed->command, .set = crossed->set, .name = name,
               .value = value);
}

// SOURCE_DATE_EPOCH put on the command as the time of the tree's head commit at `root`, where the environment gives
// none
EMBED_INLINE void cli_source_date(const RunnerCommandCtx *args)
{
    char epoch[32];
    char commit[CLI_NAME_MAX_LEN];
    long long when = 0LL;
    int clean = 0;
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "SOURCE_DATE_EPOCH",
                   .value_out = epoch, .capacity = sizeof(epoch)) ||
        !EMBED_CALL(cli_git_root_commit.head, CliGitRootCommitCfg, .root = args->root, .commit = commit, .when = &when,
                    .clean = &clean))
    {
        return;
    }
    VerbaLinea line = {epoch, sizeof(epoch), 0ULL};
    mmgr_line_decimal_signed(&line, when);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_setting, RunnerCommandCtx, .command = args->command, .set = args->set, .name = "SOURCE_DATE_EPOCH",
               .value = epoch);
}

// the PATH every step runs with, less each folder that contains the program `name`, put on the command, and the line
// it leads shown in `set`
EMBED_INLINE void cli_unpath(const RunnerCommandCtx *args)
{
    static char s_settings_buf[256][CLI_PATH_MAX_LEN];
    static char s_path[1u << 16u];
#if defined(_WIN32)
    const char separator = ';';
#else
    const char separator = ':';
#endif
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path, .size = sizeof(s_path));
    const char *const names[1] = {args->name};
    unsigned int count = 0u;
    const char *from = s_path;
    for (const char *place = s_path;; place += 1)
    {
        if ((place[0] != separator) && (place[0] != '\0'))
        {
            continue;
        }
        char folder[CLI_PATH_MAX_LEN];
        char found[CLI_PATH_MAX_LEN];
        VerbaLinea named = {folder, sizeof(folder), 0ULL};
        mmgr_line_put_n(&named, from, (unsigned long long)(place - from));
        if ((place > from) && (mmgr_line_finish(&named) != 0ULL) && (count < 256u) &&
            !EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = folder, .names = names, .count = 1u,
                        .path = found))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = s_settings_buf[count],
                              .src = folder,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}) + 1u});
            count += 1u;
        }
        if (place[0] == '\0')
        {
            break;
        }
        from = place + 1;
    }
    EMBED_CALL(cli_path_count_size_internal.path_join, CliPathCountSizeCtx,
               .folders_callback = (const char (*)[CLI_PATH_MAX_LEN])s_settings_buf, .count = count, .path_out = s_path,
               .size = sizeof(s_path));
    EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = args->command, .name = "PATH",
               .value = s_path);
    mmgr_line_put(args->set, (args->set->at != 0ULL) ? "\nPATH=<PATH without " : "PATH=<PATH without ");
    mmgr_line_put(args->set, args->name);
    mmgr_line_ch(args->set, '>');
}

// the settings a step's arguments give put on the command, each line after the one before in `set`, a `cd` first in
// `moved` where the step runs in a folder of the tree
EMBED_INLINE void cli_settings(const RunnerCommandCtx *args)
{
    const char *root = args->root;
    const CliCatalog *catalog = args->catalog;
    const CliValues *values = args->values;
    CliCommand *command = args->command;
    char bashed[CLI_PATH_MAX_LEN * 4u];
    for (unsigned int at = 0u; at < args->step->args; at += 1u)
    {
        const CliArg *const arg = &catalog->arg[args->step->first + at];
        const char *const a =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = arg->text[0]);
        const char *const b =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = arg->text[1]);
        const char *const c =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = arg->text[2]);
        const char *const d =
            EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = arg->text[3]);
        const char *const input_a = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = a);
        const char *const input_b = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b);
        const char *const raw_a =
            EMBED_CALL(cli_catalog_key.values_first, CliCatalogKeyCfg, .values = values, .key = a);
        if ((arg->type == CLI_ARG_ENV) && (input_a != NULL))
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = a,
                       .value = (args->step->program == CLI_PROGRAM_BASH)
                                    ? EMBED_CALL(cli_for_bash, RunnerCommandCtx, .value = input_a, .out = bashed)
                                    : input_a);
        }
        else if (arg->type == CLI_ARG_SET)
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = a, .value = b);
        }
        else if ((arg->type == CLI_ARG_SET_FROM) && (input_b != NULL))
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = a,
                       .value = input_b);
        }
        else if ((arg->type == CLI_ARG_SET_WHEN) && (raw_a != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = raw_a, .other = b, .cap = CLI_POOL_BYTES})))
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = c, .value = d);
        }
        else if ((arg->type == CLI_ARG_ENV_BUT) && (input_a != NULL) &&
                 (!cellul.eq(&(CatenaFinitaCfg){.src = input_a, .other = b, .cap = CLI_POOL_BYTES})))
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = a,
                       .value = input_a);
        }
        else if (arg->type == CLI_ARG_FOLDER)
        {
            char folder[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = a, .joined = folder);
            EMBED_CALL(cli_runner_command_folder.command_folder, CliRunnerCommandFolderCfg, .command = command,
                       .folder = folder);
            args->moved->at = 0ULL;
            mmgr_line_put(args->moved, "cd ");
            mmgr_line_put(args->moved, a);
            mmgr_line_put(args->moved, " &&");
        }
        else if (arg->type == CLI_ARG_SOURCE_DATE)
        {
            EMBED_CALL(cli_source_date, RunnerCommandCtx, .root = root, .command = command, .set = args->set);
        }
        else if (arg->type == CLI_ARG_CROSS)
        {
            CliCrossed crossed = {command, args->set};
            EMBED_CALL(cli_executables_type.cross_env, CliExecutablesTypeCfg, .type = a, .values = values,
                       .each = cli_cross_each, .context = &crossed);
        }
        else if (arg->type == CLI_ARG_UNPATH)
        {
            EMBED_CALL(cli_unpath, RunnerCommandCtx, .name = a, .command = command, .set = args->set);
        }
    }
}

// one word of a shown line put after the line, in quotes where it contains a space
EMBED_INLINE void cli_displayed_word(const RunnerCommandCtx *args)
{
    int *first = args->first;
    mmgr_line_put(args->displayed, *first ? "" : " ");
    *first = 0;
    int spaced = 0;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        spaced = spaced || (args->word_text[at] == ' ');
    }
    mmgr_line_put(args->displayed, spaced ? "\"" : "");
    mmgr_line_put_n(args->displayed, args->word_text, args->length);
    mmgr_line_put(args->displayed, spaced ? "\"" : "");
}

// a text copied into `displayed_out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_displayed_set(const RunnerCommandCtx *args)
{
    memor.cpy(
        &(MemoriaCfg){.dst = args->displayed_out,
                      .src = args->word_text,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_POOL_BYTES}) + 1u});
}

// the program the step runs found into `out`, its text `word_text`, and the name its line shows into `displayed_out`,
// each sized for CLI_PATH_MAX_LEN: 1, or 0 with why given where the program cannot be found
EMBED_INLINE int cli_step_program(const RunnerCommandCtx *args)
{
    const char *const text = args->word_text;
    const char *const tool_program = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                .catalog_read = args->catalog, .text_u32 = args->step->tool_program);
    switch (args->step->program)
    {
    case CLI_PROGRAM_BASH:
        EMBED_CALL(cli_displayed_set, RunnerCommandCtx, .displayed_out = args->displayed_out, .word_text = "bash");
        return EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = args->out, .err_msg = args->err_msg);
    case CLI_PROGRAM_PYTHON:
        EMBED_CALL(cli_runner_name.python, CliRunnerNameCfg, .path = args->out);
        EMBED_CALL(cli_displayed_set, RunnerCommandCtx, .displayed_out = args->displayed_out, .word_text = "python");
        return 1;
    case CLI_PROGRAM_BUILT:
        if (!EMBED_CALL(cli_runner_name.build, CliRunnerNameCfg, .root = args->root, .name = text, .path = args->out,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = args->out,
                   .relative_out = args->displayed_out);
        return 1;
    case CLI_PROGRAM_TOOL:
        EMBED_CALL(cli_displayed_set, RunnerCommandCtx, .displayed_out = args->displayed_out,
                   .word_text = tool_program);
        return EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = text, .name = tool_program,
                          .path = args->out, .err_msg = args->err_msg);
    case CLI_PROGRAM_ORIOR:
        EMBED_CALL(cli_displayed_set, RunnerCommandCtx, .displayed_out = args->displayed_out, .word_text = "orior");
        if (!EMBED_CALL(cli_path_capacity_internal.program_own, CliPathCapacityCtx, .path_out = args->out,
                        .capacity_bytes = CLI_PATH_MAX_LEN))
        {
            mmgr_line_put(args->err_msg, "impero itself: its program could not be found");
            return 0;
        }
        return 1;
    default:
        EMBED_CALL(cli_displayed_set, RunnerCommandCtx, .displayed_out = args->out, .word_text = ".");
        EMBED_CALL(cli_displayed_set, RunnerCommandCtx, .displayed_out = args->displayed_out,
                   .word_text = args->step->system ? "PATH=<the system's PATH> <the program the build made>"
                                                   : "<the program the build made>");
        return 1;
    }
}

// whether the step's arguments set the PATH themselves: one takes a program off it or sets it
EMBED_INLINE int cli_path_given(const RunnerCommandCtx *args)
{
    for (unsigned int at = 0u; at < args->step->args; at += 1u)
    {
        const CliArg *const arg = &args->catalog->arg[args->step->first + at];
        if ((arg->type == CLI_ARG_UNPATH) ||
            ((arg->type == CLI_ARG_SET) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                             .catalog_read = args->catalog, .text_u32 = arg->text[0]),
                                           .other = "PATH",
                                           .cap = CLI_POOL_BYTES}))))
        {
            return 1;
        }
    }
    return 0;
}

// the tree's own tools, which come first on the PATH every step runs with, shown leading the line where it has any
EMBED_INLINE void cli_tools_shown(const RunnerCommandCtx *args)
{
    static char s_tools[16][CLI_PATH_MAX_LEN];
    const unsigned int tools =
        EMBED_CALL(cli_toolchains_count.tree_tools, CliToolchainsCountCfg, .folders = s_tools, .capacity = 16u);
    if (tools == 0u)
    {
        return;
    }
    char line[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea tools_line = {line, sizeof(line), 0ULL};
    mmgr_line_put(&tools_line, "PATH=");
    for (unsigned int at = 0u; at < tools; at += 1u)
    {
        char relative[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = s_tools[at],
                   .relative_out = relative);
        mmgr_line_put(&tools_line, (at != 0u) ? ":" : "");
        mmgr_line_put(&tools_line, relative);
    }
    mmgr_line_put(&tools_line, ":$PATH");
    const unsigned long long length = mmgr_line_ok(&tools_line) ? tools_line.at : 0ULL;
    EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word_text = line, .length = length,
               .first = args->first);
}

// each setting of the set `word_text`, `length` bytes, one to a line, shown as a word of the line
EMBED_INLINE void cli_set_shown(const RunnerCommandCtx *args)
{
    const char *const set = args->word_text;
    unsigned long long from = 0ULL;
    for (unsigned long long at = 0ULL; at <= args->length; at += 1ULL)
    {
        const int ends = (at == args->length) || (set[at] == '\n');
        if (ends && (at > from))
        {
            EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word_text = set + from,
                       .length = at - from, .first = args->first);
        }
        from = ((at < args->length) && (set[at] == '\n')) ? (at + 1ULL) : from;
    }
}

int cli_runner_command(const CliRunnerCommandCfg *args)
{
    static CliWords s_args;
    static char s_set[1u << 16u];
    s_args.count = 0u;
    s_args.used = 0u;
    s_args.full = 0;
    s_args.word_text[0] = NULL;
    if (!EMBED_CALL(cli_step_arguments, RunnerCommandCtx, .catalog = args->catalog, .step = args->step,
                    .values = args->values, .words = &s_args, .err_msg = args->err_msg))
    {
        return 0;
    }
    char program[CLI_PATH_MAX_LEN];
    char displayed_program[CLI_PATH_MAX_LEN];
    const char *const text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = args->catalog,
                                        .text_u32 = args->step->text);
    if (!EMBED_CALL(cli_step_program, RunnerCommandCtx, .root = args->root, .catalog = args->catalog,
                    .step = args->step, .word_text = text, .out = program, .displayed_out = displayed_program,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_runner.command_begin, CliRunnerCfg, .command = args->command, .program = program);
    if (args->step->program == CLI_PROGRAM_PYTHON)
    {
        EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = args->command,
                   .word_text = "-u");
    }
    char bashed[CLI_PATH_MAX_LEN * 4u];
    if ((args->step->program == CLI_PROGRAM_BASH) || (args->step->program == CLI_PROGRAM_PYTHON))
    {
        EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = args->command,
                   .word_text = text);
    }
    for (unsigned int at = 0u; at < s_args.count; at += 1u)
    {
        EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = args->command,
                   .word_text =
                       (args->step->program == CLI_PROGRAM_BASH)
                           ? EMBED_CALL(cli_for_bash, RunnerCommandCtx, .value = s_args.word_text[at], .out = bashed)
                           : s_args.word_text[at]);
    }
    EMBED_CALL(cli_runner_command_folder.command_folder, CliRunnerCommandFolderCfg, .command = args->command,
               .folder = args->root);
    VerbaLinea set = {s_set, sizeof(s_set), 0ULL};
    char moved_text[CLI_PATH_MAX_LEN + 8u];
    VerbaLinea moved = {moved_text, sizeof(moved_text), 0ULL};
    EMBED_CALL(cli_settings, RunnerCommandCtx, .root = args->root, .catalog = args->catalog, .step = args->step,
               .values = args->values, .command = args->command, .set = &set, .moved = &moved);
    int first = 1;
    if (moved.at != 0ULL)
    {
        EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word_text = moved_text,
                   .length = moved.at, .first = &first);
    }
    if (!EMBED_CALL(cli_path_given, RunnerCommandCtx, .catalog = args->catalog, .step = args->step))
    {
        EMBED_CALL(cli_tools_shown, RunnerCommandCtx, .root = args->root, .displayed = args->displayed,
                   .first = &first);
    }
    // each setting, one to a line of the set, then the program and its words
    EMBED_CALL(cli_set_shown, RunnerCommandCtx, .word_text = s_set, .length = mmgr_line_ok(&set) ? set.at : 0ULL,
               .displayed = args->displayed, .first = &first);
    EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word_text = displayed_program,
               .length = cellul.len(&(CatenaFinitaCfg){.src = displayed_program, .cap = CLI_POOL_BYTES}),
               .first = &first);
    if ((args->step->program == CLI_PROGRAM_BASH) || (args->step->program == CLI_PROGRAM_PYTHON))
    {
        EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word_text = text,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}), .first = &first);
    }
    for (unsigned int at = 0u; at < s_args.count; at += 1u)
    {
        const char *const word_text =
            (args->step->program == CLI_PROGRAM_BASH)
                ? EMBED_CALL(cli_for_bash, RunnerCommandCtx, .value = s_args.word_text[at], .out = bashed)
                : s_args.word_text[at];
        EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word_text = word_text,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = word_text, .cap = CLI_POOL_BYTES}), .first = &first);
    }
    if (args->command->words.full || s_args.full)
    {
        mmgr_line_put(args->err_msg, "the step's command is longer than impero supports");
        return 0;
    }
    return 1;
}

int cli_runner_show(const CliRunnerErrMsgRootCfg *args)
{
    const char *root = args->root;
    const CliCatalog *catalog = args->catalog;
    const CliValues *values = args->values;
    VerbaLinea *err_msg = args->err_msg;
    static CliCommand s_command;
    static char s_shown[CLI_COMMAND_TEXT];
    const CliJob *const job_at = &catalog->job[args->job];
    for (unsigned int at = 0u; at < job_at->steps; at += 1u)
    {
        const CliStep *const step = &catalog->step[job_at->step_first + at];
        if (!EMBED_CALL(cli_runner_command_table.runs, CliRunnerCommandCfg, .catalog = catalog, .step = step,
                        .values = values))
        {
            continue;
        }
        VerbaLinea displayed = {s_shown, sizeof(s_shown), 0ULL};
        if (!EMBED_CALL(cli_runner_command_table.command, CliRunnerCommandCfg, .root = root, .catalog = catalog,
                        .step = step, .values = values, .command = &s_command, .displayed = &displayed,
                        .err_msg = err_msg))
        {
            return 0;
        }
        mmgr_line_finish(&displayed);
        EMBED_CALL(args->each, CliEachDisplayedCfg, .context = args->context, .displayed = s_shown);
    }
    return 1;
}
