// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_read.c: the tree's build scripts found, to CLI_EXECUTABLES_DEEP folders deep past the folders a build
// writes into or a package manager fills and those whose names start with a dot, and every program they make read
#include "../command_line/cells.h"
#include "../text/text.h"
#include "executables_internal.h"

// folders a build writes into or a package manager fills, which hold no build scripts of the tree's
static const char *const s_skipped[8] = {"target", "node_modules", "build", "out", "dist", "vendor", "bin", "obj"};

// The scripts found and what is read of a script, a cell taken with the lock and given back with it; each script's
// text is a cell of its own
typedef struct
{
    CliWalk walk;
    CliNames names;
    CliNames kept;
} CliScriptsRoom;

static CliScriptsRoom *s_scripts;
static CliLock s_lock = CLI_LOCK_INIT;

unsigned int cli_executables_kind_of(const char *name)
{
    static const char *const s_names_of[6] = {"Cargo.toml", "CMakeLists.txt", "meson.build",
                                              "go.mod",     "build.zig",      "package.json"};
    static const unsigned int s_kinds[6] = {CLI_KIND_CARGO, CLI_KIND_CMAKE, CLI_KIND_MESON,
                                            CLI_KIND_GO,    CLI_KIND_ZIG,   CLI_KIND_NPM};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_names_of[at], .cap = CLI_CELLS_POOL}))
        {
            return s_kinds[at];
        }
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if ((length >= 5ull) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 5ull, .other = ".spec", .cap = CLI_CELLS_POOL})))
    {
        return CLI_KIND_PYINSTALLER;
    }
    static const char *const s_projects[3] = {".csproj", ".fsproj", ".vbproj"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_projects[at], .cap = CLI_CELLS_POOL});
        if ((length >= size) && (cellul.eq(&(CatenaFinitaCfg){
                                    .src = name + length - size, .other = s_projects[at], .cap = CLI_CELLS_POOL})))
        {
            return CLI_KIND_DOTNET;
        }
    }
    return CLI_KIND_NONE;
}

int cli_text_read(const char *path, char *bytes, unsigned long long room)
{
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)bytes, room - 1ull);
    if (size == QRY_MEMBER_UNREAD)
    {
        bytes[0] = '\0';
        return 0;
    }
    bytes[size] = '\0';
    return 1;
}

char *cli_text_taken(const char *path)
{
    unsigned long long size = 0ull;
    char *const text =
        cli_file_size(path, &size) ? cli_cell_taken_by(&g_cli_executables_cells, size + 1ull, "a script's text") : NULL;
    if ((text != NULL) && !cli_text_read(path, text, size + 1ull))
    {
        cli_cell_given_by(&g_cli_executables_cells, text);
        return NULL;
    }
    return text;
}

// a walk's room of `count` paths, `*room` of them, doubled where it is full, the depths beside it where `depth` is
// not NULL: 1, or 0 where it is at `most` or the cells are refused
static int walk_grown(char (**paths)[CLI_PATH_LONGEST], unsigned int **depth, unsigned int count, unsigned int *room,
                      unsigned int most)
{
    if (count < *room)
    {
        return 1;
    }
    if (count >= most)
    {
        return 0;
    }
    const unsigned int grown = (*room == 0u) ? CLI_EXECUTABLES_WALK_FIRST : ((*room * 2u < most) ? *room * 2u : most);
    char (*const larger)[CLI_PATH_LONGEST] =
        cli_cell_taken_by(&g_cli_executables_cells, (unsigned long long)grown * CLI_PATH_LONGEST, "a walk's paths");
    unsigned int *const deeper = ((larger != NULL) && (depth != NULL))
                                     ? cli_cell_taken_by(&g_cli_executables_cells,
                                                         (unsigned long long)grown * sizeof(**depth), "a walk's depths")
                                     : NULL;
    if ((larger == NULL) || ((depth != NULL) && (deeper == NULL)))
    {
        cli_cell_given_by(&g_cli_executables_cells, larger);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = larger, .src = *paths, .bytes = (size_t)count * CLI_PATH_LONGEST});
    cli_cell_given_by(&g_cli_executables_cells, *paths);
    *paths = larger;
    if (depth != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = deeper, .src = *depth, .bytes = (size_t)count * sizeof(**depth)});
        cli_cell_given_by(&g_cli_executables_cells, *depth);
        *depth = deeper;
    }
    *room = grown;
    return 1;
}

