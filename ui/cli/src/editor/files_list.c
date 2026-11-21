// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_list.c: a folder's entries, less what the explorer's patterns hide, each marked where the tree's ignore files
// leave it out; an archive, and a folder within one, listing what the archive contains
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/commands.h"
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "archive.h"
#include "files_internal.h"

// the max entries a folder lists
#define CLI_LIST_MAX 65536u

// An entry of a folder. It holds its name and path as places in the texts, and whether it is a folder. Then whether
// the tree's ignore files leave it out, and whether it is an archive, which opens as a folder. Last, its name's own
// bytes in hex where the archive's word says they are not UTF-8
typedef struct
{
    unsigned int name;
    unsigned int path;
    int dir;
    int ignored;
    int archive;
    unsigned int raw;
    int has_raw;
} CliEntry;

// The entries of the folder in the listing now and their texts, in one cell at their max. Then the folders that hold
// a file the patterns leave, a list made of the files cellblock, under the lock
typedef struct
{
    CliEntry *item;
    unsigned int count;
    unsigned int item_capacity;
    char *text;
    unsigned long long used;
    unsigned long long text_capacity;
    CliFiles folders;
    CliPatterns patterns;
    CliChildren children;
} CliListing;

static CliListing *s_listing;

typedef struct FilesExplorerFolder FilesExplorerFolder;

typedef struct
{
    const char *text;
    const char *name;
    const char *path;
    int dir;
    int ignored;
    int archive;
    const char *raw;
    const char *one;
    const char *other;
    VerbaLinea *out;
    const char *root;
    const CellOwner *owner;
    int counting;
    unsigned long long *count;
    unsigned long long *bytes;
    const FilesExplorerFolder *folder;
    unsigned int folder_at;
    unsigned int depth;
    unsigned int *left;
    const CarcerMovableEntry *rows;
} FilesListCtx;
// a text put in the entries' texts: its place
EMBED_INLINE unsigned int cli_entry_text_store(const FilesListCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    if ((s_listing->used + length + 1ULL) > s_listing->text_capacity)
    {
        return 0u;
    }
    // a place is below CLI_FILES_TEXT, which an unsigned int stores
    const unsigned int at = (unsigned int)s_listing->used;
    memor.cpy(&(MemoriaCfg){.dst = s_listing->text + at, .src = args->text, .bytes = length + 1ULL});
    s_listing->used += length + 1ULL;
    return at;
}

// an entry put after the entries
EMBED_INLINE void cli_entry_append(const FilesListCtx *args)
{
    if (s_listing->count >= s_listing->item_capacity)
    {
        return;
    }
    s_listing->item[s_listing->count] =
        (CliEntry){EMBED_CALL(cli_entry_text_store, FilesListCtx, .text = args->name),
                   EMBED_CALL(cli_entry_text_store, FilesListCtx, .text = args->path),
                   args->dir,
                   args->ignored,
                   args->archive,
                   (args->raw != NULL) ? EMBED_CALL(cli_entry_text_store, FilesListCtx, .text = args->raw) : 0u,
                   args->raw != NULL};
    s_listing->count += 1u;
}

// the entries in the order the explorer shows them: folders first, then by name, case aside
EMBED_INLINE void cli_entries_sort(const FilesListCtx *args)
{
    (void)args;
    for (unsigned int at = 1u; at < s_listing->count; at += 1u)
    {
        const CliEntry swap = s_listing->item[at];
        unsigned int place = at;
        while (place > 0u)
        {
            const CliEntry *const before = &s_listing->item[place - 1u];
            const int first =
                (swap.dir && !before->dir) ||
                ((swap.dir == before->dir) &&
                 EMBED_CALL(cli_text_size_one.before_folded, CliTextSizeOneCfg, .one = s_listing->text + swap.name,
                            .other_pointer = s_listing->text + before->name));
            if (!first)
            {
                break;
            }
            s_listing->item[place] = *before;
            place -= 1u;
        }
        s_listing->item[place] = swap;
    }
}

