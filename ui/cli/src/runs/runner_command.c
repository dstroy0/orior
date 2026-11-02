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
    const char *word;
    unsigned long long length;
    int *first;
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
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_CELLS_POOL}) + 1u});
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->out,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->out, .cap = CLI_CELLS_POOL}), .from = '\\',
               .to = '/');
#endif
    return args->out;
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
        switch (arg->kind)
        {
        case CLI_ARG_LIT:
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                       .word = a);
            break;
        case CLI_ARG_VALUE:
            if (EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = a) != NULL)
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = a));
            }
            break;
        case CLI_ARG_FLAG:
            if (EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b) != NULL)
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word = a);
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b));
            }
            break;
        case CLI_ARG_EACH: {
            unsigned int count = 0u;
            const unsigned int *const items =
                EMBED_CALL(cli_catalog_key.values_get, CliCatalogKeyCfg, .values = values, .key = b, .count = &count);
            for (unsigned int each = 0u; each < count; each += 1u)
            {
                if (EMBED_CALL(cli_catalog_text_values.values_at, CliCatalogTextValuesCfg, .values = values,
                               .text_u32 = items[each])[0] != '\0')
                {
                    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                               .word = a);
                    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                               .word = EMBED_CALL(cli_catalog_text_values.values_at, CliCatalogTextValuesCfg,
                                                  .values = values, .text_u32 = items[each]));
                }
            }
            break;
        }
        case CLI_ARG_WORDS: {
            static CliWords s_split;
            s_split.count = 0u;
            s_split.used = 0u;
            s_split.full = 0;
            const char *const text = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = a);
            if (!EMBED_CALL(cli_runner_text.shell_words, CliRunnerTextCfg, .text = (text != NULL) ? text : "",
                            .words = &s_split, .err_msg = err_msg))
            {
                return 0;
            }
            if (arg->separated && (s_split.count != 0u))
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word = "--");
            }
            for (unsigned int each = 0u; each < s_split.count; each += 1u)
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word = s_split.word[each]);
            }
            break;
        }
        case CLI_ARG_AROUND:
            if (EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b) != NULL)
            {
                char word[CLI_PATH_MAX_LEN * 4u];
                VerbaLinea line = {word, sizeof(word), 0ull};
                mmgr_line_put(&line, a);
                mmgr_line_put(&line, EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b));
                mmgr_line_put(&line, c);
                mmgr_line_finish(&line);
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word = word);
            }
            break;
        case CLI_ARG_WHEN:
            if ((EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = a) != NULL) &&
                (cellul.eq(
                    &(CatenaFinitaCfg){.src = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = a),
                                       .other = b,
                                       .cap = CLI_CELLS_POOL})))
            {
                for (unsigned int each = 0u; each < arg->words; each += 1u)
                {
                    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                               .word = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog,
                                                  .text_u32 = catalog->list[arg->first + each]));
                }
            }
            break;
        case CLI_ARG_FORMAT: {
            // each {key} filled with the first value given of that key, the keys taken in turn
            static char s_filled[2][CLI_PATH_MAX_LEN * 4u];
            unsigned int now = 0u;
            memor.cpy(&(MemoriaCfg){.dst = s_filled[now],
                                    .src = a,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = a, .cap = CLI_CELLS_POOL}) + 1u});
            for (unsigned int key = 0u; key < values->keys; key += 1u)
            {
                char marker[CLI_NAME_MAX_LEN + 2u];
                VerbaLinea named = {marker, sizeof(marker), 0ull};
                mmgr_line_ch(&named, '{');
                mmgr_line_put(&named, values->key[key]);
                mmgr_line_ch(&named, '}');
                const unsigned long long marker_length = mmgr_line_finish(&named);
                const char *const supplied =
                    (values->count[key] != 0u)
                        ? EMBED_CALL(cli_catalog_text_values.values_at, CliCatalogTextValuesCfg, .values = values,
                                     .text_u32 = values->item[values->first[key]])
                        : "";
                VerbaLinea next = {s_filled[1u - now], CLI_PATH_MAX_LEN * 4u, 0ull};
                const char *rest = s_filled[now];
                for (const char *found = cellul.find(&(CatenaFinitaCfg){
                         .src = rest, .cap = CLI_CELLS_POOL, .other = marker, .other_cap = CLI_CELLS_POOL});
                     found != NULL;
                     found = cellul.find(&(CatenaFinitaCfg){
                         .src = rest, .cap = CLI_CELLS_POOL, .other = marker, .other_cap = CLI_CELLS_POOL}))
                {
                    mmgr_line_put_n(&next, rest, (unsigned long long)(found - rest));
                    mmgr_line_put(&next, supplied);
                    rest = found + marker_length;
                }
                mmgr_line_put(&next, rest);
                mmgr_line_finish(&next);
                now = 1u - now;
            }
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                       .word = s_filled[now]);
            break;
        }
        case CLI_ARG_FLAG_BUT:
            if ((EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b) != NULL) &&
                (!cellul.eq(
                    &(CatenaFinitaCfg){.src = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b),
                                       .other = c,
                                       .cap = CLI_CELLS_POOL})))
            {
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word = a);
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                           .word = EMBED_CALL(cli_first, RunnerCommandCtx, .values = values, .key = b));
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
    mmgr_line_put(args->set, (args->set->at != 0ull) ? "\n" : "");
    mmgr_line_put(args->set, name);
    mmgr_line_ch(args->set, '=');
    mmgr_line_put(args->set, value);
}

