// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_gather.c: the database gathered from the tree's builds: each CMake build's entries from its file API
// reply, and each Meson build's compile_commands.json, written together where they changed, and read for the
// compilers they name
#include "../command_line/pool.h"
#include "../text/sift.h"
#include "../text/text.h"
#include "compile_db_internal.h"

// the bytes of the tree's own database read and sifted at once, and the bytes each piece reads on past its own so a
// key near its end is read whole
#define CLI_COMPILER_PIECE (1ULL << 20u)
#define CLI_COMPILER_OVERLAP (4ULL * CLI_PATH_MAX_LEN)

CliGathering g_cli_gather;

static int gather_cells_alloc(void)
{
    const int entries =
        EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = &g_cli_gather.json,
                   .nodes_max = CLI_GATHER_NODES_CAPACITY, .text_max = CLI_GATHER_TEXT_CAPACITY,
                   .what = "the compile entries gathered", .owner = g_cli_build_owner);
    cli_grown_begin(&g_cli_gather.flags_text, CLI_GATHER_FLAGS_CAPACITY, g_cli_build_owner);
    const int reserved = entries && (g_cli_gather.flags_text.cell != NULL);
    if (!reserved)
    {
        g_cli_guard->persistent_buf_release(g_cli_gather.json.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_gather.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_gather.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        cli_grown_end(&g_cli_gather.flags_text);
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
// the compiler `compiler`, its backslashes made slashes, put among the database's in order where it is not there yet
// and the list has room, the database marked one that names cl where it is
static void cli_compiler_note(CliDatabase *database, char *compiler)
{
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = compiler,
               .size = cellul.len(&(CatenaFinitaCfg){.src = compiler, .cap = CLI_POOL_BYTES}), .from = '\\', .to = '/');
    unsigned int place = 0u;
    while ((place < database->compilers) &&
           (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = database->compiler[place],
                       .other_pointer = compiler) < 0))
    {
        place += 1u;
    }
    if (((place < database->compilers) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = database->compiler[place], .other = compiler, .cap = CLI_POOL_BYTES}))) ||
        (database->compilers >= CLI_COMPILERS))
    {
        return;
    }
    for (unsigned int move = database->compilers; move > place; move -= 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = database->compiler[move],
            .src = database->compiler[move - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = database->compiler[move - 1u], .cap = CLI_POOL_BYTES}) + 1u});
    }
    memor.cpy(&(MemoriaCfg){.dst = database->compiler[place],
                            .src = compiler,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = compiler, .cap = CLI_POOL_BYTES}) + 1u});
    database->compilers += 1u;
    database->msvc = database->msvc || EMBED_CALL(cli_compile_db_table.is_cl, CliCompileDbCfg, .compiler = compiler);
}

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
        if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = arguments,
                       .type = CFG_JSON_ARRAY))
        {
            found = first != NULL;
            EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = compiler, .text = first, .otherwise = "");
        }
        else if (command != NULL)
        {
            found = EMBED_CALL(cli_compile_db_text_length_internal.compile_first_word, CliCompileDbTextLengthCtx,
                               .text = command, .length = size, .first = compiler);
        }
        if (found)
        {
            cli_compiler_note(args->database, compiler);
        }
    }
}

// The tree's own database sifted for its compilers: the database, the piece of the file read, its bytes, and the
// place past which a hit is too near the piece's end for its value to be whole
typedef struct
{
    CliDatabase *database;
    const unsigned char *piece;
    unsigned long long piece_size;
    unsigned long long whole_before;
} CliCompilerSift;

// one place the sift found an entry's key at: the compiler its value names noted, the first string of an arguments
// array or the first word of a command's string, its escapes read
static void cli_compiler_found(const CliEachAtCfg *args)
{
    const CliCompilerSift *const sift = (const CliCompilerSift *)args->context;
    if (args->at >= sift->whole_before)
    {
        return;
    }
    unsigned long long at = args->at;
    // the key's closing quote, then the colon, then the value's opening bracket or quote
    while ((at < sift->piece_size) && (sift->piece[at] != ':'))
    {
        at += 1ULL;
    }
    while ((at < sift->piece_size) && (sift->piece[at] != '[') && (sift->piece[at] != '"'))
    {
        at += 1ULL;
    }
    const int arguments = (at < sift->piece_size) && (sift->piece[at] == '[');
    while (arguments && (at < sift->piece_size) && (sift->piece[at] != '"'))
    {
        at += 1ULL;
    }
    char value[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ULL;
    at += 1ULL;
    while ((at < sift->piece_size) && (sift->piece[at] != '"') && (length < (sizeof(value) - 1u)))
    {
        // an escaped byte is the byte after its backslash
        at += (sift->piece[at] == '\\') ? 1ULL : 0ULL;
        value[length] = (at < sift->piece_size) ? (char)sift->piece[at] : '\0';
        length += 1ULL;
        at += 1ULL;
    }
    value[length] = '\0';
    char compiler[CLI_PATH_MAX_LEN];
    int found = 0;
    if (arguments)
    {
        EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = compiler, .text = value, .otherwise = "");
        found = length != 0ULL;
    }
    else
    {
        found = EMBED_CALL(cli_compile_db_text_length_internal.compile_first_word, CliCompileDbTextLengthCtx,
                           .text = value, .length = length, .first = compiler);
    }
    if (found)
    {
        cli_compiler_note(sift->database, compiler);
    }
}

