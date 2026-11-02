// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols.c: how well a query names a name, and the tree: the filesystem is its trunk and its files are its branches.
// It is listed one time as it opens, each file a branch under its closest parent, and walked one time on a thread of
// its own, each branch's grammar applied as it is read and its declarations saved in a cell of its own, searched in
// symbols_find.c. Each change after that joins, leaves or walks the one branch it names. The include folders and files
// the compilation database names outside the tree are joined to it as it is walked, each mounted beside it, and their
// files are walked as the tree's own, by their mounts' names
#include "../builds/compile_db.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// the most kinds of declaration of one file whose texts are read once and shared by its declarations
#define CLI_SYMBOLS_KINDS 16u
// The storage a walk goes through: one file's declarations as read, before they are stored, their names and kinds and
// the declarations each in a cell at their most, how many and their bytes, and the place of each kind read among those
// texts; the one cell each file is read into, of the most a file read holds and the slack; the languages it reads its
// files in, a cell of their own; and the folders and files mounted beside the tree as it started, each its name and its
// whole path, how many
typedef struct
{
    char *read_text;
    unsigned int text_capacity;
    CliDeclaration *read_symbols;
    unsigned int symbol_capacity;
    unsigned int read_count;
    unsigned int read_used;
    unsigned int kind[CLI_SYMBOLS_KINDS];
    unsigned int kinds;
    unsigned char *file_bytes;
    CliSymbolsLanguages *languages;
    const char **mount_name;
    const char **mount_path;
    unsigned int mounts;
} CliTreeWalk;
// a file larger than this is not walked for its declarations
#define CLI_SYMBOLS_LARGEST (1u << 20u)

static CliTreeWalk *s_walk;

// The tree's state: its folder, whether it is listed, the generation a listing goes for, 0 for none, whether git listed
// it and when it was last listed; whether a walk goes, whether one ended and the mounts it went over; how far it has
// gone, whether it is requested to stop, which opening of the tree it is, and whether its thread runs
char g_cli_tree_root[CLI_PATH_MAX_LEN];
static int s_rooted;
static int s_list_result;
static unsigned long long s_listing;
static int s_git;
static unsigned long long s_listing_at;
int g_cli_tree_walking;
static int s_read;
static unsigned long long s_walked_mounts;
static unsigned long long s_done;
static unsigned long long s_total;
static int s_stop;
static unsigned long long s_generation;
static int s_running;
static CliWake s_wake = CLI_WAKE_INIT;

// the thread walking the tree
static CliThread s_thread;

// none: a kind not read before in the file
#define CLI_SYMBOLS_KIND_NEW 0xffffffffu

typedef struct
{
    unsigned long long size;
    unsigned long long written;
    const CliTreeWalk *walk_read;
    const char *path;
    CliTreeWalk *walk;
    const char *root;
    unsigned long long generation;
    int changed;
    unsigned int file;
} SymbolsCtx;
// a file's stamp, one word its time and its size both change, never 0, which a branch not stamped reads
EMBED_INLINE unsigned long long cli_branch_stamp(const SymbolsCtx *args)
{
    return (args->written ^ ((args->size << 44u) | (args->size >> 20u))) | 1ull;
}

