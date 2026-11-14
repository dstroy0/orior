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

// the most entries a folder lists
#define CLI_LIST_MAX 65536u

// An entry of a folder: its name and path, places in the texts, whether it is a folder, whether the tree's ignore
// files leave it out, whether it is an archive, which opens as a folder, and its name's own bytes in hex where they are
// not UTF-8 by the archive's word
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

// The entries of the folder being listed and their texts, in one cell at their most, and the folders a file the
// patterns leave sits in, a list made of the files cellblock, under the lock
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

// each folder containing the file `path`, the path cut at one of its slashes: counted into `count`, with the bytes it
// takes into `bytes`, where `counting`, else put among the listing's folders
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

// every folder of the view's files that contains a file the patterns leave, counted on a first pass, the containing
// folders made at that size, put on a second, and sorted
EMBED_INLINE void cli_folders_collect(const FilesListCtx *args)
{
    const char *root = args->root;
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root);
    unsigned long long count = 0ULL;
    unsigned long long bytes = 0ULL;
    for (unsigned int pass = 0u; (files != NULL) && (pass < 2u); pass += 1u)
    {
        if ((pass == 1u) && !EMBED_CALL(cli_files_bytes_internal.work_init, CliFilesBytesCtx,
                                        .files = &s_listing->folders, .count = count, .bytes = bytes))
        {
            EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
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
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
    (void)EMBED_CALL(cli_files_bytes_internal.sort, CliFilesBytesCtx, .files = &s_listing->folders);
}

// A folder of the tree being listed: its path in the tree, git's view, and whether a pattern includes only what it
// names
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

// the listing given back, its containing folders with it, and the lock with it
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

// One folder the explorer listed: its path from the tree's top, the entry of its parent's listing that names it, empty
// for the top folder; its entries as cli_files_entries_alloc read them; and whether it is open
typedef struct
{
    const char *dir;
    CliFileEntries *entries;
    int open;
} FilesExplorerFolder;

// The explorer: the tree it lists; the folders listed, the top folder first, and how many
static const char *s_explorer_root;
static FilesExplorerFolder s_explorer[CLI_FILES_EXPLORER_FOLDERS];
static unsigned int s_explorer_folders;

// the listing of the folder `dir`, or NULL where it was never listed
static FilesExplorerFolder *explorer_folder(const char *dir)
{
    for (unsigned int at = 0u; at < s_explorer_folders; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_explorer[at].dir, .other = dir, .cap = CLI_PATH_MAX_LEN}))
        {
            return &s_explorer[at];
        }
    }
    return NULL;
}

