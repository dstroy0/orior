// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_list.c: a folder's entries, less what the explorer's patterns hide, each marked where the tree's ignore files
// leave it out; an archive, and a folder within one, listing what the archive contains
#include "../command_line/cells.h"
#include "../text/json_tree.h"
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

// the entries and the bytes of their texts a listing first contains, each doubled as it fills
#define CLI_LIST_FIRST 256u
#define CLI_LIST_TEXT_FIRST (1u << 14u)

// The entries of the folder being listed, each list a cell of its own that doubles as it fills, and the folders a
// file the patterns leave sits in, a list made of the files cellblock, under the lock
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

// a list of `*room` items of `size` bytes, `count` stored, given room for `more` items: where they do not fit and the
// list is below `most`, moved into a cell of twice its room, `first` where it has none, its items copied and its cell
// given back. The list as it stands after, `count + more` within `*room` where there is room
static void *list_grown(void *list, unsigned long long count, unsigned long long more, unsigned long long *room,
                        unsigned long long size, unsigned long long first, unsigned long long most, const char *what)
{
    if ((count + more) <= *room)
    {
        return list;
    }
    unsigned long long grown = (*room != 0ull) ? *room : first;
    while ((grown < (count + more)) && (grown < most))
    {
        grown = ((grown * 2ull) < most) ? (grown * 2ull) : most;
    }
    if (grown < (count + more))
    {
        return list;
    }
    void *const cell = cli_cell_regrown(list, count * size, grown * size, NULL);
    if (cell == NULL)
    {
        return list;
    }
    *room = grown;
    return cell;
}

// a text put in the entries' texts: its place
static unsigned int cli_entry_text_stored(const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    s_listing->text = list_grown(s_listing->text, s_listing->used, length + 1ull, &s_listing->text_room, 1ull,
                                 CLI_LIST_TEXT_FIRST, CLI_FILES_TEXT, "a folder's listing's texts");
    if ((s_listing->used + length + 1ull) > s_listing->text_room)
    {
        return 0u;
    }
    // a place is below CLI_FILES_TEXT, which an unsigned int stores
    const unsigned int at = (unsigned int)s_listing->used;
    memor.cpy(&(MemoriaCfg){.dst = s_listing->text + at, .src = text, .bytes = length + 1ull});
    s_listing->used += length + 1ull;
    return at;
}

// an entry put after the entries
static void cli_entry_put(const char *name, const char *path, int dir, int ignored, int archive, const char *raw)
{
    unsigned long long room = s_listing->item_room;
    s_listing->item = list_grown(s_listing->item, s_listing->count, 1ull, &room, sizeof(*s_listing->item),
                                 CLI_LIST_FIRST, CLI_LIST_MOST, "a folder's listing's entries");
    // the room is bounded by CLI_LIST_MOST, which an unsigned int stores
    s_listing->item_room = (unsigned int)room;
    if (s_listing->count >= s_listing->item_room)
    {
        return;
    }
    s_listing->item[s_listing->count] = (CliEntry){cli_entry_text_stored(name),
                                                   cli_entry_text_stored(path),
                                                   dir,
                                                   ignored,
                                                   archive,
                                                   (raw != NULL) ? cli_entry_text_stored(raw) : 0u,
                                                   raw != NULL};
    s_listing->count += 1u;
}

// whether a name sorts before another, ASCII's case aside
static int cli_before(const char *one, const char *other)
{
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
static void cli_entries_written(VerbaLinea *out)
{
    for (unsigned int at = 1u; at < s_listing->count; at += 1u)
    {
        const CliEntry swap = s_listing->item[at];
        unsigned int place = at;
        while (place > 0u)
        {
            const CliEntry *const before = &s_listing->item[place - 1u];
            const int first =
                (swap.dir && !before->dir) ||
                ((swap.dir == before->dir) && cli_before(s_listing->text + swap.name, s_listing->text + before->name));
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
        cli_json_quoted(out, s_listing->text + one->name,
                        cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->name, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"path\":");
        cli_json_quoted(out, s_listing->text + one->path,
                        cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->path, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, one->dir ? ",\"dir\":true" : ",\"dir\":false");
        mmgr_line_put(out, one->ignored ? ",\"ignored\":true" : ",\"ignored\":false");
        mmgr_line_put(out, one->archive ? ",\"archive\":true" : ",\"archive\":false");
        if (one->has_raw)
        {
            mmgr_line_put(out, ",\"raw\":");
            cli_json_quoted(out, s_listing->text + one->raw,
                            cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->raw, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
}

// every folder of the view's files that contains a file the patterns leave, counted on a first pass, the containing
// folders made at that size, put on a second, and sorted
static void cli_folders_collected(const CliSeen *seen)
{
    unsigned long long count = 0ull;
    unsigned long long bytes = 0ull;
    for (unsigned int pass = 0u; (seen != NULL) && (pass < 2u); pass += 1u)
    {
        if ((pass == 1u) && !cli_files_made(&s_listing->folders, count, bytes))
        {
            return;
        }
        for (unsigned int index = 0u; index < seen->files.count; index += 1u)
        {
            const char *const file = cli_files_at(&seen->files, index);
            if (cli_patterns_hides(&s_listing->patterns, file, 0))
            {
                continue;
            }
            for (const char *at = file; at[0] != '\0'; at += 1)
            {
                // the slash lies in the path, past its start
                const unsigned long long length = (unsigned long long)(at - file);
                count += ((pass == 0u) && (at[0] == '/')) ? 1ull : 0ull;
                bytes += ((pass == 0u) && (at[0] == '/')) ? (length + 1ull) : 0ull;
                if ((pass == 1u) && (at[0] == '/'))
                {
                    cli_files_put(&s_listing->folders, file, length);
                }
            }
        }
    }
    (void)cli_files_sorted(&s_listing->folders);
}

// A folder of the tree being listed: its path in the tree, git's view, and whether a pattern includes only what it
// names
typedef struct
{
    const char *root;
    const char *folder;
    const CliSeen *seen;
    int include_only;
} CliListed;

static void cli_listed_each(void *context, const char *name, int is_folder)
{
    const CliListed *const listed = (const CliListed *)context;
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
    cli_relative(listed->root, full, path);
    if ((is_folder && (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL}))) ||
        cli_patterns_hides(&s_listing->patterns, path, is_folder))
    {
        return;
    }
    if (listed->include_only && is_folder && !cli_files_contains(&s_listing->folders, path) &&
        !cli_patterns_include_folder(&s_listing->patterns, path))
    {
        return;
    }
    int ignored = 0;
    if (listed->seen != NULL)
    {
        ignored = is_folder ? !cli_files_contains(&listed->seen->dirs, path)
                            : !cli_files_contains(&listed->seen->files, path);
    }
    else if (is_folder && ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL})) ||
                           (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "target", .cap = CLI_CELLS_POOL}))))
    {
        return;
    }
    const int archive = !is_folder && (cli_archive_kind_of(name) != CLI_ARCHIVE_NONE);
    cli_entry_put(name, path, is_folder || archive, ignored, archive, NULL);
}

