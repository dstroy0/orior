// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_read.c: the tree's build scripts found, to CLI_EXECUTABLES_DEEP folders deep past the folders a build
// writes into or a package manager fills and those whose names start with a dot, and every program they make read
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "executables_internal.h"

// folders a build writes into or a package manager fills, which contain no build scripts of the tree's
static const char *const s_skipped[8] = {"target", "node_modules", "build", "out", "dist", "vendor", "bin", "obj"};

// The scripts found and what is read of a script, a cell taken with the lock and given back with it; each script's
// text is a cell of its own
typedef struct
{
    CliWalk walk;
    CliNames names;
    CliNames merged;
} CliScriptsWorkState;

static CliScriptsWorkState *s_scripts;

unsigned int cli_executables_type_get(const CliExecutablesNameOutFolderCtx *args)
{
    static const char *const s_names_of[6] = {"Cargo.toml", "CMakeLists.txt", "meson.build",
                                              "go.mod",     "build.zig",      "package.json"};
    static const unsigned int s_types[6] = {CLI_TYPE_CARGO, CLI_TYPE_CMAKE, CLI_TYPE_MESON,
                                            CLI_TYPE_GO,    CLI_TYPE_ZIG,   CLI_TYPE_NPM};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_names_of[at], .cap = CLI_POOL_BYTES}))
        {
            return s_types[at];
        }
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    if ((length >= 5ULL) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 5ULL, .other = ".spec", .cap = CLI_POOL_BYTES})))
    {
        return CLI_TYPE_PYINSTALLER;
    }
    static const char *const s_projects[3] = {".csproj", ".fsproj", ".vbproj"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_projects[at], .cap = CLI_POOL_BYTES});
        if ((length >= size) &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = args->name + length - size, .other = s_projects[at], .cap = CLI_POOL_BYTES})))
        {
            return CLI_TYPE_DOTNET;
        }
    }
    return CLI_TYPE_NONE;
}

int cli_text_read(const CliExecutablesCapacityCtx *args)
{
    const unsigned long long size =
        qry_member_file_read(args->path, (unsigned char *)args->bytes, args->capacity - 1ULL);
    // QRY_MEMBER_UNREAD stands above every size
    if (size >= args->capacity)
    {
        args->bytes[0] = '\0';
        return 0;
    }
    args->bytes[size] = '\0';
    return 1;
}

char *cli_text_alloc(const CliExecutablesWalkCtx *args)
{
    unsigned long long size = 0ULL;
    char *const text =
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = args->path, .size_pointer = &size)
            ? g_cli_guard->persistent_buf_alloc(size + 1ULL, g_cli_executables_owner)
            : NULL;
    if ((text != NULL) && !EMBED_CALL(cli_executables_capacity_internal.text_read, CliExecutablesCapacityCtx,
                                      .path = args->path, .bytes = text, .capacity = size + 1ULL))
    {
        g_cli_guard->persistent_buf_release(text, g_cli_executables_owner);
        return NULL;
    }
    return text;
}

void cli_executables_walk_alloc(const CliExecutablesWalkCtx *args)
{
    CliWalk *walk = args->walk;
    const unsigned long long paths = (unsigned long long)CLI_EXECUTABLES_PENDING + CLI_EXECUTABLES_SCRIPTS;
    char (*const cell)[CLI_PATH_MAX_LEN] = g_cli_guard->temporary_buf_alloc(
        (paths * CLI_PATH_MAX_LEN) + ((unsigned long long)CLI_EXECUTABLES_PENDING * sizeof(unsigned int)),
        g_cli_executables_owner);
    *walk = (CliWalk){NULL, NULL, 0u, 0u, NULL, 0u, NULL, 0u, 0u, 0};
    if (cell == NULL)
    {
        return;
    }
    walk->pending = cell;
    walk->pending_capacity = CLI_EXECUTABLES_PENDING;
    walk->found = cell + CLI_EXECUTABLES_PENDING;
    walk->found_capacity = CLI_EXECUTABLES_SCRIPTS;
    walk->depth = (unsigned int *)(void *)(cell + paths);
}

