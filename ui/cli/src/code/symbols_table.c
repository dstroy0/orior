// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_table.c: the branches the tree stores, their records in chunks of CLI_BRANCH_CHUNK places, each chunk a
// movable cell taken as a place in it is first used and given back as its last place is left, the chunks' tokens a
// static table; each branch found by its path through a static order of the branches sorted by the hash of the
// path, with a run of the newest beside it; each knowing its closest parent
#include "../command_line/pool.h"
#include "../text/text.h"
#include "symbols_tree.h"

// every file a table can name, and the one taken while the cell it replaces still is
_Static_assert(CLI_BRANCHES_MAX < PLATFORM_POOL_MOVABLE, "the movable cells cannot name every symbol file");

// the places of one chunk as a two's power, the chunks, the words of a chunk's bitmap of places in use, and the
// records of the run of the newest branches
#define CLI_BRANCH_CHUNK_SHIFT 10u
#define CLI_BRANCH_CHUNK (1u << CLI_BRANCH_CHUNK_SHIFT)
#define CLI_BRANCH_CHUNKS (CLI_BRANCHES_MAX / CLI_BRANCH_CHUNK)
#define CLI_BRANCH_CHUNK_WORDS (CLI_BRANCH_CHUNK / 64u)
#define CLI_BRANCH_RUN 256u
_Static_assert((CLI_BRANCH_CHUNKS * CLI_BRANCH_CHUNK) == CLI_BRANCHES_MAX, "the chunks cover every place");

unsigned int g_cli_branch_count;
static unsigned int s_cell_max;
// each chunk's cell, NULL where no place of it is used; its places in use, and a bit for each in use
static const CarcerMovableEntry *s_chunk[CLI_BRANCH_CHUNKS];
static unsigned int s_chunk_used[CLI_BRANCH_CHUNKS];
static unsigned long long s_chunk_bits[CLI_BRANCH_CHUNKS][CLI_BRANCH_CHUNK_WORDS];
// a record of a branch: the hash of its path and its place, CLI_BRANCHES_MAX in the order where the branch left
typedef struct
{
    unsigned long long hash;
    unsigned int place;
} CliBranchRecord;

// The order: a record for each branch merged, sorted by the hash, `s_ordered` of them, `s_emptied` of them marked
// empty. The run: the records of the branches put since the last merge, in the order they were put, `s_run_count` of
// them. The room: what the sort of a full run writes as it merges
static CliBranchRecord s_order[CLI_BRANCHES_MAX];
static unsigned int s_ordered;
static unsigned int s_emptied;
static CliBranchRecord s_run[CLI_BRANCH_RUN];
static unsigned int s_run_count;
static CliBranchRecord s_run_room[CLI_BRANCH_RUN];
// the files located and their paths' bytes
static unsigned int s_files;
static unsigned long long s_file_bytes;

typedef struct
{
    const char *path;
    unsigned int file;
    unsigned long long bytes;
    unsigned int parent;
    int folder;
    const CarcerMovableEntry *entry;
} SymbolsTableCtx;
EMBED_INLINE unsigned long long cli_hash(const SymbolsTableCtx *args)
{
    unsigned long long value = 0xcbf29ce484222325ULL;
    for (const char *at = args->path; at[0] != '\0'; at += 1)
    {
        value = (value ^ (unsigned char)at[0]) * 0x100000001b3ULL;
    }
    return value;
}

// the order of two records, by the hash of the path: below 0, 0 or above 0
static int cli_record_order(const void *left, const void *right)
{
    const CliBranchRecord *const one = (const CliBranchRecord *)left;
    const CliBranchRecord *const other = (const CliBranchRecord *)right;
    return (one->hash < other->hash) ? -1 : ((one->hash > other->hash) ? 1 : 0);
}