// The settings a cross build's kind gives, put on the command
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
        if ((arg->kind == CLI_ARG_ENV) && (input_a != NULL))
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = a,
                       .value = (args->step->program == CLI_PROGRAM_BASH)
                                    ? EMBED_CALL(cli_for_bash, RunnerCommandCtx, .value = input_a, .out = bashed)
                                    : input_a);
        }
        else if (arg->kind == CLI_ARG_SET)
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = a, .value = b);
        }
        else if ((arg->kind == CLI_ARG_SET_FROM) && (input_b != NULL))
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = a,
                       .value = input_b);
        }
        else if ((arg->kind == CLI_ARG_SET_WHEN) && (raw_a != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = raw_a, .other = b, .cap = CLI_CELLS_POOL})))
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = c, .value = d);
        }
        else if ((arg->kind == CLI_ARG_ENV_BUT) && (input_a != NULL) &&
                 (!cellul.eq(&(CatenaFinitaCfg){.src = input_a, .other = b, .cap = CLI_CELLS_POOL})))
        {
            EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set, .name = a,
                       .value = input_a);
        }
        else if (arg->kind == CLI_ARG_FOLDER)
        {
            char folder[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = a, .joined = folder);
            EMBED_CALL(cli_runner_command_folder.command_folder, CliRunnerCommandFolderCfg, .command = command,
                       .folder = folder);
            args->moved->at = 0ull;
            mmgr_line_put(args->moved, "cd ");
            mmgr_line_put(args->moved, a);
            mmgr_line_put(args->moved, " &&");
        }
        else if (arg->kind == CLI_ARG_SOURCE_DATE)
        {
            char epoch[32];
            char commit[CLI_NAME_MAX_LEN];
            long long when = 0ll;
            int clean = 0;
            if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "SOURCE_DATE_EPOCH",
                            .value_out = epoch, .capacity = sizeof(epoch)) &&
                EMBED_CALL(cli_git_root_commit.head, CliGitRootCommitCfg, .root = root, .commit = commit, .when = &when,
                           .clean = &clean))
            {
                VerbaLinea line = {epoch, sizeof(epoch), 0ull};
                mmgr_line_decimal_signed(&line, when);
                mmgr_line_finish(&line);
                EMBED_CALL(cli_setting, RunnerCommandCtx, .command = command, .set = args->set,
                           .name = "SOURCE_DATE_EPOCH", .value = epoch);
            }
        }
        else if (arg->kind == CLI_ARG_CROSS)
        {
            CliCrossed crossed = {command, args->set};
            EMBED_CALL(cli_executables_kind.cross_env, CliExecutablesKindCfg, .kind = a, .values = values,
                       .each = cli_cross_each, .context = &crossed);
        }
        else if (arg->kind == CLI_ARG_UNPATH)
        {
            // the PATH every step runs with, less each folder that contains the program named
            static char s_settings_buf[256][CLI_PATH_MAX_LEN];
            static char s_path[1u << 16u];
#if defined(_WIN32)
            const char separator = ';';
#else
            const char separator = ':';
#endif
            EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path,
                       .size = sizeof(s_path));
            const char *const names[1] = {a};
            unsigned int count = 0u;
            const char *from = s_path;
            for (const char *place = s_path;; place += 1)
            {
                if ((place[0] == separator) || (place[0] == '\0'))
                {
                    char folder[CLI_PATH_MAX_LEN];
                    char found[CLI_PATH_MAX_LEN];
                    VerbaLinea named = {folder, sizeof(folder), 0ull};
                    mmgr_line_put_n(&named, from, (unsigned long long)(place - from));
                    if ((place > from) && (mmgr_line_finish(&named) != 0ull) && (count < 256u) &&
                        !EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = folder, .names = names,
                                    .count = 1u, .path = found))
                    {
                        memor.cpy(&(MemoriaCfg){
                            .dst = s_settings_buf[count],
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
                        count += 1u;
                    }
                    if (place[0] == '\0')
                    {
                        break;
                    }
                    from = place + 1;
                }
            }
            EMBED_CALL(cli_path_count_size_internal.path_join, CliPathCountSizeCtx,
                       .folders_callback = (const char (*)[CLI_PATH_MAX_LEN])s_settings_buf, .count = count,
                       .path_out = s_path, .size = sizeof(s_path));
            EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command, .name = "PATH",
                       .value = s_path);
            mmgr_line_put(args->set, (args->set->at != 0ull) ? "\nPATH=<PATH without " : "PATH=<PATH without ");
            mmgr_line_put(args->set, a);
            mmgr_line_ch(args->set, '>');
        }
        (void)c;
    }
}

