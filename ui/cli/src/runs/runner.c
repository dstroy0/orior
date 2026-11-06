// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner.c: the programs a job's steps run, a step's command made from its arguments and the values given, with the
// line the window shows for it, and whether values do for a job
#include "runner.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"

void cli_command_begin(const CliRunnerCfg *args)
{
    args->command->words.count = 0u;
    args->command->words.used = 0u;
    args->command->words.full = 0;
    args->command->words.word_text[0] = NULL;
    args->command->has_folder = 0;
    args->command->folder[0] = '\0';
    args->command->settings = 0u;
    args->command->used = 0u;
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &args->command->words,
               .word_text = args->program);
}

void cli_command_word(const CliRunnerCommandWordCfg *args)
{
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &args->command->words,
               .word_text = args->word_text);
}

typedef struct
{
    CliCommand *command;
    const char *text;
    const char *under;
    const char *name;
    char *path;
    char letter;
    const char *variable;
    char *text_out;
    size_t capacity;
    CliWords *words;
    const CliValues *values;
    const char *key;
} RunnerCtx;
// a text stored among the command's: where it starts, NULL where it does not fit
EMBED_INLINE const char *cli_text_store(const RunnerCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}) + 1ULL;
    if ((args->command->used + size) > CLI_COMMAND_TEXT)
    {
        args->command->words.full = 1;
        return NULL;
    }
    char *const at = args->command->text + args->command->used;
    memor.cpy(&(MemoriaCfg){.dst = at, .src = args->text, .bytes = size});
    // the texts are below CLI_COMMAND_TEXT
    args->command->used += (unsigned int)size;
    return at;
}

void cli_command_set(const CliRunnerCommandNameCfg *args)
{
    const char *value = args->value;
    for (unsigned int at = 0u; at < args->command->settings; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = args->command->setting[at].name, .other = args->name, .cap = CLI_POOL_BYTES}))
        {
            args->command->setting[at].value =
                (value != NULL) ? EMBED_CALL(cli_text_store, RunnerCtx, .command = args->command, .text = value) : NULL;
            return;
        }
    }
    if (args->command->settings >= CLI_COMMAND_SETTINGS)
    {
        args->command->words.full = 1;
        return;
    }
    const CliSetting setting = {
        EMBED_CALL(cli_text_store, RunnerCtx, .command = args->command, .text = args->name),
        (value != NULL) ? EMBED_CALL(cli_text_store, RunnerCtx, .command = args->command, .text = value) : NULL};
    args->command->setting[args->command->settings] = setting;
    args->command->settings += (setting.name != NULL) ? 1u : 0u;
}

int cli_command_sets(const CliRunnerNameCommandCfg *args)
{
    for (unsigned int at = 0u; at < args->command_read->settings; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = args->command_read->setting[at].name, .other = args->name, .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

void cli_command_folder(const CliRunnerCommandFolderCfg *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->command->folder,
                            .src = args->folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_POOL_BYTES}) + 1u});
    args->command->has_folder = 1;
}

void cli_lines_append(const CliRunnerLinesCfg *args)
{
    if ((args->length + 1ULL) > CLI_LINES_TEXT)
    {
        return;
    }
    if ((args->lines->count >= CLI_LINES_MAX) || ((args->lines->used + args->length + 1ULL) > CLI_LINES_TEXT))
    {
        // the oldest half let go, the newer moved down
        const unsigned int half = args->lines->count / 2u;
        const unsigned int from = args->lines->start[args->lines->count - half];
        for (unsigned int at = 0u; at < half; at += 1u)
        {
            args->lines->start[at] = args->lines->start[args->lines->count - half + at] - from;
        }
        for (unsigned int byte = from; byte < args->lines->used; byte += 1u)
        {
            args->lines->text[byte - from] = args->lines->text[byte];
        }
        args->lines->used -= from;
        args->lines->count = half;
        if ((args->lines->used + args->length + 1ULL) > CLI_LINES_TEXT)
        {
            args->lines->count = 0u;
            args->lines->used = 0u;
        }
    }
    args->lines->start[args->lines->count] = args->lines->used;
    memor.cpy(&(MemoriaCfg){.dst = args->lines->text + args->lines->used, .src = args->text, .bytes = args->length});
    args->lines->text[args->lines->used + args->length] = '\0';
    // a line is below CLI_LINES_TEXT, checked above
    args->lines->used += (unsigned int)args->length + 1u;
    args->lines->count += 1u;
}