// the run sorted once and merged into the order from its end, each record marked empty taken out as it goes
EMBED_INLINE void cli_run_merged(const SymbolsTableCtx *args)
{
    (void)args;
    (void)memor.sort(&(MemoriaCfg){.dst = s_run,
                                   .scratch = s_run_room,
                                   .count = s_run_count,
                                   .bytes = sizeof(CliBranchRecord),
                                   .order = cli_record_order});
    // the records kept from the order, moved down over the empty ones, then the merge from the end of both
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < s_ordered; at += 1u)
    {
        if (s_order[at].place != CLI_BRANCHES_MAX)
        {
            s_order[kept] = s_order[at];
            kept += 1u;
        }
    }
    unsigned int from_order = kept;
    unsigned int from_run = s_run_count;
    unsigned int to = kept + s_run_count;
    while (from_run != 0u)
    {
        const int order_first = (from_order != 0u) && (s_order[from_order - 1u].hash > s_run[from_run - 1u].hash);
        to -= 1u;
        if (order_first)
        {
            from_order -= 1u;
            s_order[to] = s_order[from_order];
        }
        else
        {
            from_run -= 1u;
            s_order[to] = s_run[from_run];
        }
    }
    s_ordered = kept + s_run_count;
    s_emptied = 0u;
    s_run_count = 0u;
}

// the first record of the order at or past `hash`, by a binary search: its place in the order, `s_ordered` where none
EMBED_INLINE unsigned int cli_order_from(unsigned long long hash)
{
    unsigned int low = 0u;
    unsigned int high = s_ordered;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (s_order[middle].hash < hash)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// 1 where the branch at `place` stores the path `path`, `bytes` of it with its end
EMBED_INLINE int cli_branch_is(unsigned int place, const char *path, embed_word bytes)
{
    CliTreeBranch branch;
    EMBED_CALL(cli_branch_read, CliSymbolsTreeAtCfg, .file = place, .branch_out = &branch);
    return g_platform_guard->movable_buf_compare(
               &(CarcerMovableCfg){.cell = branch.cell, .offset = branch.path, .src = path, .bytes = bytes}) == 0;
}

void cli_branch_read(const CliSymbolsTreeAtCfg *args)
{
    const CarcerMovableEntry *const chunk = s_chunk[args->file >> CLI_BRANCH_CHUNK_SHIFT];
    if (chunk == NULL)
    {
        // a place of no chunk is unused
        memor.set(&(MemoriaCfg){.dst = args->branch_out, .val = 0, .bytes = sizeof(*args->branch_out)});
        return;
    }
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = chunk,
        .offset = (unsigned long long)(args->file & (CLI_BRANCH_CHUNK - 1u)) * sizeof(CliTreeBranch),
        .dst = args->branch_out,
        .bytes = sizeof(CliTreeBranch)});
}

void cli_branch_write(const CliSymbolsTreeAtCfg *args)
{
    const CarcerMovableEntry *const chunk = s_chunk[args->file >> CLI_BRANCH_CHUNK_SHIFT];
    if (chunk == NULL)
    {
        return;
    }
    (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = chunk,
        .offset = (unsigned long long)(args->file & (CLI_BRANCH_CHUNK - 1u)) * sizeof(CliTreeBranch),
        .src = args->branch,
        .bytes = sizeof(CliTreeBranch)});
}

char *cli_branch_copy_alloc(void)
{
    return g_platform_guard->temporary_buf_alloc(s_cell_max, g_cli_code_owner);
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
    const embed_word measured =
        g_platform_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = args->branch->cell, .offset = args->branch->path});
    const embed_word bytes = (measured < CLI_PATH_MAX_LEN) ? measured : (CLI_PATH_MAX_LEN - 1u);

    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->branch->cell, .offset = args->branch->path, .dst = args->path_out, .bytes = bytes});
    args->path_out[bytes] = '\0';
}

void cli_branch_cell_count(const CliSymbolsTreeCfg *args)
{
    s_cell_max = (args->bytes > s_cell_max) ? args->bytes : s_cell_max;
}

