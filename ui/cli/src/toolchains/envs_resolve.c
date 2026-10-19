// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_resolve.c: an environment of the tree made ready to use, Pipenv and Nix asked where theirs are, and the
// packages its Python has not installed
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "envs.h"

// The room a resolving works in: the toolchain found, the PATH it runs with, and what Pipenv or Nix answered, a cell
// sized to the answer
typedef struct
{
    CliFound found;
    char path[CLI_PATH_TEXT];
    char *said;
    unsigned long long said_size;
} CliEnvRoom;

static CliEnvRoom *s_room;
static CliLock s_lock = CLI_LOCK_INIT;

// the Python and the bin folder of a virtual environment's folder into `environment`: 1, or 0 with why said
static int cli_venv_at(const char *folder, CliTreeEnvironment *environment, CliLine *said)
{
#if defined(_WIN32)
    static const char *const s_bin = "Scripts";
    static const char *const s_python = "python.exe";
#else
    static const char *const s_bin = "bin";
    static const char *const s_python = "python";
#endif
    char bin[CLI_PATH_LONGEST];
    char python[CLI_PATH_LONGEST];
    cli_full(folder, s_bin, bin);
    cli_full(bin, s_python, python);
    if (!query_is_file(python))
    {
        cli_line_text(said, folder);
        cli_line_text(said, " holds no Python");
        return 0;
    }
    environment->bins = 1u;
    memor.cpy(&(MemoriaCfg){.dst = environment->bin[0],
                            .src = bin,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = bin, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = environment->python,
                            .src = python,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_CELLS_POOL}) + 1u});
    environment->has_python = 1;
    environment->place[0] = '\0';
    return 1;
}

// the output of the program the manifest names `id`, run in `folder` with `args`, into the room: 1, or 0 with why
// said
static int cli_asked(const char *id, const char *folder, const char *const *args, unsigned int count, CliLine *said)
{
    CliJson json;
    char why[CLI_NAME_LONGEST];
    CliLine unknown = {why, sizeof(why), 0ull};
    const unsigned int tool = cli_toolchains_tool(&json, cli_toolchains_manifest(&json, 0u, 0ull), id, &unknown);
    if (tool == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        cli_line_text(said, id);
        cli_line_text(said, " is not in the manifest");
        return 0;
    }
    const char *const shown = cli_json_string(&json, cli_json_member(&json, tool, "name"), NULL);
    char name[CLI_NAME_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = name,
        .src = (shown != NULL) ? shown : id,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (shown != NULL) ? shown : id, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned int folders = 0u;
    char (*const path)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&folders);
    cli_toolchains_find(&json, tool, (const char (*)[CLI_PATH_LONGEST])path, folders, &s_room->found);
    cli_cell_given(path);
    cli_json_given(&json);
    if (!s_room->found.has_program)
    {
        cli_line_text(said, name);
        cli_line_text(said, " is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    const char *words[16] = {s_room->found.program};
    for (unsigned int at = 0u; (at < count) && (at < 14u); at += 1u)
    {
        words[at + 1u] = args[at];
    }
    words[((count < 14u) ? count : 14u) + 1u] = NULL;
    cli_toolchains_run_path(s_room->path, sizeof(s_room->path));
    const CliSetting settings[2] = {{"PATH", s_room->path}, {"PIPENV_IGNORE_VIRTUALENVS", "1"}};
    CliRan *const ran = cli_ran_taken();
    CliLine unstarted = {why, sizeof(why), 0ull};
    if (!cli_ran(words, folder, settings, 2u, ran, &unstarted))
    {
        cli_ran_given(ran);
        cli_line_text(said, name);
        cli_line_text(said, ": ");
        cli_line_counted(said, why, unstarted.at);
        return 0;
    }
    if (ran->code != 0ll)
    {
        // the last line of what it said, trimmed, or that it failed
        unsigned long long end = ran->err_size;
        while ((end > 0ull) &&
               ((ran->err[end - 1ull] == ' ') || ((ran->err[end - 1ull] >= '\t') && (ran->err[end - 1ull] <= '\r'))))
        {
            end -= 1ull;
        }
        unsigned long long start = end;
        while ((start > 0ull) && (ran->err[start - 1ull] != '\n'))
        {
            start -= 1ull;
        }
        unsigned long long first = 0ull;
        while ((first < end) && ((ran->err[first] == ' ') || ((ran->err[first] >= '\t') && (ran->err[first] <= '\r'))))
        {
            first += 1ull;
        }
        start = (start > first) ? start : first;
        cli_line_text(said, name);
        cli_line_text(said, ": ");
        if (end > start)
        {
            cli_lossy(said, (const unsigned char *)ran->err + start, end - start);
        }
        else
        {
            cli_line_text(said, "it failed");
        }
        cli_ran_given(ran);
        return 0;
    }
    cli_cell_given(s_room->said);
    s_room->said = cli_cell_taken(ran->out_size + 1ull, "what an environment's tool answered");
    s_room->said_size = (s_room->said != NULL) ? ran->out_size : 0ull;
    if (s_room->said != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_room->said, .src = ran->out, .bytes = ran->out_size});
        s_room->said[ran->out_size] = '\0';
    }
    cli_ran_given(ran);
    if (s_room->said == NULL)
    {
        cli_line_text(said, name);
        cli_line_text(said, ": its answer could not be kept");
        return 0;
    }
    return 1;
}