// A declaration read: its name, and its kind's text where the file's declarations have not read it before, after the
// name with its NUL
static void cli_leaf_declare(const CliDeclaredCfg *args)
{
    CliTreeWalk *const read_state = (CliTreeWalk *)args->context;
    unsigned int kind_at = CLI_SYMBOLS_KIND_NEW;
    for (unsigned int at = 0u; (kind_at == CLI_SYMBOLS_KIND_NEW) && (at < read_state->kinds); at += 1u)
    {
        kind_at = cellul.eq(&(CatenaFinitaCfg){
                      .src = read_state->read_text + read_state->kind[at], .other = args->kind, .cap = CLI_CELLS_POOL})
                      ? read_state->kind[at]
                      : CLI_SYMBOLS_KIND_NEW;
    }
    const unsigned long long kind_length =
        (kind_at == CLI_SYMBOLS_KIND_NEW)
            ? cellul.len(&(CatenaFinitaCfg){.src = args->kind, .cap = CLI_CELLS_POOL}) + 1ull
            : 0ull;
    if ((read_state->read_count >= (CLI_SYMBOLS_LARGEST / 4u)) ||
        ((read_state->read_used + args->length + kind_length) > CLI_SYMBOLS_LARGEST))
    {
        return;
    }
    if (read_state->symbol_capacity == 0u)
    {
        read_state->read_symbols = g_cli_cells->temporary_buf_alloc(
            (unsigned long long)(CLI_SYMBOLS_LARGEST / 4u) * sizeof(CliDeclaration), &g_cli_code_cells);
        read_state->read_text = g_cli_cells->temporary_buf_alloc(CLI_SYMBOLS_LARGEST, &g_cli_code_cells);
        read_state->symbol_capacity = CLI_SYMBOLS_LARGEST / 4u;
        read_state->text_capacity = CLI_SYMBOLS_LARGEST;
    }
    if ((read_state->read_symbols == NULL) || (read_state->read_text == NULL))
    {
        return;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = read_state->read_text + read_state->read_used, .src = args->name, .bytes = args->length});
    if (kind_at == CLI_SYMBOLS_KIND_NEW)
    {
        // a name and a kind are below a file's largest size, which an unsigned int counts
        kind_at = read_state->read_used + (unsigned int)args->length;
        memor.cpy(
            &(MemoriaCfg){.dst = read_state->read_text + kind_at, .src = args->kind, .bytes = (size_t)kind_length});
        read_state->kind[read_state->kinds] = kind_at;
        read_state->kinds += (read_state->kinds < (CLI_SYMBOLS_KINDS - 1u)) ? 1u : 0u;
    }
    const CliDeclaration symbol = {read_state->read_used, (unsigned int)args->length, kind_at, args->line};
    read_state->read_symbols[read_state->read_count] = symbol;
    read_state->read_count += 1u;
    read_state->read_used += (unsigned int)(args->length + kind_length);
}

// the declarations read put as the branch at `path`, stamped `written`, in a cell of its own, which takes the place of
// the one it stood in: 1, or 0 where the table fits no more branches or the cells cannot meet it. Called under the lock
EMBED_INLINE int cli_branch_join(const SymbolsCtx *args)
{
    const char *path = args->path;
    unsigned long long written = args->written;
    const unsigned int file =
        EMBED_CALL(cli_symbols_tree_path.branch_exists, CliSymbolsTreePathCfg, .path = path, .folder = 0);
    if (file == CLI_BRANCHES_MAX)
    {
        return 0;
    }
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull;
    const unsigned long long symbols_size = args->walk_read->read_count * sizeof(CliDeclaration);
    const unsigned long long bytes = symbols_size + path_length + args->walk_read->read_used;
    const CarcerMovableSlot *const cell = g_cli_cells->movable_buf_alloc(bytes, &g_cli_code_cells);
    if (cell == NULL)
    {
        return 0;
    }
    CliTreeBranch *const branch = &g_cli_branches[file];
    (void)g_cli_cells->movable_buf_release(branch->cell, &g_cli_code_cells);
    branch->cell = cell;
    // a file's declarations, path and names are below what an unsigned int counts
    branch->bytes = (unsigned int)bytes;
    branch->path = (unsigned int)symbols_size;
    branch->walked = 1;
    branch->written = written;
    branch->count = args->walk_read->read_count;
    branch->seen = 1;
    EMBED_CALL(cli_symbols_tree.branch_cell_count, CliSymbolsTreeCfg, .bytes = branch->bytes);
    // the names and kinds read, after the path, each declaration's places among them moved with them
    const unsigned int texts = (unsigned int)(symbols_size + path_length);
    for (unsigned int at = 0u; at < args->walk_read->read_count; at += 1u)
    {
        args->walk_read->read_symbols[at].name += texts;
        args->walk_read->read_symbols[at].kind += texts;
    }
    (void)g_cli_cells->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .src = args->walk_read->read_symbols, .bytes = symbols_size});
    (void)g_cli_cells->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = symbols_size, .src = path, .bytes = path_length});
    (void)g_cli_cells->movable_buf_write(&(CarcerMovableCfg){
        .cell = cell, .offset = texts, .src = args->walk_read->read_text, .bytes = args->walk_read->read_used});
    return 1;
}