const char *cli_lines_at(const CliRunnerLinesIndexCfg *args)
{
    return args->lines_read->text + args->lines_read->start[args->index];
}

#if defined(_WIN32)
// a file in a folder joined, `under` and `name`, where it is a file: 1, its path into `path`
EMBED_INLINE int cli_found_in(const RunnerCtx *args)
{
    char *path = args->path;
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->under);
    mmgr_line_put(&line, args->name);
    return (mmgr_line_finish(&line) != 0ULL) &&
           EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
}
#endif

// The bash that runs the tree's scripts. On Windows that is Git's, found beside git itself, because the bash System32
// offers is WSL's and reads none of these paths
int cli_runner_bash(const CliRunnerNameCfg *args)
{
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_BASH",
                   .value_out = args->path, .capacity = CLI_PATH_MAX_LEN) ||
        EMBED_CALL(cli_toolchains_id_path.chosen_program, CliToolchainsIdPathCfg, .id = "bash", .path = args->path))
    {
        return 1;
    }
#if !defined(_WIN32)
    memor.cpy(&(MemoriaCfg){.dst = args->path,
                            .src = "bash",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "bash", .cap = CLI_POOL_BYTES}) + 1u});
    return 1;
#else
    static char s_folders[256][CLI_PATH_MAX_LEN];
    const unsigned int count = EMBED_CALL(cli_capacity_folders_internal.path_folders, CliCapacityFoldersCtx,
                                          .folders = s_folders, .capacity_u32 = 256u);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char git[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_found_in, RunnerCtx, .under = s_folders[at], .name = "\\git.exe", .path = git))
        {
            continue;
        }
        // the folder above the one git stands in
        char *cut = EMBED_CALL(
            cli_text_byte.last, CliTextByteCfg, .text = s_folders[at],
            .length_value = cellul.len(&(CatenaFinitaCfg){.src = s_folders[at], .cap = CLI_POOL_BYTES}), .byte = '\\');
        char *const forward = EMBED_CALL(
            cli_text_byte.last, CliTextByteCfg, .text = s_folders[at],
            .length_value = cellul.len(&(CatenaFinitaCfg){.src = s_folders[at], .cap = CLI_POOL_BYTES}), .byte = '/');
        cut = ((forward != NULL) && ((cut == NULL) || (forward > cut))) ? forward : cut;
        if (cut == NULL)
        {
            continue;
        }
        cut[0] = '\0';
        if (EMBED_CALL(cli_found_in, RunnerCtx, .under = s_folders[at], .name = "\\bin\\bash.exe",
                       .path = args->path) ||
            EMBED_CALL(cli_found_in, RunnerCtx, .under = s_folders[at], .name = "\\usr\\bin\\bash.exe",
                       .path = args->path))
        {
            return 1;
        }
    }
    static const char *const s_bases[3] = {"ProgramFiles", "ProgramW6432", "LOCALAPPDATA"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char base[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = s_bases[at],
                       .value_out = base, .capacity = sizeof(base)) &&
            (EMBED_CALL(cli_found_in, RunnerCtx, .under = base, .name = "\\Git\\bin\\bash.exe", .path = args->path) ||
             EMBED_CALL(cli_found_in, RunnerCtx, .under = base, .name = "\\Programs\\Git\\bin\\bash.exe",
                        .path = args->path)))
        {
            return 1;
        }
    }
    mmgr_line_put(args->err_msg, "no Git bash found: install Git for Windows, or name a bash in ORIOR_BASH");
    return 0;
