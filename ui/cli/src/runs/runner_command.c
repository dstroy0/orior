// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_command.c: a step's arguments made from the values given, its program found, its environment set, and the
// line the window shows for it, each setting leading it as a shell writes it
#include "../code/git.h"
#include "../programs/executables.h"
#include "../toolchains/toolchains.h"
#include "runner.h"

// the first value of `key` that is not empty, NULL where it has none
static const char *cli_first(const CliValues *values, const char *key)
{
    const char *const first = cli_values_first(values, key);
    return ((first != NULL) && (first[0] != '\0')) ? first : NULL;
}

// A path given to a bash script, written with forward slashes, which Git's bash reads on Windows and every other bash
// reads already, into `out`, which holds CLI_PATH_LONGEST * 4
static const char *cli_for_bash(const char *value, char *out)
{
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = value, .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
#if defined(_WIN32)
    for (char *at = out; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
#endif
    return out;
}

// The words of a step's arguments put into `words`: 1, or 0 with why said
static int cli_step_arguments(const CliCatalog *catalog, const CliStep *step, const CliValues *values, CliWords *words,
                              CliLine *said)
{
    for (unsigned int at = 0u; at < step->args; at += 1u)
    {
        const CliArg *const arg = &catalog->arg[step->first + at];
        const char *const a = cli_catalog_at(catalog, arg->text[0]);
        const char *const b = cli_catalog_at(catalog, arg->text[1]);
        const char *const c = cli_catalog_at(catalog, arg->text[2]);
        switch (arg->kind)
        {
        case CLI_ARG_LIT:
            cli_words_put(words, a);
            break;
        case CLI_ARG_VALUE:
            if (cli_first(values, a) != NULL)
            {
                cli_words_put(words, cli_first(values, a));
            }
            break;
        case CLI_ARG_FLAG:
            if (cli_first(values, b) != NULL)
            {
                cli_words_put(words, a);
                cli_words_put(words, cli_first(values, b));
            }
            break;
        case CLI_ARG_EACH: {
            unsigned int count = 0u;
            const unsigned int *const items = cli_values_of(values, b, &count);
            for (unsigned int each = 0u; each < count; each += 1u)
            {
                if (cli_values_at(values, items[each])[0] != '\0')
                {
                    cli_words_put(words, a);
                    cli_words_put(words, cli_values_at(values, items[each]));
                }
            }
            break;
        }
        case CLI_ARG_WORDS: {
            static CliWords s_said;
            s_said.count = 0u;
            s_said.used = 0u;
            s_said.full = 0;
            const char *const text = cli_first(values, a);
            if (!cli_runner_shell_words((text != NULL) ? text : "", &s_said, said))
            {
                return 0;
            }
            if (arg->separated && (s_said.count != 0u))
            {
                cli_words_put(words, "--");
            }
            for (unsigned int each = 0u; each < s_said.count; each += 1u)
            {
                cli_words_put(words, s_said.word[each]);
            }
            break;
        }
        case CLI_ARG_AROUND:
            if (cli_first(values, b) != NULL)
            {
                char word[CLI_PATH_LONGEST * 4u];
                CliLine line = {word, sizeof(word), 0ull};
                cli_line_text(&line, a);
                cli_line_text(&line, cli_first(values, b));
                cli_line_text(&line, c);
                cli_line_closed(&line);
                cli_words_put(words, word);
            }
            break;
        case CLI_ARG_WHEN:
            if ((cli_first(values, a) != NULL) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = cli_first(values, a), .other = b, .cap = CLI_CELLS_POOL})))
            {
                for (unsigned int each = 0u; each < arg->words; each += 1u)
                {
                    cli_words_put(words, cli_catalog_at(catalog, catalog->list[arg->first + each]));
                }
            }
            break;
        case CLI_ARG_FORMAT: {
            // each {key} filled with the first value given of that key, the keys taken in turn
            static char s_filled[2][CLI_PATH_LONGEST * 4u];
            unsigned int now = 0u;
            memor.cpy(&(MemoriaCfg){.dst = s_filled[now],
                                    .src = a,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = a, .cap = CLI_CELLS_POOL}) + 1u});
            for (unsigned int key = 0u; key < values->keys; key += 1u)
            {
                char marker[CLI_NAME_LONGEST + 2u];
                CliLine named = {marker, sizeof(marker), 0ull};
                cli_line_character(&named, '{');
                cli_line_text(&named, values->key[key]);
                cli_line_character(&named, '}');
                const unsigned long long marker_length = cli_line_closed(&named);
                const char *const given =
                    (values->count[key] != 0u) ? cli_values_at(values, values->item[values->first[key]]) : "";
                CliLine next = {s_filled[1u - now], CLI_PATH_LONGEST * 4u, 0ull};
                const char *rest = s_filled[now];
                for (const char *found = cellul.find(&(CatenaFinitaCfg){
                         .src = rest, .cap = CLI_CELLS_POOL, .other = marker, .other_cap = CLI_CELLS_POOL});
                     found != NULL;
                     found = cellul.find(&(CatenaFinitaCfg){
                         .src = rest, .cap = CLI_CELLS_POOL, .other = marker, .other_cap = CLI_CELLS_POOL}))
                {
                    cli_line_counted(&next, rest, (unsigned long long)(found - rest));
                    cli_line_text(&next, given);
                    rest = found + marker_length;
                }
                cli_line_text(&next, rest);
                cli_line_closed(&next);
                now = 1u - now;
            }
            cli_words_put(words, s_filled[now]);
            break;
        }
        case CLI_ARG_FLAG_BUT:
            if ((cli_first(values, b) != NULL) &&
                (!cellul.eq(&(CatenaFinitaCfg){.src = cli_first(values, b), .other = c, .cap = CLI_CELLS_POOL})))
            {
                cli_words_put(words, a);
                cli_words_put(words, cli_first(values, b));
            }
            break;
        default:
            break;
        }
    }
    return 1;
}