int cli_executables_pending_put(CliWalk *walk, const char *path, unsigned int depth)
{
    if (!walk_grown(&walk->pending, &walk->depth, walk->pending_count, &walk->pending_room, CLI_EXECUTABLES_PENDING))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = walk->pending[walk->pending_count],
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    walk->depth[walk->pending_count] = depth;
    walk->pending_count += 1u;
    return 1;
}

int cli_executables_found_put(CliWalk *walk, const char *path)
{
    if (!walk_grown(&walk->found, NULL, walk->found_count, &walk->found_room, CLI_EXECUTABLES_SCRIPTS))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = walk->found[walk->found_count],
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    walk->found_count += 1u;
    return 1;
}

void cli_executables_walk_given(CliWalk *walk)
{
    cli_cell_given_by(&g_cli_executables_cells, walk->found);
    cli_cell_given_by(&g_cli_executables_cells, walk->depth);
    cli_cell_given_by(&g_cli_executables_cells, walk->pending);
    *walk = (CliWalk){NULL, NULL, 0u, 0u, NULL, 0u, NULL, 0u, 0u, 0};
}

int cli_executables_joined(const char *folder, const char *name, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    return cli_line_closed(&line) != 0ull;
}

int cli_executables_passed(const char *name)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_skipped[at], .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// an entry of a folder the walk looks in: a folder to look in after, or a build script found
static void cli_script_each(void *context, const char *name, int is_folder)
{
    CliWalk *const walk = (CliWalk *)context;
    char path[CLI_PATH_LONGEST];
    if (!cli_executables_joined(walk->folder, name, path))
    {
        return;
    }
    if (is_folder)
    {
        if (((walk->at_depth + 1u) < CLI_EXECUTABLES_DEEP) && (name[0] != '.') && !cli_executables_passed(name))
        {
            (void)cli_executables_pending_put(walk, path, walk->at_depth + 1u);
        }
    }
    else if (cli_executables_kind_of(name) != CLI_KIND_NONE)
    {
        (void)cli_executables_found_put(walk, path);
    }
}

void cli_executables_walked(CliWalk *walk, const char *top,
                            void (*each)(void *context, const char *name, int is_folder))
{
    walk->pending_count = 0u;
    (void)cli_executables_pending_put(walk, top, 0u);
    while (walk->pending_count > 0u)
    {
        walk->pending_count -= 1u;
        char folder[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){
            .dst = folder,
            .src = walk->pending[walk->pending_count],
            .bytes =
                cellul.len(&(CatenaFinitaCfg){.src = walk->pending[walk->pending_count], .cap = CLI_CELLS_POOL}) + 1u});
        walk->folder = folder;
        walk->at_depth = walk->depth[walk->pending_count];
        walk->main = 0;
        query_folder_each(folder, each, walk);
        if (walk->main)
        {
            (void)cli_executables_found_put(walk, folder);
        }
    }
}

// the last part of a path
static const char *cli_base(const char *path)
{
    const char *name = path;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    return name;
}

// whether `path` is `folder` or under it, part by part
static int cli_under(const char *path, const char *folder)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    return cli_text_same(path, folder, length) &&
           ((path[length] == '\0') || (path[length] == '/') || (path[length] == '\\'));
}

