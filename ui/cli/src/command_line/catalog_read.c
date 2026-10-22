// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_read.c: the catalog read from the tree: the build scripts, the sims, the driver's parts, the runners, the
// page builders, the stage scripts and their pipelines, and the test suites
#include "../text/text.h"
#include "catalog_internal.h"

// the deepest folders are looked through for a sim's source
#define CLI_FIND_DEPTH 8u

static int cli_is_build(const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "build", .cap = CLI_CELLS_POOL})) &&
           (length >= 3u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 3u, .other = ".sh", .cap = CLI_CELLS_POOL})) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "build_stamp.sh", .cap = CLI_CELLS_POOL}));
}

int cli_catalog_is_cfg(const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return (length >= 4u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".cfg", .cap = CLI_CELLS_POOL}));
}

static int cli_is_cu(const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return (length >= 3u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 3u, .other = ".cu", .cap = CLI_CELLS_POOL}));
}

int cli_catalog_is_qasm(const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return (length >= 5u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 5u, .other = ".qasm", .cap = CLI_CELLS_POOL}));
}

int cli_catalog_is_py(const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return (length >= 3u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 3u, .other = ".py", .cap = CLI_CELLS_POOL}));
}

int cli_catalog_is_builder(const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "build_", .cap = CLI_CELLS_POOL})) &&
           (length >= 8u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 8u, .other = "_view.py", .cap = CLI_CELLS_POOL}));
}

int cli_catalog_is_figure(const char *name)
{
    return (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "build_infographic.py", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "make_shadow_figure.py", .cap = CLI_CELLS_POOL}));
}

void cli_catalog_short(const char *file, char *out)
{
    const char *const last =
        cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/');
    const char *folder = file;
    if (last != NULL)
    {
        folder = last;
        while ((folder > file) && (folder[-1] != '/'))
        {
            folder -= 1;
        }
    }
    memor.cpy(&(MemoriaCfg){
        .dst = out,
        .src = (last != NULL) ? folder : file,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (last != NULL) ? folder : file, .cap = CLI_CELLS_POOL}) + 1u});
}

// a job of a script: its id the group and its file, its title its file
void cli_catalog_script_job(CliCatalog *catalog, const char *root, const char *group, const char *path,
                            const char *title, const char *opens)
{
    char file[CLI_PATH_LONGEST];
    char id[CLI_PATH_LONGEST];
    cli_relative(root, path, file);
    CliLine line = {id, sizeof(id), 0ull};
    cli_line_text(&line, group);
    cli_line_character(&line, '/');
    cli_line_text(&line, file);
    cli_line_closed(&line);
    char *const about = cli_catalog_about_taken(path);
    cli_catalog_job(catalog, id, group, (title != NULL) ? title : file, file, (about != NULL) ? about : "", opens);
    cli_catalog_text_given(about);
}

// A bash script that takes free words as its job; a build traced: bash takes SHELLOPTS from its environment as its own
// options, and gives it on, so every script the build runs says each command as it runs it
void cli_catalog_bash_alone(CliCatalog *catalog, const char *root, const char *group, const char *path)
{
    char file[CLI_PATH_LONGEST];
    char title[CLI_PATH_LONGEST];
    cli_relative(root, path, file);
    cli_catalog_short(file, title);
    cli_catalog_script_job(catalog, root, group, path, title, "");
    const int build = cellul.eq(&(CatenaFinitaCfg){.src = group, .other = "build", .cap = CLI_CELLS_POOL});
    cli_catalog_words(catalog);
    if (build)
    {
        cli_catalog_trace(catalog);
    }
    cli_catalog_step(catalog, CLI_PROGRAM_BASH, file, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_WORDS, "arguments", NULL, NULL, NULL, 0);
    if (build)
    {
        cli_catalog_arg(catalog, CLI_ARG_SET_WHEN, CLI_TRACE, CLI_TRACE_VERBOSE, "SHELLOPTS", "xtrace", 0);
    }
}

// The build scripts: the engine's, each example's, and the maintenance builds. build_stamp.sh is a library the others
// source and never a build of its own
void cli_catalog_builds(const char *root, CliCatalog *catalog)
{
    static const char *const s_places[3] = {"/src", "/utils/maint/engine", "/utils/maint/texbuild"};
    char place[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    CliEntries examples = {NULL, 0u, 0u};
    CliEntries builds = {NULL, 0u, 0u};
    CliPaths listed = {NULL, 0u};
    cli_catalog_join(root, "examples", place);
    cli_catalog_entries(place, 1, NULL, &examples);
    (void)cli_catalog_paths_room(&listed, 3u + examples.count);
    unsigned int places = 0u;
    for (unsigned int at = 0u; (at < 3u) && (places < listed.room); at += 1u)
    {
        CliLine line = {listed.path[places], CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, root);
        cli_line_text(&line, s_places[at]);
        places += (cli_line_closed(&line) != 0ull) ? 1u : 0u;
    }
    for (unsigned int at = 0u; (at < examples.count) && (places < listed.room); at += 1u)
    {
        cli_catalog_join(place, examples.name[at], listed.path[places]);
        places += 1u;
    }
    cli_catalog_entries_given(&examples);
    for (unsigned int at = 0u; at < places; at += 1u)
    {
        cli_catalog_entries(listed.path[at], 0, cli_is_build, &builds);
        for (unsigned int each = 0u; each < builds.count; each += 1u)
        {
            cli_catalog_join(listed.path[at], builds.name[each], path);
            cli_catalog_bash_alone(catalog, root, "build", path);
        }
    }
    cli_catalog_entries_given(&builds);
    cli_catalog_paths_given(&listed);
}

// the folder named `name` under `under`, the first found depth first in order, into `found`: 1, or 0
static int cli_find_dir(const char *under, const char *name, unsigned int depth, char *found)
{
    if (depth >= CLI_FIND_DEPTH)
    {
        return 0;
    }
    CliEntries dirs = {NULL, 0u, 0u};
    cli_catalog_entries(under, 1, NULL, &dirs);
    int found_one = 0;
    for (unsigned int at = 0u; !found_one && (at < dirs.count); at += 1u)
    {
        char dir[CLI_PATH_LONGEST];
        cli_catalog_join(under, dirs.name[at], dir);
        if (cellul.eq(&(CatenaFinitaCfg){.src = dirs.name[at], .other = name, .cap = CLI_CELLS_POOL}))
        {
            memor.cpy(&(MemoriaCfg){.dst = found,
                                    .src = dir,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_CELLS_POOL}) + 1u});
            found_one = 1;
        }
        else
        {
            found_one = cli_find_dir(dir, name, depth + 1u, found);
        }
    }
    cli_catalog_entries_given(&dirs);
    return found_one;
}

