// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_launch.c: a server started: its program found as its toolchain is, its options the projects it is given and
// what a tree whose code is held back or runs adds, its environment the PATH every step runs with; clangd given the
// build's database and its compilers, and Visual Studio's environment where one is cl
#include "../builds/compile_db.h"
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

#include "../text/text.h"

// how many folders below the top a search for projects goes; and the bytes the projects found and a server's options
// are begun to be written in, doubling as they fill
#define CLI_SERVERS_PROJECT_DEPTH 6u
#define CLI_SERVERS_FOUND_FIRST 1024u
#define CLI_SERVERS_OPTIONS_FIRST 4096u

// what parts the folders of a PATH
#if defined(_WIN32)
#define CLI_SERVERS_PATH_PARTS ';'
#else
#define CLI_SERVERS_PATH_PARTS ':'
#endif

// where what each server says on its own goes, a place for each server started, taken in turn
static CliServersEmit s_heard[64];
static unsigned int s_heard_next;

CliCellsTally g_cli_servers_launch_cells = CLI_CELLS_TALLY("cli_servers_launch");

// The search for projects: the file name looked for, the folder read and its depth, and the projects found, their
// paths one after another, each with its zero after it, in a growing line, and their count
typedef struct
{
    const char *name;
    const char *at;
    unsigned int at_depth;
    CliLine found;
    unsigned int count;
} CliServersProjects;

// What a server is started with, a cell taken as it starts and given back once it has: its program's folder, its
// words, the build's database clangd is given, and the PATH every step runs with
typedef struct
{
    CliFound found;
    CliWords words;
    CliDatabase database;
    char path[CLI_PATH_TEXT];
} CliServersLaunching;

// an entry of a folder the search reads: a folder gone into, no deeper than the search goes, or a project
static void launch_project_entry(void *context, const char *name, int is_folder)
{
    CliServersProjects *const projects = context;
    static const char *const s_not[4] = {"target", "build", "node_modules", "archive"};
    char path[CLI_PATH_LONGEST];
    if (is_folder)
    {
        int skipped = (name[0] == '.') || (projects->at_depth >= CLI_SERVERS_PROJECT_DEPTH);
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            skipped |= (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_not[at], .cap = CLI_CELLS_POOL}));
        }
        if (!skipped)
        {
            cli_full(projects->at, name, path);
            const char *const above = projects->at;
            projects->at = path;
            projects->at_depth += 1u;
            (void)query_folder_each(path, launch_project_entry, projects);
            projects->at = above;
            projects->at_depth -= 1u;
        }
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = projects->name, .cap = CLI_CELLS_POOL}))
    {
        cli_full(projects->at, name, path);
        cli_line_counted(&projects->found, path,
                         cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u);
        projects->count += 1u;
    }
}