// the entries read into one cell of `owner`, sized to them: the cell, or NULL where the cells cannot meet it
EMBED_INLINE CliFileEntries *cli_entries_alloc(const FilesListCtx *args)
{
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < s_listing->count; at += 1u)
    {
        const CliEntry *const one = &s_listing->item[at];
        bytes += cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->name, .cap = CLI_POOL_BYTES}) +
                 cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->path, .cap = CLI_POOL_BYTES}) + 2ULL;
    }
    // the caller gives the entries back within its call: a cell of the temporary tier
    CliFileEntries *const entries = g_cli_guard->temporary_buf_alloc(
        sizeof(*entries) + (s_listing->count * sizeof(CliFileEntry)) + bytes, args->owner);
    if (entries == NULL)
    {
        return NULL;
    }
    entries->count = s_listing->count;
    entries->entry = (CliFileEntry *)(void *)(entries + 1);
    char *text = (char *)(void *)(entries->entry + s_listing->count);
    for (unsigned int at = 0u; at < s_listing->count; at += 1u)
    {
        const CliEntry *const one = &s_listing->item[at];
        const unsigned long long name =
            cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->name, .cap = CLI_POOL_BYTES}) + 1ULL;
        const unsigned long long full =
            cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->path, .cap = CLI_POOL_BYTES}) + 1ULL;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = s_listing->text + one->name, .bytes = name});
        memor.cpy(&(MemoriaCfg){.dst = text + name, .src = s_listing->text + one->path, .bytes = full});
        entries->entry[at] = (CliFileEntry){text, text + name, one->dir, one->ignored};
        text += name + full;
    }
    return entries;
}

// Each folder that holds the file `path`, the path cut at one of its slashes. Where `counting`, each goes into
// `count`, with the bytes it takes into `bytes`. Else each goes among the listing's folders
EMBED_INLINE void cli_folders_of(const FilesListCtx *args)
{
    const char *const file = args->path;
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_POOL_BYTES});
    for (unsigned long long slash = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = file,
                                               .at = 0ULL, .size = whole, .set = "/");
         slash < whole; slash = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = file,
                                           .at = slash + 1ULL, .size = whole, .set = "/"))
    {
        *args->count += args->counting ? 1ULL : 0ULL;
        *args->bytes += args->counting ? (slash + 1ULL) : 0ULL;
        if (!args->counting)
        {
            EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &s_listing->folders,
                       .path = file, .length = slash);
        }
    }
}

// Every folder of the view's files that holds a file the patterns leave. A first pass counts them, and the list of
// folders comes at that size. A second pass puts them in, and a sort puts them in order
EMBED_INLINE void cli_folders_collect(const FilesListCtx *args)
{
    const char *root = args->root;
    const CliFiles *const files = EMBED_CALL(cli_files_view_internal.view, CliFilesRootPathsCfg, .root = root);
    unsigned long long count = 0ULL;
    unsigned long long bytes = 0ULL;
    for (unsigned int pass = 0u; (files != NULL) && (pass < 2u); pass += 1u)
    {
        if ((pass == 1u) && !EMBED_CALL(cli_files_bytes_internal.work_init, CliFilesBytesCtx,
                                        .files = &s_listing->folders, .count = count, .bytes = bytes))
        {
            EMBED_CALL(cli_files_let_internal.let, CliFilesCfg, .files = files);
            return;
        }
        for (unsigned int index = 0u; index < files->count; index += 1u)
        {
            const char *const file = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = index);
            if (EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = &s_listing->patterns, .path = file,
                           .dir = 0))
            {
                continue;
            }
            EMBED_CALL(cli_folders_of, FilesListCtx, .path = file, .counting = pass == 0u, .count = &count,
                       .bytes = &bytes);
        }
    }
    EMBED_CALL(cli_files_let_internal.let, CliFilesCfg, .files = files);
    (void)EMBED_CALL(cli_files_bytes_internal.sort, CliFilesBytesCtx, .files = &s_listing->folders);
}

// A folder of the tree in the listing now: its path in the tree, git's view, and whether a pattern includes only what
// it names
typedef struct
{
    const char *root;
    const char *folder;
    int git;
    int include_only;
} CliListResult;

