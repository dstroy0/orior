// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols.c: how well a query names a name, and the tree: the filesystem is its trunk and its files are its branches.
// It is listed one time as it opens, each file a branch under its closest parent, and walked one time on a thread of
// its own, each branch's grammar applied as it is read and its declarations kept in a cell of its own, searched in
// symbols_find.c. Each change after that joins, leaves or walks the one branch it names. The include folders and files
// the compilation database names outside the tree are joined to it as it is walked, each mounted beside it, and their
// files are walked as the tree's own, by their mounts' names
#include "../builds/compile_db.h"
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// a file larger than this is not walked for its declarations
#define CLI_SYMBOLS_LARGEST (1u << 20u)

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
    unsigned int text_room;
    CliDeclaration *read_symbols;
    unsigned int symbol_room;
    unsigned int read_count;
    unsigned int read_used;
    unsigned int kind[CLI_SYMBOLS_KINDS];
    unsigned int kinds;
    unsigned char *file_bytes;
    CliSymbolsLanguages *languages;
    char mount_name[CLI_MOUNTS_MOST][CLI_NAME_MAX_LEN];
    char mount_path[CLI_MOUNTS_MOST][CLI_PATH_MAX_LEN];
    unsigned int mounts;
} CliTreeWalk;

static CliTreeWalk *s_walk;

// The tree's state: its folder, whether it is listed, the generation a listing goes for, 0 for none, whether git listed
// it and when it was last listed; whether a walk goes, whether one ended and the mounts it went over; how far it has
// gone, whether it is requested to stop, which opening of the tree it is, and whether its thread runs
char g_cli_tree_root[CLI_PATH_MAX_LEN];
static int s_rooted;
static int s_listed;
static unsigned long long s_listing;
static int s_git;
static unsigned long long s_listed_at;
int g_cli_tree_walking;
static int s_read;
static unsigned long long s_walked_mounts;
static unsigned long long s_done;
static unsigned long long s_total;
static int s_stop;
static unsigned long long s_generation;
static int s_running;
CliLock g_cli_walk_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;

// the thread walking the tree
static CliThread s_thread;

// none: a kind not read before in the file
#define CLI_SYMBOLS_KIND_NEW 0xffffffffu

// a file's stamp, one word its time and its size both change, never 0, which a branch not stamped reads
static unsigned long long cli_branch_stamp(unsigned long long size, unsigned long long written)
{
    return (written ^ ((size << 44u) | (size >> 20u))) | 1ull;
}

// A declaration read: its name, and its kind's text where the file's declarations have not read it before, after the
// name with its NUL
static void cli_leaf_declared(void *context, const char *name, unsigned long long length, const char *kind,
                              unsigned int line)
{
    CliTreeWalk *const reading = (CliTreeWalk *)context;
    unsigned int kind_at = CLI_SYMBOLS_KIND_NEW;
    for (unsigned int at = 0u; (kind_at == CLI_SYMBOLS_KIND_NEW) && (at < reading->kinds); at += 1u)
    {
        kind_at = cellul.eq(&(CatenaFinitaCfg){
                      .src = reading->read_text + reading->kind[at], .other = kind, .cap = CLI_CELLS_POOL})
                      ? reading->kind[at]
                      : CLI_SYMBOLS_KIND_NEW;
    }
    const unsigned long long kind_length =
        (kind_at == CLI_SYMBOLS_KIND_NEW) ? cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}) + 1ull
                                          : 0ull;
    if ((reading->read_count >= (CLI_SYMBOLS_LARGEST / 4u)) || ((reading->read_used + length + kind_length) > CLI_SYMBOLS_LARGEST))
    {
        return;
    }
    if (reading->symbol_room == 0u)
    {
        reading->read_symbols = g_cli_cells->temporary_buf_alloc(
            (unsigned long long)(CLI_SYMBOLS_LARGEST / 4u) * sizeof(CliDeclaration), &g_cli_code_cells);
        reading->read_text = g_cli_cells->temporary_buf_alloc(CLI_SYMBOLS_LARGEST, &g_cli_code_cells);
        reading->symbol_room = CLI_SYMBOLS_LARGEST / 4u;
        reading->text_room = CLI_SYMBOLS_LARGEST;
    }
    if ((reading->read_symbols == NULL) || (reading->read_text == NULL))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = reading->read_text + reading->read_used, .src = name, .bytes = length});
    if (kind_at == CLI_SYMBOLS_KIND_NEW)
    {
        // a name and a kind are below a file's largest size, which an unsigned int counts
        kind_at = reading->read_used + (unsigned int)length;
        memor.cpy(&(MemoriaCfg){.dst = reading->read_text + kind_at, .src = kind, .bytes = (size_t)kind_length});
        reading->kind[reading->kinds] = kind_at;
        reading->kinds += (reading->kinds < (CLI_SYMBOLS_KINDS - 1u)) ? 1u : 0u;
    }
    const CliDeclaration symbol = {reading->read_used, (unsigned int)length, kind_at, line};
    reading->read_symbols[reading->read_count] = symbol;
    reading->read_count += 1u;
    reading->read_used += (unsigned int)(length + kind_length);
}