void cli_servers_projects_in(const char *root, const char *name, CliLine *out)
{
    CliServersProjects projects = {name, root, 0u, {NULL, 0ull, 0ull, NULL, NULL, 0ull}, 0u};
    (void)cli_line_growing(&projects.found, CLI_SERVERS_FOUND_FIRST, "the projects a server is given, as found");
    (void)query_folder_each(root, launch_project_entry, &projects);
    // the projects in order, each by where it starts among those found
    const unsigned int count = cli_line_fits(&projects.found) ? projects.count : 0u;
    const char **const found =
        (count != 0u) ? cli_cell_taken_by(&g_cli_servers_launch_cells, count * sizeof(*found), "the projects, in order")
                      : NULL;
    const unsigned int listed = (found != NULL) ? count : 0u;
    const char *path = projects.found.out;
    for (unsigned int at = 0u; at < listed; at += 1u)
    {
        found[at] = path;
        path += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
    }
    for (unsigned int at = 1u; at < listed; at += 1u)
    {
        const char *const held = found[at];
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(found[place - 1u], held) > 0))
        {
            found[place] = found[place - 1u];
            place -= 1u;
        }
        found[place] = held;
    }
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < listed; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(out, (const unsigned char *)found[at],
                              cellul.len(&(CatenaFinitaCfg){.src = found[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_character(out, ']');
    cli_cell_given_by(&g_cli_servers_launch_cells, found);
    cli_cell_given(projects.found.out);
}

// the options a server starts with: the projects it is given, then what the tree adds, `null` where neither is
static void launch_options(const char *root, const CliServerSpec *spec, CliLine *out)
{
    const char *const added = cli_trust_held_back(root) ? spec->held : spec->trusted;
    const int adds = (added[0] == '{');
    if ((spec->projects[0] == '\0') && !adds)
    {
        cli_line_text(out, "null");
        return;
    }
    cli_line_character(out, '{');
    int members = 0;
    if (spec->projects[0] != '\0')
    {
        cli_line_text(out, "\"linkedProjects\":");
        cli_servers_projects_in(root, spec->projects, out);
        members = 1;
    }
    if (adds)
    {
        unsigned long long from = 1ull;
        unsigned long long to = cellul.len(&(CatenaFinitaCfg){.src = added, .cap = CLI_CELLS_POOL});
        to -= ((to > 1u) && (added[to - 1u] == '}')) ? 1u : 0u;
        unsigned long long inner_from = 0ull;
        unsigned long long inner_to = 0ull;
        cli_text_trimmed(added + from, to - from, &inner_from, &inner_to);
        if (inner_to > inner_from)
        {
            cli_line_text(out, members ? "," : "");
            cli_line_counted(out, added + from + inner_from, inner_to - inner_from);
        }
    }
    cli_line_character(out, '}');
}

CliLsp *cli_servers_launch(const char *root, const CliServerSpec *spec, const CliServersEmit *emit, CliLine *said)
{
    CliJson manifest_json;
    const unsigned int manifest = cli_toolchains_manifest(&manifest_json, 0u, 0ull);
    char quiet[256];
    CliLine ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int tool = cli_toolchains_tool(&manifest_json, manifest, spec->tool, &ignored);
    CliServersLaunching *const launching =
        cli_cell_taken_by(&g_cli_servers_launch_cells, sizeof(*launching), "what a server is started with");
    if (launching != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = &launching->found, .val = 0, .bytes = sizeof(launching->found)});
    }
    if ((launching != NULL) && (tool != CLI_JSON_NONE))
    {
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
        cli_toolchains_find(&manifest_json, tool, (const char (*)[CLI_PATH_LONGEST])folders, count, &launching->found);
        cli_cell_given(folders);
    }
    cli_json_given(&manifest_json);
    if (launching == NULL)
    {
        cli_line_text(said, "the cells cannot meet what ");
        cli_line_text(said, spec->program);
        cli_line_text(said, " is started with");
        return NULL;
    }
    char program[CLI_PATH_LONGEST];
    const char *const names[1] = {spec->program};
    if ((launching->found.folder[0] == '\0') || !cli_toolchains_program_in(launching->found.folder, names, 1u, program))
    {
        cli_line_text(said, spec->program);
        cli_line_text(said, " is not found: File, Toolchains opens ");
        cli_line_text(said, spec->name);
        cli_line_text(said, "'s install page or takes its folder");
        cli_cell_given_by(&g_cli_servers_launch_cells, launching);
        return NULL;
    }
    CliLine options = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_line_growing(&options, CLI_SERVERS_OPTIONS_FIRST, "the options a server starts with");
    launch_options(root, spec, &options);
    CliWords *const words = &launching->words;
    words->count = 0u;
    words->used = 0u;
    words->full = 0;
    for (unsigned int at = 0u; at < spec->args; at += 1u)
    {
        cli_words_put(words, spec->arg[at]);
    }
    cli_toolchains_run_path(launching->path, sizeof(launching->path));
    const CliVsEnvironment *vs = NULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = spec->program, .other = "clangd", .cap = CLI_CELLS_POOL}))
    {
        (void)cli_compile_db_refresh(root, &launching->database);
        cli_compile_db_words(&launching->database, words);
        vs = launching->database.msvc ? cli_compile_db_vs_environment() : NULL;
    }
    // the settings of the server's environment, the PATH and Visual Studio's where one is cl, in a cell of their count
    const unsigned int most = 1u + ((vs != NULL) ? vs->count : 0u);
    CliSetting *const setting =
        cli_cell_taken_by(&g_cli_servers_launch_cells, most * sizeof(*setting), "a server's environment");
    char *vs_path = NULL;
    unsigned int settings = (setting != NULL) ? 1u : 0u;
    if (setting != NULL)
    {
        setting[0] = (CliSetting){"PATH", launching->path};
    }
    for (unsigned int at = 0u; (setting != NULL) && (vs != NULL) && (at < vs->count); at += 1u)
    {
        char upper[8] = {0};
        for (unsigned int letter = 0u; (letter < 7u) && (vs->name[at][letter] != '\0'); letter += 1u)
        {
            const char one = vs->name[at][letter];
            upper[letter] = ((one >= 'a') && (one <= 'z')) ? (char)(one - 'a' + 'A') : one;
        }
        if ((cellul.len(&(CatenaFinitaCfg){.src = vs->name[at], .cap = CLI_CELLS_POOL}) == 4u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = upper, .other = "PATH", .cap = CLI_CELLS_POOL})))
        {
            // Visual Studio's PATH before the one every step runs with, in a cell of the two and the mark between
            const unsigned long long joined_size =
                cellul.len(&(CatenaFinitaCfg){.src = vs->value[at], .cap = CLI_CELLS_POOL}) +
                cellul.len(&(CatenaFinitaCfg){.src = launching->path, .cap = CLI_CELLS_POOL}) + 2ull;
            cli_cell_given_by(&g_cli_servers_launch_cells, vs_path);
            vs_path = cli_cell_taken_by(&g_cli_servers_launch_cells, joined_size, "Visual Studio's PATH");
            CliLine joined = {vs_path, (vs_path != NULL) ? joined_size : 0ull, 0ull};
            cli_line_text(&joined, vs->value[at]);
            cli_line_character(&joined, CLI_SERVERS_PATH_PARTS);
            cli_line_text(&joined, launching->path);
            setting[0].value = (cli_line_closed(&joined) != 0ull) ? vs_path : launching->path;
            continue;
        }
        setting[settings] = (CliSetting){vs->name[at], vs->value[at]};
        settings += 1u;
    }
    CliServersEmit *const heard = &s_heard[s_heard_next % 64u];
    s_heard_next += 1u;
    *heard = *emit;
    char why[1024];
    CliLine reason = {why, sizeof(why), 0ull};
    if ((setting == NULL) || !cli_line_fits(&options))
    {
        cli_line_text(&reason, "the cells cannot meet its options or its environment");
    }
    CliLsp *const server = ((setting != NULL) && cli_line_fits(&options))
                               ? cli_lsp_started(program, words->word, words->count, root, options.out, options.at,
                                                 setting, settings, cli_servers_heard, heard, &reason)
                               : NULL;
    cli_cell_given_by(&g_cli_servers_launch_cells, vs_path);
    cli_cell_given_by(&g_cli_servers_launch_cells, setting);
    cli_cell_given(options.out);
    cli_cell_given_by(&g_cli_servers_launch_cells, launching);
    if (server == NULL)
    {
        cli_line_text(said, spec->program);
        cli_line_text(said, " did not start");
        if (spec->setup[0] != '\0')
        {
            cli_line_text(said, " (");
            cli_line_counted(said, why, reason.at);
            cli_line_text(said, "): ");
            cli_line_text(said, spec->setup);
            cli_line_text(said, " installs it");
        }
        else
        {
            cli_line_text(said, ": ");
            cli_line_counted(said, why, reason.at);
        }
    }
    return server;
}