// the branch at `path` of the tree, a path from its top or under a mount's name, walked where its stamp moved since it
// was last walked or where `changed` says it did, its declarations stored; called under the lock, given up while the
// file is stamped and read: 1, or 0 where the walk was asked to stop or the tree it walks was let go
static int cli_branch_walk(const SymbolsCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    int changed = args->changed;
    if (s_stop || (s_generation != args->generation))
    {
        return 0;
    }
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
    unsigned long long size = 0ull;
    unsigned long long written = 0ull;
    const int stamped = EMBED_CALL(cli_path_size_internal.file_stamp, CliPathSizeCtx, .path = full,
                                   .size_pointer = &size, .changed = &written) &&
                        (size <= CLI_SYMBOLS_LARGEST);
    const unsigned long long stamp = EMBED_CALL(cli_branch_stamp, SymbolsCtx, .size = size, .written = written);
    if (s_generation != args->generation)
    {
        return 0;
    }
    const unsigned int file =
        stamped ? EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = path) : CLI_BRANCHES_MAX;
    if (file != CLI_BRANCHES_MAX)
    {
        g_cli_branches[file].seen = 1;
        if (!changed && g_cli_branches[file].walked && (g_cli_branches[file].written == stamp))
        {
            return 1;
        }
    }
    if (!stamped)
    {
        return 1;
    }
    // the file read into the walk's one cell, used from file to file, which fits the most a file read holds
    unsigned long long mapped_size = 0ull;
    const int mapped = (args->walk->file_bytes != NULL) &&
                       EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = full, .from = 0ull,
                                  .bytes_bytes = args->walk->file_bytes, .count_u64 = size, .got = &mapped_size);
    const unsigned long long read = mapped ? mapped_size : QRY_MEMBER_UNREAD;
    args->walk->read_count = 0u;
    args->walk->read_used = 0u;
    args->walk->kinds = 0u;
    if (read != QRY_MEMBER_UNREAD)
    {
        memor.set(&(MemoriaCfg){.dst = args->walk->file_bytes + read, .val = 0, .bytes = CLI_FILE_SLACK});
        // the file's bytes are read as its text
        EMBED_CALL(cli_symbols_path_read_languages_internal.declared_in, CliSymbolsPathReadLanguagesCtx,
                   .read_symbols_languages_pointer_other = args->walk->languages, .path = path,
                   .text = (const char *)args->walk->file_bytes, .size = read, .each = cli_leaf_declare,
                   .context = args->walk);
    }
    if (s_generation != args->generation)
    {
        return 0;
    }
    if (read != QRY_MEMBER_UNREAD)
    {
        EMBED_CALL(cli_branch_join, SymbolsCtx, .walk_read = args->walk, .path = path, .written = stamp);
    }
    return 1;
}