// the declarations read put as the branch at `path`, stamped `written`, in a cell of its own, which takes the place of
// the one it stood in: 1, or 0 where the table fits no more branches or the cells cannot meet it. Called under the lock
static int cli_branch_joined(const CliTreeWalk *walk, const char *path, unsigned long long written)
{
    const unsigned int file = cli_branch_stands(path, 0);
    if (file == CLI_BRANCHES_MOST)
    {
        return 0;
    }
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull;
    const unsigned long long symbols_size = walk->read_count * sizeof(CliDeclaration);
    const unsigned long long bytes = symbols_size + path_length + walk->read_used;
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
    branch->count = walk->read_count;
    branch->seen = 1;
    cli_branch_cell_counted(branch->bytes);
    // the names and kinds read, after the path, each declaration's places among them moved with them
    const unsigned int texts = (unsigned int)(symbols_size + path_length);
    for (unsigned int at = 0u; at < walk->read_count; at += 1u)
    {
        walk->read_symbols[at].name += texts;
        walk->read_symbols[at].kind += texts;
    }
    (void)g_cli_cells->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .src = walk->read_symbols, .bytes = symbols_size});
    (void)g_cli_cells->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = symbols_size, .src = path, .bytes = path_length});
    (void)g_cli_cells->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = texts, .src = walk->read_text, .bytes = walk->read_used});
    return 1;
}

// the branch at `path` of the tree, a path from its top or under a mount's name, walked where its stamp moved since it
// was last walked or where `changed` says it did, its declarations stored; called under the lock, given up while the
// file is stamped and read: 1, or 0 where the walk was asked to stop or the tree it walks was let go
static int cli_branch_walk(CliTreeWalk *walk, const char *root, const char *path, unsigned long long generation,
                           int changed)
{
    if (s_stop || (s_generation != generation))
    {
        return 0;
    }
    cli_lock_give(&g_cli_walk_lock);
    char full[CLI_PATH_MAX_LEN];
    cli_full(root, path, full);
    unsigned long long size = 0ull;
    unsigned long long written = 0ull;
    const int stamped = cli_file_stamp(full, &size, &written) && (size <= CLI_SYMBOLS_LARGEST);
    const unsigned long long stamp = cli_branch_stamp(size, written);
    cli_lock_take(&g_cli_walk_lock);
    if (s_generation != generation)
    {
        return 0;
    }
    const unsigned int file = stamped ? cli_branch_of(path) : CLI_BRANCHES_MOST;
    if (file != CLI_BRANCHES_MOST)
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
    cli_lock_give(&g_cli_walk_lock);
    // the file read into the walk's one cell, used from file to file, which fits the most a file read holds
    unsigned long long mapped_size = 0ull;
    const int mapped = (walk->file_bytes != NULL) && cli_file_read_at(full, 0ull, walk->file_bytes, size, &mapped_size);
    const unsigned long long read = mapped ? mapped_size : QRY_MEMBER_UNREAD;
    walk->read_count = 0u;
    walk->read_used = 0u;
    walk->kinds = 0u;
    if (read != QRY_MEMBER_UNREAD)
    {
        memor.set(&(MemoriaCfg){.dst = walk->file_bytes + read, .val = 0, .bytes = CLI_FILE_SLACK});
        // the file's bytes are read as its text
        cli_symbols_declared_in(walk->languages, path, (const char *)walk->file_bytes, read, cli_leaf_declared, walk);
    }
    cli_lock_take(&g_cli_walk_lock);
    if (s_generation != generation)
    {
        return 0;
    }
    if (read != QRY_MEMBER_UNREAD)
    {
        cli_branch_joined(walk, path, stamp);
    }
    return 1;
}