static void cli_listing_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    const CliListResult *const list_entries = (const CliListResult *)args->context;
    char full[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {full, sizeof(full), 0ULL};
    mmgr_line_put(&line, list_entries->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return;
    }
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = list_entries->root, .path = full,
               .relative_out = path);
    if ((args->is_folder && (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_POOL_BYTES}))) ||
        EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = &s_listing->patterns, .path = path,
                   .dir = args->is_folder))
    {
        return;
    }
    if (list_entries->include_only && args->is_folder &&
        !EMBED_CALL(cli_files_path_type.contains, CliFilesPathTypeCfg, .files = &s_listing->folders, .path = path) &&
        !EMBED_CALL(cli_patterns_path.include_folder, CliPatternsPathCfg, .patterns = &s_listing->patterns,
                    .path = path))
    {
        return;
    }
    int ignored = 0;
    if (list_entries->git)
    {
        ignored = args->is_folder
                      ? !EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = path)
                      : !EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = path);
    }
    else if (args->is_folder &&
             ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_POOL_BYTES})) ||
              (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "target", .cap = CLI_POOL_BYTES}))))
    {
        return;
    }
    const int archive =
        !args->is_folder && (EMBED_CALL(cli_archive.type_get, CliArchiveCfg, .name = name) != CLI_ARCHIVE_NONE);
    EMBED_CALL(cli_entry_append, FilesListCtx, .name = name, .path = path, .dir = args->is_folder || archive,
               .ignored = ignored, .archive = archive, .raw = NULL);
}

// the listing given back, the folders that hold its files with it, and the lock with it
static void list_release(void)
{
    EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = &s_listing->folders);
    g_cli_guard->persistent_buf_release(s_listing->item, g_cli_editor_owner);
    g_cli_guard->persistent_buf_release(s_listing, g_cli_editor_owner);
    s_listing = NULL;
}

CliFileEntries *cli_files_entries_alloc(const CliFilesEntriesCfg *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    // the listing lives for the one call that reads a folder: a cell of the temporary tier
    s_listing = g_cli_guard->temporary_buf_alloc(sizeof(*s_listing), g_cli_editor_owner);
    if (s_listing == NULL)
    {
        mmgr_line_put(err_msg, "a folder's listing could not be taken");
        return NULL;
    }
    const unsigned long long item_bytes = (unsigned long long)CLI_LIST_MAX * sizeof(*s_listing->item);
    char *const lists = g_cli_guard->temporary_buf_alloc(item_bytes + CLI_FILES_TEXT, g_cli_editor_owner);
    s_listing->item = NULL;
    s_listing->count = 0u;
    s_listing->item_capacity = 0u;
    s_listing->text = NULL;
    s_listing->used = 0ULL;
    s_listing->text_capacity = 0ULL;
    if (lists != NULL)
    {
        s_listing->item = (CliEntry *)(void *)lists;
        s_listing->item_capacity = CLI_LIST_MAX;
        s_listing->text = lists + item_bytes;
        s_listing->text_capacity = CLI_FILES_TEXT;
    }
    s_listing->folders = (CliFiles){NULL, 0u, 0u, NULL, 0ULL, 0ULL};
    CliFileEntries *entries = NULL;
    // an archive, and a folder within one, lists what the archive contains, in its own order
    char file[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_archive_root.split, CliArchiveRootCfg, .root = root, .path = args->dir, .file = file,
                   .inner = inner))
    {
        if (EMBED_CALL(cli_archive_err_msg_root.list, CliArchiveErrMsgRootCfg, .root = root, .path = args->dir,
                       .found = &s_listing->children, .err_msg = err_msg))
        {
            for (unsigned int at = 0u; at < s_listing->children.count; at += 1u)
            {
                const CliChild *const child = &s_listing->children.item[at];
                const char *const text = s_listing->children.text;
                EMBED_CALL(cli_entry_append, FilesListCtx, .name = text + child->name, .path = text + child->path,
                           .dir = child->dir, .ignored = 0, .archive = child->archive,
                           .raw = child->has_raw ? (text + child->raw) : NULL);
            }
            entries = EMBED_CALL(cli_entries_alloc, FilesListCtx, .owner = args->owner);
        }
        list_release();
        return entries;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = args->dir, .joined = folder,
                    .err_msg = err_msg))
    {
        list_release();
        return NULL;
    }
    EMBED_CALL(cli_patterns_part.get, CliPatternsPartCfg, .part = CLI_PART_EXPLORER,
               .patterns_out = &s_listing->patterns);
    const int include_only = EMBED_CALL(cli_patterns.include_only, CliPatternsCfg, .patterns = &s_listing->patterns);
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = root);
    if (include_only)
    {
        EMBED_CALL(cli_folders_collect, FilesListCtx, .root = root);
    }
    // a tree git cannot read marks nothing as left out
    CliListResult list_entries = {root, folder, cli_tree_git(), include_only};
    const int read = EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder,
                                .each = cli_listing_each, .context_bytess = &list_entries);
    if (!read)
    {
        mmgr_line_put(err_msg, args->dir);
        mmgr_line_put(err_msg, ": it could not be read");
        list_release();
        return NULL;
    }
    EMBED_CALL(cli_entries_sort, FilesListCtx, .owner = NULL);
    entries = EMBED_CALL(cli_entries_alloc, FilesListCtx, .owner = args->owner);
    list_release();
    return entries;
}