// the cells of a walk given back: the file's and the declarations', the languages' and its own
EMBED_INLINE void cli_tree_walk_free(const SymbolsCtx *args)
{
    g_cli_cells->persistent_buf_release(args->walk->read_text, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(args->walk->read_symbols, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(args->walk->file_bytes, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(args->walk->mount_name, &g_cli_code_cells);
    EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = args->walk->languages);
    g_cli_cells->persistent_buf_release(args->walk, &g_cli_code_cells);
}

// A mount's files being walked: the walk, the tree's top, the mount's name, which opening of the tree it is, and
// whether it was stopped
typedef struct
{
    CliTreeWalk *walk;
    const char *root;
    const char *name;
    unsigned long long generation;
    int stopped;
} CliMountWalk;

// One file of a mounted folder, its path from the mount's top, walked as the tree's own under the mount's name where
// its language is one read here
static void cli_mount_branch_walk(const CliEachPathLengthCfg *args)
{
    const char *path = args->path;
    CliMountWalk *const mount = (CliMountWalk *)args->context;
    char named[CLI_PATH_MAX_LEN];
    VerbaLinea line = {named, sizeof(named), 0ull};
    mmgr_line_ch(&line, '@');
    mmgr_line_put(&line, mount->name);
    mmgr_line_ch(&line, '/');
    mmgr_line_put_n(&line, path, args->length);
    if (mount->stopped || (mmgr_line_finish(&line) == 0ull) ||
        !EMBED_CALL(cli_symbols_read_languages_internal.reads_in, CliSymbolsReadLanguagesCtx,
                    .read_symbols_languages_pointer = mount->walk->languages, .path = named))
    {
        return;
    }
    s_total += 1ull;
    s_done += 1ull;
    mount->stopped = !EMBED_CALL(cli_branch_walk, SymbolsCtx, .walk = mount->walk, .root = mount->root, .path = named,
                                 .generation = mount->generation, .changed = 0);
}

// The walk: each file branch of the tree, then each file of each folder mounted beside it and each file mounted; past
// the last, the mounted branches it did not see let go where it was not stopped
static void cli_tree_walk(void *context)
{
    CliTreeWalk *const walk = (CliTreeWalk *)context;
    const unsigned long long generation = s_generation;
    char root[CLI_PATH_MAX_LEN];
    memor.cpy(
        &(MemoriaCfg){.dst = root,
                      .src = g_cli_tree_root,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_tree_root, .cap = CLI_CELLS_POOL}) + 1u});
    s_done = 0ull;
    unsigned int files = 0u;
    unsigned long long file_bytes = 0ull;
    EMBED_CALL(cli_symbols_tree_count.branch_files_count, CliSymbolsTreeCountCfg, .count = &files,
               .bytes_u64s = &file_bytes);
    s_total = files;
    for (unsigned int at = 0u; at < g_cli_branch_count; at += 1u)
    {
        g_cli_branches[at].seen = (g_cli_branches[at].parent == CLI_BRANCH_MOUNTED) ? 0 : g_cli_branches[at].seen;
    }
    int stopped = 0;
    for (unsigned int at = 0u; !stopped && (at < g_cli_branch_count); at += 1u)
    {
        const CliTreeBranch *const branch = &g_cli_branches[at];
        if (branch->used && !branch->folder && (branch->parent != CLI_BRANCH_MOUNTED))
        {
            char path[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg, .branch = branch,
                       .path_out = path);
            s_done += 1ull;
            stopped = EMBED_CALL(cli_symbols_read_languages_internal.reads_in, CliSymbolsReadLanguagesCtx,
                                 .read_symbols_languages_pointer = walk->languages, .path = path) &&
                      !EMBED_CALL(cli_branch_walk, SymbolsCtx, .walk = walk, .root = root, .path = path,
                                  .generation = generation, .changed = 0);
            stopped = stopped || s_stop || (s_generation != generation);
        }
    }
    for (unsigned int at = 0u; !stopped && (at < walk->mounts); at += 1u)
    {
        CliMountWalk mount = {walk, root, walk->mount_name[at], generation, 0};
        if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = walk->mount_path[at]))
        {
            char named[CLI_PATH_MAX_LEN];
            VerbaLinea line = {named, sizeof(named), 0ull};
            mmgr_line_ch(&line, '@');
            mmgr_line_put(&line, mount.name);
            mount.stopped = (mmgr_line_finish(&line) != 0ull) &&
                            EMBED_CALL(cli_symbols_read_languages_internal.reads_in, CliSymbolsReadLanguagesCtx,
                                       .read_symbols_languages_pointer = walk->languages, .path = named) &&
                            !EMBED_CALL(cli_branch_walk, SymbolsCtx, .walk = walk, .root = root, .path = named,
                                        .generation = generation, .changed = 0);
        }
        else
        {
            EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = walk->mount_path[at],
                       .each = cli_mount_branch_walk, .context = &mount);
        }
        stopped = mount.stopped || s_stop || (s_generation != generation);
    }
    if (s_generation == generation)
    {
        // A walk stopped retains every branch it has walked, and those it had not come to
        for (unsigned int at = 0u; !stopped && (at < g_cli_branch_count); at += 1u)
        {
            const CliTreeBranch *const branch = &g_cli_branches[at];
            if (branch->used && !branch->seen && (branch->parent == CLI_BRANCH_MOUNTED))
            {
                EMBED_CALL(cli_symbols_tree_file.branch_leave, CliSymbolsTreeFileCfg, .file = at);
            }
        }
        g_cli_tree_walking = 0;
        s_read = 1;
    }
    EMBED_CALL(cli_tree_walk_free, SymbolsCtx, .walk = walk);
    s_walk = NULL;
    s_running = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

void cli_tree_forgotten(void)
{
    s_generation += 1ull;
    s_rooted = 0;
    s_list_result = 0;
    s_listing = 0ull;
    s_git = 0;
    g_cli_tree_walking = 0;
    s_read = 0;
    s_walked_mounts = 0ull;
    s_stop = 0;
    cli_tree_clear();
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

// whether the tree stored is the one at `root`. Called under the lock
EMBED_INLINE int cli_tree_rooted_at(const SymbolsCtx *args)
{
    return s_rooted &&
           (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tree_root, .other = args->root, .cap = CLI_CELLS_POOL}));
}

// A listing going: which opening of the tree it is for, and who is told of each path it joins
typedef struct
{
    unsigned long long generation;
    CliTreeChange emit;
    void *context;
} CliTreeListing;