int cli_executables_pending_append(const CliExecutablesWalkCtx *args)
{
    if (args->walk->pending_count >= args->walk->pending_capacity)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->walk->pending[args->walk->pending_count],
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    args->walk->depth[args->walk->pending_count] = args->depth;
    args->walk->pending_count += 1u;
    return 1;
}

int cli_executables_found_append(const CliExecutablesWalkCtx *args)
{
    if (args->walk->found_count >= args->walk->found_capacity)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->walk->found[args->walk->found_count],
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    args->walk->found_count += 1u;
    return 1;
}

int cli_executables_join(const CliExecutablesNameOutFolderCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    return mmgr_line_finish(&line) != 0ULL;
}

int cli_executables_pass(const CliExecutablesNameOutFolderCtx *args)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_skipped[at], .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

// an entry of a folder the walk looks in: a folder to look in after, or a build script found
static void cli_script_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliWalk *const walk = (CliWalk *)args->context;
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx,
                    .folder = walk->folder, .name = name, .out = path))
    {
        return;
    }
    if (args->is_folder)
    {
        if (((walk->at_depth + 1u) < CLI_EXECUTABLES_DEEP) && (name[0] != '.') &&
            !EMBED_CALL(cli_executables_name_out_folder_internal.pass, CliExecutablesNameOutFolderCtx, .name = name))
        {
            (void)EMBED_CALL(cli_executables_walk_internal.pending_append, CliExecutablesWalkCtx, .walk = walk,
                             .path = path, .depth = walk->at_depth + 1u);
        }
    }
    else if (EMBED_CALL(cli_executables_name_out_folder_internal.type_get, CliExecutablesNameOutFolderCtx,
                        .name = name) != CLI_TYPE_NONE)
    {
        (void)EMBED_CALL(cli_executables_walk_internal.found_append, CliExecutablesWalkCtx, .walk = walk, .path = path);
    }
}

void cli_executables_walk(const CliExecutablesWalkTopCtx *args)
{
    CliWalk *walk = args->walk;
    walk->pending_count = 0u;
    (void)EMBED_CALL(cli_executables_walk_internal.pending_append, CliExecutablesWalkCtx, .walk = walk,
                     .path = args->top, .depth = 0u);
    while (walk->pending_count > 0u)
    {
        walk->pending_count -= 1u;
        char folder[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){
            .dst = folder,
            .src = walk->pending[walk->pending_count],
            .bytes =
                cellul.len(&(CatenaFinitaCfg){.src = walk->pending[walk->pending_count], .cap = CLI_POOL_BYTES}) + 1u});
        walk->folder = folder;
        walk->at_depth = walk->depth[walk->pending_count];
        walk->main = 0;
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = args->each,
                   .context_bytess = walk);
        if (walk->main)
        {
            (void)EMBED_CALL(cli_executables_walk_internal.found_append, CliExecutablesWalkCtx, .walk = walk,
                             .path = folder);
        }
    }
}

typedef struct
{
    const char *path;
    const char *folder;
    CliExecutables *found;
    unsigned int type;
    const char *script;
    const char *name;
    const CliWalk *walk;
    const char *file;
    const char *root;
    const char *text;
} ExecutablesReadCtx;
// the last part of a path
EMBED_INLINE const char *cli_base(const ExecutablesReadCtx *args)
{
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    return name;
}

// whether `path` is `folder` or under it, part by part
EMBED_INLINE int cli_under(const ExecutablesReadCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_POOL_BYTES});
    return EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->path, .other_pointer = args->folder,
                      .size = length) &&
           ((args->path[length] == '\0') || (args->path[length] == '/') || (args->path[length] == '\\'));
}

// a program put after those found
EMBED_INLINE void cli_found_append(const ExecutablesReadCtx *args)
{
    unsigned int type = args->type;
    if (args->found->count >= args->found->capacity)
    {
        return;
    }
    CliExecutable *const one = &args->found->item[args->found->count];
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    one->type = type;
    memor.cpy(&(MemoriaCfg){.dst = one->script,
                            .src = args->script,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->script, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = one->name,
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) + 1u});
    args->found->count += 1u;
}