int cli_files_entries_each(const CliFilesEntriesCfg *args)
{
    CliFileEntries *const entries = cli_files_entries_alloc(&(CliFilesEntriesCfg){
        .root = args->root, .dir = args->dir, .owner = g_cli_editor_owner, .err_msg = args->err_msg});
    if (entries == NULL)
    {
        return 0;
    }
    // the entries lie first in the cell, and each name and path after them with its 0
    const unsigned long long rows_bytes = (unsigned long long)entries->count * sizeof(CliFileListed);
    unsigned long long text_bytes = 0ULL;
    for (unsigned int at = 0u; at < entries->count; at += 1u)
    {
        text_bytes += cellul.len(&(CatenaFinitaCfg){.src = entries->entry[at].name, .cap = CLI_PATH_MAX_LEN}) +
                      cellul.len(&(CatenaFinitaCfg){.src = entries->entry[at].path, .cap = CLI_PATH_MAX_LEN}) + 2ULL;
    }
    const CarcerMovableEntry *const cell =
        (entries->count != 0u) ? g_cli_guard->movable_buf_alloc(rows_bytes + text_bytes, g_cli_editor_owner) : NULL;
    if ((entries->count != 0u) && (cell == NULL))
    {
        g_cli_guard->persistent_buf_release(entries, g_cli_editor_owner);
        mmgr_line_put(args->err_msg, "a folder's entries could not be taken");
        return 0;
    }
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < entries->count; at += 1u)
    {
        const CliFileEntry *const entry = &entries->entry[at];
        const unsigned long long name =
            cellul.len(&(CatenaFinitaCfg){.src = entry->name, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
        const unsigned long long path =
            cellul.len(&(CatenaFinitaCfg){.src = entry->path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
        const CliFileListed listed = {used, used + name, entry->dir, entry->ignored};
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = cell, .offset = (unsigned long long)at * sizeof(listed), .src = &listed, .bytes = sizeof(listed)});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = rows_bytes + used, .src = entry->name, .bytes = name});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = rows_bytes + used + name, .src = entry->path, .bytes = path});
        used += name + path;
    }
    const CliFilesListed listed = {cell, entries->count, rows_bytes};
    // the module lets its work go before the entries go to the caller
    g_cli_guard->persistent_buf_release(entries, g_cli_editor_owner);
    args->each(&(CliFilesListedEachCfg){.context = args->context, .listed = &listed});
    if (cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(cell, g_cli_editor_owner);
    }
    return 1;
}

// the text at `at` of the listed cell read into `out`, CLI_PATH_MAX_LEN bytes, with a 0 after it
EMBED_INLINE void listed_text_read(const CliFilesListed *listed, unsigned long long at, char *out)
{
    const unsigned long long length =
        g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = listed->cell, .offset = listed->text_at + at});
    const unsigned long long bytes = (length < CLI_PATH_MAX_LEN - 1ULL) ? length : CLI_PATH_MAX_LEN - 1ULL;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = listed->cell, .offset = listed->text_at + at, .dst = out, .bytes = bytes});
    out[bytes] = '\0';
}

