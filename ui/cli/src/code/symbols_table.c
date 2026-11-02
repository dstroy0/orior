// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_table.c: the branches the tree stores, in a table at its most, each found by its path through a table of
// buckets twice its capacity, each knowing its closest parent
#include "../command_line/cells.h"
#include "../text/text.h"
#include "symbols_tree.h"

// the files the table's capacity is taken with, its paths' table twice as many buckets

// an empty bucket of the table of paths, and one whose file was let go
#define CLI_BUCKET_EMPTY 0u

#define CLI_BUCKET_REMOVED 0xffffffffu

// every file a table can name, and the one taken while the cell it replaces still stands
_Static_assert(CLI_BRANCHES_MAX < CLI_CELLS_MOVABLE, "the movable cells cannot name every symbol file");

CliTreeBranch *g_cli_branches;
unsigned int g_cli_branch_count;
static unsigned int s_cell_max;
static unsigned int s_file_capacity;
static unsigned int *s_buckets;
static unsigned int s_bucket_capacity;
// the first place left, counted from 1, 0 for none, each naming the next through its parent; and the files standing
// and their paths' bytes
static unsigned int s_free;
static unsigned int s_files;
static unsigned long long s_file_bytes;

typedef struct
{
    const char *path;
    unsigned int file;
    unsigned long long bytes;
    unsigned int parent;
    int folder;
} SymbolsTableCtx;
EMBED_INLINE unsigned long long cli_hash(const SymbolsTableCtx *args)
{
    unsigned long long value = 0xcbf29ce484222325ull;
    for (const char *at = args->path; at[0] != '\0'; at += 1)
    {
        value = (value ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    return value;
}

char *cli_branch_copy_alloc(void)
{
    return g_cli_cells->temporary_buf_alloc(s_cell_max, &g_cli_code_cells);
}

void cli_branch_copy(const CliSymbolsTreeBranchCfg *args)
{
    (void)g_cli_cells->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->branch->cell, .dst = args->scratch, .bytes = args->branch->bytes});
}

void cli_branch_copy_free(const CliSymbolsTreeBranchCfg *args)
{
    g_cli_cells->persistent_buf_release(args->scratch, &g_cli_code_cells);
}

const char *cli_branch_copy_path(const CliSymbolsTreeBranchScratchCfg *args)
{
    return args->scratch_read + args->branch->path;
}

const CliDeclaration *cli_branch_copy_declarations(const CliSymbolsTreeBranchScratchCfg *args)
{
    return (const CliDeclaration *)(const void *)args->scratch_read;
}

void cli_branch_path_read(const CliSymbolsTreeBranchPathCfg *args)
{
    const size_t measured =
        g_cli_cells->movable_buf_measure(&(CarcerMovableCfg){.cell = args->branch->cell, .offset = args->branch->path});
    const size_t bytes = (measured < CLI_PATH_MAX_LEN) ? measured : (CLI_PATH_MAX_LEN - 1u);

    (void)g_cli_cells->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->branch->cell, .offset = args->branch->path, .dst = args->path_out, .bytes = bytes});
    args->path_out[bytes] = '\0';
}

void cli_branch_cell_count(const CliSymbolsTreeCfg *args)
{
    s_cell_max = (args->bytes > s_cell_max) ? args->bytes : s_cell_max;
}

