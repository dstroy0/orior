// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_table.c: the branches the tree stores, in a table at its most, each found by its path through a table of
// buckets twice its room, each knowing its closest parent
#include "../command_line/cells.h"
#include "../text/text.h"
#include "symbols_tree.h"

// the files the table's room is taken with, its paths' table twice as many buckets

// an empty bucket of the table of paths, and one whose file was let go
#define CLI_BUCKET_EMPTY 0u

#define CLI_BUCKET_GONE 0xffffffffu

// every file a table can name, and the one taken while the cell it replaces still stands
_Static_assert(CLI_BRANCHES_MOST < CLI_CELLS_MOVABLE, "the movable cells cannot name every symbol file");

CliTreeBranch *g_cli_branches;
unsigned int g_cli_branch_count;
static unsigned int s_cell_most;
static unsigned int s_file_room;
static unsigned int *s_buckets;
static unsigned int s_bucket_room;
// the first place left, counted from 1, 0 for none, each naming the next through its parent; and the files standing
// and their paths' bytes
static unsigned int s_free;
static unsigned int s_files;
static unsigned long long s_file_bytes;

static unsigned long long cli_hash(const char *path)
{
    unsigned long long value = 0xcbf29ce484222325ull;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        value = (value ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    return value;
}

char *cli_branch_copy_taken(void)
{
    return g_cli_cells->temporary_buf_alloc(s_cell_most, &g_cli_code_cells);
}

void cli_branch_copied(const CliTreeBranch *branch, char *scratch)
{
    (void)g_cli_cells->movable_buf_read(&(CarcerMovableCfg){.cell = branch->cell, .dst = scratch, .bytes = branch->bytes});
}

void cli_branch_copy_given(char *scratch)
{
    g_cli_cells->persistent_buf_release(scratch, &g_cli_code_cells);
}

const char *cli_branch_copy_path(const CliTreeBranch *branch, const char *scratch)
{
    return scratch + branch->path;
}

const CliDeclaration *cli_branch_copy_declarations(const char *scratch)
{
    return (const CliDeclaration *)(const void *)scratch;
}

void cli_branch_path_read(const CliTreeBranch *branch, char *path)
{
    const size_t measured = g_cli_cells->movable_buf_measure(&(CarcerMovableCfg){.cell = branch->cell, .offset = branch->path});
    const size_t bytes = (measured < CLI_PATH_MAX_LEN) ? measured : (CLI_PATH_MAX_LEN - 1u);

    (void)g_cli_cells->movable_buf_read(
        &(CarcerMovableCfg){.cell = branch->cell, .offset = branch->path, .dst = path, .bytes = bytes});
    path[bytes] = '\0';
}

void cli_branch_cell_counted(unsigned int bytes)
{
    s_cell_most = (bytes > s_cell_most) ? bytes : s_cell_most;
}

unsigned int cli_branch_of(const char *path)
{
    if (s_bucket_room == 0u)
    {
        return CLI_BRANCHES_MOST;
    }
    unsigned long long bucket = cli_hash(path) % s_bucket_room;
    // the path with its terminator, so that a stored path it only starts differs from it there
    const size_t bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
    for (unsigned int tried = 0u; (tried < s_bucket_room) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
    {
        const unsigned int place = s_buckets[bucket];
        if ((place != CLI_BUCKET_GONE) &&
            (g_cli_cells->movable_buf_compare(&(CarcerMovableCfg){.cell = g_cli_branches[place - 1u].cell,
                                                                  .offset = g_cli_branches[place - 1u].path,
                                                                  .src = path,
                                                                  .bytes = bytes}) == 0))
        {
            return place - 1u;
        }
        bucket = (bucket + 1u) % s_bucket_room;
    }
    return CLI_BRANCHES_MOST;
}

// a branch's place put in the paths' table
static void cli_bucket_put(const char *path, unsigned int file)
{
    unsigned long long bucket = cli_hash(path) % s_bucket_room;
    while ((s_buckets[bucket] != CLI_BUCKET_EMPTY) && (s_buckets[bucket] != CLI_BUCKET_GONE))
    {
        bucket = (bucket + 1u) % s_bucket_room;
    }
    s_buckets[bucket] = file + 1u;
}

// the table of branches and the paths' table taken at their most, each once: 1, or 0 where a cell is refused
static int cli_table_made(void)
{
    if (s_file_room != 0u)
    {
        return 1;
    }
    CliTreeBranch *const files = g_cli_cells->persistent_buf_alloc(
        (unsigned long long)CLI_BRANCHES_MOST * sizeof(CliTreeBranch), &g_cli_code_cells);
    if (files == NULL)
    {
        return 0;
    }
    unsigned int *const buckets =
        g_cli_cells->persistent_buf_alloc(2ull * CLI_BRANCHES_MOST * sizeof(unsigned int), &g_cli_code_cells);
    if (buckets == NULL)
    {
        g_cli_cells->persistent_buf_release(files, &g_cli_code_cells);
        return 0;
    }
    cli_cells_movable_ready();
    g_cli_branches = files;
    s_file_room = CLI_BRANCHES_MOST;
    s_buckets = buckets;
    s_bucket_room = 2u * CLI_BRANCHES_MOST;
    memor.set(&(MemoriaCfg){.dst = s_buckets, .val = 0, .bytes = (size_t)s_bucket_room * sizeof(unsigned int)});
    return 1;
}

// the branch of `path`, `bytes` with its end, joined under `parent`: its place, or CLI_BRANCHES_MOST where the table or
// the cells cannot meet it
static unsigned int cli_branch_made(const char *path, unsigned long long bytes, unsigned int parent, int folder)
{
    if (!cli_table_made() || ((s_free == 0u) && (g_cli_branch_count >= s_file_room)))
    {
        return CLI_BRANCHES_MOST;
    }
    const CarcerMovableSlot *const cell = g_cli_cells->movable_buf_alloc(bytes, &g_cli_code_cells);
    if (cell == NULL)
    {
        return CLI_BRANCHES_MOST;
    }
    (void)g_cli_cells->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = path, .bytes = bytes});
    const unsigned int place = (s_free != 0u) ? (s_free - 1u) : g_cli_branch_count;
    s_free = (s_free != 0u) ? g_cli_branches[place].parent : s_free;
    g_cli_branch_count += (place == g_cli_branch_count) ? 1u : 0u;
    // a path is below CLI_PATH_MAX_LEN, which an unsigned int counts
    g_cli_branches[place] = (CliTreeBranch){cell, (unsigned int)bytes, 0u, 1, folder, 0, 0ull, 0u, 1, parent, 0u};
    cli_branch_cell_counted((unsigned int)bytes);
    cli_bucket_put(path, place);
    g_cli_branches[(parent < CLI_BRANCHES_MOST) ? parent : place].below += (parent < CLI_BRANCHES_MOST) ? 1u : 0u;
    s_files += (!folder && (parent != CLI_BRANCH_MOUNTED)) ? 1u : 0u;
    s_file_bytes += (!folder && (parent != CLI_BRANCH_MOUNTED)) ? bytes : 0ull;
    return place;
}