// the programs of the CMake or the Meson projects among the scripts found: each script named `file` that calls
// project() with none above it in the tree names a project, whose targets are those of every such file under its
// folder
static void cli_projects(const ExecutablesReadCtx *args)
{
    for (unsigned int top = 0u; top < args->walk->found_count; top += 1u)
    {
        const char *const script = args->walk->found[top];
        char folder[CLI_PATH_MAX_LEN];
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_base, ExecutablesReadCtx, .path = script),
                                           .other = args->file,
                                           .cap = CLI_POOL_BYTES})) ||
            !EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = script, .out = folder))
        {
            continue;
        }
        int above = 0;
        for (unsigned int other = 0u; !above && (other < args->walk->found_count); other += 1u)
        {
            char dir[CLI_PATH_MAX_LEN];
            above = (cellul.eq(&(CatenaFinitaCfg){
                        .src = EMBED_CALL(cli_base, ExecutablesReadCtx, .path = args->walk->found[other]),
                        .other = args->file,
                        .cap = CLI_POOL_BYTES})) &&
                    EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx,
                               .path = args->walk->found[other], .out = dir) &&
                    EMBED_CALL(cli_under, ExecutablesReadCtx, .path = folder, .folder = dir) &&
                    (!cellul.eq(&(CatenaFinitaCfg){.src = dir, .other = folder, .cap = CLI_POOL_BYTES}));
        }
        char project[CLI_NAME_MAX_LEN];
        char *const top_text =
            above ? NULL : EMBED_CALL(cli_executables_walk_internal.text_alloc, CliExecutablesWalkCtx, .path = script);
        const int named =
            (top_text != NULL) &&
            ((args->type == CLI_TYPE_CMAKE) ? EMBED_CALL(cli_executables_text_out_internal.cmake_project,
                                                         CliExecutablesTextOutCtx, .text = top_text, .out = project)
                                            : EMBED_CALL(cli_executables_text_out_internal.meson_project,
                                                         CliExecutablesTextOutCtx, .text = top_text, .out = project));
        g_cli_guard->persistent_buf_release(top_text, g_cli_executables_owner);
        if (!named)
        {
            continue;
        }
        s_scripts->merged.count = 0u;
        for (unsigned int list = 0u; list < args->walk->found_count; list += 1u)
        {
            if ((!cellul.eq(
                    &(CatenaFinitaCfg){.src = EMBED_CALL(cli_base, ExecutablesReadCtx, .path = args->walk->found[list]),
                                       .other = args->file,
                                       .cap = CLI_POOL_BYTES})) ||
                !EMBED_CALL(cli_under, ExecutablesReadCtx, .path = args->walk->found[list], .folder = folder))
            {
                continue;
            }
            char *const reserved = EMBED_CALL(cli_executables_walk_internal.text_alloc, CliExecutablesWalkCtx,
                                              .path = args->walk->found[list]);
            const char *const text = (reserved != NULL) ? reserved : "";
            char own[CLI_NAME_MAX_LEN];
            if (args->type == CLI_TYPE_CMAKE)
            {
                EMBED_CALL(cli_executables_names_text_internal.cmake_targets, CliExecutablesNamesTextCtx, .text = text,
                           .project = EMBED_CALL(cli_executables_text_out_internal.cmake_project,
                                                 CliExecutablesTextOutCtx, .text = text, .out = own)
                                          ? own
                                          : project,
                           .names = &s_scripts->names);
            }
            else
            {
                EMBED_CALL(cli_executables_names_internal.meson_targets, CliExecutablesNamesCtx, .text = text,
                           .names = &s_scripts->names);
            }
            g_cli_guard->persistent_buf_release(reserved, g_cli_executables_owner);
            for (unsigned int at = 0u; at < s_scripts->names.count; at += 1u)
            {
                EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx,
                           .names = &s_scripts->merged, .name = s_scripts->names.name[at],
                           .length =
                               cellul.len(&(CatenaFinitaCfg){.src = s_scripts->names.name[at], .cap = CLI_POOL_BYTES}),
                           .value = NULL);
            }
        }
        for (unsigned int at = 0u; at < s_scripts->merged.count; at += 1u)
        {
            EMBED_CALL(cli_found_append, ExecutablesReadCtx, .found = args->found, .type = args->type, .script = script,
                       .name = s_scripts->merged.name[at]);
        }
    }
}