// A setting put on the command and leading its line as a shell writes it
static void cli_setting(CliCommand *command, CliLine *set, const char *name, const char *value)
{
    cli_command_set(command, name, value);
    cli_line_text(set, (set->at != 0ull) ? "\n" : "");
    cli_line_text(set, name);
    cli_line_character(set, '=');
    cli_line_text(set, value);
}

// The settings a cross build's kind gives, put on the command
typedef struct
{
    CliCommand *command;
    CliLine *set;
} CliCrossed;

static void cli_cross_each(void *context, const char *name, const char *value)
{
    CliCrossed *const crossed = (CliCrossed *)context;
    cli_setting(crossed->command, crossed->set, name, value);
}

// the settings a step's arguments give put on the command, each line after the one before in `set`, a `cd` first in
// `moved` where the step runs in a folder of the tree
static void cli_settings(const char *root, const CliCatalog *catalog, const CliStep *step, const CliValues *values,
                         CliCommand *command, CliLine *set, CliLine *moved)
{
    char bashed[CLI_PATH_LONGEST * 4u];
    for (unsigned int at = 0u; at < step->args; at += 1u)
    {
        const CliArg *const arg = &catalog->arg[step->first + at];
        const char *const a = cli_catalog_at(catalog, arg->text[0]);
        const char *const b = cli_catalog_at(catalog, arg->text[1]);
        const char *const c = cli_catalog_at(catalog, arg->text[2]);
        const char *const d = cli_catalog_at(catalog, arg->text[3]);
        const char *const given_a = cli_first(values, a);
        const char *const given_b = cli_first(values, b);
        const char *const raw_a = cli_values_first(values, a);
        if ((arg->kind == CLI_ARG_ENV) && (given_a != NULL))
        {
            cli_setting(command, set, a, (step->program == CLI_PROGRAM_BASH) ? cli_for_bash(given_a, bashed) : given_a);
        }
        else if (arg->kind == CLI_ARG_SET)
        {
            cli_setting(command, set, a, b);
        }
        else if ((arg->kind == CLI_ARG_SET_FROM) && (given_b != NULL))
        {
            cli_setting(command, set, a, given_b);
        }
        else if ((arg->kind == CLI_ARG_SET_WHEN) && (raw_a != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = raw_a, .other = b, .cap = CLI_CELLS_POOL})))
        {
            cli_setting(command, set, c, d);
        }
        else if ((arg->kind == CLI_ARG_ENV_BUT) && (given_a != NULL) &&
                 (!cellul.eq(&(CatenaFinitaCfg){.src = given_a, .other = b, .cap = CLI_CELLS_POOL})))
        {
            cli_setting(command, set, a, given_a);
        }
        else if (arg->kind == CLI_ARG_FOLDER)
        {
            char folder[CLI_PATH_LONGEST];
            cli_full(root, a, folder);
            cli_command_folder(command, folder);
            moved->at = 0ull;
            cli_line_text(moved, "cd ");
            cli_line_text(moved, a);
            cli_line_text(moved, " &&");
        }
        else if (arg->kind == CLI_ARG_SOURCE_DATE)
        {
            char epoch[32];
            char commit[CLI_NAME_LONGEST];
            long long when = 0ll;
            int clean = 0;
            if (!query_environment_read("SOURCE_DATE_EPOCH", epoch, sizeof(epoch)) &&
                cli_git_head(root, commit, &when, &clean))
            {
                CliLine line = {epoch, sizeof(epoch), 0ull};
                cli_line_signed(&line, when);
                cli_line_closed(&line);
                cli_setting(command, set, "SOURCE_DATE_EPOCH", epoch);
            }
        }
        else if (arg->kind == CLI_ARG_CROSS)
        {
            CliCrossed crossed = {command, set};
            cli_executables_cross_env(a, values, cli_cross_each, &crossed);
        }
        else if (arg->kind == CLI_ARG_UNPATH)
        {
            // the PATH every step runs with, less each folder that holds the program named
            static char s_kept[256][CLI_PATH_LONGEST];
            static char s_path[1u << 16u];
#if defined(_WIN32)
            const char separator = ';';
#else
            const char separator = ':';
#endif
            cli_toolchains_run_path(s_path, sizeof(s_path));
            const char *const names[1] = {a};
            unsigned int kept = 0u;
            const char *from = s_path;
            for (const char *place = s_path;; place += 1)
            {
                if ((place[0] == separator) || (place[0] == '\0'))
                {
                    char folder[CLI_PATH_LONGEST];
                    char found[CLI_PATH_LONGEST];
                    CliLine named = {folder, sizeof(folder), 0ull};
                    cli_line_counted(&named, from, (unsigned long long)(place - from));
                    if ((place > from) && (cli_line_closed(&named) != 0ull) && (kept < 256u) &&
                        !cli_toolchains_program_in(folder, names, 1u, found))
                    {
                        memor.cpy(&(MemoriaCfg){
                            .dst = s_kept[kept],
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
                        kept += 1u;
                    }
                    if (place[0] == '\0')
                    {
                        break;
                    }
                    from = place + 1;
                }
            }
            cli_path_joined((const char (*)[CLI_PATH_LONGEST])s_kept, kept, s_path, sizeof(s_path));
            cli_command_set(command, "PATH", s_path);
            cli_line_text(set, (set->at != 0ull) ? "\nPATH=<PATH without " : "PATH=<PATH without ");
            cli_line_text(set, a);
            cli_line_character(set, '>');
        }
        (void)c;
    }
}

