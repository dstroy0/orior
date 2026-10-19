// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_list.c: a folder's entries, less what the explorer's patterns hide, each marked where the tree's ignore files
// leave it out; an archive, and a folder within one, listing what the archive holds
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

// The entries of the folder being listed, and the folders a file the patterns leave sits in, a list made of the files
// cellblock, under the lock
typedef struct
{
    CliEntry item[CLI_LIST_MOST];
    unsigned int count;
    char text[CLI_FILES_TEXT];
    unsigned long long used;
    CliFiles holding;
    CliPatterns patterns;
    CliChildren children;
} CliListing;

static CliListing *s_listing;
static CliLock s_lock = CLI_LOCK_INIT;

// a text put in the entries' texts: its place
static unsigned int cli_kept(const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    if ((s_listing->used + length + 1ull) > CLI_FILES_TEXT)
    {
        return 0u;
    }
    // a place is below CLI_FILES_TEXT, which an unsigned int holds
    const unsigned int at = (unsigned int)s_listing->used;
    memor.cpy(&(MemoriaCfg){.dst = s_listing->text + at, .src = text, .bytes = length + 1ull});
    s_listing->used += length + 1ull;
    return at;
}

// an entry put after the entries
static void cli_entry_put(const char *name, const char *path, int dir, int ignored, int archive, const char *raw)
{
    if (s_listing->count >= CLI_LIST_MOST)
    {
        return;
    }
    s_listing->item[s_listing->count] = (CliEntry){
        cli_kept(name), cli_kept(path), dir, ignored, archive, (raw != NULL) ? cli_kept(raw) : 0u, raw != NULL};
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
static void cli_entries_written(CliLine *out)
{
    for (unsigned int at = 1u; at < s_listing->count; at += 1u)
    {
        const CliEntry held = s_listing->item[at];
        unsigned int place = at;
        while (place > 0u)
        {
            const CliEntry *const before = &s_listing->item[place - 1u];
            const int first =
                (held.dir && !before->dir) ||
                ((held.dir == before->dir) && cli_before(s_listing->text + held.name, s_listing->text + before->name));
            if (!first)
            {
                break;
            }
            s_listing->item[place] = *before;
            place -= 1u;
        }
        s_listing->item[place] = held;
    }
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < s_listing->count; at += 1u)
    {
        const CliEntry *const one = &s_listing->item[at];
        cli_line_text(out, (at != 0u) ? ",{\"name\":" : "{\"name\":");
        cli_json_quoted(out, s_listing->text + one->name,
                        cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->name, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"path\":");
        cli_json_quoted(out, s_listing->text + one->path,
                        cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->path, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, one->dir ? ",\"dir\":true" : ",\"dir\":false");
        cli_line_text(out, one->ignored ? ",\"ignored\":true" : ",\"ignored\":false");
        cli_line_text(out, one->archive ? ",\"archive\":true" : ",\"archive\":false");
        if (one->has_raw)
        {
            cli_line_text(out, ",\"raw\":");
            cli_json_quoted(out, s_listing->text + one->raw,
                            cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->raw, .cap = CLI_CELLS_POOL}));
        }
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
}

// every folder of the view's files that holds a file the patterns leave, counted on a first pass, the holding folders
// made at that size, put on a second, and sorted
static void cli_holding(const CliSeen *seen)
{
    unsigned long long count = 0ull;
    unsigned long long bytes = 0ull;
    for (unsigned int pass = 0u; (seen != NULL) && (pass < 2u); pass += 1u)
    {
        if ((pass == 1u) && !cli_files_made(&s_listing->holding, count, bytes))
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
                    cli_files_put(&s_listing->holding, file, length);
                }
            }
        }
    }
    (void)cli_files_sorted(&s_listing->holding);
}

// A folder of the tree being listed: its path in the tree, git's view, and whether a pattern keeps only what it names
typedef struct
{
    const char *root;
    const char *folder;
    const CliSeen *seen;
    int keeps_only;
} CliListed;