// the programs a script of a type other than CMake's and Meson's, whose text is `text`, makes put after those found
EMBED_INLINE void cli_script_text_read(const ExecutablesReadCtx *args)
{
    const char *root = args->root;
    const char *script = args->script;
    const char *text = args->text;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = script, .out = folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_POOL_BYTES}) + 1u});
    }
    int module = 0;
    switch (args->type)
    {
    case CLI_TYPE_CARGO: {
        EMBED_CALL(cli_executables_names_internal.cargo_bins, CliExecutablesNamesCtx, .manifest = script, .text = text,
                   .names = &s_scripts->names);
        break;
    }
    case CLI_TYPE_GO: {
        EMBED_CALL(cli_executables_text_module_internal.go_mains, CliExecutablesTextModuleCtx, .module_text = script,
                   .text = text, .mains = &s_scripts->names);
        break;
    }
    case CLI_TYPE_ZIG: {
        EMBED_CALL(cli_executables_names_internal.zig_exes, CliExecutablesNamesCtx, .text = text,
                   .names = &s_scripts->names);
        break;
    }
    case CLI_TYPE_NPM: {
        EMBED_CALL(cli_executables_names_text_module_internal.npm_bins, CliExecutablesNamesTextModuleCtx, .text = text,
                   .names = &s_scripts->names, .module = &module);
        break;
    }
    default: {
        s_scripts->names.count = 0u;
        char name[CLI_NAME_MAX_LEN];
        char path[CLI_PATH_MAX_LEN];
        const int one = (args->type == CLI_TYPE_DOTNET)
                            ? EMBED_CALL(cli_executables_text_out_internal.dotnet_program, CliExecutablesTextOutCtx,
                                         .project = script, .text = text, .out = name)
                            : EMBED_CALL(cli_executables_text_exe_internal.spec_init, CliExecutablesTextExeCtx,
                                         .text = text, .exe_capacity = name, .path_out = path);
        if (one)
        {
            EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx,
                       .names = &s_scripts->names, .name = name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}),
                       .value = (args->type == CLI_TYPE_DOTNET) ? NULL : path);
        }
        break;
    }
    }
    const int optimize =
        cellul.find(&(CatenaFinitaCfg){
            .src = text, .cap = CLI_POOL_BYTES, .other = "standardOptimizeOption", .other_cap = CLI_POOL_BYTES}) !=
        NULL;
    for (unsigned int at = 0u; at < s_scripts->names.count; at += 1u)
    {
        const unsigned int before = args->found->count;
        EMBED_CALL(cli_found_append, ExecutablesReadCtx, .found = args->found, .type = args->type, .script = script,
                   .name = s_scripts->names.name[at]);
        if (args->found->count == before)
        {
            return;
        }
        CliExecutable *const one = &args->found->item[args->found->count - 1u];
        one->optimize = optimize;
        if ((args->type == CLI_TYPE_GO) || (args->type == CLI_TYPE_PYINSTALLER))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = one->detail,
                .src = s_scripts->names.value[at],
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){.src = s_scripts->names.value[at], .cap = CLI_POOL_BYTES}) + 1u});
        }
        if (args->type == CLI_TYPE_NPM)
        {
            const char *const entry = s_scripts->names.value[at];
            const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = entry, .cap = CLI_POOL_BYTES});
            one->commonjs =
                ((length >= 4ULL) && (cellul.eq(&(CatenaFinitaCfg){
                                         .src = entry + length - 4ULL, .other = ".cjs", .cap = CLI_POOL_BYTES}))) ||
                ((length >= 3ULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = entry + length - 3ULL, .other = ".js", .cap = CLI_POOL_BYTES})) &&
                 !module);
            const char *trimmed = entry;
            while (cellul.starts(&(CatenaFinitaCfg){.src = trimmed, .other = "./", .cap = CLI_POOL_BYTES}))
            {
                trimmed += 2;
            }
            char path[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = folder,
                       .name = trimmed, .out = path);
            EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = path,
                       .relative_out = one->detail);
            char *const bin = EMBED_CALL(cli_executables_walk_internal.text_alloc, CliExecutablesWalkCtx, .path = path);
            EMBED_CALL(cli_executables_folder_internal.npm_builders, CliExecutablesFolderCtx, .folder = folder,
                       .script = (bin != NULL) ? bin : "", .one = one);
            g_cli_guard->persistent_buf_release(bin, g_cli_executables_owner);
        }
    }
}

