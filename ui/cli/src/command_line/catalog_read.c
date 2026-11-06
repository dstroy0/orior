// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_read.c: the catalog read from the tree: the build scripts, the sims, the driver's parts, the runners, the
// page builders, the stage scripts and their pipelines, and the test suites
#include "../command_line/pool.h"
#include "../text/text.h"
#include "callback_cfg.h"
#include "catalog_internal.h"

// the deepest folders are looked through for a sim's source
#define CLI_FIND_DEPTH 8u

static int cli_is_build(const CliNameCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    return (cellul.starts(&(CatenaFinitaCfg){.src = args->name, .other = "build", .cap = CLI_POOL_BYTES})) &&
           (length >= 3u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 3u, .other = ".sh", .cap = CLI_POOL_BYTES})) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "build_stamp.sh", .cap = CLI_POOL_BYTES}));
}

int cli_catalog_is_cfg(const CliNameCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    return (length >= 4u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 4u, .other = ".cfg", .cap = CLI_POOL_BYTES}));
}

static int cli_is_cu(const CliNameCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    return (length >= 3u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 3u, .other = ".cu", .cap = CLI_POOL_BYTES}));
}

int cli_catalog_is_qasm(const CliNameCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    return (length >= 5u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 5u, .other = ".qasm", .cap = CLI_POOL_BYTES}));
}

int cli_catalog_is_py(const CliNameCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    return (length >= 3u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 3u, .other = ".py", .cap = CLI_POOL_BYTES}));
}

int cli_catalog_is_builder(const CliNameCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    return (cellul.starts(&(CatenaFinitaCfg){.src = args->name, .other = "build_", .cap = CLI_POOL_BYTES})) &&
           (length >= 8u) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 8u, .other = "_view.py", .cap = CLI_POOL_BYTES}));
}

int cli_catalog_is_figure(const CliNameCfg *args)
{
    return (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "build_infographic.py", .cap = CLI_POOL_BYTES})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "make_shadow_figure.py", .cap = CLI_POOL_BYTES}));
}

void cli_catalog_short(const CliCatalogOutCtx *args)
{
    char *out = args->out;
    const char *const last = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->file,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES}), .byte = '/');
    const char *folder = args->file;
    if (last != NULL)
    {
        folder = last;
        folder = args->file + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->file,
                                         .at = (unsigned long long)(folder - args->file), .set = "/");
    }
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = out, .text = folder, .otherwise = args->file);
}

// a job of a script: its id the group and its file, its title its file
void cli_catalog_script_job(const CliCatalogPathCtx *args)
{
    CliCatalog *catalog = args->catalog;
    const char *group = args->group;
    const char *title = args->title;
    const char *opens = args->opens;
    char file[CLI_PATH_MAX_LEN];
    char id[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = args->path,
               .relative_out = file);
    VerbaLinea line = {id, sizeof(id), 0ULL};
    mmgr_line_put(&line, group);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, file);
    mmgr_line_finish(&line);
    char *const about = cli_catalog_path_internal.about_alloc(args);
    EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = catalog, .id = id, .group = group,
               .title = (title != NULL) ? title : file, .file = file, .about = (about != NULL) ? about : "",
               .opens = opens);
    g_cli_guard->persistent_buf_release(about, g_cli_catalog_owner);
}

// A bash script that takes free words as its job; a build traced: bash takes SHELLOPTS from its environment as its own
// options, and gives it on, so every script the build runs prints each command as it runs it
void cli_catalog_bash_alone(const CliCatalogPathCtx *args)
{
    CliCatalog *catalog = args->catalog;
    char file[CLI_PATH_MAX_LEN];
    char title[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = args->path,
               .relative_out = file);
    EMBED_CALL(cli_catalog_out_internal.catalog_short, CliCatalogOutCtx, .file = file, .out = title);
    EMBED_CALL(cli_catalog_path_internal.script_job, CliCatalogPathCtx, .catalog = catalog, .root = args->root,
               .group = args->group, .path = args->path, .title = title, .opens = "");
    const int build = cellul.eq(&(CatenaFinitaCfg){.src = args->group, .other = "build", .cap = CLI_POOL_BYTES});
    EMBED_CALL(cli_catalog_choice_table.words, CliCatalogChoiceCfg, .catalog = catalog);
    if (build)
    {
        EMBED_CALL(cli_catalog_choice_table.trace, CliCatalogChoiceCfg, .catalog = catalog);
    }
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog, .program = CLI_PROGRAM_BASH,
               .text = file, .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_WORDS, .a = "arguments",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    if (build)
    {
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SET_WHEN,
                   .a = CLI_TRACE, .b = CLI_TRACE_VERBOSE, .c = "SHELLOPTS", .d = "xtrace", .separated = 0);
    }
}