int cli_files_listed_at(const CliFilesListedAtCfg *args)
{
    const CliFilesListed *const listed = args->listed;
    args->name_out[0] = '\0';
    args->path_out[0] = '\0';
    if ((listed->cell == NULL) || (args->index >= listed->count))
    {
        return 0;
    }
    CliFileListed one = {0ULL, 0ULL, 0, 0};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = listed->cell,
                                                            .offset = (unsigned long long)args->index * sizeof(one),
                                                            .dst = &one,
                                                            .bytes = sizeof(one)});
    listed_text_read(listed, one.name_at, args->name_out);
    listed_text_read(listed, one.path_at, args->path_out);
    *args->entry_out = (CliFileEntry){args->name_out, args->path_out, one.dir, one.ignored};
    return 1;
}

// One folder the explorer listed. It holds its path from the tree's top, and the entry of its parent's listing that
// names it, empty for the top folder. Then its entries as cli_files_entries_alloc read them, and whether it is open
struct FilesExplorerFolder
{
    const char *dir;
    CliFileEntries *entries;
    int open;
};

// The explorer. It holds the tree it lists, and the folders listed, the top folder first. They lie in a movable cell
// that grows as the explorer lists more folders. Last, how many there are and the cell's capacity
static const char *s_explorer_root;
static const CarcerMovableEntry *s_explorer;
static unsigned int s_explorer_folders;
static unsigned int s_explorer_capacity;
// no folder, where a search looks for a folder by its path
#define FILES_EXPLORER_NONE 0xffffffffu
// how many times the watch marked the explorer stale, counted on the watch's thread and read on the window's
static embed_atomic_u32 s_explorer_stale;
// A row of the explorer: its folder's place, its entry's place in that folder, its depth, and whether it opens as a
// folder
typedef struct
{
    unsigned int folder;
    unsigned int entry;
    unsigned int depth;
    int open;
} FilesExplorerRow;
// the rows in a movable cell of their exact count, how many there are, and whether the listing changed after their
// last lay
static const CarcerMovableEntry *s_explorer_rows;
static unsigned int s_explorer_row_count;
static int s_explorer_rows_stale;

// the place of the listing of the folder `dir`, or FILES_EXPLORER_NONE where it was never listed
static unsigned int explorer_folder(const FilesListCtx *args)
{
    for (unsigned int at = 0u; at < s_explorer_folders; at += 1u)
    {
        FilesExplorerFolder folder = {NULL, NULL, 0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_explorer, .offset = (embed_word)at * sizeof(folder), .dst = &folder, .bytes = sizeof(folder)});
        if (cellul.eq(&(CatenaFinitaCfg){.src = folder.dir, .other = args->path, .cap = CLI_PATH_MAX_LEN}))
        {
            return at;
        }
    }
    return FILES_EXPLORER_NONE;
}

// The folder `dir` listed for the explorer, its path valid while its parent's listing is. Where the folders' cell is
// full, it grows to twice its capacity. Returns the listing's place, or FILES_EXPLORER_NONE where the read fails or
// the cells refuse the growth
static unsigned int explorer_list(const FilesListCtx *args)
{
    if (s_explorer_root == NULL)
    {
        return FILES_EXPLORER_NONE;
    }
    if (s_explorer_folders == s_explorer_capacity)
    {
        const unsigned int capacity = (s_explorer_capacity != 0u) ? s_explorer_capacity * 2u : 16u;
        const CarcerMovableEntry *const grown = g_cli_guard->movable_buf_alloc(
            (unsigned long long)capacity * sizeof(FilesExplorerFolder), g_cli_editor_owner);
        if (grown == NULL)
        {
            return FILES_EXPLORER_NONE;
        }
        for (unsigned int at = 0u; at < s_explorer_folders; at += 1u)
        {
            FilesExplorerFolder folder = {NULL, NULL, 0};
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_explorer,
                                                                    .offset = (embed_word)at * sizeof(folder),
                                                                    .dst = &folder,
                                                                    .bytes = sizeof(folder)});
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = grown, .offset = (embed_word)at * sizeof(folder), .src = &folder, .bytes = sizeof(folder)});
        }
        if (s_explorer != NULL)
        {
            (void)g_cli_guard->movable_buf_release(s_explorer, g_cli_editor_owner);
        }
        s_explorer = grown;
        s_explorer_capacity = capacity;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliFileEntries *const entries =
        EMBED_CALL(cli_files_listing.entries_alloc, CliFilesEntriesCfg, .root = s_explorer_root, .dir = args->path,
                   .owner = g_cli_editor_owner, .err_msg = &err_msg);
    if (entries == NULL)
    {
        return FILES_EXPLORER_NONE;
    }
    const FilesExplorerFolder folder = {args->path, entries, 0};
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_explorer,
                                                             .offset = (embed_word)s_explorer_folders * sizeof(folder),
                                                             .src = &folder,
                                                             .bytes = sizeof(folder)});
    s_explorer_folders += 1u;
    s_explorer_rows_stale = 1;
    return s_explorer_folders - 1u;
}