unsigned int cli_branch_stands(const char *path, int folder)
{
    const unsigned int found = cli_branch_of(path);
    if (found != CLI_BRANCHES_MOST)
    {
        return found;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    if (length >= CLI_PATH_MAX_LEN)
    {
        return CLI_BRANCHES_MOST;
    }
    unsigned int parent = (path[0] == '@') ? CLI_BRANCH_MOUNTED : CLI_BRANCH_TOP;
    char above[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = above, .src = path, .bytes = (size_t)length + 1u});
    for (unsigned long long slash = cli_text_to_any(path, 0ull, length, "/");
         (parent != CLI_BRANCH_MOUNTED) && (slash < length); slash = cli_text_to_any(path, slash + 1ull, length, "/"))
    {
        above[slash] = '\0';
        const unsigned int folder_at = cli_branch_of(above);
        parent = (folder_at != CLI_BRANCHES_MOST) ? folder_at : cli_branch_made(above, slash + 1ull, parent, 1);
        above[slash] = '/';
        if (parent == CLI_BRANCHES_MOST)
        {
            return CLI_BRANCHES_MOST;
        }
    }
    const unsigned int place = cli_branch_made(path, length + 1ull, parent, folder);
    if ((place == CLI_BRANCHES_MOST) && (parent < CLI_BRANCHES_MOST) && (g_cli_branches[parent].below == 0u))
    {
        cli_branch_left(parent);
    }
    return place;
}

void cli_branch_left(unsigned int file)
{
    for (unsigned int at = file; at < CLI_BRANCHES_MOST;)
    {
        CliTreeBranch *const branch = &g_cli_branches[at];
        char path[CLI_PATH_MAX_LEN];
        cli_branch_path_read(branch, path);
        unsigned long long bucket = cli_hash(path) % s_bucket_room;
        for (unsigned int tried = 0u; (tried < s_bucket_room) && (s_buckets[bucket] != CLI_BUCKET_EMPTY); tried += 1u)
        {
            if (s_buckets[bucket] == (at + 1u))
            {
                s_buckets[bucket] = CLI_BUCKET_GONE;
                break;
            }
            bucket = (bucket + 1u) % s_bucket_room;
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
        g_cli_branches[(parent < CLI_BRANCHES_MOST) ? parent : at].below -= (parent < CLI_BRANCHES_MOST) ? 1u : 0u;
        at = ((parent < CLI_BRANCHES_MOST) && (g_cli_branches[parent].below == 0u)) ? parent : CLI_BRANCHES_MOST;
    }
}

void cli_branch_files_counted(unsigned int *count, unsigned long long *bytes)
{
    *count = s_files;
    *bytes = s_file_bytes;
}

void cli_tree_cleared(void)
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
    s_file_room = 0u;
    s_bucket_room = 0u;
    s_free = 0u;
    s_files = 0u;
    s_file_bytes = 0ull;
}