// The build scripts: the engine's, each example's, and the maintenance builds. build_stamp.sh is a library the others
// source and never a build of its own
void cli_catalog_builds(const CliCatalogPathCtx *args)
{
    static const char *const s_positions[3] = {"/src", "/utils/maint/engine", "/utils/maint/texbuild"};
    char place[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    CliEntries examples = {NULL, 0u, 0u};
    CliEntries builds = {NULL, 0u, 0u};
    CliPaths list_entries = {NULL, 0u};
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->root, .name = "examples",
               .path_out = place);
    EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = place, .folders = 1, .filter = NULL,
               .entries = &examples);
    (void)EMBED_CALL(cli_catalog_max_internal.paths_capacity, CliCatalogMaxCtx, .paths = &list_entries,
                     .most = 3u + examples.count);
    unsigned int positions = 0u;
    for (unsigned int at = 0u; (at < 3u) && (positions < list_entries.capacity); at += 1u)
    {
        VerbaLinea line = {list_entries.path[positions], CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, args->root);
        mmgr_line_put(&line, s_positions[at]);
        positions += (mmgr_line_finish(&line) != 0ULL) ? 1u : 0u;
    }
    for (unsigned int at = 0u; (at < examples.count) && (positions < list_entries.capacity); at += 1u)
    {
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = place,
                   .name = examples.name[at], .path_out = list_entries.path[positions]);
        positions += 1u;
    }
    g_cli_guard->persistent_buf_release(examples.name, g_cli_catalog_owner);
    examples.name = NULL;
    examples.count = 0u;
    examples.capacity = 0u;
    for (unsigned int at = 0u; at < positions; at += 1u)
    {
        EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = list_entries.path[at],
                   .folders = 0, .filter = cli_is_build, .entries = &builds);
        for (unsigned int each = 0u; each < builds.count; each += 1u)
        {
            EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = list_entries.path[at],
                       .name = builds.name[each], .path_out = path);
            EMBED_CALL(cli_catalog_path_internal.bash_alone, CliCatalogPathCtx, .catalog = args->catalog,
                       .root = args->root, .group = "build", .path = path);
        }
    }
    g_cli_guard->persistent_buf_release(builds.name, g_cli_catalog_owner);
    builds.name = NULL;
    builds.count = 0u;
    builds.capacity = 0u;
    g_cli_guard->persistent_buf_release(list_entries.path, g_cli_catalog_owner);
    list_entries.path = NULL;
    list_entries.capacity = 0u;
}

typedef struct
{
    const char *under;
    const char *name;
    unsigned int depth;
    char *found;
} CatalogReadCtx;
// the folder named `name` under `under`, the first found depth first in order, into `found`: 1, or 0
static int cli_find_dir(const CatalogReadCtx *args)
{
    const char *name = args->name;
    if (args->depth >= CLI_FIND_DEPTH)
    {
        return 0;
    }
    CliEntries dirs = {NULL, 0u, 0u};
    EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = args->under, .folders = 1,
               .filter = NULL, .entries = &dirs);
    int found_one = 0;
    for (unsigned int at = 0u; !found_one && (at < dirs.count); at += 1u)
    {
        char dir[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->under,
                   .name = dirs.name[at], .path_out = dir);
        if (cellul.eq(&(CatenaFinitaCfg){.src = dirs.name[at], .other = name, .cap = CLI_POOL_BYTES}))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->found,
                                    .src = dir,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_POOL_BYTES}) + 1u});
            found_one = 1;
        }
        else
        {
            found_one = EMBED_CALL(cli_find_dir, CatalogReadCtx, .under = dir, .name = name, .depth = args->depth + 1u,
                                   .found = args->found);
        }
    }
    g_cli_guard->persistent_buf_release(dirs.name, g_cli_catalog_owner);
    dirs.name = NULL;
    dirs.count = 0u;
    dirs.capacity = 0u;
    return found_one;
}

// the file named `name` under `under`, here first, then in each folder depth first in order, into `found`: 1, or 0
static int cli_find_file(const CatalogReadCtx *args)
{
    const char *name = args->name;
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->under, .name = name,
               .path_out = args->found);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->found))
    {
        return 1;
    }
    if (args->depth >= CLI_FIND_DEPTH)
    {
        return 0;
    }
    CliEntries dirs = {NULL, 0u, 0u};
    EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = args->under, .folders = 1,
               .filter = NULL, .entries = &dirs);
    int found_one = 0;
    for (unsigned int at = 0u; !found_one && (at < dirs.count); at += 1u)
    {
        char dir[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->under,
                   .name = dirs.name[at], .path_out = dir);
        found_one = EMBED_CALL(cli_find_file, CatalogReadCtx, .under = dir, .name = name, .depth = args->depth + 1u,
                               .found = args->found);
    }
    g_cli_guard->persistent_buf_release(dirs.name, g_cli_catalog_owner);
    dirs.name = NULL;
    dirs.count = 0u;
    dirs.capacity = 0u;
    return found_one;
}

char *cli_catalog_script_alloc(const CliCatalogPathOutCtx *args)
{
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = g_cli_catalog_owner);
    if (bytes == NULL)
    {
        return NULL;
    }
    char *const text = g_cli_guard->persistent_buf_alloc(size + 1ULL, g_cli_catalog_owner);
    if (text != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = text, .src = bytes, .bytes = (size_t)size});
        text[size] = '\0';
    }
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_catalog_owner));
    return text;
}