void cli_files_explorer_show(const CliFilesExplorerCfg *args)
{
    if ((s_explorer_root == NULL) && (args->root != NULL))
    {
        s_explorer_root = args->root;
        const unsigned int top = EMBED_CALL(explorer_list, FilesListCtx, .path = "");
        if (top != FILES_EXPLORER_NONE)
        {
            FilesExplorerFolder folder = {NULL, NULL, 0};
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_explorer,
                                                                    .offset = (embed_word)top * sizeof(folder),
                                                                    .dst = &folder,
                                                                    .bytes = sizeof(folder)});
            folder.open = 1;
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_explorer,
                                                                     .offset = (embed_word)top * sizeof(folder),
                                                                     .src = &folder,
                                                                     .bytes = sizeof(folder)});
        }
    }
}

void cli_files_explorer_end(void)
{
    for (unsigned int at = 0u; at < s_explorer_folders; at += 1u)
    {
        FilesExplorerFolder folder = {NULL, NULL, 0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_explorer, .offset = (embed_word)at * sizeof(folder), .dst = &folder, .bytes = sizeof(folder)});
        g_cli_guard->persistent_buf_release(folder.entries, g_cli_editor_owner);
    }
    if (s_explorer != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_explorer, g_cli_editor_owner);
    }
    if (s_explorer_rows != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_explorer_rows, g_cli_editor_owner);
    }
    s_explorer = NULL;
    s_explorer_folders = 0u;
    s_explorer_capacity = 0u;
    s_explorer_root = NULL;
    s_explorer_rows = NULL;
    s_explorer_row_count = 0u;
    s_explorer_rows_stale = 0;
}

// counts the rows of the folder `folder`, at `folder_at`, and of the open folders under it into `*left`, `depth`
// deep. Where `rows` is not NULL, it writes each row at its count into `rows`
static void explorer_walk(const FilesListCtx *args)
{
    for (unsigned int at = 0u; at < args->folder->entries->count; at += 1u)
    {
        const CliFileEntry *const entry = &args->folder->entries->entry[at];
        const unsigned int under_at =
            entry->dir ? EMBED_CALL(explorer_folder, FilesListCtx, .path = entry->path) : FILES_EXPLORER_NONE;
        FilesExplorerFolder under = {NULL, NULL, 0};
        if (under_at != FILES_EXPLORER_NONE)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_explorer,
                                                                    .offset = (embed_word)under_at * sizeof(under),
                                                                    .dst = &under,
                                                                    .bytes = sizeof(under)});
        }
        const int open = (under_at != FILES_EXPLORER_NONE) && under.open;
        if (args->rows != NULL)
        {
            const FilesExplorerRow laid = {args->folder_at, at, args->depth, open};
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = args->rows,
                                                                     .offset = (embed_word)*args->left * sizeof(laid),
                                                                     .src = &laid,
                                                                     .bytes = sizeof(laid)});
        }
        *args->left += 1u;
        if (open)
        {
            EMBED_CALL(explorer_walk, FilesListCtx, .folder = &under, .folder_at = under_at, .depth = args->depth + 1u,
                       .left = args->left, .rows = args->rows);
        }
    }
}

