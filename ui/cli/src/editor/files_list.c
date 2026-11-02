// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_list.c: a folder's entries, less what the explorer's patterns hide, each marked where the tree's ignore files
// leave it out; an archive, and a folder within one, listing what the archive contains
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "archive.h"
#include "files_internal.h"

// the most entries a folder lists
#define CLI_LIST_MOST 65536u

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
    unsigned int item_room;
    char *text;
    unsigned long long used;
    unsigned long long text_room;
    CliFiles folders;
    CliPatterns patterns;
    CliChildren children;
} CliListing;

static CliListing *s_listing;
static CliLock s_lock = CLI_LOCK_INIT;

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
} FilesListCtx;
// a text put in the entries' texts: its place
EMBED_INLINE unsigned int cli_entry_text_store(const FilesListCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    if ((s_listing->used + length + 1ull) > s_listing->text_room)
    {
        return 0u;
    }
    // a place is below CLI_FILES_TEXT, which an unsigned int stores
    const unsigned int at = (unsigned int)s_listing->used;
    memor.cpy(&(MemoriaCfg){.dst = s_listing->text + at, .src = args->text, .bytes = length + 1ull});
    s_listing->used += length + 1ull;
    return at;
}

// an entry put after the entries
EMBED_INLINE void cli_entry_put(const FilesListCtx *args)
{
    if (s_listing->count >= s_listing->item_room)
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

// whether a name sorts before another, ASCII's case aside
EMBED_INLINE int cli_before(const FilesListCtx *args)
{
    const char *one = args->one;
    const char *other = args->other;
    for (;; one += 1, other += 1)
    {
        const char left = ((one[0] >= 'A') && (one[0] <= 'Z')) ? (char)(one[0] + 32) : one[0];
        const char right = ((other[0] >= 'A') && (other[0] <= 'Z')) ? (char)(other[0] + 32) : other[0];
        if ((left != right) || (left == '\0'))
        {
            return (unsigned char)left < (unsigned char)right;
        }
    }
}

// the entries written as the window reads them, folders first, then by name, case aside
EMBED_INLINE void cli_entries_write(const FilesListCtx *args)
{
    VerbaLinea *out = args->out;
    for (unsigned int at = 1u; at < s_listing->count; at += 1u)
    {
        const CliEntry swap = s_listing->item[at];
        unsigned int place = at;
        while (place > 0u)
        {
            const CliEntry *const before = &s_listing->item[place - 1u];
            const int first =
                (swap.dir && !before->dir) ||
                ((swap.dir == before->dir) && EMBED_CALL(cli_before, FilesListCtx, .one = s_listing->text + swap.name,
                                                         .other = s_listing->text + before->name));
            if (!first)
            {
                break;
            }
            s_listing->item[place] = *before;
            place -= 1u;
        }
        s_listing->item[place] = swap;
    }
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < s_listing->count; at += 1u)
    {
        const CliEntry *const one = &s_listing->item[at];
        mmgr_line_put(out, (at != 0u) ? ",{\"name\":" : "{\"name\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = s_listing->text + one->name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->name, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"path\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = s_listing->text + one->path,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->path, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, one->dir ? ",\"dir\":true" : ",\"dir\":false");
        mmgr_line_put(out, one->ignored ? ",\"ignored\":true" : ",\"ignored\":false");
        mmgr_line_put(out, one->archive ? ",\"archive\":true" : ",\"archive\":false");
        if (one->has_raw)
        {
            mmgr_line_put(out, ",\"raw\":");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                       .text = s_listing->text + one->raw,
                       .length =
                           cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->raw, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
}

// every folder of the view's files that contains a file the patterns leave, counted on a first pass, the containing
// folders made at that size, put on a second, and sorted
EMBED_INLINE void cli_folders_collect(const FilesListCtx *args)
{
    const char *root = args->root;
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root);
    unsigned long long count = 0ull;
    unsigned long long bytes = 0ull;
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
            const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL});
            for (unsigned long long slash = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                       .text = file, .at = 0ull, .size = whole, .set = "/");
                 slash < whole; slash = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = file, .at = slash + 1ull, .size = whole, .set = "/"))
            {
                count += (pass == 0u) ? 1ull : 0ull;
                bytes += (pass == 0u) ? (slash + 1ull) : 0ull;
                if (pass == 1u)
                {
                    EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &s_listing->folders,
                               .path = file, .length = slash);
                }
            }
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
} CliListed;