// the folder `dir` listed for the explorer, its path standing as long as its parent's listing does: the listing, or
// NULL where it could not be read or the explorer lists no more folders
static FilesExplorerFolder *explorer_list(const char *dir)
{
    if ((s_explorer_root == NULL) || (s_explorer_folders == CLI_FILES_EXPLORER_FOLDERS))
    {
        return NULL;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliFileEntries *const entries =
        EMBED_CALL(cli_files_listing.entries_alloc, CliFilesEntriesCfg, .root = s_explorer_root, .dir = dir,
                   .owner = g_cli_editor_owner, .err_msg = &err_msg);
    if (entries == NULL)
    {
        return NULL;
    }
    s_explorer[s_explorer_folders] = (FilesExplorerFolder){dir, entries, 0};
    s_explorer_folders += 1u;
    return &s_explorer[s_explorer_folders - 1u];
}

void cli_files_explorer_show(const char *root)
{
    if ((s_explorer_root == NULL) && (root != NULL))
    {
        s_explorer_root = root;
        FilesExplorerFolder *const top = explorer_list("");
        if (top != NULL)
        {
            top->open = 1;
        }
    }
}

void cli_files_explorer_end(void)
{
    for (unsigned int at = 0u; at < s_explorer_folders; at += 1u)
    {
        g_cli_guard->persistent_buf_release(s_explorer[at].entries, g_cli_editor_owner);
    }
    s_explorer_folders = 0u;
    s_explorer_root = NULL;
}

// the row `*left` rows on in the folder `folder` and the open folders under it, `depth` deep, read into `row`: 1 where
// it stood among them, else 0 with `*left` less the rows they show
static int explorer_walk(const FilesExplorerFolder *folder, unsigned int depth, unsigned int *left, CliExplorerRow *row)
{
    for (unsigned int at = 0u; at < folder->entries->count; at += 1u)
    {
        const CliFileEntry *const entry = &folder->entries->entry[at];
        const FilesExplorerFolder *const under = entry->dir ? explorer_folder(entry->path) : NULL;
        const int open = (under != NULL) && under->open;
        if (*left == 0u)
        {
            if (row != NULL)
            {
                *row = (CliExplorerRow){entry->path, entry->name, depth, entry->dir, entry->ignored, open};
            }
            return 1;
        }
        *left -= 1u;
        if (open && explorer_walk(under, depth + 1u, left, row))
        {
            return 1;
        }
    }
    return 0;
}

int cli_files_explorer_row(unsigned int at, CliExplorerRow *row)
{
    unsigned int left = at;
    return (s_explorer_folders != 0u) && explorer_walk(&s_explorer[0], 0u, &left, row);
}

unsigned int cli_files_explorer_rows(void)
{
    unsigned int count = 0u;
    while (cli_files_explorer_row(count, NULL))
    {
        count += 1u;
    }
    return count;
}

int cli_files_explorer_open(const char *path)
{
    CliExplorerRow row;
    for (unsigned int at = 0u; cli_files_explorer_row(at, &row); at += 1u)
    {
        if (row.dir && cellul.eq(&(CatenaFinitaCfg){.src = row.path, .other = path, .cap = CLI_PATH_MAX_LEN}))
        {
            return cli_files_explorer_press(at);
        }
    }
    return 0;
}

// the bytes the open folders' paths are copied into as the explorer lists again
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
        const char *const dir = s_explorer[at].dir;
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_PATH_MAX_LEN});
        if (s_explorer[at].open && (length + 1ULL <= (unsigned long long)(FILES_EXPLORER_OPEN_TEXT - used)))
        {
            memor.cpy(&(MemoriaCfg){.dst = open + used, .src = dir, .bytes = length + 1ULL});
            // a path that fits the bytes is counted by an unsigned int
            used += (unsigned int)length + 1u;
            count += 1u;
        }
    }
    cli_files_explorer_end();
    cli_files_explorer_show(root);
    // the folders opened again in their order, a parent before the folders under it
    const char *path = open;
    for (unsigned int at = 0u; (open != NULL) && (at < count); at += 1u)
    {
        (void)cli_files_explorer_open(path);
        path += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    if (open != NULL)
    {
        g_cli_guard->persistent_buf_release(open, g_cli_editor_owner);
    }
}

void cli_files_explorer_collapse(void)
{
    for (unsigned int at = 1u; at < s_explorer_folders; at += 1u)
    {
        s_explorer[at].open = 0;
    }
}

int cli_files_explorer_menu(unsigned int at)
{
    if (s_explorer_folders == 0u)
    {
        return 0;
    }
    cli_menus_own_begin();
    CliExplorerRow row;
    if (!cli_files_explorer_row(at, &row))
    {
        // off the rows the pane's own, the list's Explorer popup
        return cli_menus_popup("Explorer", "");
    }
    // the rows the list's Files popup gives, each run with the row's path, which stands in the listing while the menu
    // is open; an open folder's first row closes it
    if (!cli_menus_popup("Files", row.path))
    {
        return 0;
    }
    if (row.dir && row.open)
    {
        cli_menus_own_relabel(0u, "Close");
    }
    return 1;
}

int cli_files_explorer_press(unsigned int at)
{
    CliExplorerRow row;
    if (!cli_files_explorer_row(at, &row) || !row.dir)
    {
        return 0;
    }
    FilesExplorerFolder *const folder = explorer_folder(row.path);
    if (folder != NULL)
    {
        folder->open = !folder->open;
        return 1;
    }
    FilesExplorerFolder *const listed = explorer_list(row.path);
    if (listed != NULL)
    {
        listed->open = 1;
    }
    return 1;
}