// the listing given back, its containing folders with it, and the lock with it
static void list_released(void)
{
    cli_files_emptied(&s_listing->folders);
    cli_cell_release(s_listing->item, NULL);
    cli_cell_release(s_listing->text, NULL);
    cli_cell_release(s_listing, NULL);
    s_listing = NULL;
    cli_lock_give(&s_lock);
}

int cli_files_list(const char *root, const char *dir, VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    s_listing = g_cli_cells->persistent_buf_alloc(sizeof(*s_listing), NULL);
    if (s_listing == NULL)
    {
        cli_lock_give(&s_lock);
        mmgr_line_put(err_msg, "a folder's listing could not be taken");
        return 0;
    }
    s_listing->item = NULL;
    s_listing->count = 0u;
    s_listing->item_room = 0u;
    s_listing->text = NULL;
    s_listing->used = 0ull;
    s_listing->text_room = 0ull;
    s_listing->folders = (CliFiles){NULL, 0u, 0u, NULL, 0ull, 0ull};
    // an archive, and a folder within one, lists what the archive contains
    char file[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    if (cli_archive_split(root, dir, file, inner))
    {
        const int listed = cli_archive_list(root, dir, &s_listing->children, err_msg);
        for (unsigned int at = 0u; listed && (at < s_listing->children.count); at += 1u)
        {
            const CliChild *const child = &s_listing->children.item[at];
            const char *const text = s_listing->children.text;
            cli_entry_put(text + child->name, text + child->path, child->dir, 0, child->archive,
                          child->has_raw ? (text + child->raw) : NULL);
        }
        if (listed)
        {
            // the archive lists its children in its own order, retained
            mmgr_line_ch(out, '[');
            for (unsigned int at = 0u; at < s_listing->count; at += 1u)
            {
                const CliEntry *const one = &s_listing->item[at];
                mmgr_line_put(out, (at != 0u) ? ",{\"name\":" : "{\"name\":");
                cli_json_quoted(
                    out, s_listing->text + one->name,
                    cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->name, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(out, ",\"path\":");
                cli_json_quoted(
                    out, s_listing->text + one->path,
                    cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->path, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(out, one->dir ? ",\"dir\":true,\"ignored\":false" : ",\"dir\":false,\"ignored\":false");
                mmgr_line_put(out, one->archive ? ",\"archive\":true" : ",\"archive\":false");
                if (one->has_raw)
                {
                    mmgr_line_put(out, ",\"raw\":");
                    cli_json_quoted(
                        out, s_listing->text + one->raw,
                        cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->raw, .cap = CLI_CELLS_POOL}));
                }
                mmgr_line_ch(out, '}');
            }
            mmgr_line_ch(out, ']');
        }
        list_released();
        return listed;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_inside(root, dir, folder, err_msg))
    {
        list_released();
        return 0;
    }
    cli_patterns_of(CLI_PART_EXPLORER, &s_listing->patterns);
    const int include_only = cli_patterns_include_only(&s_listing->patterns);
    const CliSeen *const seen = cli_seen_acquire(root);
    if (include_only)
    {
        cli_folders_collected(seen);
    }
    // a tree git cannot read marks nothing as left out
    CliListed listed = {root, folder, ((seen != NULL) && seen->git) ? seen : NULL, include_only};
    const int read = query_folder_each(folder, cli_listed_each, &listed);
    cli_seen_let(seen);
    if (!read)
    {
        mmgr_line_put(err_msg, dir);
        mmgr_line_put(err_msg, ": it could not be read");
        list_released();
        return 0;
    }
    cli_entries_written(out);
    list_released();
    return 1;
}