unsigned int cli_branch_get(const CliSymbolsTreePathCfg *args)
{
    if (s_bucket_capacity == 0u)
    {
        return CLI_BRANCHES_MAX;
    }
    unsigned long long bucket = EMBED_CALL(cli_hash, SymbolsTableCtx, .path = args->path) % s_bucket_capacity;
    // the path with its terminator, so that a stored path it only starts differs from it there
    const size_t bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u;
    for (unsigned int tried = 0u; (tried < s_bucket_capacity) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
    {
        const unsigned int place = s_buckets[bucket];
        if ((place != CLI_BUCKET_REMOVED) &&
            (g_cli_cells->movable_buf_compare(&(CarcerMovableCfg){.cell = g_cli_branches[place - 1u].cell,
                                                                  .offset = g_cli_branches[place - 1u].path,
                                                                  .src = args->path,
                                                                  .bytes = bytes}) == 0))
        {
            return place - 1u;
        }
        bucket = (bucket + 1u) % s_bucket_capacity;
    }
    return CLI_BRANCHES_MAX;
}

// a branch's place put in the paths' table
EMBED_INLINE void cli_bucket_append(const SymbolsTableCtx *args)
{
    unsigned long long bucket = cli_hash(args) % s_bucket_capacity;
    while ((s_buckets[bucket] != CLI_BUCKET_EMPTY) && (s_buckets[bucket] != CLI_BUCKET_REMOVED))
    {
        bucket = (bucket + 1u) % s_bucket_capacity;
    }
    s_buckets[bucket] = args->file + 1u;
}

// the table of branches and the paths' table taken at their most, each once: 1, or 0 where a cell is refused
static int cli_table_init(void)
{
    if (s_file_capacity != 0u)
    {
        return 1;
    }
    CliTreeBranch *const files = g_cli_cells->persistent_buf_alloc(
        (unsigned long long)CLI_BRANCHES_MAX * sizeof(CliTreeBranch), &g_cli_code_cells);
    if (files == NULL)
    {
        return 0;
    }
    unsigned int *const buckets =
        g_cli_cells->persistent_buf_alloc(2ull * CLI_BRANCHES_MAX * sizeof(unsigned int), &g_cli_code_cells);
    if (buckets == NULL)
    {
        g_cli_cells->persistent_buf_release(files, &g_cli_code_cells);
        return 0;
    }
    cli_cells_movable_ready();
    g_cli_branches = files;
    s_file_capacity = CLI_BRANCHES_MAX;
    s_buckets = buckets;
    s_bucket_capacity = 2u * CLI_BRANCHES_MAX;
    memor.set(&(MemoriaCfg){.dst = s_buckets, .val = 0, .bytes = (size_t)s_bucket_capacity * sizeof(unsigned int)});
    return 1;
}

// the branch of `path`, `bytes` with its end, joined under `parent`: its place, or CLI_BRANCHES_MAX where the table or
// the cells cannot meet it
EMBED_INLINE unsigned int cli_branch_init(const SymbolsTableCtx *args)
{
    unsigned long long bytes = args->bytes;
    if (!cli_table_init() || ((s_free == 0u) && (g_cli_branch_count >= s_file_capacity)))
    {
        return CLI_BRANCHES_MAX;
    }
    const CarcerMovableSlot *const cell = g_cli_cells->movable_buf_alloc(bytes, &g_cli_code_cells);
    if (cell == NULL)
    {
        return CLI_BRANCHES_MAX;
    }
    (void)g_cli_cells->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = args->path, .bytes = bytes});
    const unsigned int place = (s_free != 0u) ? (s_free - 1u) : g_cli_branch_count;
    s_free = (s_free != 0u) ? g_cli_branches[place].parent : s_free;
    g_cli_branch_count += (place == g_cli_branch_count) ? 1u : 0u;
    // a path is below CLI_PATH_MAX_LEN, which an unsigned int counts
    g_cli_branches[place] =
        (CliTreeBranch){cell, (unsigned int)bytes, 0u, 1, args->folder, 0, 0ull, 0u, 1, args->parent, 0u};
    EMBED_CALL(cli_symbols_tree.branch_cell_count, CliSymbolsTreeCfg, .bytes = (unsigned int)bytes);
    EMBED_CALL(cli_bucket_append, SymbolsTableCtx, .path = args->path, .file = place);
    g_cli_branches[(args->parent < CLI_BRANCHES_MAX) ? args->parent : place].below +=
        (args->parent < CLI_BRANCHES_MAX) ? 1u : 0u;
    s_files += (!args->folder && (args->parent != CLI_BRANCH_MOUNTED)) ? 1u : 0u;
    s_file_bytes += (!args->folder && (args->parent != CLI_BRANCH_MOUNTED)) ? bytes : 0ull;
    return place;
}