// one word of a shown line put after the line, in quotes where it contains a space
EMBED_INLINE void cli_displayed_word(const RunnerCommandCtx *args)
{
    int *first = args->first;
    mmgr_line_put(args->displayed, *first ? "" : " ");
    *first = 0;
    int spaced = 0;
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        spaced = spaced || (args->word[at] == ' ');
    }
    mmgr_line_put(args->displayed, spaced ? "\"" : "");
    mmgr_line_put_n(args->displayed, args->word, args->length);
    mmgr_line_put(args->displayed, spaced ? "\"" : "");
}

int cli_runner_command(const CliRunnerCommandCfg *args)
{
    static CliWords s_args;
    static char s_set[1u << 16u];
    s_args.count = 0u;
    s_args.used = 0u;
    s_args.full = 0;
    s_args.word[0] = NULL;
    if (!EMBED_CALL(cli_step_arguments, RunnerCommandCtx, .catalog = args->catalog, .step = args->step,
                    .values = args->values, .words = &s_args, .err_msg = args->err_msg))
    {
        return 0;
    }
    char program[CLI_PATH_MAX_LEN];
    char displayed_program[CLI_PATH_MAX_LEN];
    const char *const text = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = args->catalog,
                                        .text_u32 = args->step->text);
    switch (args->step->program)
    {
    case CLI_PROGRAM_BASH:
        if (!EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = program, .err_msg = args->err_msg))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = displayed_program,
                                .src = "bash",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "bash", .cap = CLI_CELLS_POOL}) + 1u});
        break;
    case CLI_PROGRAM_PYTHON:
        EMBED_CALL(cli_runner_name.python, CliRunnerNameCfg, .path = program);
        memor.cpy(&(MemoriaCfg){.dst = displayed_program,
                                .src = "python",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python", .cap = CLI_CELLS_POOL}) + 1u});
        break;
    case CLI_PROGRAM_BUILT:
        if (!EMBED_CALL(cli_runner_name.build, CliRunnerNameCfg, .root = args->root, .name = text, .path = program,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = program,
                   .relative_out = displayed_program);
        break;
    case CLI_PROGRAM_TOOL:
        if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = text,
                        .name = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = args->catalog,
                                           .text_u32 = args->step->tool_program),
                        .path = program, .err_msg = args->err_msg))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){
            .dst = displayed_program,
            .src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = args->catalog,
                              .text_u32 = args->step->tool_program),
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                                     .catalog_read = args->catalog,
                                                                     .text_u32 = args->step->tool_program),
                                                   .cap = CLI_CELLS_POOL}) +
                     1u});
        break;
    case CLI_PROGRAM_ORIOR:
        if (!EMBED_CALL(cli_path_capacity_internal.program_own, CliPathCapacityCtx, .path_out = program,
                        .capacity_bytes = sizeof(program)))
        {
            mmgr_line_put(args->err_msg, "orior_cli itself: its program could not be found");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = displayed_program,
                                .src = "orior",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "orior", .cap = CLI_CELLS_POOL}) + 1u});
        break;
    default:
        memor.cpy(&(MemoriaCfg){.dst = program,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = displayed_program,
            .src = args->step->system ? "PATH=<the system's PATH> <the program the build made>"
                                      : "<the program the build made>",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->step->system
                                                              ? "PATH=<the system's PATH> <the program the build made>"
                                                              : "<the program the build made>",
                                                   .cap = CLI_CELLS_POOL}) +
                     1u});
        break;
    }
    EMBED_CALL(cli_runner.command_begin, CliRunnerCfg, .command = args->command, .program = program);
    if (args->step->program == CLI_PROGRAM_PYTHON)
    {
        EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = args->command,
                   .word = "-u");
    }
    char bashed[CLI_PATH_MAX_LEN * 4u];
    if ((args->step->program == CLI_PROGRAM_BASH) || (args->step->program == CLI_PROGRAM_PYTHON))
    {
        EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = args->command,
                   .word = text);
    }
    for (unsigned int at = 0u; at < s_args.count; at += 1u)
    {
        EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = args->command,
                   .word = (args->step->program == CLI_PROGRAM_BASH)
                               ? EMBED_CALL(cli_for_bash, RunnerCommandCtx, .value = s_args.word[at], .out = bashed)
                               : s_args.word[at]);
    }
    EMBED_CALL(cli_runner_command_folder.command_folder, CliRunnerCommandFolderCfg, .command = args->command,
               .folder = args->root);
    VerbaLinea set = {s_set, sizeof(s_set), 0ull};
    char moved_text[CLI_PATH_MAX_LEN + 8u];
    VerbaLinea moved = {moved_text, sizeof(moved_text), 0ull};
    EMBED_CALL(cli_settings, RunnerCommandCtx, .root = args->root, .catalog = args->catalog, .step = args->step,
               .values = args->values, .command = args->command, .set = &set, .moved = &moved);
    // the tree's own tools come first on the PATH every step runs with, and the line shows so
    static char s_tools[16][CLI_PATH_MAX_LEN];
    const unsigned int tools =
        EMBED_CALL(cli_toolchains_count.tree_tools, CliToolchainsCountCfg, .folders = s_tools, .capacity = 16u);
    int path_set = 0;
    for (unsigned int at = 0u; at < args->step->args; at += 1u)
    {
        const CliArg *const arg = &args->catalog->arg[args->step->first + at];
        path_set =
            path_set || (arg->kind == CLI_ARG_UNPATH) ||
            ((arg->kind == CLI_ARG_SET) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                             .catalog_read = args->catalog, .text_u32 = arg->text[0]),
                                           .other = "PATH",
                                           .cap = CLI_CELLS_POOL})));
    }
    int first = 1;
    if (moved.at != 0ull)
    {
        EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word = moved_text,
                   .length = moved.at, .first = &first);
    }
    if ((tools != 0u) && !path_set)
    {
        char line[CLI_PATH_MAX_LEN * 4u];
        VerbaLinea tools_line = {line, sizeof(line), 0ull};
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
        const unsigned long long length = mmgr_line_ok(&tools_line) ? tools_line.at : 0ull;
        EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word = line, .length = length,
                   .first = &first);
    }
    // each setting, one to a line of the set, then the program and its words
    const unsigned long long set_length = mmgr_line_ok(&set) ? set.at : 0ull;
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at <= set_length; at += 1ull)
    {
        if (((at == set_length) || (s_set[at] == '\n')) && (at > from))
        {
            EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word = s_set + from,
                       .length = at - from, .first = &first);
        }
        from = ((at < set_length) && (s_set[at] == '\n')) ? (at + 1ull) : from;
    }
    EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word = displayed_program,
               .length = cellul.len(&(CatenaFinitaCfg){.src = displayed_program, .cap = CLI_CELLS_POOL}),
               .first = &first);
    if ((args->step->program == CLI_PROGRAM_BASH) || (args->step->program == CLI_PROGRAM_PYTHON))
    {
        EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word = text,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), .first = &first);
    }
    for (unsigned int at = 0u; at < s_args.count; at += 1u)
    {
        const char *const word =
            (args->step->program == CLI_PROGRAM_BASH)
                ? EMBED_CALL(cli_for_bash, RunnerCommandCtx, .value = s_args.word[at], .out = bashed)
                : s_args.word[at];
        EMBED_CALL(cli_displayed_word, RunnerCommandCtx, .displayed = args->displayed, .word = word,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}), .first = &first);
    }
    if (args->command->words.full || s_args.full)
    {
        mmgr_line_put(args->err_msg, "the step's command is longer than orior_cli supports");
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
        VerbaLinea displayed = {s_shown, sizeof(s_shown), 0ull};
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