unsigned int cli_branch_get(const CliSymbolsTreePathCfg *args)
{
    // the path with its terminator, so that a stored path it only starts differs from it there
    const embed_word bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u;
    const unsigned long long hash = cli_hash(&(SymbolsTableCtx){.path = args->path});
    for (unsigned int at = 0u; at < s_run_count; at += 1u)
    {
        if ((s_run[at].hash == hash) && cli_branch_is(s_run[at].place, args->path, bytes))
        {
            return s_run[at].place;
        }
    }
    for (unsigned int at = cli_order_from(hash); (at < s_ordered) && (s_order[at].hash == hash); at += 1u)
    {
        if ((s_order[at].place != CLI_BRANCHES_MAX) && cli_branch_is(s_order[at].place, args->path, bytes))
        {
            return s_order[at].place;
        }
    }
    return CLI_BRANCHES_MAX;
}

// `bytes` of zeros written from the start of the movable cell `cell`, a block at a time
EMBED_INLINE void cli_cell_zeroed(const SymbolsTableCtx *args)
{
    static const unsigned char s_zeros[4096];
    for (unsigned long long done = 0ULL; done < args->bytes; done += sizeof(s_zeros))
    {
        const unsigned long long step =
            ((args->bytes - done) < sizeof(s_zeros)) ? (args->bytes - done) : sizeof(s_zeros);
        (void)g_platform_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = args->entry, .offset = done, .src = s_zeros, .bytes = step});
    }
}

// a free place taken: the first unused place of the first chunk with room, the chunk's cell taken and zeroed where
// none is, the place's bit set: the place, or CLI_BRANCHES_MAX where every place is used or the cells cannot meet
// the chunk
EMBED_INLINE unsigned int cli_place_taken(const SymbolsTableCtx *args)
{
    (void)args;
    unsigned int chunk = 0u;
    while ((chunk < CLI_BRANCH_CHUNKS) && (s_chunk_used[chunk] == CLI_BRANCH_CHUNK))
    {
        chunk += 1u;
    }
    if (chunk == CLI_BRANCH_CHUNKS)
    {
        return CLI_BRANCHES_MAX;
    }
    if (s_chunk[chunk] == NULL)
    {
        s_chunk[chunk] = g_platform_guard->movable_buf_alloc((unsigned long long)CLI_BRANCH_CHUNK * sizeof(CliTreeBranch),
                                                        g_cli_code_owner);
        if (s_chunk[chunk] == NULL)
        {
            return CLI_BRANCHES_MAX;
        }
        EMBED_CALL(cli_cell_zeroed, SymbolsTableCtx, .entry = s_chunk[chunk],
                   .bytes = (unsigned long long)CLI_BRANCH_CHUNK * sizeof(CliTreeBranch));
    }
    unsigned int bits_at = 0u;
    while (s_chunk_bits[chunk][bits_at] == ~0ULL)
    {
        bits_at += 1u;
    }
    unsigned int bit = 0u;
    while (((s_chunk_bits[chunk][bits_at] >> bit) & 1ULL) != 0ULL)
    {
        bit += 1u;
    }
    s_chunk_bits[chunk][bits_at] |= 1ULL << bit;
    s_chunk_used[chunk] += 1u;
    const unsigned int place = (chunk << CLI_BRANCH_CHUNK_SHIFT) + (bits_at * 64u) + bit;
    g_cli_branch_count = (place >= g_cli_branch_count) ? place + 1u : g_cli_branch_count;
    return place;
}

// the place `file` left: its bit cleared, its chunk's cell given back where no place of it is used
EMBED_INLINE void cli_place_left(const SymbolsTableCtx *args)
{
    const unsigned int chunk = args->file >> CLI_BRANCH_CHUNK_SHIFT;
    const unsigned int inside = args->file & (CLI_BRANCH_CHUNK - 1u);
    s_chunk_bits[chunk][inside / 64u] &= ~(1ULL << (inside % 64u));
    s_chunk_used[chunk] -= 1u;
    if (s_chunk_used[chunk] == 0u)
    {
        (void)g_platform_guard->movable_buf_release(s_chunk[chunk], g_cli_code_owner);
        s_chunk[chunk] = NULL;
    }
}