// the cells of a walk given back: the file's and the declarations', the languages' and its own
static void cli_tree_walk_given(CliTreeWalk *walk)
{
    g_cli_cells->persistent_buf_release(walk->read_text, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(walk->read_symbols, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(walk->file_bytes, &g_cli_code_cells);
    cli_symbols_languages_given(walk->languages);
    g_cli_cells->persistent_buf_release(walk, &g_cli_code_cells);
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
static void cli_mount_branch_walk(void *context, const char *path, unsigned long long length)
{
    CliMountWalk *const mount = (CliMountWalk *)context;
    char named[CLI_PATH_MAX_LEN];
    VerbaLinea line = {named, sizeof(named), 0ull};
    mmgr_line_ch(&line, '@');
    mmgr_line_put(&line, mount->name);
    mmgr_line_ch(&line, '/');
    mmgr_line_put_n(&line, path, length);
    if (mount->stopped || (mmgr_line_finish(&line) == 0ull) || !cli_symbols_reads_in(mount->walk->languages, named))
    {
        return;
    }
    cli_lock_take(&g_cli_walk_lock);
    s_total += 1ull;
    s_done += 1ull;
    mount->stopped = !cli_branch_walk(mount->walk, mount->root, named, mount->generation, 0);
    cli_lock_give(&g_cli_walk_lock);
}

// The walk: each file branch of the tree, then each file of each folder mounted beside it and each file mounted; past
// the last, the mounted branches it did not see let go where it was not stopped
static void cli_tree_walk(void *context)
{
    CliTreeWalk *const walk = (CliTreeWalk *)context;
    cli_lock_take(&g_cli_walk_lock);
    const unsigned long long generation = s_generation;
    char root[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = root,
                            .src = g_cli_tree_root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_tree_root, .cap = CLI_CELLS_POOL}) + 1u});
    s_done = 0ull;
    unsigned int files = 0u;
    unsigned long long file_bytes = 0ull;
    cli_branch_files_counted(&files, &file_bytes);
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
            cli_branch_path_read(branch, path);
            s_done += 1ull;
            stopped = cli_symbols_reads_in(walk->languages, path) && !cli_branch_walk(walk, root, path, generation, 0);
            stopped = stopped || s_stop || (s_generation != generation);
        }
    }
    for (unsigned int at = 0u; !stopped && (at < walk->mounts); at += 1u)
    {
        CliMountWalk mount = {walk, root, walk->mount_name[at], generation, 0};
        cli_lock_give(&g_cli_walk_lock);
        if (query_is_file(walk->mount_path[at]))
        {
            char named[CLI_PATH_MAX_LEN];
            VerbaLinea line = {named, sizeof(named), 0ull};
            mmgr_line_ch(&line, '@');
            mmgr_line_put(&line, mount.name);
            cli_lock_take(&g_cli_walk_lock);
            mount.stopped = (mmgr_line_finish(&line) != 0ull) && cli_symbols_reads_in(walk->languages, named) &&
                            !cli_branch_walk(walk, root, named, generation, 0);
            cli_lock_give(&g_cli_walk_lock);
        }
        else
        {
            cli_files_walked(walk->mount_path[at], cli_mount_branch_walk, &mount);
        }
        cli_lock_take(&g_cli_walk_lock);
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
                cli_branch_left(at);
            }
        }
        g_cli_tree_walking = 0;
        s_read = 1;
    }
    cli_tree_walk_given(walk);
    s_walk = NULL;
    s_running = 0;
    cli_wake_all(&s_wake);
    cli_lock_give(&g_cli_walk_lock);
}

