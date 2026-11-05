// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_gather.c: the database gathered from the tree's builds: each CMake build's entries from its file API
// reply, and each Meson build's compile_commands.json, written together where they changed, and read for the
// compilers they name
#include "../command_line/cells.h"
#include "../text/text.h"
#include "compile_db_internal.h"

CliGathering g_cli_gather;

static int gather_cells_alloc(void)
{
    const int entries =
        EMBED_CALL(cli_json_tree_nodes_max.json_reserved_temporary, CliJsonTreeNodesMaxCfg, .json = &g_cli_gather.json,
                   .nodes_max = CLI_GATHER_NODES_CAPACITY, .text_max = CLI_GATHER_TEXT_CAPACITY,
                   .what = "the compile entries gathered", .owner = &g_cli_build_cells);
    char *const flags_cell = g_cli_cells->temporary_buf_alloc(CLI_GATHER_FLAGS_CAPACITY, &g_cli_build_cells);
    char *const fragment_cell = g_cli_cells->temporary_buf_alloc(CLI_GATHER_FLAGS_CAPACITY, &g_cli_build_cells);
    g_cli_gather.flags_text = mmgr_line_over(flags_cell, CLI_GATHER_FLAGS_CAPACITY);
    g_cli_gather.fragment_text = mmgr_line_over(fragment_cell, CLI_GATHER_FLAGS_CAPACITY);
    const int reserved = entries && (flags_cell != NULL) && (fragment_cell != NULL);
    if (!reserved)
    {
        g_cli_cells->persistent_buf_release(g_cli_gather.json.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg,
                                                                               .cell = g_cli_gather.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_gather.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_cells->persistent_buf_release(g_cli_gather.flags_text.out, &g_cli_build_cells);
        g_cli_cells->persistent_buf_release(g_cli_gather.fragment_text.out, &g_cli_build_cells);
        g_cli_gather.flags_text = (VerbaLinea){NULL, 0ULL, 0ULL, NULL, NULL, 0ULL};
        g_cli_gather.fragment_text = (VerbaLinea){NULL, 0ULL, 0ULL, NULL, NULL, 0ULL};
    }
    return reserved;
}

typedef struct
{
    const char *folder;
    unsigned int entries;
    CliDatabase *database;
    const char *project;
    unsigned long long time;
    const char *path;
    const char *root;
    const CliInputs *inputs;
} CompileDbGatherCtx;
// a database of the entries `entries`, given to clangd from `folder` where one is given, with the compilers they name
static void cli_of_entries(const CompileDbGatherCtx *args)
{
    const CliJson *const json = &g_cli_gather.json;
    args->database->has_folder = args->folder != NULL;
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = args->database->folder, .text = args->folder,
               .otherwise = "");
    args->database->compilers = 0u;
    args->database->msvc = 0;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = args->entries, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int entry = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = args->entries, .index = at);
        char compiler[CLI_PATH_MAX_LEN];
        const unsigned int arguments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = json, .object = entry, .name = "arguments");
        const char *const first =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = arguments, .index = 0u),
                       .length_pointer = NULL);
        unsigned long long size = 0ULL;
        const char *const command =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = entry, .name = "command"),
                       .length_pointer = &size);
        int found = 0;
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = arguments,
                       .kind = CFG_JSON_ARRAY))
        {
            found = first != NULL;
            EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = compiler, .text = first, .otherwise = "");
        }
        else if (command != NULL)
        {
            found = EMBED_CALL(cli_compile_db_text_length_internal.compile_first_word, CliCompileDbTextLengthCtx,
                               .text = command, .length = size, .first = compiler);
        }
        if (!found)
        {
            continue;
        }
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = compiler,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = compiler, .cap = CLI_CELLS_POOL}), .from = '\\',
                   .to = '/');
        // the compilers stored in order, each once
        unsigned int place = 0u;
        while ((place < args->database->compilers) &&
               (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->database->compiler[place],
                           .other_pointer = compiler) < 0))
        {
            place += 1u;
        }
        if (((place < args->database->compilers) &&
             (cellul.eq(&(CatenaFinitaCfg){
                 .src = args->database->compiler[place], .other = compiler, .cap = CLI_CELLS_POOL}))) ||
            (args->database->compilers >= CLI_COMPILERS))
        {
            continue;
        }
        for (unsigned int move = args->database->compilers; move > place; move -= 1u)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->database->compiler[move],
                                    .src = args->database->compiler[move - 1u],
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->database->compiler[move - 1u],
                                                                           .cap = CLI_CELLS_POOL}) +
                                             1u});
        }
        memor.cpy(&(MemoriaCfg){.dst = args->database->compiler[place],
                                .src = compiler,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = compiler, .cap = CLI_CELLS_POOL}) + 1u});
        args->database->compilers += 1u;
        args->database->msvc =
            args->database->msvc || EMBED_CALL(cli_compile_db_table.is_cl, CliCompileDbCfg, .compiler = compiler);
    }
}

