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

// the most projects a server is given, and how many folders below the top a search for them goes
#define CLI_SERVERS_PROJECTS 256u
#define CLI_SERVERS_PROJECT_DEPTH 6u

// what parts the folders of a PATH
#if defined(_WIN32)
#define CLI_SERVERS_PATH_PARTS ';'
#else
#define CLI_SERVERS_PATH_PARTS ':'
#endif

// where what each server says on its own goes, a place for each server started, taken in turn
static CliServersEmit s_heard[64];
static unsigned int s_heard_next;

static char s_options[1u << 16u];

// The search for projects: the folders waiting, each with its depth, and the projects found
typedef struct
{
    char folder[1024][CLI_PATH_LONGEST];
    unsigned int depth[1024];
    unsigned int waiting;
    char found[CLI_SERVERS_PROJECTS][CLI_PATH_LONGEST];
    unsigned int count;
    const char *name;
    unsigned int at_depth;
    const char *at;
} CliServersProjects;

static CliServersProjects s_projects;

// an entry of a folder the search reads: a folder to go into, or a project
static void launch_project_entry(void *context, const char *name, int is_folder)
{
    CliServersProjects *const projects = context;
    static const char *const s_not[4] = {"target", "build", "node_modules", "archive"};
    if (is_folder)
    {
        int skipped = (name[0] == '.') || (projects->at_depth >= CLI_SERVERS_PROJECT_DEPTH);
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            skipped |= (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_not[at], .cap = CLI_CELLS_POOL}));
        }
        if (!skipped && (projects->waiting < 1024u))
        {
            cli_full(projects->at, name, projects->folder[projects->waiting]);
            projects->depth[projects->waiting] = projects->at_depth + 1u;
            projects->waiting += 1u;
        }
        return;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = projects->name, .cap = CLI_CELLS_POOL})) &&
        (projects->count < CLI_SERVERS_PROJECTS))
    {
        cli_full(projects->at, name, projects->found[projects->count]);
        projects->count += 1u;
    }
}

// every file named `name` in the tree at `root`, outside the folders a build writes, sorted, written as a JSON array
static void launch_projects_in(const char *root, const char *name, CliLine *out)
{
    CliServersProjects *const projects = &s_projects;
    projects->waiting = 1u;
    projects->count = 0u;
    projects->name = name;
    CliLine top = {projects->folder[0], CLI_PATH_LONGEST, 0ull};
    cli_line_text(&top, root);
    (void)cli_line_closed(&top);
    projects->depth[0] = 0u;
    static char s_at[CLI_PATH_LONGEST];
    while (projects->waiting > 0u)
    {
        projects->waiting -= 1u;
        memor.cpy(&(MemoriaCfg){.dst = s_at, .src = projects->folder[projects->waiting], .bytes = CLI_PATH_LONGEST});
        projects->at = s_at;
        projects->at_depth = projects->depth[projects->waiting];
        (void)query_folder_each(s_at, launch_project_entry, projects);
    }
    for (unsigned int at = 1u; at < projects->count; at += 1u)
    {
        static char s_kept[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = s_kept, .src = projects->found[at], .bytes = CLI_PATH_LONGEST});
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(projects->found[place - 1u], s_kept) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = projects->found[place], .src = projects->found[place - 1u], .bytes = CLI_PATH_LONGEST});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = projects->found[place], .src = s_kept, .bytes = CLI_PATH_LONGEST});
    }
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < projects->count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(out, (const unsigned char *)projects->found[at],
                              cellul.len(&(CatenaFinitaCfg){.src = projects->found[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_character(out, ']');
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
        launch_projects_in(root, spec->projects, out);
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
    static CliFound s_found;
    memor.set(&(MemoriaCfg){.dst = &s_found, .val = 0, .bytes = sizeof(s_found)});
    if (tool != CLI_JSON_NONE)
    {
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
        cli_toolchains_find(&manifest_json, tool, (const char (*)[CLI_PATH_LONGEST])folders, count, &s_found);
        cli_cell_given(folders);
    }
    cli_json_given(&manifest_json);
    char program[CLI_PATH_LONGEST];
    const char *const names[1] = {spec->program};
    if ((s_found.folder[0] == '\0') || !cli_toolchains_program_in(s_found.folder, names, 1u, program))
    {
        cli_line_text(said, spec->program);
        cli_line_text(said, " is not found: File, Toolchains opens ");
        cli_line_text(said, spec->name);
        cli_line_text(said, "'s install page or takes its folder");
        return NULL;
    }
    CliLine options = {s_options, sizeof(s_options), 0ull};
    launch_options(root, spec, &options);
    static CliWords s_words;
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    for (unsigned int at = 0u; at < spec->args; at += 1u)
    {
        cli_words_put(&s_words, spec->arg[at]);
    }
    static char s_path[CLI_PATH_TEXT];
    cli_toolchains_run_path(s_path, sizeof(s_path));
    static CliSetting s_settings[1024];
    static char s_vs_path[CLI_PATH_TEXT * 2u];
    unsigned int settings = 1u;
    s_settings[0] = (CliSetting){"PATH", s_path};
    if (cellul.eq(&(CatenaFinitaCfg){.src = spec->program, .other = "clangd", .cap = CLI_CELLS_POOL}))
    {
        static CliDatabase s_database;
        (void)cli_compile_db_refresh(root, &s_database);
        cli_compile_db_words(&s_database, &s_words);
        const CliVsEnvironment *const vs = s_database.msvc ? cli_compile_db_vs_environment() : NULL;
        for (unsigned int at = 0u; (vs != NULL) && (at < vs->count) && (settings < 1024u); at += 1u)
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
                CliLine joined = {s_vs_path, sizeof(s_vs_path), 0ull};
                cli_line_text(&joined, vs->value[at]);
                cli_line_character(&joined, CLI_SERVERS_PATH_PARTS);
                cli_line_text(&joined, s_path);
                (void)cli_line_closed(&joined);
                s_settings[0].value = s_vs_path;
                continue;
            }
            s_settings[settings] = (CliSetting){vs->name[at], vs->value[at]};
            settings += 1u;
        }
    }
    CliServersEmit *const heard = &s_heard[s_heard_next % 64u];
    s_heard_next += 1u;
    *heard = *emit;
    char why[1024];
    CliLine reason = {why, sizeof(why), 0ull};
    CliLsp *const server = cli_lsp_started(program, s_words.word, s_words.count, root, s_options, options.at,
                                           s_settings, settings, cli_servers_heard, heard, &reason);
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