// the record of the branch `file`, of the path `path`, taken out: from the run, the run's last record put in its
// place, or from the order, marked empty there until the next merge
EMBED_INLINE void cli_bucket_unlinked(const SymbolsTableCtx *args)
{
    const unsigned long long hash = cli_hash(args);
    for (unsigned int at = 0u; at < s_run_count; at += 1u)
    {
        if (s_run[at].place == args->file)
        {
            s_run_count -= 1u;
            s_run[at] = s_run[s_run_count];
            return;
        }
    }
    for (unsigned int at = cli_order_from(hash); (at < s_ordered) && (s_order[at].hash == hash); at += 1u)
    {
        if (s_order[at].place == args->file)
        {
            s_order[at].place = CLI_BRANCHES_MAX;
            s_emptied += 1u;
            return;
        }
    }
}

// the branch of `path`, `bytes` with its end, joined under `parent`: its place, or CLI_BRANCHES_MAX where the table or
// the cells cannot meet it
EMBED_INLINE unsigned int cli_branch_init(const SymbolsTableCtx *args)
{
    unsigned long long bytes = args->bytes;
    const CarcerMovableEntry *const cell = g_platform_guard->movable_buf_alloc(bytes, g_cli_code_owner);
    if (cell == NULL)
    {
        return CLI_BRANCHES_MAX;
    }
    const unsigned int place = EMBED_CALL(cli_place_taken, SymbolsTableCtx, .file = 0u);
    if (place == CLI_BRANCHES_MAX)
    {
        (void)g_platform_guard->movable_buf_release(cell, g_cli_code_owner);
        return CLI_BRANCHES_MAX;
    }
    (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = args->path, .bytes = bytes});
    // a path is below CLI_PATH_MAX_LEN, which an unsigned int counts
    const CliTreeBranch branch = {.cell = cell,
                                  .bytes = (unsigned int)bytes,
                                  .used = 1,
                                  .folder = args->folder,
                                  .seen = 1,
                                  .parent = args->parent};
    EMBED_CALL(cli_branch_write, CliSymbolsTreeAtCfg, .file = place, .branch = &branch);
    // the branch's record into the run, the run merged into the order first where it is full
    if (s_run_count == CLI_BRANCH_RUN)
    {
        EMBED_CALL(cli_run_merged, SymbolsTableCtx, .file = 0u);
    }
    s_run[s_run_count] = (CliBranchRecord){cli_hash(args), place};
    s_run_count += 1u;
    EMBED_CALL(cli_symbols_tree.branch_cell_count, CliSymbolsTreeCfg, .bytes = (unsigned int)bytes);
    if (args->parent < CLI_BRANCHES_MAX)
    {
        CliTreeBranch above;
        EMBED_CALL(cli_branch_read, CliSymbolsTreeAtCfg, .file = args->parent, .branch_out = &above);
        above.below += 1u;
        EMBED_CALL(cli_branch_write, CliSymbolsTreeAtCfg, .file = args->parent, .branch = &above);
    }
    s_files += (!args->folder && (args->parent != CLI_BRANCH_MOUNTED)) ? 1u : 0u;
    s_file_bytes += (!args->folder && (args->parent != CLI_BRANCH_MOUNTED)) ? bytes : 0ULL;
    return place;
}