// one word of a shown line put after the line, in quotes where it holds a space
static void cli_shown_word(CliLine *shown, const char *word, unsigned long long length, int *first)
{
    cli_line_text(shown, *first ? "" : " ");
    *first = 0;
    int spaced = 0;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        spaced = spaced || (word[at] == ' ');
    }
    cli_line_text(shown, spaced ? "\"" : "");
    cli_line_counted(shown, word, length);
    cli_line_text(shown, spaced ? "\"" : "");
}

int cli_runner_command(const char *root, const CliCatalog *catalog, const CliStep *step, const CliValues *values,
                       CliCommand *command, CliLine *shown, CliLine *said)
{
    static CliWords s_args;
    static char s_set[1u << 16u];
    s_args.count = 0u;
    s_args.used = 0u;
    s_args.full = 0;
    s_args.word[0] = NULL;
    if (!cli_step_arguments(catalog, step, values, &s_args, said))
    {
        return 0;
    }
    char program[CLI_PATH_LONGEST];
    char shown_program[CLI_PATH_LONGEST];
    const char *const text = cli_catalog_at(catalog, step->text);
    switch (step->program)
    {
    case CLI_PROGRAM_BASH:
        if (!cli_runner_bash(program, said))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = shown_program,
                                .src = "bash",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "bash", .cap = CLI_CELLS_POOL}) + 1u});
        break;
    case CLI_PROGRAM_PYTHON:
        cli_runner_python(program);
        memor.cpy(&(MemoriaCfg){.dst = shown_program,
                                .src = "python",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python", .cap = CLI_CELLS_POOL}) + 1u});
        break;
    case CLI_PROGRAM_BUILT:
        if (!cli_runner_built(root, text, program, said))
        {
            return 0;
        }
        cli_relative(root, program, shown_program);
        break;
    case CLI_PROGRAM_TOOL:
        if (!cli_executables_program(text, cli_catalog_at(catalog, step->tool_program), program, said))
        {
            return 0;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = shown_program,
                          .src = cli_catalog_at(catalog, step->tool_program),
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = cli_catalog_at(catalog, step->tool_program),
                                                                 .cap = CLI_CELLS_POOL}) +
                                   1u});
        break;
    case CLI_PROGRAM_ORIOR:
        if (!cli_program_own(program, sizeof(program)))
        {
            cli_line_text(said, "orior_cli itself: its program could not be found");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = shown_program,
                                .src = "orior",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "orior", .cap = CLI_CELLS_POOL}) + 1u});
        break;
    default:
        memor.cpy(&(MemoriaCfg){.dst = program,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = shown_program,
            .src =
                step->system ? "PATH=<the system's PATH> <the program the build made>" : "<the program the build made>",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = step->system
                                                              ? "PATH=<the system's PATH> <the program the build made>"
                                                              : "<the program the build made>",
                                                   .cap = CLI_CELLS_POOL}) +
                     1u});
        break;
    }
    cli_command_begun(command, program);
    if (step->program == CLI_PROGRAM_PYTHON)
    {
        cli_command_word(command, "-u");
    }
    char bashed[CLI_PATH_LONGEST * 4u];
    if ((step->program == CLI_PROGRAM_BASH) || (step->program == CLI_PROGRAM_PYTHON))
    {
        cli_command_word(command, text);
    }
    for (unsigned int at = 0u; at < s_args.count; at += 1u)
    {
        cli_command_word(command,
                         (step->program == CLI_PROGRAM_BASH) ? cli_for_bash(s_args.word[at], bashed) : s_args.word[at]);
    }
    cli_command_folder(command, root);
    CliLine set = {s_set, sizeof(s_set), 0ull};
    char moved_text[CLI_PATH_LONGEST + 8u];
    CliLine moved = {moved_text, sizeof(moved_text), 0ull};
    cli_settings(root, catalog, step, values, command, &set, &moved);
    // the tree's own tools come first on the PATH every step runs with, and the line says so
    static char s_tools[16][CLI_PATH_LONGEST];
    const unsigned int tools = cli_toolchains_tree_tools(s_tools, 16u);
    int path_set = 0;
    for (unsigned int at = 0u; at < step->args; at += 1u)
    {
        const CliArg *const arg = &catalog->arg[step->first + at];
        path_set = path_set || (arg->kind == CLI_ARG_UNPATH) ||
                   ((arg->kind == CLI_ARG_SET) &&
                    (cellul.eq(&(CatenaFinitaCfg){
                        .src = cli_catalog_at(catalog, arg->text[0]), .other = "PATH", .cap = CLI_CELLS_POOL})));
    }
    int first = 1;
    if (moved.at != 0ull)
    {
        cli_shown_word(shown, moved_text, moved.at, &first);
    }
    if ((tools != 0u) && !path_set)
    {
        char line[CLI_PATH_LONGEST * 4u];
        CliLine tools_line = {line, sizeof(line), 0ull};
        cli_line_text(&tools_line, "PATH=");
        for (unsigned int at = 0u; at < tools; at += 1u)
        {
            char relative[CLI_PATH_LONGEST];
            cli_relative(root, s_tools[at], relative);
            cli_line_text(&tools_line, (at != 0u) ? ":" : "");
            cli_line_text(&tools_line, relative);
        }
        cli_line_text(&tools_line, ":$PATH");
        const unsigned long long length = cli_line_fits(&tools_line) ? tools_line.at : 0ull;
        cli_shown_word(shown, line, length, &first);
    }
    // each setting, one to a line of the set, then the program and its words
    const unsigned long long set_length = cli_line_fits(&set) ? set.at : 0ull;
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at <= set_length; at += 1ull)
    {
        if (((at == set_length) || (s_set[at] == '\n')) && (at > from))
        {
            cli_shown_word(shown, s_set + from, at - from, &first);
        }
        from = ((at < set_length) && (s_set[at] == '\n')) ? (at + 1ull) : from;
    }
    cli_shown_word(shown, shown_program, cellul.len(&(CatenaFinitaCfg){.src = shown_program, .cap = CLI_CELLS_POOL}),
                   &first);
    if ((step->program == CLI_PROGRAM_BASH) || (step->program == CLI_PROGRAM_PYTHON))
    {
        cli_shown_word(shown, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &first);
    }
    for (unsigned int at = 0u; at < s_args.count; at += 1u)
    {
        const char *const word =
            (step->program == CLI_PROGRAM_BASH) ? cli_for_bash(s_args.word[at], bashed) : s_args.word[at];
        cli_shown_word(shown, word, cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}), &first);
    }
    if (command->words.full || s_args.full)
    {
        cli_line_text(said, "the step's command is longer than orior_cli holds");
        return 0;
    }
    return 1;
}

int cli_runner_shown(const char *root, const CliCatalog *catalog, unsigned int job, const CliValues *values,
                     void (*each)(void *context, const char *shown), void *context, CliLine *said)
{
    static CliCommand s_command;
    static char s_shown[CLI_COMMAND_TEXT];
    static CliLock s_lock = CLI_LOCK_INIT;
    const CliJob *const held = &catalog->job[job];
    cli_lock_take(&s_lock);
    for (unsigned int at = 0u; at < held->steps; at += 1u)
    {
        const CliStep *const step = &catalog->step[held->step_first + at];
        if (!cli_runner_runs(catalog, step, values))
        {
            continue;
        }
        CliLine shown = {s_shown, sizeof(s_shown), 0ull};
        if (!cli_runner_command(root, catalog, step, values, &s_command, &shown, said))
        {
            cli_lock_give(&s_lock);
            return 0;
        }
        cli_line_closed(&shown);
        each(context, s_shown);
    }
    cli_lock_give(&s_lock);
    return 1;
}