// one path listed: seen where its branch stands, joined and told where none does
static void cli_tree_path_list(const CliEachPathLengthCfg *args)
{
    const char *path = args->path;
    const CliTreeListing *const listing = (const CliTreeListing *)args->context;
    if (s_generation == listing->generation)
    {
        unsigned int file = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = path);
        if (file == CLI_BRANCHES_MAX)
        {
            file = EMBED_CALL(cli_symbols_tree_path.branch_exists, CliSymbolsTreePathCfg, .path = path, .folder = 0);
            if ((file != CLI_BRANCHES_MAX) && (listing->emit != NULL))
            {
                EMBED_CALL(listing->emit, CliEachPathKindCfg, .context = listing->context, .path = path,
                           .kind = CLI_CHANGE_ADDED);
            }
        }
        if (file != CLI_BRANCHES_MAX)
        {
            g_cli_branches[file].seen = 1;
        }
    }
}

void cli_tree_list(const CliSymbolsContextCfg *args)
{
    for (;;)
    {
        if (!EMBED_CALL(cli_tree_rooted_at, SymbolsCtx, .root = args->root))
        {
            s_generation += 1ull;
            memor.cpy(
                &(MemoriaCfg){.dst = g_cli_tree_root,
                              .src = args->root,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
            s_rooted = 1;
            s_list_result = 0;
            s_listing = 0ull;
            g_cli_tree_walking = 0;
            s_read = 0;
            s_walked_mounts = 0ull;
            s_stop = 0;
            cli_tree_clear();
        }
        if (s_listing != s_generation)
        {
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    if (s_list_result)
    {
        return;
    }
    const unsigned long long generation = s_generation;
    s_listing = generation;
    CliTreeListing listing = {generation, NULL, NULL};
    int git = 0;
    (void)EMBED_CALL(cli_files_root_each_context.list, CliFilesRootEachContextCfg, .root = args->root,
                     .each = cli_tree_path_list, .context = &listing, .git_pointer = &git);
    if (s_generation == generation)
    {
        s_list_result = 1;
        s_git = git;
        s_listing_at = cli_clock_milliseconds();
        s_listing = 0ull;
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

int cli_tree_is_list(const CliSymbolsRootCfg *args)
{
    const int list_entries = EMBED_CALL(cli_tree_rooted_at, SymbolsCtx, .root = args->root) && s_list_result;
    return list_entries;
}

int cli_tree_git(void)
{
    const int git = s_git;
    return git;
}

unsigned long long cli_tree_listing_at(void)
{
    const unsigned long long at = s_listing_at;
    return at;
}

void cli_tree_files_count(const CliSymbolsCountCfg *args)
{
    EMBED_CALL(cli_symbols_tree_count.branch_files_count, CliSymbolsTreeCountCfg, .count = args->count_pointer,
               .bytes_u64s = args->bytes);
}

int cli_tree_next_file(const CliSymbolsAtCfg *args)
{
    unsigned int *at = args->at;
    for (unsigned int place = *at; place < g_cli_branch_count; place += 1u)
    {
        const CliTreeBranch *const branch = &g_cli_branches[place];
        if (branch->used && !branch->folder && (branch->parent != CLI_BRANCH_MOUNTED))
        {
            EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg, .branch = branch,
                       .path_out = args->path_out);
            *at = place + 1u;
            return 1;
        }
    }
    return 0;
}

int cli_tree_has_file(const CliSymbolsPathNameCfg *args)
{
    const unsigned int file = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = args->path);
    const int has = (file != CLI_BRANCHES_MAX) && !g_cli_branches[file].folder;
    return has;
}

int cli_tree_has_folder(const CliSymbolsPathNameCfg *args)
{
    const unsigned int file = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = args->path);
    const int has = (file != CLI_BRANCHES_MAX) && g_cli_branches[file].folder;
    return has;
}

// a branch gone from the trunk let go, a folder with every branch under it. Called under the lock
static void cli_branch_remove(const SymbolsCtx *args)
{
    unsigned int file = args->file;
    if (!g_cli_branches[file].folder)
    {
        EMBED_CALL(cli_symbols_tree_file.branch_leave, CliSymbolsTreeFileCfg, .file = file);
        return;
    }
    // the last branch left under a folder lets the folder go
    for (unsigned int at = 0u; (at < g_cli_branch_count) && g_cli_branches[file].used; at += 1u)
    {
        if (g_cli_branches[at].used && (g_cli_branches[at].parent == file))
        {
            EMBED_CALL(cli_branch_remove, SymbolsCtx, .file = at);
        }
    }
}

void cli_tree_mirror(const CliSymbolsRootCfg *args)
{
    const int exists = EMBED_CALL(cli_tree_rooted_at, SymbolsCtx, .root = args->root) && s_list_result;
    const int git = s_git;
    const unsigned long long generation = s_generation;
    if (!exists || (args->count == 0u))
    {
        return;
    }
    const char **const joining =
        g_cli_cells->temporary_buf_alloc((unsigned long long)args->count * sizeof(const char *), &g_cli_code_cells);
    unsigned char *const ignored = g_cli_cells->temporary_buf_alloc(args->count, &g_cli_code_cells);
    if ((joining == NULL) || (ignored == NULL))
    {
        g_cli_cells->persistent_buf_release((void *)joining, &g_cli_code_cells);
        g_cli_cells->persistent_buf_release(ignored, &g_cli_code_cells);
        return;
    }
    CliTreeListing listing = {generation, NULL, NULL};
    unsigned int joins = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const path = args->paths[at];
        if ((path[0] == '\0') || EMBED_CALL(cli_files_git.path_skip, CliFilesGitCfg, .path = path, .git = git))
        {
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = path, .joined = full);
        if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = full))
        {
            (void)EMBED_CALL(cli_files_root_each.folder_list, CliFilesRootEachCfg, .root = args->root, .folder = path,
                             .git = git, .each = cli_tree_path_list, .context = &listing);
            continue;
        }
        const int there = EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = full);
        const unsigned int file = (s_generation == generation) ? EMBED_CALL(cli_symbols_tree_path.branch_get,
                                                                            CliSymbolsTreePathCfg, .path = path)
                                                               : CLI_BRANCHES_MAX;
        if (!there && (file != CLI_BRANCHES_MAX))
        {
            EMBED_CALL(cli_branch_remove, SymbolsCtx, .file = file);
        }
        if (there && (file == CLI_BRANCHES_MAX))
        {
            joining[joins] = path;
            joins += 1u;
        }
    }
    mmgr_zero_buf(ignored, args->count);
    if (git && (joins != 0u))
    {
        EMBED_CALL(cli_files_root_paths.ignored_read, CliFilesRootPathsCfg, .root = args->root, .paths = joining,
                   .count = joins, .ignored = ignored);
    }
    for (unsigned int at = 0u; (s_generation == generation) && (at < joins); at += 1u)
    {
        if (!ignored[at])
        {
            (void)EMBED_CALL(cli_symbols_tree_path.branch_exists, CliSymbolsTreePathCfg, .path = joining[at],
                             .folder = 0);
        }
    }
    g_cli_cells->persistent_buf_release((void *)joining, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(ignored, &g_cli_code_cells);
}

