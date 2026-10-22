// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_paste.c: files and folders copied or moved into a folder of the tree, a name the folder holds already
// numbered, and a file brought from another machine written a part at a time beside the file it becomes
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "files_internal.h"

// the bytes of the paths a paste's walk holds to go to, and the most of them; and the most numbers a free name is
// looked for under
#define CLI_PENDING_TEXT (1u << 23u)
#define CLI_PENDING_MOST 65536u
#define CLI_FREE_MOST 1000000u

// whether a byte parts a path
static int cli_parts(char letter)
{
    return (letter == '/') || (letter == '\\');
}

int cli_paste_name(const char *path, char *name)
{
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    while ((end > 0ull) && cli_parts(path[end - 1ull]))
    {
        end -= 1ull;
    }
    unsigned long long start = end;
    while ((start > 0ull) && !cli_parts(path[start - 1ull]))
    {
        start -= 1ull;
    }
    const unsigned long long length = end - start;
    const int up = (length == 2ull) && (path[start] == '.') && (path[start + 1ull] == '.');
    const int drive = (start == 0ull) && (length == 2ull) && (path[1] == ':');
    if ((length == 0ull) || (length >= CLI_PATH_LONGEST) || up || drive)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = name, .src = path + start, .bytes = length});
    name[length] = '\0';
    return 1;
}

int cli_paste_under(const char *folder, const char *path)
{
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    while ((length > 0ull) && cli_parts(folder[length - 1ull]))
    {
        length -= 1ull;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if ((folder[at] != path[at]) && !(cli_parts(folder[at]) && cli_parts(path[at])))
        {
            return 0;
        }
    }
    return (path[length] == '\0') || cli_parts(path[length]);
}

// whether a file or a folder is at `path`
static int cli_there(const char *path)
{
    return query_is_file(path) || query_is_folder(path);
}

void cli_paste_free(const char *folder, const char *name, char *path)
{
    CliLine first = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&first, folder);
    cli_line_character(&first, '/');
    cli_line_text(&first, name);
    cli_line_closed(&first);
    if (!cli_there(path))
    {
        return;
    }
    // the stem and the extension, split at the last dot, a dot that starts the name splitting nothing
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    const int split = (dot != NULL) && (dot != name) &&
                      (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "..", .cap = CLI_CELLS_POOL}));
    // the dot lies in the name, at or past its start
    const unsigned long long stem =
        split ? (unsigned long long)(dot - name) : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    char numbered[CLI_PATH_LONGEST];
    for (unsigned int number = 2u; number < CLI_FREE_MOST; number += 1u)
    {
        CliLine line = {numbered, sizeof(numbered), 0ull};
        cli_line_text(&line, folder);
        cli_line_character(&line, '/');
        cli_line_counted(&line, name, stem);
        cli_line_text(&line, " (");
        cli_line_decimal(&line, number, 0u);
        cli_line_character(&line, ')');
        cli_line_text(&line, split ? dot : "");
        if ((cli_line_closed(&line) != 0ull) && !cli_there(numbered))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = path,
                              .src = numbered,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = numbered, .cap = CLI_CELLS_POOL}) + 1u});
            return;
        }
    }
}

