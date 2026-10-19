// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner.c: the programs a job's steps run, a step's command made from its arguments and the values given, with the
// line the window shows for it, and whether values do for a job
#include "runner.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"

void cli_command_begun(CliCommand *command, const char *program)
{
    command->words.count = 0u;
    command->words.used = 0u;
    command->words.full = 0;
    command->words.word[0] = NULL;
    command->has_folder = 0;
    command->folder[0] = '\0';
    command->settings = 0u;
    command->used = 0u;
    cli_words_put(&command->words, program);
}

void cli_command_word(CliCommand *command, const char *word)
{
    cli_words_put(&command->words, word);
}

// a text kept among the command's: where it starts, NULL where it does not fit
static const char *cli_kept(CliCommand *command, const char *text)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((command->used + size) > CLI_COMMAND_TEXT)
    {
        command->words.full = 1;
        return NULL;
    }
    char *const at = command->text + command->used;
    memor.cpy(&(MemoriaCfg){.dst = at, .src = text, .bytes = size});
    // the texts are below CLI_COMMAND_TEXT
    command->used += (unsigned int)size;
    return at;
}

void cli_command_set(CliCommand *command, const char *name, const char *value)
{
    for (unsigned int at = 0u; at < command->settings; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = command->setting[at].name, .other = name, .cap = CLI_CELLS_POOL}))
        {
            command->setting[at].value = (value != NULL) ? cli_kept(command, value) : NULL;
            return;
        }
    }
    if (command->settings >= CLI_COMMAND_SETTINGS)
    {
        command->words.full = 1;
        return;
    }
    const CliSetting setting = {cli_kept(command, name), (value != NULL) ? cli_kept(command, value) : NULL};
    command->setting[command->settings] = setting;
    command->settings += (setting.name != NULL) ? 1u : 0u;
}