void cli_tree_forgotten(void)
{
    cli_lock_take(&g_cli_walk_lock);
    s_generation += 1ull;
    s_rooted = 0;
    s_listed = 0;
    s_listing = 0ull;
    s_git = 0;
    g_cli_tree_walking = 0;
    s_read = 0;
    s_walked_mounts = 0ull;
    s_stop = 0;
    cli_tree_cleared();
    cli_wake_all(&s_wake);
    cli_lock_give(&g_cli_walk_lock);
}

// whether the tree stored is the one at `root`. Called under the lock
static int cli_tree_rooted_at(const char *root)
{
    return s_rooted && (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tree_root, .other = root, .cap = CLI_CELLS_POOL}));
}

// A listing going: which opening of the tree it is for, and who is told of each path it joins
typedef struct
{
    unsigned long long generation;
    CliTreeChange emit;
    void *context;
} CliTreeListing;

// one path listed: seen where its branch stands, joined and told where none does
static void cli_tree_path_listed(void *context, const char *path, unsigned long long length)
{
    const CliTreeListing *const listing = (const CliTreeListing *)context;
    (void)length;
    cli_lock_take(&g_cli_walk_lock);
    if (s_generation == listing->generation)
    {
        unsigned int file = cli_branch_of(path);
        if (file == CLI_BRANCHES_MOST)
        {
            file = cli_branch_stands(path, 0);
            if ((file != CLI_BRANCHES_MOST) && (listing->emit != NULL))
            {
                listing->emit(listing->context, path, CLI_CHANGE_ADDED);
            }
        }
        if (file != CLI_BRANCHES_MOST)
        {
            g_cli_branches[file].seen = 1;
        }
    }
    cli_lock_give(&g_cli_walk_lock);
}