void cli_pasted_written(CliLine *out, int first, const char *from, const char *to)
{
    cli_line_text(out, first ? "{\"from\":" : ",{\"from\":");
    if (from != NULL)
    {
        cli_json_quoted(out, from, cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"to\":");
    cli_json_quoted(out, to, cellul.len(&(CatenaFinitaCfg){.src = to, .cap = CLI_CELLS_POOL}));
    cli_line_character(out, '}');
}

// A walk's paths still to go to, each a pair, where it is and where it goes, the last put first taken; and the names a
// folder holds as it is read
typedef struct
{
    char text[CLI_PENDING_TEXT];
    unsigned long long used;
    unsigned long long start[CLI_PENDING_MOST];
    unsigned int count;
    char names[CLI_PENDING_TEXT];
    unsigned long long names_used;
    unsigned long long name_start[CLI_PENDING_MOST];
    unsigned int name_count;
    int full;
} CliPending;

// the walk's pending pairs, a cell taken with the walk's lock and given back with it
static CliPending *s_pending;

// a pair put to go to: 1, or 0 where the walk holds no more
static int cli_pending_put(const char *source, const char *target)
{
    const unsigned long long source_length =
        cellul.len(&(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL}) + 1ull;
    const unsigned long long target_length =
        cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((s_pending->count >= CLI_PENDING_MOST) ||
        ((s_pending->used + source_length + target_length) > CLI_PENDING_TEXT))
    {
        s_pending->full = 1;
        return 0;
    }
    s_pending->start[s_pending->count] = s_pending->used;
    memor.cpy(&(MemoriaCfg){.dst = s_pending->text + s_pending->used, .src = source, .bytes = source_length});
    memor.cpy(
        &(MemoriaCfg){.dst = s_pending->text + s_pending->used + source_length, .src = target, .bytes = target_length});
    s_pending->used += source_length + target_length;
    s_pending->count += 1u;
    return 1;
}

static void cli_pending_name(void *context, const char *name, int is_folder)
{
    (void)context;
    (void)is_folder;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((s_pending->name_count >= CLI_PENDING_MOST) || ((s_pending->names_used + length) > CLI_PENDING_TEXT))
    {
        s_pending->full = 1;
        return;
    }
    s_pending->name_start[s_pending->name_count] = s_pending->names_used;
    memor.cpy(&(MemoriaCfg){.dst = s_pending->names + s_pending->names_used, .src = name, .bytes = length});
    s_pending->names_used += length;
    s_pending->name_count += 1u;
}

// a folder's entries put to go to, the first last so it is taken first: 1, or 0 with why said
static int cli_pending_entries(const char *source, const char *target, CliLine *said)
{
    s_pending->name_count = 0u;
    s_pending->names_used = 0ull;
    if (!query_folder_each(source, cli_pending_name, NULL))
    {
        cli_line_text(said, source);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    for (unsigned int index = s_pending->name_count; !s_pending->full && (index > 0u); index -= 1u)
    {
        const char *const name = s_pending->names + s_pending->name_start[index - 1u];
        char from[CLI_PATH_LONGEST];
        char to[CLI_PATH_LONGEST];
        CliLine from_line = {from, sizeof(from), 0ull};
        CliLine to_line = {to, sizeof(to), 0ull};
        cli_line_text(&from_line, source);
        cli_line_character(&from_line, '/');
        cli_line_text(&from_line, name);
        cli_line_text(&to_line, target);
        cli_line_character(&to_line, '/');
        cli_line_text(&to_line, name);
        s_pending->full = s_pending->full || (cli_line_closed(&from_line) == 0ull) ||
                          (cli_line_closed(&to_line) == 0ull) || !cli_pending_put(from, to);
    }
    if (s_pending->full)
    {
        cli_line_text(said, source);
        cli_line_text(said, ": it holds more than a paste takes");
        return 0;
    }
    return 1;
}

int cli_paste_walked(const char *source, const char *target, CliPasteEach each, void *context, CliLine *said)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    s_pending = cli_cell_taken(sizeof(*s_pending), "a paste's walk");
    if (s_pending == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "a paste's walk could not be taken");
        return 0;
    }
    s_pending->count = 0u;
    s_pending->used = 0ull;
    s_pending->full = 0;
    int walked = cli_pending_put(source, target);
    while (walked && (s_pending->count > 0u))
    {
        s_pending->count -= 1u;
        const char *const pair = s_pending->text + s_pending->start[s_pending->count];
        char from[CLI_PATH_LONGEST];
        char to[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = from,
                                .src = pair,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = pair, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = to,
            .src = pair + cellul.len(&(CatenaFinitaCfg){.src = pair, .cap = CLI_CELLS_POOL}) + 1,
            .bytes = cellul.len(&(CatenaFinitaCfg){
                         .src = pair + cellul.len(&(CatenaFinitaCfg){.src = pair, .cap = CLI_CELLS_POOL}) + 1,
                         .cap = CLI_CELLS_POOL}) +
                     1u});
        s_pending->used = s_pending->start[s_pending->count];
        const int folder = query_is_folder(from);
        walked = each(context, from, to, folder, said) && (!folder || cli_pending_entries(from, to, said));
    }
    cli_cell_given(s_pending);
    s_pending = NULL;
    cli_lock_give(&s_lock);
    return walked;
}

// What a paste's walk copies: the path it was asked for, which a failure names
typedef struct
{
    const char *source;
} CliCopying;

// a folder made, or a file copied
static int cli_copied(void *context, const char *source, const char *target, int folder, CliLine *said)
{
    const CliCopying *const copying = (const CliCopying *)context;
    if (folder ? query_folder_made(target) : query_file_copied(source, target))
    {
        return 1;
    }
    cli_line_text(said, copying->source);
    cli_line_text(said, ": it could not be copied");
    return 0;
}

// that a path could not be moved, said
static int cli_unmoved(const char *path, CliLine *said)
{
    cli_line_text(said, path);
    cli_line_text(said, ": it could not be moved");
    return 0;
}