static void cli_listed_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    const CliListed *const listed = (const CliListed *)args->context;
    char full[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {full, sizeof(full), 0ull};
    mmgr_line_put(&line, listed->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = listed->root, .path = full,
               .relative_out = path);
    if ((args->is_folder && (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL}))) ||
        EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = &s_listing->patterns, .path = path,
                   .dir = args->is_folder))
    {
        return;
    }
    if (listed->include_only && args->is_folder &&
        !EMBED_CALL(cli_files_path_kind.contains, CliFilesPathKindCfg, .files = &s_listing->folders, .path = path) &&
        !EMBED_CALL(cli_patterns_path.include_folder, CliPatternsPathCfg, .patterns = &s_listing->patterns,
                    .path = path))
    {
        return;
    }
    int ignored = 0;
    if (listed->git)
    {
        ignored = args->is_folder
                      ? !EMBED_CALL(cli_symbols_path_name.tree_has_folder, CliSymbolsPathNameCfg, .path = path)
                      : !EMBED_CALL(cli_symbols_path_name.tree_has_file, CliSymbolsPathNameCfg, .path = path);
    }
    else if (args->is_folder &&
             ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL})) ||
              (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "target", .cap = CLI_CELLS_POOL}))))
    {
        return;
    }
    const int archive =
        !args->is_folder && (EMBED_CALL(cli_archive.kind_get, CliArchiveCfg, .name = name) != CLI_ARCHIVE_NONE);
    EMBED_CALL(cli_entry_put, FilesListCtx, .name = name, .path = path, .dir = args->is_folder || archive,
               .ignored = ignored, .archive = archive, .raw = NULL);
}

// the listing given back, its containing folders with it, and the lock with it
static void list_release(void)
{
    EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = &s_listing->folders);
    g_cli_cells->persistent_buf_release(s_listing->item, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(s_listing, &g_cli_editor_cells);
    s_listing = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

int cli_files_list(const CliFilesRootOutDirCfg *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_listing = g_cli_cells->persistent_buf_alloc(sizeof(*s_listing), &g_cli_editor_cells);
    if (s_listing == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        mmgr_line_put(err_msg, "a folder's listing could not be taken");
        return 0;
    }
    const unsigned long long item_bytes = (unsigned long long)CLI_LIST_MOST * sizeof(*s_listing->item);
    char *const lists = g_cli_cells->temporary_buf_alloc(item_bytes + CLI_FILES_TEXT, &g_cli_editor_cells);
    s_listing->item = NULL;
    s_listing->count = 0u;
    s_listing->item_room = 0u;
    s_listing->text = NULL;
    s_listing->used = 0ull;
    s_listing->text_room = 0ull;
    if (lists != NULL)
    {
        s_listing->item = (CliEntry *)(void *)lists;
        s_listing->item_room = CLI_LIST_MOST;
        s_listing->text = lists + item_bytes;
        s_listing->text_room = CLI_FILES_TEXT;
    }
    s_listing->folders = (CliFiles){NULL, 0u, 0u, NULL, 0ull, 0ull};
    // an archive, and a folder within one, lists what the archive contains
    char file[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_archive_root.split, CliArchiveRootCfg, .root = root, .path = args->dir, .file = file,
                   .inner = inner))
    {
        const int listed = EMBED_CALL(cli_archive_err_msg_root.list, CliArchiveErrMsgRootCfg, .root = root,
                                      .path = args->dir, .found = &s_listing->children, .err_msg = err_msg);
        for (unsigned int at = 0u; listed && (at < s_listing->children.count); at += 1u)
        {
            const CliChild *const child = &s_listing->children.item[at];
            const char *const text = s_listing->children.text;
            EMBED_CALL(cli_entry_put, FilesListCtx, .name = text + child->name, .path = text + child->path,
                       .dir = child->dir, .ignored = 0, .archive = child->archive,
                       .raw = child->has_raw ? (text + child->raw) : NULL);
        }
        if (listed)
        {
            // the archive lists its children in its own order, retained
            mmgr_line_ch(args->out, '[');
            for (unsigned int at = 0u; at < s_listing->count; at += 1u)
            {
                const CliEntry *const one = &s_listing->item[at];
                mmgr_line_put(args->out, (at != 0u) ? ",{\"name\":" : "{\"name\":");
                EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                           .text = s_listing->text + one->name,
                           .length = cellul.len(
                               &(CatenaFinitaCfg){.src = s_listing->text + one->name, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(args->out, ",\"path\":");
                EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                           .text = s_listing->text + one->path,
                           .length = cellul.len(
                               &(CatenaFinitaCfg){.src = s_listing->text + one->path, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(args->out,
                              one->dir ? ",\"dir\":true,\"ignored\":false" : ",\"dir\":false,\"ignored\":false");
                mmgr_line_put(args->out, one->archive ? ",\"archive\":true" : ",\"archive\":false");
                if (one->has_raw)
                {
                    mmgr_line_put(args->out, ",\"raw\":");
                    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                               .text = s_listing->text + one->raw,
                               .length = cellul.len(
                                   &(CatenaFinitaCfg){.src = s_listing->text + one->raw, .cap = CLI_CELLS_POOL}));
                }
                mmgr_line_ch(args->out, '}');
            }
            mmgr_line_ch(args->out, ']');
        }
        list_release();
        return listed;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = args->dir, .joined = folder,
                    .err_msg = err_msg))
    {
        list_release();
        return 0;
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
    CliListed listed = {root, folder, cli_tree_git(), include_only};
    const int read = EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder,
                                .each = cli_listed_each, .context_bytess = &listed);
    if (!read)
    {
        mmgr_line_put(err_msg, args->dir);
        mmgr_line_put(err_msg, ": it could not be read");
        list_release();
        return 0;
    }
    EMBED_CALL(cli_entries_write, FilesListCtx, .out = args->out);
    list_release();
    return 1;
}