// One job per sim in the case src/sims/run.sh accepts, each with the first comment its source opens with
void cli_catalog_sims(const CliCatalogPathCtx *args)
{
    CliCatalog *catalog = args->catalog;
    char script[CLI_PATH_MAX_LEN];
    char sims_cu[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->root,
               .name = "src/sims/run.sh", .path_out = script);
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->root,
               .name = "src/sims/cu", .path_out = sims_cu);
    char *const text = EMBED_CALL(cli_catalog_path_out_internal.script_alloc, CliCatalogPathOutCtx, .path = script);
    if (text == NULL)
    {
        return;
    }
    CliEntries names = {NULL, 0u, 0u};
    CliEntries sources = {NULL, 0u, 0u};
    (void)EMBED_CALL(cli_catalog_names_internal.names_capacity, CliCatalogNamesCtx, .names = &names,
                     .most = EMBED_CALL(cli_catalog_text_variable_internal.case_names, CliCatalogTextVariableCtx,
                                        .text_read = text, .variable = "SIM", .names_callback = NULL, .capacity = 0u));
    const unsigned int count =
        (names.name != NULL)
            ? EMBED_CALL(cli_catalog_text_variable_internal.case_names, CliCatalogTextVariableCtx, .text_read = text,
                         .variable = "SIM", .names_callback = names.name, .capacity = names.capacity)
            : 0u;
    g_cli_guard->persistent_buf_release(text, g_cli_catalog_owner);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const sim = names.name[at];
        char dir[CLI_PATH_MAX_LEN];
        char source[CLI_PATH_MAX_LEN];
        int found = 0;
        if (EMBED_CALL(cli_find_dir, CatalogReadCtx, .under = sims_cu, .name = sim, .depth = 0u, .found = dir))
        {
            EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = dir, .folders = 0,
                       .filter = cli_is_cu, .entries = &sources);
            found = sources.count != 0u;
            if (found)
            {
                EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = dir,
                           .name = sources.name[0], .path_out = source);
            }
        }
        if (!found)
        {
            char named[CLI_NAME_MAX_LEN];
            VerbaLinea line = {named, sizeof(named), 0ULL};
            mmgr_line_put(&line, sim);
            mmgr_line_put(&line, ".cu");
            mmgr_line_finish(&line);
            found = EMBED_CALL(cli_find_file, CatalogReadCtx, .under = sims_cu, .name = named, .depth = 0u,
                               .found = source);
        }
        char id[CLI_PATH_MAX_LEN];
        char file[CLI_PATH_MAX_LEN];
        VerbaLinea line = {id, sizeof(id), 0ULL};
        mmgr_line_put(&line, "sim/");
        mmgr_line_put(&line, sim);
        mmgr_line_finish(&line);
        char *about = NULL;
        if (found)
        {
            EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = source,
                       .relative_out = file);
            about = EMBED_CALL(cli_catalog_path_internal.about_alloc, CliCatalogPathCtx, .path = source);
        }
        else
        {
            memor.cpy(&(MemoriaCfg){
                .dst = file,
                .src = "src/sims/run.sh",
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "src/sims/run.sh", .cap = CLI_POOL_BYTES}) + 1u});
        }
        EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = catalog, .id = id, .group = "sim",
                   .title = sim, .file = file, .about = (about != NULL) ? about : "", .opens = "");
        g_cli_guard->persistent_buf_release(about, g_cli_catalog_owner);
        EMBED_CALL(cli_catalog_choice_table.words, CliCatalogChoiceCfg, .catalog = catalog);
        EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
                   .program = CLI_PROGRAM_BASH, .text = "src/sims/run.sh", .tool_program = NULL, .system = 0);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_LIT, .a = sim,
                   .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_WORDS,
                   .a = "arguments", .b = NULL, .c = NULL, .d = NULL, .separated = 1);
    }
    g_cli_guard->persistent_buf_release(sources.name, g_cli_catalog_owner);
    sources.name = NULL;
    sources.count = 0u;
    sources.capacity = 0u;
    g_cli_guard->persistent_buf_release(names.name, g_cli_catalog_owner);
    names.name = NULL;
    names.count = 0u;
    names.capacity = 0u;
}

unsigned int cli_catalog_paths(const CliCatalogFilterCtx *args)
{
    const char *root = args->root;
    unsigned int at = args->at;
    CliEntries files = {NULL, 0u, 0u};
    EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = args->dir, .folders = 0,
               .filter = args->filter, .entries = &files);
    (void)EMBED_CALL(cli_catalog_max_internal.paths_capacity, CliCatalogMaxCtx, .paths = args->list,
                     .most = at + files.count);
    for (unsigned int each = 0u; (each < files.count) && (at < args->list->capacity); each += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->dir,
                   .name = files.name[each], .path_out = path);
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = path,
                   .relative_out = args->list->path[at]);
        at += 1u;
    }
    g_cli_guard->persistent_buf_release(files.name, g_cli_catalog_owner);
    files.name = NULL;
    files.count = 0u;
    files.capacity = 0u;
    return at;
}