unsigned int cli_branch_exists(const CliSymbolsTreePathCfg *args)
{
    const unsigned int found = cli_symbols_tree_path.branch_get(args);
    if (found != CLI_BRANCHES_MAX)
    {
        return found;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    if (length >= CLI_PATH_MAX_LEN)
    {
        return CLI_BRANCHES_MAX;
    }
    unsigned int parent = (args->path[0] == '@') ? CLI_BRANCH_MOUNTED : CLI_BRANCH_TOP;
    char above[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = above, .src = args->path, .bytes = (embed_word)length + 1u});
    for (unsigned long long slash = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                               .text = args->path, .at = 0ULL, .size = length, .set = "/");
         (parent != CLI_BRANCH_MOUNTED) && (slash < length);
         slash = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->path,
                            .at = slash + 1ULL, .size = length, .set = "/"))
    {
        above[slash] = '\0';
        const unsigned int folder_at =
            EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = above);
        parent = (folder_at != CLI_BRANCHES_MAX) ? folder_at
                                                 : EMBED_CALL(cli_branch_init, SymbolsTableCtx, .path = above,
                                                              .bytes = slash + 1ULL, .parent = parent, .folder = 1);
        above[slash] = '/';
        if (parent == CLI_BRANCHES_MAX)
        {
            return CLI_BRANCHES_MAX;
        }
    }
    const unsigned int place = EMBED_CALL(cli_branch_init, SymbolsTableCtx, .path = args->path, .bytes = length + 1ULL,
                                          .parent = parent, .folder = args->folder);
    if ((place == CLI_BRANCHES_MAX) && (parent < CLI_BRANCHES_MAX))
    {
        CliTreeBranch folder;
        EMBED_CALL(cli_branch_read, CliSymbolsTreeAtCfg, .file = parent, .branch_out = &folder);
        if (folder.below == 0u)
        {
            EMBED_CALL(cli_symbols_tree_file.branch_leave, CliSymbolsTreeFileCfg, .file = parent);
        }
    }
    return place;
}

void cli_branch_leave(const CliSymbolsTreeFileCfg *args)
{
    for (unsigned int at = args->file; at < CLI_BRANCHES_MAX;)
    {
        CliTreeBranch branch;
        EMBED_CALL(cli_branch_read, CliSymbolsTreeAtCfg, .file = at, .branch_out = &branch);
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg, .branch = &branch,
                   .path_out = path);
        EMBED_CALL(cli_bucket_unlinked, SymbolsTableCtx, .path = path, .file = at);
        (void)g_platform_guard->movable_buf_release(branch.cell, g_cli_code_owner);
        const unsigned int parent = branch.parent;
        if (!branch.folder && (parent != CLI_BRANCH_MOUNTED))
        {
            s_files -= 1u;
            s_file_bytes -= cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1ULL;
        }
        const CliTreeBranch left = {0};
        EMBED_CALL(cli_branch_write, CliSymbolsTreeAtCfg, .file = at, .branch = &left);
        EMBED_CALL(cli_place_left, SymbolsTableCtx, .file = at);
        unsigned int next = CLI_BRANCHES_MAX;
        if (parent < CLI_BRANCHES_MAX)
        {
            CliTreeBranch above;
            EMBED_CALL(cli_branch_read, CliSymbolsTreeAtCfg, .file = parent, .branch_out = &above);
            above.below -= 1u;
            EMBED_CALL(cli_branch_write, CliSymbolsTreeAtCfg, .file = parent, .branch = &above);
            next = (above.below == 0u) ? parent : CLI_BRANCHES_MAX;
        }
        at = next;
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
        CliTreeBranch branch;
        EMBED_CALL(cli_branch_read, CliSymbolsTreeAtCfg, .file = at, .branch_out = &branch);
        (void)g_platform_guard->movable_buf_release(branch.cell, g_cli_code_owner);
    }
    g_cli_branch_count = 0u;
    for (unsigned int chunk = 0u; chunk < CLI_BRANCH_CHUNKS; chunk += 1u)
    {
        (void)g_platform_guard->movable_buf_release(s_chunk[chunk], g_cli_code_owner);
        s_chunk[chunk] = NULL;
    }
    memor.set(&(MemoriaCfg){.dst = s_chunk_used, .val = 0, .bytes = sizeof(s_chunk_used)});
    memor.set(&(MemoriaCfg){.dst = s_chunk_bits, .val = 0, .bytes = sizeof(s_chunk_bits)});
    s_ordered = 0u;
    s_emptied = 0u;
    s_run_count = 0u;
    s_files = 0u;
    s_file_bytes = 0ULL;
}