static void cli_listed_each(void *context, const char *name, int is_folder)
{
    const CliListed *const listed = (const CliListed *)context;
    char full[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    CliLine line = {full, sizeof(full), 0ull};
    cli_line_text(&line, listed->folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    if (cli_line_closed(&line) == 0ull)
    {
        return;
    }
    cli_relative(listed->root, full, path);
    if ((is_folder && (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL}))) ||
        cli_patterns_hides(&s_listing->patterns, path, is_folder))
    {
        return;
    }
    if (listed->keeps_only && is_folder && !cli_files_holds(&s_listing->holding, path) &&
        !cli_patterns_keeps_folder(&s_listing->patterns, path))
    {
        return;
    }
    int ignored = 0;
    if (listed->seen != NULL)
    {
        ignored =
            is_folder ? !cli_files_holds(&listed->seen->dirs, path) : !cli_files_holds(&listed->seen->files, path);
    }
    else if (is_folder && ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL})) ||
                           (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "target", .cap = CLI_CELLS_POOL}))))
    {
        return;
    }
    const int archive = !is_folder && (cli_archive_kind_of(name) != CLI_ARCHIVE_NONE);
    cli_entry_put(name, path, is_folder || archive, ignored, archive, NULL);
}

// the listing given back, its holding folders with it, and the lock with it
static void list_released(void)
{
    cli_files_emptied(&s_listing->holding);
    cli_cell_given(s_listing);
    s_listing = NULL;
    cli_lock_give(&s_lock);
}

int cli_files_list(const char *root, const char *dir, CliLine *out, CliLine *said)
{
    cli_lock_take(&s_lock);
    s_listing = cli_cell_taken(sizeof(*s_listing), "a folder's listing");
    if (s_listing == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "a folder's listing could not be taken");
        return 0;
    }
    s_listing->count = 0u;
    s_listing->used = 0ull;
    s_listing->holding = (CliFiles){NULL, 0u, 0u, NULL, 0ull, 0ull};
    // an archive, and a folder within one, lists what the archive holds
    char file[CLI_PATH_LONGEST];
    char inner[CLI_PATH_LONGEST];
    if (cli_archive_split(root, dir, file, inner))
    {
        const int listed = cli_archive_list(root, dir, &s_listing->children, said);
        for (unsigned int at = 0u; listed && (at < s_listing->children.count); at += 1u)
        {
            const CliChild *const child = &s_listing->children.item[at];
            const char *const text = s_listing->children.text;
            cli_entry_put(text + child->name, text + child->path, child->dir, 0, child->archive,
                          child->has_raw ? (text + child->raw) : NULL);
        }
        if (listed)
        {
            // the archive lists its children in its own order, kept
            cli_line_character(out, '[');
            for (unsigned int at = 0u; at < s_listing->count; at += 1u)
            {
                const CliEntry *const one = &s_listing->item[at];
                cli_line_text(out, (at != 0u) ? ",{\"name\":" : "{\"name\":");
                cli_json_quoted(
                    out, s_listing->text + one->name,
                    cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->name, .cap = CLI_CELLS_POOL}));
                cli_line_text(out, ",\"path\":");
                cli_json_quoted(
                    out, s_listing->text + one->path,
                    cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->path, .cap = CLI_CELLS_POOL}));
                cli_line_text(out, one->dir ? ",\"dir\":true,\"ignored\":false" : ",\"dir\":false,\"ignored\":false");
                cli_line_text(out, one->archive ? ",\"archive\":true" : ",\"archive\":false");
                if (one->has_raw)
                {
                    cli_line_text(out, ",\"raw\":");
                    cli_json_quoted(
                        out, s_listing->text + one->raw,
                        cellul.len(&(CatenaFinitaCfg){.src = s_listing->text + one->raw, .cap = CLI_CELLS_POOL}));
                }
                cli_line_character(out, '}');
            }
            cli_line_character(out, ']');
        }
        list_released();
        return listed;
    }
    char folder[CLI_PATH_LONGEST];
    if (!cli_inside(root, dir, folder, said))
    {
        list_released();
        return 0;
    }
    cli_patterns_of(CLI_PART_EXPLORER, &s_listing->patterns);
    const int keeps_only = cli_patterns_keeps_only(&s_listing->patterns);
    const CliSeen *const seen = cli_seen_held(root);
    if (keeps_only)
    {
        cli_holding(seen);
    }
    // a tree git cannot read marks nothing as left out
    CliListed listed = {root, folder, ((seen != NULL) && seen->git) ? seen : NULL, keeps_only};
    const int read = query_folder_each(folder, cli_listed_each, &listed);
    cli_seen_let(seen);
    if (!read)
    {
        cli_line_text(said, dir);
        cli_line_text(said, ": it could not be read");
        list_released();
        return 0;
    }
    cli_entries_written(out);
    list_released();
    return 1;
}