void cli_tree_relist(const CliSymbolsContextCfg *args)
{
    while (EMBED_CALL(cli_tree_rooted_at, SymbolsCtx, .root = args->root) && (s_listing == s_generation))
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    if (!EMBED_CALL(cli_tree_rooted_at, SymbolsCtx, .root = args->root) || !s_list_result)
    {
        cli_symbols_context.tree_list(args);
        return;
    }
    const unsigned long long generation = s_generation;
    s_listing = generation;
    for (unsigned int at = 0u; at < g_cli_branch_count; at += 1u)
    {
        g_cli_branches[at].seen = (g_cli_branches[at].parent == CLI_BRANCH_MOUNTED) ? g_cli_branches[at].seen : 0;
    }
    CliTreeListing listing = {generation, args->emit, args->context};
    int git = 0;
    const int whole = EMBED_CALL(cli_files_root_each_context.list, CliFilesRootEachContextCfg, .root = args->root,
                                 .each = cli_tree_path_list, .context = &listing, .git_pointer = &git);
    if (s_generation == generation)
    {
        for (unsigned int at = 0u; whole && (at < g_cli_branch_count); at += 1u)
        {
            const CliTreeBranch *const branch = &g_cli_branches[at];
            if (branch->used && !branch->folder && !branch->seen && (branch->parent != CLI_BRANCH_MOUNTED))
            {
                if (args->emit != NULL)
                {
                    char path[CLI_PATH_MAX_LEN];
                    EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg,
                               .branch = branch, .path_out = path);
                    EMBED_CALL(args->emit, CliEachPathKindCfg, .context = args->context, .path = path,
                               .kind = CLI_CHANGE_REMOVED);
                }
                EMBED_CALL(cli_symbols_tree_file.branch_leave, CliSymbolsTreeFileCfg, .file = at);
            }
        }
        s_git = git;
        s_listing_at = cli_clock_milliseconds();
        s_listing = 0ull;
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

void cli_tree_stamp(const CliSymbolsContextCfg *args)
{
    const unsigned long long generation = s_generation;
    for (unsigned int at = 0u; EMBED_CALL(cli_tree_rooted_at, SymbolsCtx, .root = args->root) &&
                               (s_generation == generation) && (at < g_cli_branch_count);
         at += 1u)
    {
        const CliTreeBranch *const branch = &g_cli_branches[at];
        if (!branch->used || branch->folder || (branch->parent == CLI_BRANCH_MOUNTED))
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg, .branch = branch,
                   .path_out = path);
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = path, .joined = full);
        unsigned long long size = 0ull;
        unsigned long long written = 0ull;
        const int there = EMBED_CALL(cli_path_size_internal.file_stamp, CliPathSizeCtx, .path = full,
                                     .size_pointer = &size, .changed = &written);
        if (there && (s_generation == generation) && g_cli_branches[at].used)
        {
            const unsigned long long stamp = EMBED_CALL(cli_branch_stamp, SymbolsCtx, .size = size, .written = written);
            const int moved = (g_cli_branches[at].written != 0ull) && (g_cli_branches[at].written != stamp);
            g_cli_branches[at].written = stamp;
            if (moved && (args->emit != NULL))
            {
                EMBED_CALL(args->emit, CliEachPathKindCfg, .context = args->context, .path = path,
                           .kind = CLI_CHANGE_MODIFIED);
            }
        }
    }
}