// one of a paste's paths copied or moved into `folder`, where it went into `target`: 1, or 0 with why said. A move
// between drives is a copy and then a removal
static int cli_pasted_one(const char *source, const char *folder, const char *name, int moving, char *target,
                          CliLine *said)
{
    if (query_is_folder(source) && cli_paste_under(source, folder))
    {
        cli_line_text(said, source);
        cli_line_text(said, " is not pasted into itself");
        return 0;
    }
    cli_paste_free(folder, name, target);
    CliCopying copying = {source};
    if (!moving)
    {
        return cli_paste_walked(source, target, cli_copied, &copying, said);
    }
    const int moved = cli_moved(source, target);
    if (moved == CLI_MOVE_FAILED)
    {
        return cli_unmoved(source, said);
    }
    if (moved == CLI_MOVE_ACROSS)
    {
        if (!cli_paste_walked(source, target, cli_copied, &copying, said))
        {
            return 0;
        }
        return query_removed(source) || cli_unmoved(source, said);
    }
    return 1;
}

int cli_paste_begun(const char *root, const char *into, char *folder, CliLine *out, CliLine *said)
{
    if (!cli_inside(root, into, folder, said))
    {
        return 0;
    }
    if (!query_is_folder(folder))
    {
        cli_line_text(said, into);
        cli_line_text(said, " is no folder");
        return 0;
    }
    cli_line_character(out, '[');
    return 1;
}

int cli_paste_named(const char *source, char *name, CliLine *said)
{
    if (!cli_paste_name(source, name))
    {
        cli_line_text(said, source);
        cli_line_text(said, " has no name");
        return 0;
    }
    return 1;
}

int cli_files_paste(const char *root, const char *into, const char *const *from, unsigned int count, int moving,
                    CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_paste_begun(root, into, folder, out, said))
    {
        return 0;
    }
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const source = from[index];
        char name[CLI_PATH_LONGEST];
        if (!cli_paste_named(source, name, said))
        {
            return 0;
        }
        char was[CLI_PATH_LONGEST];
        const int inside = cli_paste_under(root, source);
        if (inside)
        {
            cli_relative(root, source, was);
        }
        // a file moved into the folder it is in stays where it is
        char parent[CLI_PATH_LONGEST];
        const int stays = moving && cli_path_parent(source, parent) && cli_paste_under(parent, folder) &&
                          cli_paste_under(folder, parent);
        char target[CLI_PATH_LONGEST];
        if (!stays && !cli_pasted_one(source, folder, name, moving, target, said))
        {
            return 0;
        }
        char to[CLI_PATH_LONGEST];
        cli_relative(root, stays ? source : target, to);
        cli_pasted_written(out, index == 0u, inside ? was : NULL, to);
    }
    cli_line_character(out, ']');
    cli_files_forget_seen();
    return 1;
}

int cli_files_free_in(const char *root, const char *into, const char *name, char *path, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_inside(root, into, folder, said))
    {
        return 0;
    }
    if (!query_is_folder(folder))
    {
        cli_line_text(said, into);
        cli_line_text(said, " is no folder");
        return 0;
    }
    char free_path[CLI_PATH_LONGEST];
    cli_paste_free(folder, name, free_path);
    cli_relative(root, free_path, path);
    return 1;
}

int cli_files_write_part(const char *root, const char *path, unsigned long long at, const void *data,
                         unsigned long long size, int last, CliLine *said)
{
    char target[CLI_PATH_LONGEST];
    if (!cli_inside(root, path, target, said))
    {
        return 0;
    }
    char name[CLI_PATH_LONGEST];
    char parent[CLI_PATH_LONGEST];
    if (!cli_paste_name(target, name) || !cli_path_parent(target, parent))
    {
        cli_line_text(said, path);
        cli_line_text(said, " has no name");
        return 0;
    }
    // the parts go to a file beside the target, begun afresh by the part at 0
    char part[CLI_PATH_LONGEST];
    CliLine line = {part, sizeof(part), 0ull};
    cli_line_text(&line, parent);
    cli_line_text(&line, "/.");
    cli_line_text(&line, name);
    cli_line_text(&line, ".orior-part");
    unsigned long long has = 0ull;
    if ((cli_line_closed(&line) == 0ull) || ((at != 0ull) && !cli_file_size(part, &has)))
    {
        cli_line_text(said, part);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    if (has != at)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": a part at ");
        cli_line_decimal(said, at, 0u);
        cli_line_text(said, " came after ");
        cli_line_decimal(said, has, 0u);
        cli_line_text(said, " bytes");
        return 0;
    }
    if (!cli_file_appended(part, data, size, at == 0ull))
    {
        cli_line_text(said, part);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    if (last)
    {
        if (!query_file_renamed(part, target))
        {
            return cli_unmoved(target, said);
        }
        cli_files_forget_seen();
    }
    return 1;
}