// the programs a script of a type other than CMake's and Meson's makes put after those found, its text read into a
// cell of its size
EMBED_INLINE void cli_script_read(const ExecutablesReadCtx *args)
{
    char *const text =
        EMBED_CALL(cli_executables_walk_internal.text_alloc, CliExecutablesWalkCtx, .path = args->script);
    if (text != NULL)
    {
        EMBED_CALL(cli_script_text_read, ExecutablesReadCtx, .root = args->root, .script = args->script, .text = text,
                   .type = args->type, .found = args->found);
    }
    g_cli_guard->persistent_buf_release(text, g_cli_executables_owner);
}

void cli_executables_read(const CliExecutablesFoundCtx *args)
{
    g_cli_guard->persistent_buf_release(args->found->item, g_cli_executables_owner);
    g_cli_guard->persistent_buf_release(args->found->order, g_cli_executables_owner);
    *args->found = (CliExecutables){NULL, 0u, 0u, NULL};
    args->found->item = g_cli_guard->persistent_buf_alloc(
        (unsigned long long)CLI_EXECUTABLES_MAX * sizeof(*args->found->item), g_cli_executables_owner);
    args->found->capacity = (args->found->item != NULL) ? CLI_EXECUTABLES_MAX : 0u;
    s_scripts = g_cli_guard->persistent_buf_alloc(sizeof(*s_scripts), g_cli_executables_owner);
    if (s_scripts == NULL)
    {
        return;
    }
    CliWalk *const walk = &s_scripts->walk;
    EMBED_CALL(cli_executables_walk_internal.walk_alloc, CliExecutablesWalkCtx, .walk = walk);
    EMBED_CALL(cli_executables_walk_top_internal.walk, CliExecutablesWalkTopCtx, .walk = walk, .top = args->root,
               .each = cli_script_each);
    EMBED_CALL(cli_text_paths.paths_sort, CliTextPathsCfg, .paths = walk->found, .count = walk->found_count);
    EMBED_CALL(cli_projects, ExecutablesReadCtx, .walk = walk, .file = "CMakeLists.txt", .type = CLI_TYPE_CMAKE,
               .found = args->found);
    EMBED_CALL(cli_projects, ExecutablesReadCtx, .walk = walk, .file = "meson.build", .type = CLI_TYPE_MESON,
               .found = args->found);
    for (unsigned int at = 0u; at < walk->found_count; at += 1u)
    {
        const unsigned int type =
            EMBED_CALL(cli_executables_name_out_folder_internal.type_get, CliExecutablesNameOutFolderCtx,
                       .name = EMBED_CALL(cli_base, ExecutablesReadCtx, .path = walk->found[at]));
        if ((type != CLI_TYPE_CMAKE) && (type != CLI_TYPE_MESON) && (type != CLI_TYPE_NONE))
        {
            EMBED_CALL(cli_script_read, ExecutablesReadCtx, .root = args->root, .script = walk->found[at], .type = type,
                       .found = args->found);
        }
    }
    g_cli_guard->persistent_buf_release(walk->pending, g_cli_executables_owner);
    *walk = (CliWalk){NULL, NULL, 0u, 0u, NULL, 0u, NULL, 0u, 0u, 0};
    g_cli_guard->persistent_buf_release(s_scripts, g_cli_executables_owner);
    s_scripts = NULL;
    cli_executables_found_internal.order(args);
}