// a project's entries stored where it has none yet or these were configured later
EMBED_INLINE void cli_project_append(const CompileDbGatherCtx *args)
{
    unsigned long long time = args->time;
    unsigned int entries = args->entries;
    unsigned int at = 0u;
    while ((at < g_cli_gather.projects) &&
           (!cellul.eq(&(CatenaFinitaCfg){
               .src = g_cli_gather.project[at].project, .other = args->project, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    if ((at == g_cli_gather.projects) && (g_cli_gather.projects < CLI_GATHER_PROJECTS))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = g_cli_gather.project[at].project,
                          .src = args->project,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->project, .cap = CLI_CELLS_POOL}) + 1u});
        g_cli_gather.project[at].time = time;
        g_cli_gather.project[at].entries = entries;
        g_cli_gather.projects += 1u;
    }
    else if ((at < g_cli_gather.projects) && (g_cli_gather.project[at].time < time))
    {
        g_cli_gather.project[at].time = time;
        g_cli_gather.project[at].entries = entries;
    }
}

// the entries written, in a line given back after, where they differ from the file there
EMBED_INLINE void cli_write(const CompileDbGatherCtx *args)
{
    const char *path = args->path;
    const unsigned long long out_capacity =
        EMBED_CALL(cli_json_tree_node_out.json_pretty_capacity, CliJsonTreeNodeOutCfg, .json_read = &g_cli_gather.json,
                   .indent = 2u);
    char *const out_cell = g_cli_cells->temporary_buf_alloc(out_capacity, &g_cli_build_cells);
    VerbaLinea out = mmgr_line_over(out_cell, out_capacity);
    if (out.out == NULL)
    {
        return;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &g_cli_gather.json,
               .node = args->entries, .out = &out, .indent = 2u, .sorted = 0);
    if (mmgr_line_ok(&out))
    {
        unsigned long long size = 0ULL;
        const unsigned char *const there = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                      .size_pointer = &size, .owner = &g_cli_build_cells);
        const int same = (there != NULL) && (size == out.at) &&
                         (memor.cmp(&(MemoriaCfg){.src = there, .other = out.out, .bytes = size}) == 0);
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)there, (&g_cli_build_cells));
        char folder[CLI_PATH_MAX_LEN];
        if (!same && EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder))
        {
            EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
            EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = out.out,
                       .size = out.at);
        }
    }
    g_cli_cells->persistent_buf_release(out.out, &g_cli_build_cells);
}

// the gathering itself, its cells taken: the database's compilers and folder
EMBED_INLINE void gather_reserve(const CompileDbGatherCtx *args);