// lays the rows again after the listing changes. One walk counts them, the explorer takes a cell of that count, and a
// second walk writes them. A refused cell leaves the rows stale, with none
static void explorer_rows_lay(void)
{
    if (!s_explorer_rows_stale)
    {
        return;
    }
    s_explorer_rows_stale = 0;
    if (s_explorer_rows != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_explorer_rows, g_cli_editor_owner);
        s_explorer_rows = NULL;
    }
    s_explorer_row_count = 0u;
    if (s_explorer_folders == 0u)
    {
        return;
    }
    FilesExplorerFolder top = {NULL, NULL, 0};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_explorer, .offset = (embed_word)0u * sizeof(top), .dst = &top, .bytes = sizeof(top)});
    unsigned int count = 0u;
    EMBED_CALL(explorer_walk, FilesListCtx, .folder = &top, .folder_at = 0u, .depth = 0u, .left = &count);
    if (count == 0u)
    {
        return;
    }
    const CarcerMovableEntry *const rows =
        g_cli_guard->movable_buf_alloc((unsigned long long)count * sizeof(FilesExplorerRow), g_cli_editor_owner);
    if (rows == NULL)
    {
        s_explorer_rows_stale = 1;
        return;
    }
    unsigned int written = 0u;
    EMBED_CALL(explorer_walk, FilesListCtx, .folder = &top, .folder_at = 0u, .depth = 0u, .left = &written,
               .rows = rows);
    s_explorer_rows = rows;
    s_explorer_row_count = count;
}

int cli_files_explorer_row(const CliFilesExplorerCfg *args)
{
    explorer_rows_lay();
    if (args->at >= s_explorer_row_count)
    {
        return 0;
    }
    if (args->row == NULL)
    {
        return 1;
    }
    FilesExplorerRow laid = {0u, 0u, 0u, 0};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_explorer_rows, .offset = (embed_word)args->at * sizeof(laid), .dst = &laid, .bytes = sizeof(laid)});
    FilesExplorerFolder folder = {NULL, NULL, 0};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_explorer,
                                                            .offset = (embed_word)laid.folder * sizeof(folder),
                                                            .dst = &folder,
                                                            .bytes = sizeof(folder)});
    const CliFileEntry *const entry = &folder.entries->entry[laid.entry];
    *args->row = (CliExplorerRow){entry->path, entry->name, laid.depth, entry->dir, entry->ignored, laid.open};
    return 1;
}

unsigned int cli_files_explorer_rows(void)
{
    explorer_rows_lay();
    return s_explorer_row_count;
}

int cli_files_explorer_open(const CliFilesExplorerCfg *args)
{
    CliExplorerRow row;
    for (unsigned int at = 0u; cli_files_explorer_row(&(CliFilesExplorerCfg){.at = at, .row = &row}); at += 1u)
    {
        if (row.dir && cellul.eq(&(CatenaFinitaCfg){.src = row.path, .other = args->path, .cap = CLI_PATH_MAX_LEN}))
        {
            return cli_files_explorer_press(&(CliFilesExplorerCfg){.at = at});
        }
    }
    return 0;
}

// the bytes that take a copy of the open folders' paths as the explorer lists again
#define FILES_EXPLORER_OPEN_TEXT (1u << 20u)

void cli_files_explorer_refresh(void)
{
    const char *const root = s_explorer_root;
    if (root == NULL)
    {
        return;
    }
    // the open folders' paths copied, the top's aside, each ended by a NUL, those past the bytes closing
    char *const open = g_cli_guard->temporary_buf_alloc(FILES_EXPLORER_OPEN_TEXT, g_cli_editor_owner);
    unsigned int used = 0u;
    unsigned int count = 0u;
    for (unsigned int at = 1u; (open != NULL) && (at < s_explorer_folders); at += 1u)
    {
        FilesExplorerFolder folder = {NULL, NULL, 0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_explorer, .offset = (embed_word)at * sizeof(folder), .dst = &folder, .bytes = sizeof(folder)});
        const char *const dir = folder.dir;
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_PATH_MAX_LEN});
        if (folder.open && (length + 1ULL <= (unsigned long long)(FILES_EXPLORER_OPEN_TEXT - used)))
        {
            memor.cpy(&(MemoriaCfg){.dst = open + used, .src = dir, .bytes = length + 1ULL});
            // an unsigned int counts a path that fits the bytes
            used += (unsigned int)length + 1u;
            count += 1u;
        }
    }
    cli_files_explorer_end();
    cli_files_explorer_show(&(CliFilesExplorerCfg){.root = root});
    // the folders opened again in their order, a parent before the folders under it
    const char *path = open;
    for (unsigned int at = 0u; (open != NULL) && (at < count); at += 1u)
    {
        (void)cli_files_explorer_open(&(CliFilesExplorerCfg){.path = path});
        path += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    if (open != NULL)
    {
        g_cli_guard->persistent_buf_release(open, g_cli_editor_owner);
    }
}