unsigned int cli_branch_exists(const CliSymbolsTreePathCfg *args)
{
    const unsigned int found = cli_symbols_tree_path.branch_get(args);
    if (found != CLI_BRANCHES_MAX)
    {
        return found;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    if (length >= CLI_PATH_MAX_LEN)
    {
        return CLI_BRANCHES_MAX;
    }
    unsigned int parent = (args->path[0] == '@') ? CLI_BRANCH_MOUNTED : CLI_BRANCH_TOP;
    char above[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = above, .src = args->path, .bytes = (size_t)length + 1u});
    for (unsigned long long slash = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                               .text = args->path, .at = 0ull, .size = length, .set = "/");
         (parent != CLI_BRANCH_MOUNTED) && (slash < length);
         slash = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->path,
                            .at = slash + 1ull, .size = length, .set = "/"))
    {
        above[slash] = '\0';
        const unsigned int folder_at =
            EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = above);
        parent = (folder_at != CLI_BRANCHES_MAX) ? folder_at
                                                 : EMBED_CALL(cli_branch_init, SymbolsTableCtx, .path = above,
                                                              .bytes = slash + 1ull, .parent = parent, .folder = 1);
        above[slash] = '/';
        if (parent == CLI_BRANCHES_MAX)
        {
            return CLI_BRANCHES_MAX;
        }
    }
    const unsigned int place = EMBED_CALL(cli_branch_init, SymbolsTableCtx, .path = args->path, .bytes = length + 1ull,
                                          .parent = parent, .folder = args->folder);
    if ((place == CLI_BRANCHES_MAX) && (parent < CLI_BRANCHES_MAX) && (g_cli_branches[parent].below == 0u))
    {
        EMBED_CALL(cli_symbols_tree_file.branch_leave, CliSymbolsTreeFileCfg, .file = parent);
    }
    return place;
}

void cli_branch_leave(const CliSymbolsTreeFileCfg *args)
{
    for (unsigned int at = args->file; at < CLI_BRANCHES_MAX;)
    {
        CliTreeBranch *const branch = &g_cli_branches[at];
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg, .branch = branch,
                   .path_out = path);
        unsigned long long bucket = EMBED_CALL(cli_hash, SymbolsTableCtx, .path = path) % s_bucket_capacity;
        for (unsigned int tried = 0u; (tried < s_bucket_capacity) && (s_buckets[bucket] != CLI_BUCKET_EMPTY);
             tried += 1u)
        {
            if (s_buckets[bucket] == (at + 1u))
            {
                s_buckets[bucket] = CLI_BUCKET_REMOVED;
                break;
            }
            bucket = (bucket + 1u) % s_bucket_capacity;
        }
        (void)g_cli_cells->movable_buf_release(branch->cell, &g_cli_code_cells);
        const unsigned int parent = branch->parent;
        if (!branch->folder && (parent != CLI_BRANCH_MOUNTED))
        {
            s_files -= 1u;
            s_file_bytes -= cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull;
        }
        branch->cell = NULL;
        branch->used = 0;
        branch->parent = s_free;
        s_free = at + 1u;
        g_cli_branches[(parent < CLI_BRANCHES_MAX) ? parent : at].below -= (parent < CLI_BRANCHES_MAX) ? 1u : 0u;
        at = ((parent < CLI_BRANCHES_MAX) && (g_cli_branches[parent].below == 0u)) ? parent : CLI_BRANCHES_MAX;
    }
}

void cli_branch_files_count(const CliSymbolsTreeCountCfg *args)
{
    unsigned int *count = args->count;
    unsigned long long *bytes = args->bytes_u64s;
    *count = s_files;
    *bytes = s_file_bytes;
}

void cli_tree_clear(void)
{
    for (unsigned int at = 0u; at < g_cli_branch_count; at += 1u)
    {
        (void)g_cli_cells->movable_buf_release(g_cli_branches[at].cell, &g_cli_code_cells);
        g_cli_branches[at].cell = NULL;
        g_cli_branches[at].used = 0;
    }
    g_cli_branch_count = 0u;
    g_cli_cells->persistent_buf_release(g_cli_branches, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(s_buckets, &g_cli_code_cells);
    g_cli_branches = NULL;
    s_buckets = NULL;
    s_file_capacity = 0u;
    s_bucket_capacity = 0u;
    s_free = 0u;
    s_files = 0u;
    s_file_bytes = 0ull;
}