// the folders of a Nix development shell's PATH that are in its store, and its Python where one of them holds it
static int cli_nix_read(CliTreeEnvironment *environment, CliLine *said)
{
    CliJson json;
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, s_room->said, s_room->said_size, 0u, 0ull, &unread);
    if (root == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        cli_line_text(said, "Nix: ");
        cli_line_counted(said, why, unread.at);
        return 0;
    }
    const unsigned int path = cli_json_member(&json, cli_json_member(&json, root, "variables"), "PATH");
    unsigned long long length = 0ull;
    const char *folders = cli_json_string(&json, cli_json_member(&json, path, "value"), &length);
    folders = (folders != NULL) ? folders : "";
#if defined(_WIN32)
    const char parting = ';';
#else
    const char parting = ':';
#endif
    environment->bins = 0u;
    environment->has_python = 0;
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at <= length; at += 1ull)
    {
        if ((at < length) && (folders[at] != parting))
        {
            continue;
        }
        char folder[CLI_PATH_LONGEST];
        const unsigned long long size = at - from;
        const char *const start = folders + from;
        from = at + 1ull;
        if ((size >= CLI_PATH_LONGEST) || (environment->bins >= CLI_ENVIRONMENT_BINS))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = start, .bytes = size});
        folder[size] = '\0';
        if (cellul.find(&(CatenaFinitaCfg){
                .src = folder, .cap = CLI_CELLS_POOL, .other = "/nix/store/", .other_cap = CLI_CELLS_POOL}) == NULL)
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = environment->bin[environment->bins],
                                .src = folder,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
        environment->bins += 1u;
        char python[CLI_PATH_LONGEST];
        cli_full(folder, "python3", python);
        if (!environment->has_python && query_is_file(python))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = environment->python,
                              .src = python,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_CELLS_POOL}) + 1u});
            environment->has_python = 1;
        }
    }
    cli_json_given(&json);
    return 1;
}