// a program put after those found
static void cli_found_put(CliExecutables *found, unsigned int kind, const char *script, const char *name)
{
    if (!cli_executables_room(found))
    {
        return;
    }
    CliExecutable *const one = &found->item[found->count];
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    one->kind = kind;
    memor.cpy(&(MemoriaCfg){.dst = one->script,
                            .src = script,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = one->name,
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    found->count += 1u;
}

// the programs of the CMake or the Meson projects among the scripts found: each script named `file` that calls
// project() with none above it in the tree names a project, whose targets are those of every such file under its
// folder
static void cli_projects(const CliWalk *walk, const char *file, unsigned int kind, CliExecutables *found)
{
    for (unsigned int top = 0u; top < walk->found_count; top += 1u)
    {
        const char *const script = walk->found[top];
        char folder[CLI_PATH_LONGEST];
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = cli_base(script), .other = file, .cap = CLI_CELLS_POOL})) ||
            !cli_path_parent(script, folder))
        {
            continue;
        }
        int above = 0;
        for (unsigned int other = 0u; !above && (other < walk->found_count); other += 1u)
        {
            char dir[CLI_PATH_LONGEST];
            above = (cellul.eq(&(CatenaFinitaCfg){
                        .src = cli_base(walk->found[other]), .other = file, .cap = CLI_CELLS_POOL})) &&
                    cli_path_parent(walk->found[other], dir) && cli_under(folder, dir) &&
                    (!cellul.eq(&(CatenaFinitaCfg){.src = dir, .other = folder, .cap = CLI_CELLS_POOL}));
        }
        char project[CLI_NAME_LONGEST];
        char *const top_text = above ? NULL : cli_text_taken(script);
        const int named = (top_text != NULL) && ((kind == CLI_KIND_CMAKE) ? cli_cmake_project(top_text, project)
                                                                          : cli_meson_project(top_text, project));
        cli_cell_given_by(&g_cli_executables_cells, top_text);
        if (!named)
        {
            continue;
        }
        s_scripts->kept.count = 0u;
        for (unsigned int list = 0u; list < walk->found_count; list += 1u)
        {
            if ((!cellul.eq(
                    &(CatenaFinitaCfg){.src = cli_base(walk->found[list]), .other = file, .cap = CLI_CELLS_POOL})) ||
                !cli_under(walk->found[list], folder))
            {
                continue;
            }
            char *const taken = cli_text_taken(walk->found[list]);
            const char *const text = (taken != NULL) ? taken : "";
            char own[CLI_NAME_LONGEST];
            if (kind == CLI_KIND_CMAKE)
            {
                cli_cmake_targets(text, cli_cmake_project(text, own) ? own : project, &s_scripts->names);
            }
            else
            {
                cli_meson_targets(text, &s_scripts->names);
            }
            cli_cell_given_by(&g_cli_executables_cells, taken);
            for (unsigned int at = 0u; at < s_scripts->names.count; at += 1u)
            {
                cli_names_put(&s_scripts->kept, s_scripts->names.name[at],
                              cellul.len(&(CatenaFinitaCfg){.src = s_scripts->names.name[at], .cap = CLI_CELLS_POOL}),
                              NULL);
            }
        }
        for (unsigned int at = 0u; at < s_scripts->kept.count; at += 1u)
        {
            cli_found_put(found, kind, script, s_scripts->kept.name[at]);
        }
    }
}