void cli_tree_listed(const char *root)
{
    cli_lock_take(&g_cli_walk_lock);
    for (;;)
    {
        if (!cli_tree_rooted_at(root))
        {
            s_generation += 1ull;
            memor.cpy(&(MemoriaCfg){.dst = g_cli_tree_root,
                                    .src = root,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
            s_rooted = 1;
            s_listed = 0;
            s_listing = 0ull;
            g_cli_tree_walking = 0;
            s_read = 0;
            s_walked_mounts = 0ull;
            s_stop = 0;
            cli_tree_cleared();
        }
        if (s_listing != s_generation)
        {
            break;
        }
        cli_wake_wait(&s_wake, &g_cli_walk_lock);
    }
    if (s_listed)
    {
        cli_lock_give(&g_cli_walk_lock);
        return;
    }
    const unsigned long long generation = s_generation;
    s_listing = generation;
    cli_lock_give(&g_cli_walk_lock);
    CliTreeListing listing = {generation, NULL, NULL};
    int git = 0;
    (void)cli_files_listed(root, cli_tree_path_listed, &listing, &git);
    cli_lock_take(&g_cli_walk_lock);
    if (s_generation == generation)
    {
        s_listed = 1;
        s_git = git;
        s_listed_at = cli_clock_milliseconds();
        s_listing = 0ull;
    }
    cli_wake_all(&s_wake);
    cli_lock_give(&g_cli_walk_lock);
}

int cli_tree_is_listed(const char *root)
{
    cli_lock_take(&g_cli_walk_lock);
    const int listed = cli_tree_rooted_at(root) && s_listed;
    cli_lock_give(&g_cli_walk_lock);
    return listed;
}

int cli_tree_git(void)
{
    cli_lock_take(&g_cli_walk_lock);
    const int git = s_git;
    cli_lock_give(&g_cli_walk_lock);
    return git;
}

unsigned long long cli_tree_listed_at(void)
{
    cli_lock_take(&g_cli_walk_lock);
    const unsigned long long at = s_listed_at;
    cli_lock_give(&g_cli_walk_lock);
    return at;
}

void cli_tree_files_counted(unsigned int *count, unsigned long long *bytes)
{
    cli_lock_take(&g_cli_walk_lock);
    cli_branch_files_counted(count, bytes);
    cli_lock_give(&g_cli_walk_lock);
}

int cli_tree_next_file(unsigned int *at, char *path)
{
    cli_lock_take(&g_cli_walk_lock);
    for (unsigned int place = *at; place < g_cli_branch_count; place += 1u)
    {
        const CliTreeBranch *const branch = &g_cli_branches[place];
        if (branch->used && !branch->folder && (branch->parent != CLI_BRANCH_MOUNTED))
        {
            cli_branch_path_read(branch, path);
            *at = place + 1u;
            cli_lock_give(&g_cli_walk_lock);
            return 1;
        }
    }
    cli_lock_give(&g_cli_walk_lock);
    return 0;
}

int cli_tree_has_file(const char *path)
{
    cli_lock_take(&g_cli_walk_lock);
    const unsigned int file = cli_branch_of(path);
    const int has = (file != CLI_BRANCHES_MOST) && !g_cli_branches[file].folder;
    cli_lock_give(&g_cli_walk_lock);
    return has;
}

int cli_tree_has_folder(const char *path)
{
    cli_lock_take(&g_cli_walk_lock);
    const unsigned int file = cli_branch_of(path);
    const int has = (file != CLI_BRANCHES_MOST) && g_cli_branches[file].folder;
    cli_lock_give(&g_cli_walk_lock);
    return has;
}

// a branch gone from the trunk let go, a folder with every branch under it. Called under the lock
static void cli_branch_gone(unsigned int file)
{
    if (!g_cli_branches[file].folder)
    {
        cli_branch_left(file);
        return;
    }
    // the last branch left under a folder lets the folder go
    for (unsigned int at = 0u; (at < g_cli_branch_count) && g_cli_branches[file].used; at += 1u)
    {
        if (g_cli_branches[at].used && (g_cli_branches[at].parent == file))
        {
            cli_branch_gone(at);
        }
    }
}

void cli_tree_mirrored(const char *root, const char *const *paths, unsigned int count)
{
    cli_lock_take(&g_cli_walk_lock);
    const int stands = cli_tree_rooted_at(root) && s_listed;
    const int git = s_git;
    const unsigned long long generation = s_generation;
    cli_lock_give(&g_cli_walk_lock);
    if (!stands || (count == 0u))
    {
        return;
    }
    const char **const joining =
        g_cli_cells->temporary_buf_alloc((unsigned long long)count * sizeof(const char *), &g_cli_code_cells);
    unsigned char *const ignored = g_cli_cells->temporary_buf_alloc(count, &g_cli_code_cells);
    if ((joining == NULL) || (ignored == NULL))
    {
        g_cli_cells->persistent_buf_release((void *)joining, &g_cli_code_cells);
        g_cli_cells->persistent_buf_release(ignored, &g_cli_code_cells);
        return;
    }
    CliTreeListing listing = {generation, NULL, NULL};
    unsigned int joins = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = paths[at];
        if ((path[0] == '\0') || cli_files_path_skipped(path, git))
        {
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        cli_full(root, path, full);
        if (query_is_folder(full))
        {
            (void)cli_files_folder_listed(root, path, git, cli_tree_path_listed, &listing);
            continue;
        }
        const int there = query_is_file(full);
        cli_lock_take(&g_cli_walk_lock);
        const unsigned int file = (s_generation == generation) ? cli_branch_of(path) : CLI_BRANCHES_MOST;
        if (!there && (file != CLI_BRANCHES_MOST))
        {
            cli_branch_gone(file);
        }
        cli_lock_give(&g_cli_walk_lock);
        if (there && (file == CLI_BRANCHES_MOST))
        {
            joining[joins] = path;
            joins += 1u;
        }
    }
    mmgr_zero_buf(ignored, count);
    if (git && (joins != 0u))
    {
        cli_files_ignored_read(root, joining, joins, ignored);
    }
    cli_lock_take(&g_cli_walk_lock);
    for (unsigned int at = 0u; (s_generation == generation) && (at < joins); at += 1u)
    {
        if (!ignored[at])
        {
            (void)cli_branch_stands(joining[at], 0);
        }
    }
    cli_lock_give(&g_cli_walk_lock);
    g_cli_cells->persistent_buf_release((void *)joining, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(ignored, &g_cli_code_cells);
}

void cli_tree_relisted(const char *root, CliTreeChange emit, void *context)
{
    cli_lock_take(&g_cli_walk_lock);
    while (cli_tree_rooted_at(root) && (s_listing == s_generation))
    {
        cli_wake_wait(&s_wake, &g_cli_walk_lock);
    }
    if (!cli_tree_rooted_at(root) || !s_listed)
    {
        cli_lock_give(&g_cli_walk_lock);
        cli_tree_listed(root);
        return;
    }
    const unsigned long long generation = s_generation;
    s_listing = generation;
    for (unsigned int at = 0u; at < g_cli_branch_count; at += 1u)
    {
        g_cli_branches[at].seen = (g_cli_branches[at].parent == CLI_BRANCH_MOUNTED) ? g_cli_branches[at].seen : 0;
    }
    cli_lock_give(&g_cli_walk_lock);
    CliTreeListing listing = {generation, emit, context};
    int git = 0;
    const int whole = cli_files_listed(root, cli_tree_path_listed, &listing, &git);
    cli_lock_take(&g_cli_walk_lock);
    if (s_generation == generation)
    {
        for (unsigned int at = 0u; whole && (at < g_cli_branch_count); at += 1u)
        {
            const CliTreeBranch *const branch = &g_cli_branches[at];
            if (branch->used && !branch->folder && !branch->seen && (branch->parent != CLI_BRANCH_MOUNTED))
            {
                if (emit != NULL)
                {
                    char path[CLI_PATH_MAX_LEN];
                    cli_branch_path_read(branch, path);
                    emit(context, path, CLI_CHANGE_REMOVED);
                }
                cli_branch_left(at);
            }
        }
        s_git = git;
        s_listed_at = cli_clock_milliseconds();
        s_listing = 0ull;
    }
    cli_wake_all(&s_wake);
    cli_lock_give(&g_cli_walk_lock);
}

void cli_tree_stamped(const char *root, CliTreeChange emit, void *context)
{
    cli_lock_take(&g_cli_walk_lock);
    const unsigned long long generation = s_generation;
    for (unsigned int at = 0u; cli_tree_rooted_at(root) && (s_generation == generation) && (at < g_cli_branch_count);
         at += 1u)
    {
        const CliTreeBranch *const branch = &g_cli_branches[at];
        if (!branch->used || branch->folder || (branch->parent == CLI_BRANCH_MOUNTED))
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        cli_branch_path_read(branch, path);
        cli_lock_give(&g_cli_walk_lock);
        char full[CLI_PATH_MAX_LEN];
        cli_full(root, path, full);
        unsigned long long size = 0ull;
        unsigned long long written = 0ull;
        const int there = cli_file_stamp(full, &size, &written);
        cli_lock_take(&g_cli_walk_lock);
        if (there && (s_generation == generation) && g_cli_branches[at].used)
        {
            const unsigned long long stamp = cli_branch_stamp(size, written);
            const int moved = (g_cli_branches[at].written != 0ull) && (g_cli_branches[at].written != stamp);
            g_cli_branches[at].written = stamp;
            if (moved && (emit != NULL))
            {
                emit(context, path, CLI_CHANGE_MODIFIED);
            }
        }
    }
    cli_lock_give(&g_cli_walk_lock);
}

// the mounts' paths folded into one word, which any mount taken or added changes. Called under the lock
static unsigned long long cli_tree_mounts_word(void)
{
    const char *names[CLI_MOUNTS_MOST];
    const char *paths[CLI_MOUNTS_MOST];
    const unsigned int mounts = cli_mounts(names, paths, CLI_MOUNTS_MOST);
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
static void cli_symbols_include_joined(void *context, const char *path, unsigned int kind)
{
    (void)kind;
    const char *const root = (const char *)context;
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
        (void)cli_mounted(path, name, &err_msg);
    }
}

void cli_tree_walked(const char *root)
{
    cli_tree_listed(root);
    cli_lock_take(&g_cli_walk_lock);
    if (g_cli_tree_walking || (s_read && (s_walked_mounts == cli_tree_mounts_word())))
    {
        cli_lock_give(&g_cli_walk_lock);
        return;
    }
    g_cli_tree_walking = 1;
    s_stop = 0;
    // the storage of the walk before is let go only once it has ended
    while (s_running)
    {
        cli_wake_wait(&s_wake, &g_cli_walk_lock);
    }
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    s_walk = (languages != NULL) ? g_cli_cells->persistent_buf_alloc(sizeof(*s_walk), &g_cli_code_cells) : NULL;
    if (s_walk == NULL)
    {
        cli_symbols_languages_given(languages);
        g_cli_tree_walking = 0;
        cli_lock_give(&g_cli_walk_lock);
        return;
    }
    s_walk->languages = languages;
    // the database's include folders and files joined to the tree, then every mount beside it walked with it
    cli_compile_db_includes_all(root, cli_symbols_include_joined, (void *)root);
    const char *mount_names[CLI_MOUNTS_MOST];
    const char *mount_paths[CLI_MOUNTS_MOST];
    s_walk->mounts = cli_mounts(mount_names, mount_paths, CLI_MOUNTS_MOST);
    for (unsigned int at = 0u; at < s_walk->mounts; at += 1u)
    {
        (void)cellul.copy(
            &(CatenaFinitaCfg){.dst = s_walk->mount_name[at], .src = mount_names[at], .cap = CLI_NAME_MAX_LEN});
        (void)cellul.copy(
            &(CatenaFinitaCfg){.dst = s_walk->mount_path[at], .src = mount_paths[at], .cap = CLI_PATH_MAX_LEN});
    }
    s_walked_mounts = cli_tree_mounts_word();
    s_walk->read_text = NULL;
    s_walk->text_room = 0u;
    s_walk->read_symbols = NULL;
    s_walk->symbol_room = 0u;
    s_walk->read_count = 0u;
    s_walk->read_used = 0u;
    s_walk->kinds = 0u;
    s_walk->file_bytes = g_cli_cells->persistent_buf_alloc(CLI_SYMBOLS_LARGEST + CLI_FILE_SLACK, &g_cli_code_cells);
    s_thread.run = cli_tree_walk;
    s_thread.context = s_walk;
    s_running = cli_thread_started(&s_thread);
    if (!s_running)
    {
        cli_tree_walk_given(s_walk);
        s_walk = NULL;
    }
    g_cli_tree_walking = s_running;
    cli_lock_give(&g_cli_walk_lock);
}

void cli_tree_branches_walked(const char *root, const char *const *paths, unsigned int count)
{
    cli_lock_take(&g_cli_walk_lock);
    const int stands = cli_tree_rooted_at(root);
    const unsigned long long generation = s_generation;
    cli_lock_give(&g_cli_walk_lock);
    CliSymbolsLanguages *const languages = (stands && (count != 0u)) ? cli_symbols_languages_for_call() : NULL;
    CliTreeWalk *const walk =
        (languages != NULL) ? g_cli_cells->temporary_buf_alloc(sizeof(*walk), &g_cli_code_cells) : NULL;
    if (walk == NULL)
    {
        cli_symbols_languages_given(languages);
        return;
    }
    walk->languages = languages;
    walk->mounts = 0u;
    walk->read_text = NULL;
    walk->text_room = 0u;
    walk->read_symbols = NULL;
    walk->symbol_room = 0u;
    walk->read_count = 0u;
    walk->read_used = 0u;
    walk->kinds = 0u;
    walk->file_bytes = g_cli_cells->temporary_buf_alloc(CLI_SYMBOLS_LARGEST + CLI_FILE_SLACK, &g_cli_code_cells);
    cli_lock_take(&g_cli_walk_lock);
    for (unsigned int at = 0u; (s_generation == generation) && (at < count); at += 1u)
    {
        if (cli_symbols_reads_in(languages, paths[at]))
        {
            (void)cli_branch_walk(walk, root, paths[at], generation, 1);
        }
    }
    cli_lock_give(&g_cli_walk_lock);
    cli_tree_walk_given(walk);
}

int cli_tree_walk_progress(unsigned long long *done, unsigned long long *total)
{
    cli_lock_take(&g_cli_walk_lock);
    const int going = g_cli_tree_walking && !s_read;
    *done = s_done;
    *total = s_total;
    cli_lock_give(&g_cli_walk_lock);
    return going;
}

void cli_tree_walk_stopped(void)
{
    cli_lock_take(&g_cli_walk_lock);
    s_stop = 1;
    cli_lock_give(&g_cli_walk_lock);
}

int cli_tree_unwalked(void)
{
    cli_lock_take(&g_cli_walk_lock);
    const int unread = g_cli_tree_walking && !s_read;
    cli_lock_give(&g_cli_walk_lock);
    return unread;
}