// the tree's own database at `path` read a piece at a time and sifted for the keys that hold a compiler, no tree of it
// built, its compilers into `database`, which is given to clangd from no folder
static void cli_of_file(const CompileDbGatherCtx *args)
{
    args->database->has_folder = 0;
    args->database->folder[0] = '\0';
    args->database->compilers = 0u;
    args->database->msvc = 0;
    unsigned long long size = 0ULL;
    unsigned char *const piece =
        g_cli_guard->temporary_buf_alloc(CLI_COMPILER_PIECE + CLI_COMPILER_OVERLAP, g_cli_compile_owner);
    if ((piece == NULL) ||
        !EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = args->path, .size_pointer = &size))
    {
        g_cli_guard->persistent_buf_release(piece, g_cli_compile_owner);
        return;
    }
    static const char *const s_keys[] = {"\"arguments\"", "\"command\""};
    // each piece reads on past its own bytes by the overlap, and a hit inside the overlap is the next piece's
    for (unsigned long long from = 0ULL; from < size; from += CLI_COMPILER_PIECE)
    {
        unsigned long long got = 0ULL;
        if (!EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = args->path, .from = from,
                        .bytes_bytes = piece, .count_u64 = CLI_COMPILER_PIECE + CLI_COMPILER_OVERLAP, .got = &got))
        {
            break;
        }
        const int last = (from + got) >= size;
        CliCompilerSift sift = {args->database, piece, got, last ? got : CLI_COMPILER_PIECE};
        for (unsigned int key = 0u; key < (sizeof(s_keys) / sizeof(s_keys[0])); key += 1u)
        {
            (void)EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = piece, .length = got,
                             .needle = (const unsigned char *)s_keys[key],
                             .needle_length = cellul.len(&(CatenaFinitaCfg){.src = s_keys[key], .cap = 16u}), .fold = 0,
                             .found = cli_compiler_found, .context = &sift);
        }
    }
    g_cli_guard->persistent_buf_release(piece, g_cli_compile_owner);
}

// a project's entries stored where it has none yet or these were configured later
EMBED_INLINE void cli_project_append(const CompileDbGatherCtx *args)
{
    unsigned long long time = args->time;
    unsigned int entries = args->entries;
    unsigned int at = 0u;
    while ((at < g_cli_gather.projects) &&
           (!cellul.eq(&(CatenaFinitaCfg){
               .src = g_cli_gather.project[at].project, .other = args->project, .cap = CLI_POOL_BYTES})))
    {
        at += 1u;
    }
    if ((at == g_cli_gather.projects) && (g_cli_gather.projects < CLI_GATHER_PROJECTS))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = g_cli_gather.project[at].project,
                          .src = args->project,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->project, .cap = CLI_POOL_BYTES}) + 1u});
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
    char *const out_cell = g_cli_guard->temporary_buf_alloc(out_capacity, g_cli_build_owner);
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
                                                      .size_pointer = &size, .owner = g_cli_build_owner);
        const int same = (there != NULL) && (size == out.at) &&
                         (memor.cmp(&(MemoriaCfg){.src = there, .other = out.out, .bytes = size}) == 0);
        g_cli_guard->persistent_buf_release((void *)(embed_word)there, (g_cli_build_owner));
        char folder[CLI_PATH_MAX_LEN];
        if (!same && EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = path, .out = folder))
        {
            EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
            EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = out.out,
                       .size = out.at);
        }
    }
    g_cli_guard->persistent_buf_release(out.out, g_cli_build_owner);
}

// the gathering itself, its cells taken: the database's compilers and folder
EMBED_INLINE void gather_reserve(const CompileDbGatherCtx *args);

void cli_compile_gather(const CliCompileDbRootCtx *args)
{
    // the tree's own database is sifted for its compilers, no tree of it built
    if (args->inputs_read->own)
    {
        EMBED_CALL(cli_of_file, CompileDbGatherCtx, .path = args->inputs_read->file[0].path,
                   .database = args->database);
        return;
    }
    if (gather_cells_alloc())
    {
        EMBED_CALL(gather_reserve, CompileDbGatherCtx, .root = args->root, .inputs = args->inputs_read,
                   .database = args->database);
        g_cli_guard->persistent_buf_release(g_cli_gather.json.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_gather.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_gather.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        cli_grown_end(&g_cli_gather.flags_text);
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
    // each CMake project's newest configuration, by the project's own folder, and each Meson build's
    g_cli_gather.projects = 0u;
    for (unsigned int index = 0u; index < args->inputs->count; index += 1u)
    {
        const CliStamped *const file = &args->inputs->file[index];
        const char *name = file->path;
        const unsigned long long name_end =
            EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = file->path,
                       .at = cellul.len(&(CatenaFinitaCfg){.src = file->path, .cap = CLI_POOL_BYTES}), .set = "/\\");
        name = (name_end != 0ULL) ? (file->path + name_end) : name;
        char project[CLI_PATH_MAX_LEN] = "";
        unsigned int entries = CLI_JSON_NONE;
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "compile_commands.json", .cap = CLI_POOL_BYTES}))
        {
            char folder[CLI_PATH_MAX_LEN];
            entries = EMBED_CALL(cli_compile_db_path_internal.compile_json_read, CliCompileDbPathCtx, .json = json,
                                 .path = file->path);
            entries = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                 .node = entries, .type = CFG_JSON_ARRAY)
                          ? entries
                          : EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json,
                                       .type = CFG_JSON_ARRAY);
            if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = file->path,
                            .out = folder) ||
                !EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = folder, .out = project))
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
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json, .type = CFG_JSON_ARRAY);
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
                   EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = path, .out = folder)
                       ? folder
                       : NULL,
               .entries = all, .database = args->database);
}