#endif
}

void cli_runner_python(const CliRunnerNameCfg *args)
{
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_PYTHON",
                   .value_out = args->path, .capacity = CLI_PATH_MAX_LEN) ||
        EMBED_CALL(cli_toolchains_id_path.chosen_program, CliToolchainsIdPathCfg, .id = "python", .path = args->path))
    {
        return;
    }
#if defined(_WIN32)
    memor.cpy(&(MemoriaCfg){.dst = args->path,
                            .src = "python",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python", .cap = CLI_POOL_BYTES}) + 1u});
#else
    memor.cpy(&(MemoriaCfg){.dst = args->path,
                            .src = "python3",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python3", .cap = CLI_POOL_BYTES}) + 1u});
#endif
}

int cli_runner_build(const CliRunnerNameCfg *args)
{
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->root);
    mmgr_line_put(&line, "/build/");
    mmgr_line_put(&line, args->name);
#if defined(_WIN32)
    mmgr_line_put(&line, ".exe");
#endif
    if ((mmgr_line_finish(&line) != 0ULL) &&
        EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->path))
    {
        return 1;
    }
    mmgr_line_put(args->err_msg, "no ");
    mmgr_line_put(args->err_msg, args->name);
#if defined(_WIN32)
    mmgr_line_put(args->err_msg, ".exe");
#endif
    mmgr_line_put(args->err_msg, " under build/: run its build job first");
    return 0;
}

EMBED_INLINE int cli_space(const RunnerCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

int cli_runner_shell_words(const CliRunnerTextCfg *args)
{
    static char s_word[CLI_WORDS_TEXT];
    unsigned long long length = 0ULL;
    int started = 0;
    char quote = '\0';
    const char *at = args->text;
    while (at[0] != '\0')
    {
        const char letter = at[0];
        if ((quote != '\0') && (letter == quote))
        {
            quote = '\0';
        }
        else if ((quote == '"') && (letter == '\\'))
        {
            s_word[length] = (at[1] != '\0') ? at[1] : '\\';
            length += (length + 1u < sizeof(s_word)) ? 1ULL : 0ULL;
            at += (at[1] != '\0') ? 1 : 0;
        }
        else if (quote != '\0')
        {
            s_word[length] = letter;
            length += (length + 1u < sizeof(s_word)) ? 1ULL : 0ULL;
        }
        else if ((letter == '\'') || (letter == '"'))
        {
            quote = letter;
            started = 1;
        }
        else if (EMBED_CALL(cli_space, RunnerCtx, .letter = letter))
        {
            if (started)
            {
                s_word[length] = '\0';
                EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                           .word_text = s_word);
                length = 0ULL;
                started = 0;
            }
        }
        else
        {
            s_word[length] = letter;
            length += (length + 1u < sizeof(s_word)) ? 1ULL : 0ULL;
            started = 1;
        }
        at += 1;
    }
    if (quote != '\0')
    {
        mmgr_line_put(args->err_msg, "a quote is left open in: ");
        mmgr_line_put(args->err_msg, args->text);
        return 0;
    }
    if (started)
    {
        s_word[length] = '\0';
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = s_word);
    }
    return 1;
}

EMBED_INLINE int cli_runner_text_words(const RunnerCtx *args);

int cli_runner_program_words(const CliRunnerVariableCfg *args)
{
    static char s_text[QUERY_ENVIRONMENT_MAX_LEN];
    const int words_read = EMBED_CALL(cli_runner_text_words, RunnerCtx, .variable = args->variable, .text_out = s_text,
                                      .capacity = sizeof(s_text), .words = args->words);
    return words_read;
}