// the mounts' paths folded into one word, which any mount taken or added changes. Called under the lock
static unsigned long long cli_tree_mounts_word(void)
{
    const char *names[CLI_MOUNTS_MAX];
    const char *paths[CLI_MOUNTS_MAX];
    const unsigned int mounts = EMBED_CALL(cli_capacity_names_internal.mounts, CliCapacityNamesCtx, .names = names,
                                           .paths_read = paths, .capacity_u32 = CLI_MOUNTS_MAX);
    unsigned long long word = 0ull;
    for (unsigned int at = 0u; at < mounts; at += 1u)
    {
        for (const char *letter = paths[at]; letter[0] != '\0'; letter += 1)
        {
            word = (word ^ (unsigned char)letter[0]) * 0x100000001b3ull;
        }
    }
    return word;
}

// One include folder or file the compilation database names, joined to the tree at the top `context` is: mounted
// beside it where it lies outside it
static void cli_symbols_include_join(const CliEachPathKindCfg *args)
{
    const char *path = args->path;
    const char *const root = (const char *)args->context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    int inside = 1;
    for (unsigned long long at = 0ull; inside && (at < length); at += 1ull)
    {
        const char one = (root[at] == '\\') ? '/' : root[at];
        const char other = (path[at] == '\\') ? '/' : path[at];
        inside = one == other;
    }
    inside = inside && ((path[length] == '/') || (path[length] == '\\') || (path[length] == '\0'));
    char name[CLI_NAME_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!inside)
    {
        (void)EMBED_CALL(cli_path_err_msg_name_internal.mount, CliPathErrMsgNameCtx, .path = path, .name_out = name,
                         .err_msg = &err_msg);
    }
}