int cli_envs_resolve(const char *root, const char *kind, const char *place, CliTreeEnvironment *environment,
                     CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (place[0] == '\0')
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        cli_full(root, place, folder);
    }
    cli_lock_take(&s_lock);
    s_room = cli_cell_taken(sizeof(*s_room), "an environment's resolving");
    if (s_room == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "an environment's resolving could not be taken");
        return 0;
    }
    s_room->said = NULL;
    s_room->said_size = 0ull;
    int made = 0;
    if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "venv", .cap = CLI_CELLS_POOL}))
    {
        made = cli_venv_at(folder, environment, said);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "pipenv", .cap = CLI_CELLS_POOL}))
    {
        static const char *const s_args[1] = {"--venv"};
        made = cli_asked("pipenv", folder, s_args, 1u, said);
        if (made)
        {
            // the folder Pipenv gave, trimmed
            unsigned long long start = 0ull;
            unsigned long long end = s_room->said_size;
            while ((start < end) &&
                   ((s_room->said[start] == ' ') || ((s_room->said[start] >= '\t') && (s_room->said[start] <= '\r'))))
            {
                start += 1ull;
            }
            while ((end > start) && ((s_room->said[end - 1ull] == ' ') ||
                                     ((s_room->said[end - 1ull] >= '\t') && (s_room->said[end - 1ull] <= '\r'))))
            {
                end -= 1ull;
            }
            s_room->said[end] = '\0';
            made = cli_venv_at(s_room->said + start, environment, said);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "nix", .cap = CLI_CELLS_POOL}))
    {
        static const char *const s_args[2] = {"print-dev-env", "--json"};
        made = cli_asked("nix", folder, s_args, 2u, said) && cli_nix_read(environment, said);
    }
    else
    {
        cli_line_text(said, kind);
        cli_line_text(said, " is no kind of environment");
    }
    if (made)
    {
        memor.cpy(&(MemoriaCfg){.dst = environment->place,
                                .src = place,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_cell_given(s_room->said);
    cli_cell_given(s_room);
    s_room = NULL;
    cli_lock_give(&s_lock);
    return made;
}

// `text` of `length` bytes as pip names a package, in lower case with `_` written as `-`, compared with another
static int cli_package_same(const char *one, unsigned long long one_length, const char *other,
                            unsigned long long other_length)
{
    if (one_length != other_length)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < one_length; at += 1ull)
    {
        char left = ((one[at] >= 'A') && (one[at] <= 'Z')) ? (char)(one[at] + 32) : one[at];
        char right = ((other[at] >= 'A') && (other[at] <= 'Z')) ? (char)(other[at] + 32) : other[at];
        left = (left == '_') ? '-' : left;
        right = (right == '_') ? '-' : right;
        if (left != right)
        {
            return 0;
        }
    }
    return 1;
}

void cli_envs_missing(const CliTreeEnvironment *environment, const char *const *requires, unsigned int count,
                      CliLine *out)
{
    // pip's list read into a tree sized to it; where it cannot be read none is said to be missing, as where the
    // environment has no Python
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    unsigned int listed = CLI_JSON_NONE;
    int started = 0;
    if (environment->has_python)
    {
        const char *const words[8] = {environment->python,           "-m", "pip", "list", "--format", "json",
                                      "--disable-pip-version-check", NULL};
        CliRan *const ran = cli_ran_taken();
        char why[CLI_NAME_LONGEST];
        CliLine unstarted = {why, sizeof(why), 0ull};
        started = cli_ran(words, NULL, NULL, 0u, ran, &unstarted);
        if (started)
        {
            CliLine unread = {why, sizeof(why), 0ull};
            listed = cli_json_taken(&json, ran->out, ran->out_size, 0u, 0ull, &unread);
        }
        cli_ran_given(ran);
    }
    cli_line_character(out, '[');
    unsigned int written = 0u;
    for (unsigned int index = 0u; started && (index < count); index += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = requires[index], .cap = CLI_CELLS_POOL});
        int installed = 0;
        for (unsigned int at = 0u; !installed && (cli_json_item(&json, listed, at) != CLI_JSON_NONE); at += 1u)
        {
            unsigned long long name_length = 0ull;
            const char *const name =
                cli_json_string(&json, cli_json_member(&json, cli_json_item(&json, listed, at), "name"), &name_length);
            installed = (name != NULL) && cli_package_same(name, name_length, requires[index], length);
        }
        if (!installed)
        {
            cli_line_text(out, (written != 0u) ? "," : "");
            cli_json_quoted(out, requires[index], length);
            written += 1u;
        }
    }
    cli_line_character(out, ']');
    cli_json_given(&json);
}