// the program `variable` names, read into `text`, sized for `capacity`, as its words put into `words`: 1, or 0
// where it is not set or names none
EMBED_INLINE int cli_runner_text_words(const RunnerCtx *args)
{
    char *text = args->text_out;
    size_t capacity = args->capacity;
    CliWords *words = args->words;
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = args->variable,
                    .value_out = text, .capacity = capacity))
    {
        return 0;
    }
    const char *from = text;
    char *to = text + cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES});
    while ((from < to) && EMBED_CALL(cli_space, RunnerCtx, .letter = from[0]))
    {
        from += 1;
    }
    while ((to > from) && EMBED_CALL(cli_space, RunnerCtx, .letter = to[-1]))
    {
        to -= 1;
    }
    to[0] = '\0';
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = from))
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                   .word_text = from);
        return 1;
    }
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    return EMBED_CALL(cli_runner_text.shell_words, CliRunnerTextCfg, .text = from, .words = words,
                      .err_msg = &err_msg) &&
           (words->count != 0u);
}

int cli_runner_runs(const CliRunnerCommandCfg *args)
{
    for (unsigned int at = 0u; at < args->step->args; at += 1u)
    {
        const CliArg *const arg = &args->catalog->arg[args->step->first + at];
        const char *const key = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = args->catalog,
                                           .text_u32 = arg->text[0]);
        const char *const supplied =
            EMBED_CALL(cli_catalog_key.values_first, CliCatalogKeyCfg, .values = args->values, .key = key);
        if ((arg->kind == CLI_ARG_ONLY) &&
            ((supplied == NULL) || (!cellul.eq(&(CatenaFinitaCfg){
                                       .src = supplied,
                                       .other = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg,
                                                           .catalog_read = args->catalog, .text_u32 = arg->text[1]),
                                       .cap = CLI_POOL_BYTES}))))
        {
            return 0;
        }
        if ((arg->kind == CLI_ARG_INPUT) && ((supplied == NULL) || (supplied[0] == '\0')))
        {
            return 0;
        }
    }
    return 1;
}

int cli_runner_check(const CliRunnerErrMsgCfg *args)
{
    const CliJob *const job_at = &args->catalog->job[args->job];
    for (unsigned int at = 0u; at < job_at->params; at += 1u)
    {
        const CliParam *const param = &args->catalog->param[job_at->param_first + at];
        const char *const key = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = args->catalog,
                                           .text_u32 = param->key);
        unsigned int count = 0u;
        const unsigned int *const items = EMBED_CALL(cli_catalog_key.values_get, CliCatalogKeyCfg,
                                                     .values = args->values, .key = key, .count = &count);
        int supplied = 0;
        for (unsigned int each = 0u; each < count; each += 1u)
        {
            supplied = supplied || (EMBED_CALL(cli_catalog_text_values.values_at, CliCatalogTextValuesCfg,
                                               .values = args->values, .text_u32 = items[each])[0] != '\0');
        }
        if (param->required && !supplied)
        {
            mmgr_line_put(args->err_msg, key);
            mmgr_line_put(args->err_msg, " needs a value");
            return 0;
        }
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = param->kind, .other = "choice", .cap = CLI_POOL_BYTES})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = param->kind, .other = "many", .cap = CLI_POOL_BYTES})))
        {
            continue;
        }
        for (unsigned int each = 0u; each < count; each += 1u)
        {
            const char *const value = EMBED_CALL(cli_catalog_text_values.values_at, CliCatalogTextValuesCfg,
                                                 .values = args->values, .text_u32 = items[each]);
            int among = value[0] == '\0';
            for (unsigned int choice = 0u; !among && (choice < param->choices); choice += 1u)
            {
                among = cellul.eq(&(CatenaFinitaCfg){
                    .src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = args->catalog,
                                      .text_u32 = args->catalog->list[param->first + choice]),
                    .other = value,
                    .cap = CLI_POOL_BYTES});
            }
            if (!among)
            {
                mmgr_line_put(args->err_msg, value);
                mmgr_line_put(args->err_msg, " is not one of ");
                mmgr_line_put(args->err_msg, key);
                mmgr_line_put(args->err_msg, "'s values");
                return 0;
            }
        }
    }
    return 1;
}