void cli_tree_on_walk(const CliSymbolsRootCfg *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = root);
    if (g_cli_tree_walking || (s_read && (s_walked_mounts == cli_tree_mounts_word())))
    {
        return;
    }
    g_cli_tree_walking = 1;
    s_stop = 0;
    // the storage of the walk before is let go only once it has ended
    while (s_running)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    s_walk = (languages != NULL) ? g_cli_cells->persistent_buf_alloc(sizeof(*s_walk), &g_cli_code_cells) : NULL;
    if (s_walk == NULL)
    {
        EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = languages);
        g_cli_tree_walking = 0;
        return;
    }
    s_walk->languages = languages;
    // the database's include folders and files joined to the tree, then every mount beside it walked with it
    EMBED_CALL(cli_compile_db_root_path.includes_all, CliCompileDbRootPathCfg, .root = root,
               .each = cli_symbols_include_join, .context = (void *)root);
    const char *mount_names[CLI_MOUNTS_MAX];
    const char *mount_paths[CLI_MOUNTS_MAX];
    const unsigned int mounts =
        EMBED_CALL(cli_capacity_names_internal.mounts, CliCapacityNamesCtx, .names = mount_names,
                   .paths_read = mount_paths, .capacity_u32 = CLI_MOUNTS_MAX);
    // the mounts' names and paths in one cell sized to them
    unsigned long long mount_bytes = 2ull * mounts * sizeof(*s_walk->mount_name);
    for (unsigned int at = 0u; at < mounts; at += 1u)
    {
        mount_bytes += cellul.len(&(CatenaFinitaCfg){.src = mount_names[at], .cap = CLI_NAME_MAX_LEN}) +
                       cellul.len(&(CatenaFinitaCfg){.src = mount_paths[at], .cap = CLI_PATH_MAX_LEN}) + 2ull;
    }
    s_walk->mount_name = (mounts != 0u) ? g_cli_cells->persistent_buf_alloc(mount_bytes, &g_cli_code_cells) : NULL;
    s_walk->mounts = (s_walk->mount_name != NULL) ? mounts : 0u;
    s_walk->mount_path = (s_walk->mount_name != NULL) ? (s_walk->mount_name + mounts) : NULL;
    char *mount_text = (s_walk->mount_name != NULL) ? (char *)(void *)(s_walk->mount_path + mounts) : NULL;
    for (unsigned int at = 0u; at < s_walk->mounts; at += 1u)
    {
        const unsigned long long name_size =
            cellul.len(&(CatenaFinitaCfg){.src = mount_names[at], .cap = CLI_NAME_MAX_LEN}) + 1ull;
        memor.cpy(&(MemoriaCfg){.dst = mount_text, .src = mount_names[at], .bytes = name_size});
        s_walk->mount_name[at] = mount_text;
        mount_text += name_size;
        const unsigned long long path_size =
            cellul.len(&(CatenaFinitaCfg){.src = mount_paths[at], .cap = CLI_PATH_MAX_LEN}) + 1ull;
        memor.cpy(&(MemoriaCfg){.dst = mount_text, .src = mount_paths[at], .bytes = path_size});
        s_walk->mount_path[at] = mount_text;
        mount_text += path_size;
    }
    s_walked_mounts = cli_tree_mounts_word();
    s_walk->read_text = NULL;
    s_walk->text_capacity = 0u;
    s_walk->read_symbols = NULL;
    s_walk->symbol_capacity = 0u;
    s_walk->read_count = 0u;
    s_walk->read_used = 0u;
    s_walk->kinds = 0u;
    s_walk->file_bytes = g_cli_cells->persistent_buf_alloc(CLI_SYMBOLS_LARGEST + CLI_FILE_SLACK, &g_cli_code_cells);
    s_thread.run = cli_tree_walk;
    s_thread.context = s_walk;
    s_running = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread);
    if (!s_running)
    {
        EMBED_CALL(cli_tree_walk_free, SymbolsCtx, .walk = s_walk);
        s_walk = NULL;
    }
    g_cli_tree_walking = s_running;
}

void cli_tree_branches_walk(const CliSymbolsRootCfg *args)
{
    const int exists = EMBED_CALL(cli_tree_rooted_at, SymbolsCtx, .root = args->root);
    const unsigned long long generation = s_generation;
    CliSymbolsLanguages *const languages = (exists && (args->count != 0u)) ? cli_symbols_languages_for_call() : NULL;
    CliTreeWalk *const walk =
        (languages != NULL) ? g_cli_cells->temporary_buf_alloc(sizeof(*walk), &g_cli_code_cells) : NULL;
    if (walk == NULL)
    {
        EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = languages);
        return;
    }
    walk->languages = languages;
    walk->mounts = 0u;
    walk->read_text = NULL;
    walk->text_capacity = 0u;
    walk->read_symbols = NULL;
    walk->symbol_capacity = 0u;
    walk->read_count = 0u;
    walk->read_used = 0u;
    walk->kinds = 0u;
    walk->file_bytes = g_cli_cells->temporary_buf_alloc(CLI_SYMBOLS_LARGEST + CLI_FILE_SLACK, &g_cli_code_cells);
    for (unsigned int at = 0u; (s_generation == generation) && (at < args->count); at += 1u)
    {
        if (EMBED_CALL(cli_symbols_read_languages_internal.reads_in, CliSymbolsReadLanguagesCtx,
                       .read_symbols_languages_pointer = languages, .path = args->paths[at]))
        {
            (void)EMBED_CALL(cli_branch_walk, SymbolsCtx, .walk = walk, .root = args->root, .path = args->paths[at],
                             .generation = generation, .changed = 1);
        }
    }
    EMBED_CALL(cli_tree_walk_free, SymbolsCtx, .walk = walk);
}

int cli_tree_walk_progress(const CliSymbolsDoneCfg *args)
{
    unsigned long long *done = args->done;
    unsigned long long *total = args->total;
    const int going = g_cli_tree_walking && !s_read;
    *done = s_done;
    *total = s_total;
    return going;
}

void cli_tree_walk_stop(void)
{
    s_stop = 1;
}

int cli_tree_unwalked(void)
{
    const int unread = g_cli_tree_walking && !s_read;
    return unread;
}