void cli_files_explorer_stale(void)
{
    (void)embed_atomic_u32_fetch_add(&s_explorer_stale, 1u);
}

int cli_files_explorer_changed(void)
{
    static unsigned int s_seen;
    const unsigned int marks = embed_atomic_u32_load(&s_explorer_stale);
    const int changed = marks != s_seen;
    s_seen = marks;
    return changed;
}

void cli_files_explorer_collapse(void)
{
    for (unsigned int at = 1u; at < s_explorer_folders; at += 1u)
    {
        FilesExplorerFolder folder = {NULL, NULL, 0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_explorer, .offset = (embed_word)at * sizeof(folder), .dst = &folder, .bytes = sizeof(folder)});
        folder.open = 0;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = s_explorer, .offset = (embed_word)at * sizeof(folder), .src = &folder, .bytes = sizeof(folder)});
    }
    s_explorer_rows_stale = 1;
}

int cli_files_explorer_menu(const CliFilesExplorerCfg *args)
{
    if (s_explorer_folders == 0u)
    {
        return 0;
    }
    cli_menus_own_begin();
    CliExplorerRow row;
    if (!cli_files_explorer_row(&(CliFilesExplorerCfg){.at = args->at, .row = &row}))
    {
        // off the rows the pane's own, the list's Explorer popup
        return cli_menus_popup(&(CliMenusCfg){.title = "Explorer", .item_args = ""});
    }
    // The rows the list's Files popup gives, each run with the row's path, which is in the listing while the menu
    // is open. An open folder's first row closes it
    if (!cli_menus_popup(&(CliMenusCfg){.title = "Files", .item_args = row.path}))
    {
        return 0;
    }
    if (row.dir && row.open)
    {
        cli_menus_own_relabel(&(CliMenusCfg){.at = 0u, .label = "Close"});
    }
    return 1;
}

int cli_files_explorer_press(const CliFilesExplorerCfg *args)
{
    CliExplorerRow row;
    if (!cli_files_explorer_row(&(CliFilesExplorerCfg){.at = args->at, .row = &row}) || !row.dir)
    {
        return 0;
    }
    const unsigned int listed = EMBED_CALL(explorer_folder, FilesListCtx, .path = row.path);
    if (listed != FILES_EXPLORER_NONE)
    {
        FilesExplorerFolder folder = {NULL, NULL, 0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_explorer,
                                                                .offset = (embed_word)listed * sizeof(folder),
                                                                .dst = &folder,
                                                                .bytes = sizeof(folder)});
        folder.open = !folder.open;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_explorer,
                                                                 .offset = (embed_word)listed * sizeof(folder),
                                                                 .src = &folder,
                                                                 .bytes = sizeof(folder)});
        s_explorer_rows_stale = 1;
        return 1;
    }
    const unsigned int opened = EMBED_CALL(explorer_list, FilesListCtx, .path = row.path);
    if (opened != FILES_EXPLORER_NONE)
    {
        FilesExplorerFolder folder = {NULL, NULL, 0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_explorer,
                                                                .offset = (embed_word)opened * sizeof(folder),
                                                                .dst = &folder,
                                                                .bytes = sizeof(folder)});
        folder.open = 1;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_explorer,
                                                                 .offset = (embed_word)opened * sizeof(folder),
                                                                 .src = &folder,
                                                                 .bytes = sizeof(folder)});
        s_explorer_rows_stale = 1;
    }
    return 1;
}