// the programs a script of a kind other than CMake's and Meson's, whose text is `text`, makes put after those found
static void cli_script_text_read(const char *root, const char *script, const char *text, unsigned int kind,
                                 CliExecutables *found)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_path_parent(script, folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    int module = 0;
    if (kind == CLI_KIND_CARGO)
    {
        cli_cargo_bins(script, text, &s_scripts->names);
    }
    else if (kind == CLI_KIND_GO)
    {
        cli_go_mains(script, text, &s_scripts->names);
    }
    else if (kind == CLI_KIND_ZIG)
    {
        cli_zig_exes(text, &s_scripts->names);
    }
    else if (kind == CLI_KIND_NPM)
    {
        cli_npm_bins(text, &s_scripts->names, &module);
    }
    else
    {
        s_scripts->names.count = 0u;
        char name[CLI_NAME_LONGEST];
        char path[CLI_PATH_LONGEST];
        const int one =
            (kind == CLI_KIND_DOTNET) ? cli_dotnet_program(script, text, name) : cli_spec_made(text, name, path);
        if (one)
        {
            cli_names_put(&s_scripts->names, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}),
                          (kind == CLI_KIND_DOTNET) ? NULL : path);
        }
    }
    const int optimize =
        cellul.find(&(CatenaFinitaCfg){
            .src = text, .cap = CLI_CELLS_POOL, .other = "standardOptimizeOption", .other_cap = CLI_CELLS_POOL}) !=
        NULL;
    for (unsigned int at = 0u; at < s_scripts->names.count; at += 1u)
    {
        const unsigned int before = found->count;
        cli_found_put(found, kind, script, s_scripts->names.name[at]);
        if (found->count == before)
        {
            return;
        }
        CliExecutable *const one = &found->item[found->count - 1u];
        one->optimize = optimize;
        if ((kind == CLI_KIND_GO) || (kind == CLI_KIND_PYINSTALLER))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = one->detail,
                .src = s_scripts->names.value[at],
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){.src = s_scripts->names.value[at], .cap = CLI_CELLS_POOL}) + 1u});
        }
        if (kind == CLI_KIND_NPM)
        {
            const char *const entry = s_scripts->names.value[at];
            const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = entry, .cap = CLI_CELLS_POOL});
            one->commonjs =
                ((length >= 4ull) && (cellul.eq(&(CatenaFinitaCfg){
                                         .src = entry + length - 4ull, .other = ".cjs", .cap = CLI_CELLS_POOL}))) ||
                ((length >= 3ull) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = entry + length - 3ull, .other = ".js", .cap = CLI_CELLS_POOL})) &&
                 !module);
            const char *trimmed = entry;
            while (cellul.starts(&(CatenaFinitaCfg){.src = trimmed, .other = "./", .cap = CLI_CELLS_POOL}))
            {
                trimmed += 2;
            }
            char path[CLI_PATH_LONGEST];
            cli_executables_joined(folder, trimmed, path);
            cli_relative(root, path, one->detail);
            char *const bin = cli_text_taken(path);
            cli_npm_builders(folder, (bin != NULL) ? bin : "", one);
            cli_cell_given_by(&g_cli_executables_cells, bin);
        }
    }
}

// the programs a script of a kind other than CMake's and Meson's makes put after those found, its text read into a
// cell of its size
static void cli_script_read(const char *root, const char *script, unsigned int kind, CliExecutables *found)
{
    char *const text = cli_text_taken(script);
    if (text != NULL)
    {
        cli_script_text_read(root, script, text, kind, found);
    }
    cli_cell_given_by(&g_cli_executables_cells, text);
}

void cli_executables_read(const char *root, CliExecutables *found)
{
    cli_executables_given(found);
    cli_lock_take(&s_lock);
    s_scripts = cli_cell_taken(sizeof(*s_scripts), "a reading of the tree's build scripts");
    if (s_scripts == NULL)
    {
        cli_lock_give(&s_lock);
        return;
    }
    CliWalk *const walk = &s_scripts->walk;
    *walk = (CliWalk){NULL, NULL, 0u, 0u, NULL, 0u, NULL, 0u, 0u, 0};
    cli_executables_walked(walk, root, cli_script_each);
    cli_paths_sorted(walk->found, walk->found_count);
    cli_projects(walk, "CMakeLists.txt", CLI_KIND_CMAKE, found);
    cli_projects(walk, "meson.build", CLI_KIND_MESON, found);
    for (unsigned int at = 0u; at < walk->found_count; at += 1u)
    {
        const unsigned int kind = cli_executables_kind_of(cli_base(walk->found[at]));
        if ((kind != CLI_KIND_CMAKE) && (kind != CLI_KIND_MESON) && (kind != CLI_KIND_NONE))
        {
            cli_script_read(root, walk->found[at], kind, found);
        }
    }
    cli_executables_walk_given(walk);
    cli_cell_given(s_scripts);
    s_scripts = NULL;
    cli_lock_give(&s_lock);
    cli_executables_ordered(found);
}