// the file named `name` under `under`, here first, then in each folder depth first in order, into `found`: 1, or 0
static int cli_find_file(const char *under, const char *name, unsigned int depth, char *found)
{
    cli_catalog_join(under, name, found);
    if (query_is_file(found))
    {
        return 1;
    }
    if (depth >= CLI_FIND_DEPTH)
    {
        return 0;
    }
    CliEntries dirs = {NULL, 0u, 0u};
    cli_catalog_entries(under, 1, NULL, &dirs);
    int found_one = 0;
    for (unsigned int at = 0u; !found_one && (at < dirs.count); at += 1u)
    {
        char dir[CLI_PATH_LONGEST];
        cli_catalog_join(under, dirs.name[at], dir);
        found_one = cli_find_file(dir, name, depth + 1u, found);
    }
    cli_catalog_entries_given(&dirs);
    return found_one;
}

char *cli_catalog_script_taken(const char *path)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    if (bytes == NULL)
    {
        return NULL;
    }
    char *const text = cli_cell_taken_by(&g_cli_catalog_cells, size + 1ull, "a script's text");
    if (text != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = text, .src = bytes, .bytes = (size_t)size});
        text[size] = '\0';
    }
    cli_file_let_go(bytes, size);
    return text;
}

// One job per sim in the case src/sims/run.sh accepts, each with the first comment its source opens with
void cli_catalog_sims(const char *root, CliCatalog *catalog)
{
    char script[CLI_PATH_LONGEST];
    char sims_cu[CLI_PATH_LONGEST];
    cli_catalog_join(root, "src/sims/run.sh", script);
    cli_catalog_join(root, "src/sims/cu", sims_cu);
    char *const text = cli_catalog_script_taken(script);
    if (text == NULL)
    {
        return;
    }
    CliEntries names = {NULL, 0u, 0u};
    CliEntries sources = {NULL, 0u, 0u};
    (void)cli_catalog_names_room(&names, cli_catalog_case_names(text, "SIM", NULL, 0u));
    const unsigned int count = (names.name != NULL) ? cli_catalog_case_names(text, "SIM", names.name, names.room) : 0u;
    cli_catalog_text_given(text);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const sim = names.name[at];
        char dir[CLI_PATH_LONGEST];
        char source[CLI_PATH_LONGEST];
        int found = 0;
        if (cli_find_dir(sims_cu, sim, 0u, dir))
        {
            cli_catalog_entries(dir, 0, cli_is_cu, &sources);
            found = sources.count != 0u;
            if (found)
            {
                cli_catalog_join(dir, sources.name[0], source);
            }
        }
        if (!found)
        {
            char named[CLI_NAME_LONGEST];
            CliLine line = {named, sizeof(named), 0ull};
            cli_line_text(&line, sim);
            cli_line_text(&line, ".cu");
            cli_line_closed(&line);
            found = cli_find_file(sims_cu, named, 0u, source);
        }
        char id[CLI_PATH_LONGEST];
        char file[CLI_PATH_LONGEST];
        CliLine line = {id, sizeof(id), 0ull};
        cli_line_text(&line, "sim/");
        cli_line_text(&line, sim);
        cli_line_closed(&line);
        char *about = NULL;
        if (found)
        {
            cli_relative(root, source, file);
            about = cli_catalog_about_taken(source);
        }
        else
        {
            memor.cpy(&(MemoriaCfg){
                .dst = file,
                .src = "src/sims/run.sh",
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "src/sims/run.sh", .cap = CLI_CELLS_POOL}) + 1u});
        }
        cli_catalog_job(catalog, id, "sim", sim, file, (about != NULL) ? about : "", "");
        cli_catalog_text_given(about);
        cli_catalog_words(catalog);
        cli_catalog_step(catalog, CLI_PROGRAM_BASH, "src/sims/run.sh", NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_LIT, sim, NULL, NULL, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_WORDS, "arguments", NULL, NULL, NULL, 1);
    }
    cli_catalog_entries_given(&sources);
    cli_catalog_entries_given(&names);
}

unsigned int cli_catalog_paths(const char *root, const char *dir, int (*keep)(const char *name), CliPaths *list,
                               unsigned int at)
{
    CliEntries files = {NULL, 0u, 0u};
    cli_catalog_entries(dir, 0, keep, &files);
    (void)cli_catalog_paths_room(list, at + files.count);
    for (unsigned int each = 0u; (each < files.count) && (at < list->room); each += 1u)
    {
        char path[CLI_PATH_LONGEST];
        cli_catalog_join(dir, files.name[each], path);
        cli_relative(root, path, list->path[at]);
        at += 1u;
    }
    cli_catalog_entries_given(&files);
    return at;
}
