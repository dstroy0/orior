// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_launch.c: a server started: its program found as its toolchain is, its options the projects it is given and
// what a tree whose code is disabled or runs adds, its environment the PATH every step runs with; clangd given the
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

// where what each server sends unprompted goes, a place for each server started, taken in turn
static CliServersSink s_server_sinks[64];
static unsigned int s_server_sink_next;

CellOwner g_cli_servers_launch_cells = MMGR_CELL_OWNER("cli_servers_launch");

// The search for projects: the file name looked for, the folder read and its depth, and the projects found, their
// paths one after another, each with its zero after it, in a growing line, and their count
typedef struct
{
    const char *name;
    const char *at;
    unsigned int at_depth;
    VerbaLinea found;
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
    char path[CLI_PATH_MAX_LEN];
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
        mmgr_line_put_n(&projects->found, path,
                        cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u);
        projects->count += 1u;
    }
}

void cli_servers_projects_in(const char *root, const char *name, VerbaLinea *out)
{
    CliServersProjects projects = {name, root, 0u, {NULL, 0ull, 0ull, NULL, NULL, 0ull}, 0u};
    (void)cli_growing_line_made(&projects.found, CLI_SERVERS_FOUND_FIRST);
    (void)query_folder_each(root, launch_project_entry, &projects);
    // the projects in order, each by where it starts among those found
    const unsigned int count = mmgr_line_ok(&projects.found) ? projects.count : 0u;
    const char **const found =
        (count != 0u) ? cli_lane_alloc(count * sizeof(*found), &g_cli_servers_launch_cells) : NULL;
    const unsigned int listed = (found != NULL) ? count : 0u;
    const char *path = projects.found.out;
    for (unsigned int at = 0u; at < listed; at += 1u)
    {
        found[at] = path;
        path += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
    }
    for (unsigned int at = 1u; at < listed; at += 1u)
    {
        const char *const swap = found[at];
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(found[place - 1u], swap) > 0))
        {
            found[place] = found[place - 1u];
            place -= 1u;
        }
        found[place] = swap;
    }
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < listed; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(out, (const unsigned char *)found[at],
                              cellul.len(&(CatenaFinitaCfg){.src = found[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_ch(out, ']');
    cli_cell_release(found, &g_cli_servers_launch_cells);
    cli_cell_release(projects.found.out, NULL);
}

// the options a server starts with: the projects it is given, then what the tree adds, `null` where neither is
static void launch_options(const char *root, const CliServerSpec *spec, VerbaLinea *out)
{
    const char *const added = cli_trust_blocked(root) ? spec->blocked_args : spec->trusted;
    const int adds = (added[0] == '{');
    if ((spec->projects[0] == '\0') && !adds)
    {
        mmgr_line_put(out, "null");
        return;
    }
    mmgr_line_ch(out, '{');
    int members = 0;
    if (spec->projects[0] != '\0')
    {
        mmgr_line_put(out, "\"linkedProjects\":");
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
            mmgr_line_put(out, members ? "," : "");
            mmgr_line_put_n(out, added + from + inner_from, inner_to - inner_from);
        }
    }
    mmgr_line_ch(out, '}');
}

CliLsp *cli_servers_launch(const char *root, const CliServerSpec *spec, const CliServersSink *sink, VerbaLinea *err_msg)
{
    CliJson manifest_json;
    const unsigned int manifest = cli_toolchains_manifest(&manifest_json, 0u, 0ull);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int tool = cli_toolchains_tool(&manifest_json, manifest, spec->tool, &ignored);
    CliServersLaunching *const launching =
        cli_lane_alloc(sizeof(*launching), &g_cli_servers_launch_cells);
    if (launching != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = &launching->found, .val = 0, .bytes = sizeof(launching->found)});
    }
    if ((launching != NULL) && (tool != CLI_JSON_NONE))
    {
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_taken(&count);
        cli_toolchains_find(&manifest_json, tool, (const char (*)[CLI_PATH_MAX_LEN])folders, count, &launching->found);
        cli_cell_release(folders, NULL);
    }
    cli_json_given(&manifest_json);
    if (launching == NULL)
    {
        mmgr_line_put(err_msg, "the cells cannot meet what ");
        mmgr_line_put(err_msg, spec->program);
        mmgr_line_put(err_msg, " is started with");
        return NULL;
    }
    char program[CLI_PATH_MAX_LEN];
    const char *const names[1] = {spec->program};
    if ((launching->found.folder[0] == '\0') || !cli_toolchains_program_in(launching->found.folder, names, 1u, program))
    {
        mmgr_line_put(err_msg, spec->program);
        mmgr_line_put(err_msg, " is not found: File, Toolchains opens ");
        mmgr_line_put(err_msg, spec->name);
        mmgr_line_put(err_msg, "'s install page or takes its folder");
        cli_cell_release(launching, &g_cli_servers_launch_cells);
        return NULL;
    }
    VerbaLinea options = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_growing_line_made(&options, CLI_SERVERS_OPTIONS_FIRST);
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
    CliSetting *const setting = cli_lane_alloc(most * sizeof(*setting), &g_cli_servers_launch_cells);
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
            cli_cell_release(vs_path, &g_cli_servers_launch_cells);
            vs_path = g_cli_cells->persistent_buf_alloc(joined_size, &g_cli_servers_launch_cells);
            VerbaLinea joined = {vs_path, (vs_path != NULL) ? joined_size : 0ull, 0ull};
            mmgr_line_put(&joined, vs->value[at]);
            mmgr_line_ch(&joined, CLI_SERVERS_PATH_PARTS);
            mmgr_line_put(&joined, launching->path);
            setting[0].value = (mmgr_line_finish(&joined) != 0ull) ? vs_path : launching->path;
            continue;
        }
        setting[settings] = (CliSetting){vs->name[at], vs->value[at]};
        settings += 1u;
    }
    CliServersSink *const server_sink = &s_server_sinks[s_server_sink_next % 64u];
    s_server_sink_next += 1u;
    *server_sink = *sink;
    char why[1024];
    VerbaLinea reason = {why, sizeof(why), 0ull};
    if ((setting == NULL) || !mmgr_line_ok(&options))
    {
        mmgr_line_put(&reason, "the cells cannot meet its options or its environment");
    }
    CliLsp *const server = ((setting != NULL) && mmgr_line_ok(&options))
                               ? cli_lsp_started(program, words->word, words->count, root, options.out, options.at,
                                                 setting, settings, cli_servers_recv, server_sink, &reason)
                               : NULL;
    cli_cell_release(vs_path, &g_cli_servers_launch_cells);
    cli_cell_release(setting, &g_cli_servers_launch_cells);
    cli_cell_release(options.out, NULL);
    cli_cell_release(launching, &g_cli_servers_launch_cells);
    if (server == NULL)
    {
        mmgr_line_put(err_msg, spec->program);
        mmgr_line_put(err_msg, " did not start");
        if (spec->setup[0] != '\0')
        {
            mmgr_line_put(err_msg, " (");
            mmgr_line_put_n(err_msg, why, reason.at);
            mmgr_line_put(err_msg, "): ");
            mmgr_line_put(err_msg, spec->setup);
            mmgr_line_put(err_msg, " installs it");
        }
        else
        {
            mmgr_line_put(err_msg, ": ");
            mmgr_line_put_n(err_msg, why, reason.at);
        }
    }
    return server;
}