void cli_compile_gather(const CliCompileDbRootCtx *args)
{
    if (gather_cells_alloc())
    {
        EMBED_CALL(gather_reserve, CompileDbGatherCtx, .root = args->root, .inputs = args->inputs_read,
                   .database = args->database);
        g_cli_cells->persistent_buf_release(g_cli_gather.json.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg,
                                                                               .cell = g_cli_gather.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_gather.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_cells->persistent_buf_release(g_cli_gather.flags_text.out, &g_cli_build_cells);
        g_cli_cells->persistent_buf_release(g_cli_gather.fragment_text.out, &g_cli_build_cells);
        g_cli_gather.flags_text = (VerbaLinea){NULL, 0ULL, 0ULL, NULL, NULL, 0ULL};
        g_cli_gather.fragment_text = (VerbaLinea){NULL, 0ULL, 0ULL, NULL, NULL, 0ULL};
    }
    else
    {
        args->database->has_folder = 0;
        args->database->folder[0] = '\0';
        args->database->compilers = 0u;
        args->database->msvc = 0;
    }
}

EMBED_INLINE void gather_reserve(const CompileDbGatherCtx *args)
{
    const char *root = args->root;
    CliJson *const json = &g_cli_gather.json;
    if (args->inputs->own)
    {
        const unsigned int own = EMBED_CALL(cli_compile_db_path_internal.compile_json_read, CliCompileDbPathCtx,
                                            .json = json, .path = args->inputs->file[0].path);
        EMBED_CALL(cli_of_entries, CompileDbGatherCtx, .folder = NULL, .entries = own, .database = args->database);
        return;
    }
    // each CMake project's newest configuration, by the project's own folder, and each Meson build's
    g_cli_gather.projects = 0u;
    for (unsigned int index = 0u; index < args->inputs->count; index += 1u)
    {
        const CliStamped *const file = &args->inputs->file[index];
        const char *name = file->path;
        const unsigned long long name_end =
            EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = file->path,
                       .at = cellul.len(&(CatenaFinitaCfg){.src = file->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
        name = (name_end != 0ULL) ? (file->path + name_end) : name;
        char project[CLI_PATH_MAX_LEN] = "";
        unsigned int entries = CLI_JSON_NONE;
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "compile_commands.json", .cap = CLI_CELLS_POOL}))
        {
            char folder[CLI_PATH_MAX_LEN];
            entries = EMBED_CALL(cli_compile_db_path_internal.compile_json_read, CliCompileDbPathCtx, .json = json,
                                 .path = file->path);
            entries = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                 .node = entries, .kind = CFG_JSON_ARRAY)
                          ? entries
                          : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json,
                                       .kind = CFG_JSON_ARRAY);
            if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = file->path,
                            .out = folder) ||
                !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = folder, .out = project))
            {
                project[0] = '\0';
            }
        }
        else
        {
            entries = EMBED_CALL(cli_compile_db_index_internal.compile_cmake_entries, CliCompileDbIndexCtx,
                                 .index_read = file->path, .source = project);
        }
        if (entries != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_project_append, CompileDbGatherCtx, .project = project, .time = file->time,
                       .entries = entries);
        }
    }
    // the projects by their folders, their entries one after another
    for (unsigned int at = 1u; at < g_cli_gather.projects; at += 1u)
    {
        const CliProject entry = g_cli_gather.project[at];
        unsigned int place = at;
        while ((place > 0u) &&
               (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = g_cli_gather.project[place - 1u].project,
                           .other_pointer = entry.project) > 0))
        {
            g_cli_gather.project[place] = g_cli_gather.project[place - 1u];
            place -= 1u;
        }
        g_cli_gather.project[place] = entry;
    }
    const unsigned int all =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    for (unsigned int at = 0u; at < g_cli_gather.projects; at += 1u)
    {
        const unsigned int entries = g_cli_gather.project[at].entries;
        for (unsigned int item = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                .json_read = json, .array = entries, .index = item) != CLI_JSON_NONE;
             item += 1u)
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = all,
                       .value =
                           EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = json,
                                      .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                         .json_read = json, .array = entries, .index = item)));
        }
    }
    if (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = all,
                   .index = 0u) == CLI_JSON_NONE)
    {
        args->database->has_folder = 0;
        args->database->folder[0] = '\0';
        args->database->compilers = 0u;
        args->database->msvc = 0;
        return;
    }
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = CLI_COMPILE_WRITTEN, .joined = path);
    EMBED_CALL(cli_write, CompileDbGatherCtx, .path = path, .entries = all);
    EMBED_CALL(cli_of_entries, CompileDbGatherCtx,
               .folder =
                   EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder)
                       ? folder
                       : NULL,
               .entries = all, .database = args->database);
}