int cli_command_sets(const CliCommand *command, const char *name)
{
    for (unsigned int at = 0u; at < command->settings; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = command->setting[at].name, .other = name, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

void cli_command_folder(CliCommand *command, const char *folder)
{
    memor.cpy(&(MemoriaCfg){.dst = command->folder,
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    command->has_folder = 1;
}

void cli_lines_put(CliLines *lines, const char *text, unsigned long long length)
{
    if ((length + 1ull) > CLI_LINES_TEXT)
    {
        return;
    }
    if ((lines->count >= CLI_LINES_MOST) || ((lines->used + length + 1ull) > CLI_LINES_TEXT))
    {
        // the oldest half let go, the newer moved down
        const unsigned int kept = lines->count / 2u;
        const unsigned int from = lines->start[lines->count - kept];
        for (unsigned int at = 0u; at < kept; at += 1u)
        {
            lines->start[at] = lines->start[lines->count - kept + at] - from;
        }
        for (unsigned int byte = from; byte < lines->used; byte += 1u)
        {
            lines->text[byte - from] = lines->text[byte];
        }
        lines->used -= from;
        lines->count = kept;
        if ((lines->used + length + 1ull) > CLI_LINES_TEXT)
        {
            lines->count = 0u;
            lines->used = 0u;
        }
    }
    lines->start[lines->count] = lines->used;
    memor.cpy(&(MemoriaCfg){.dst = lines->text + lines->used, .src = text, .bytes = length});
    lines->text[lines->used + length] = '\0';
    // a line is below CLI_LINES_TEXT, checked above
    lines->used += (unsigned int)length + 1u;
    lines->count += 1u;
}

const char *cli_lines_at(const CliLines *lines, unsigned int index)
{
    return lines->text + lines->start[index];
}

// a file in a folder joined, `under` and `name`, where it is a file: 1, its path into `path`
static int cli_found_in(const char *under, const char *name, char *path)
{
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, under);
    cli_line_text(&line, name);
    return (cli_line_closed(&line) != 0ull) && query_is_file(path);
}

// The bash that runs the tree's scripts. On Windows that is Git's, found beside git itself, because the bash System32
// offers is WSL's and reads none of these paths
int cli_runner_bash(char *path, CliLine *said)
{
    if (query_environment_read("ORIOR_BASH", path, CLI_PATH_LONGEST) || cli_toolchains_chosen_program("bash", path))
    {
        return 1;
    }
#if !defined(_WIN32)
    memor.cpy(&(MemoriaCfg){.dst = path,
                            .src = "bash",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "bash", .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
#else
    static char s_folders[256][CLI_PATH_LONGEST];
    const unsigned int count = cli_path_folders(s_folders, 256u);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char git[CLI_PATH_LONGEST];
        if (!cli_found_in(s_folders[at], "\\git.exe", git))
        {
            continue;
        }
        // the folder above the one git stands in
        char *cut = cli_text_last(s_folders[at],
                                  cellul.len(&(CatenaFinitaCfg){.src = s_folders[at], .cap = CLI_CELLS_POOL}), '\\');
        char *const forward = cli_text_last(
            s_folders[at], cellul.len(&(CatenaFinitaCfg){.src = s_folders[at], .cap = CLI_CELLS_POOL}), '/');
        cut = ((forward != NULL) && ((cut == NULL) || (forward > cut))) ? forward : cut;
        if (cut == NULL)
        {
            continue;
        }
        cut[0] = '\0';
        if (cli_found_in(s_folders[at], "\\bin\\bash.exe", path) ||
            cli_found_in(s_folders[at], "\\usr\\bin\\bash.exe", path))
        {
            return 1;
        }
    }
    static const char *const s_bases[3] = {"ProgramFiles", "ProgramW6432", "LOCALAPPDATA"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char base[CLI_PATH_LONGEST];
        if (query_environment_read(s_bases[at], base, sizeof(base)) &&
            (cli_found_in(base, "\\Git\\bin\\bash.exe", path) ||
             cli_found_in(base, "\\Programs\\Git\\bin\\bash.exe", path)))
        {
            return 1;
        }
    }
    cli_line_text(said, "no Git bash found: install Git for Windows, or name a bash in ORIOR_BASH");
    return 0;
#endif
}

void cli_runner_python(char *path)
{
    if (query_environment_read("ORIOR_PYTHON", path, CLI_PATH_LONGEST) || cli_toolchains_chosen_program("python", path))
    {
        return;
    }
#if defined(_WIN32)
    memor.cpy(&(MemoriaCfg){.dst = path,
                            .src = "python",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python", .cap = CLI_CELLS_POOL}) + 1u});
#else
    memor.cpy(&(MemoriaCfg){.dst = path,
                            .src = "python3",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python3", .cap = CLI_CELLS_POOL}) + 1u});
#endif
}

int cli_runner_built(const char *root, const char *name, char *path, CliLine *said)
{
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, root);
    cli_line_text(&line, "/build/");
    cli_line_text(&line, name);
#if defined(_WIN32)
    cli_line_text(&line, ".exe");
#endif
    if ((cli_line_closed(&line) != 0ull) && query_is_file(path))
    {
        return 1;
    }
    cli_line_text(said, "no ");
    cli_line_text(said, name);
#if defined(_WIN32)
    cli_line_text(said, ".exe");
#endif
    cli_line_text(said, " under build/: run its build job first");
    return 0;
}

static int cli_space(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

int cli_runner_shell_words(const char *text, CliWords *words, CliLine *said)
{
    static char s_word[CLI_WORDS_TEXT];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    unsigned long long length = 0ull;
    int started = 0;
    char quote = '\0';
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        const char letter = at[0];
        if ((quote != '\0') && (letter == quote))
        {
            quote = '\0';
        }
        else if ((quote == '"') && (letter == '\\'))
        {
            s_word[length] = (at[1] != '\0') ? at[1] : '\\';
            length += (length + 1u < sizeof(s_word)) ? 1ull : 0ull;
            at += (at[1] != '\0') ? 1 : 0;
        }
        else if (quote != '\0')
        {
            s_word[length] = letter;
            length += (length + 1u < sizeof(s_word)) ? 1ull : 0ull;
        }
        else if ((letter == '\'') || (letter == '"'))
        {
            quote = letter;
            started = 1;
        }
        else if (cli_space(letter))
        {
            if (started)
            {
                s_word[length] = '\0';
                cli_words_put(words, s_word);
                length = 0ull;
                started = 0;
            }
        }
        else
        {
            s_word[length] = letter;
            length += (length + 1u < sizeof(s_word)) ? 1ull : 0ull;
            started = 1;
        }
    }
    if (quote != '\0')
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "a quote is left open in: ");
        cli_line_text(said, text);
        return 0;
    }
    if (started)
    {
        s_word[length] = '\0';
        cli_words_put(words, s_word);
    }
    cli_lock_give(&s_lock);
    return 1;
}

static int cli_runner_text_words(const char *variable, char *text, size_t room, CliWords *words);

int cli_runner_program_words(const char *variable, CliWords *words)
{
    static char s_text[QUERY_ENVIRONMENT_LONGEST];
    static CliLock s_text_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_text_lock);
    const int words_read = cli_runner_text_words(variable, s_text, sizeof(s_text), words);
    cli_lock_give(&s_text_lock);
    return words_read;
}

// the program `variable` names, read into `text`, which holds `room`, as its words put into `words`: 1, or 0
// where it is not set or names none
static int cli_runner_text_words(const char *variable, char *text, size_t room, CliWords *words)
{
    if (!query_environment_read(variable, text, room))
    {
        return 0;
    }
    const char *from = text;
    char *to = text + cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    while ((from < to) && cli_space(from[0]))
    {
        from += 1;
    }
    while ((to > from) && cli_space(to[-1]))
    {
        to -= 1;
    }
    to[0] = '\0';
    if (query_is_file(from))
    {
        cli_words_put(words, from);
        return 1;
    }
    char unsaid[CLI_PATH_LONGEST];
    CliLine said = {unsaid, sizeof(unsaid), 0ull};
    return cli_runner_shell_words(from, words, &said) && (words->count != 0u);
}

// the first value of `key` that is not empty, NULL where it has none
static const char *cli_first(const CliValues *values, const char *key)
{
    const char *const first = cli_values_first(values, key);
    return ((first != NULL) && (first[0] != '\0')) ? first : NULL;
}

int cli_runner_runs(const CliCatalog *catalog, const CliStep *step, const CliValues *values)
{
    for (unsigned int at = 0u; at < step->args; at += 1u)
    {
        const CliArg *const arg = &catalog->arg[step->first + at];
        const char *const key = cli_catalog_at(catalog, arg->text[0]);
        const char *const given = cli_values_first(values, key);
        if ((arg->kind == CLI_ARG_ONLY) &&
            ((given == NULL) ||
             (!cellul.eq(&(CatenaFinitaCfg){
                 .src = given, .other = cli_catalog_at(catalog, arg->text[1]), .cap = CLI_CELLS_POOL}))))
        {
            return 0;
        }
        if ((arg->kind == CLI_ARG_GIVEN) && ((given == NULL) || (given[0] == '\0')))
        {
            return 0;
        }
    }
    return 1;
}

int cli_runner_check(const CliCatalog *catalog, unsigned int job, const CliValues *values, CliLine *said)
{
    const CliJob *const held = &catalog->job[job];
    for (unsigned int at = 0u; at < held->params; at += 1u)
    {
        const CliParam *const param = &catalog->param[held->param_first + at];
        const char *const key = cli_catalog_at(catalog, param->key);
        unsigned int count = 0u;
        const unsigned int *const items = cli_values_of(values, key, &count);
        int given = 0;
        for (unsigned int each = 0u; each < count; each += 1u)
        {
            given = given || (cli_values_at(values, items[each])[0] != '\0');
        }
        if (param->required && !given)
        {
            cli_line_text(said, key);
            cli_line_text(said, " needs a value");
            return 0;
        }
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = param->kind, .other = "choice", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = param->kind, .other = "many", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        for (unsigned int each = 0u; each < count; each += 1u)
        {
            const char *const value = cli_values_at(values, items[each]);
            int among = value[0] == '\0';
            for (unsigned int choice = 0u; !among && (choice < param->choices); choice += 1u)
            {
                among =
                    cellul.eq(&(CatenaFinitaCfg){.src = cli_catalog_at(catalog, catalog->list[param->first + choice]),
                                                 .other = value,
                                                 .cap = CLI_CELLS_POOL});
            }
            if (!among)
            {
                cli_line_text(said, value);
                cli_line_text(said, " is not one of ");
                cli_line_text(said, key);
                cli_line_text(said, "'s values");
                return 0;
            }
        }
    }
    return 1;
}
