// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// templates.c: the reader's templates listed, a project begun from one with its name written in, and a tree saved as
// one
#include "templates.h"
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "../text/text.h"

CellOwner g_cli_templates_cells = MMGR_CELL_OWNER("cli_templates");

int cli_templates_folder(char *folder)
{
    char home[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(home))
    {
        return 0;
    }
    cli_full(home, "templates", folder);
    return 1;
}

// The names listed as a folder is read: counted with their bytes where `name` is NULL, and else put in the cell a
// count took, `most` names and `text_most` bytes of them at the most
typedef struct
{
    const char **name;
    char *text;
    unsigned int count;
    unsigned int most;
    unsigned long long used;
    unsigned long long text_most;
} CliTemplateNames;

static void cli_name_each(void *context, const char *name, int is_folder)
{
    CliTemplateNames *const names = (CliTemplateNames *)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1ull;
    if (!is_folder ||
        ((names->name != NULL) && ((names->count >= names->most) || ((names->used + length) > names->text_most))))
    {
        return;
    }
    if (names->name != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = names->text + names->used, .src = name, .bytes = length});
        names->name[names->count] = names->text + names->used;
    }
    names->count += 1u;
    names->used += length;
}

void cli_templates_list(VerbaLinea *out)
{
    char folder[CLI_PATH_MAX_LEN];
    CliTemplateNames names = {NULL, NULL, 0u, 0u, 0ull, 0ull};
    const int has_folder = cli_templates_folder(folder);
    if (has_folder)
    {
        query_folder_each(folder, cli_name_each, &names);
    }
    // the names in a cell of the count and bytes a first reading found, read again into it
    const unsigned int most = names.count;
    const unsigned long long text_most = names.used;
    names = (CliTemplateNames){NULL, NULL, 0u, most, 0ull, text_most};
    names.name =
        (has_folder && (most != 0u))
            ? g_cli_cells->persistent_buf_alloc((most * sizeof(*names.name)) + text_most, &g_cli_templates_cells)
            : NULL;
    if (names.name != NULL)
    {
        names.text = (char *)(names.name + most);
        query_folder_each(folder, cli_name_each, &names);
    }
    for (unsigned int at = 1u; at < names.count; at += 1u)
    {
        const char *const name = names.name[at];
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(names.name[place - 1u], name) > 0))
        {
            names.name[place] = names.name[place - 1u];
            place -= 1u;
        }
        names.name[place] = name;
    }
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < names.count; at += 1u)
    {
        mmgr_line_put(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, names.name[at],
                        cellul.len(&(CatenaFinitaCfg){.src = names.name[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_ch(out, ']');
    g_cli_cells->persistent_buf_release(names.name, &g_cli_templates_cells);
}

// whether a name is one a folder may take: not blank, no separator, no `..`, and none of the characters a path reads
// as more than a name
static int cli_plain(const char *name)
{
    int blank = 1;
    unsigned long long at = 0ull;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    while (at < length)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)name, length, at, &letter);
        blank = blank &&
                ((letter == ' ') || ((letter >= '\t') && (letter <= '\r')) || (letter == 0x85u) || (letter == 0xa0u) ||
                 (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
                 (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u));
    }
    return !blank && (name[cli_text_upto(name, "/\\:*?\"<>|")] == '\0') &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".", .cap = CLI_CELLS_POOL})) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "..", .cap = CLI_CELLS_POOL}));
}

// `text` with every `{{name}}` in it written as `name`, into `out`, sized for `room`: 1, or 0 where it does not fit
static int cli_named(const char *text, const char *name, char *out, unsigned long long room)
{
    VerbaLinea line = {out, room, 0ull};
    for (const char *at = text; at[0] != '\0';)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = at, .other = "{{name}}", .cap = CLI_CELLS_POOL}))
        {
            mmgr_line_put(&line, name);
            at += 8;
            continue;
        }
        mmgr_line_ch(&line, at[0]);
        at += 1;
    }
    mmgr_line_finish(&line);
    return mmgr_line_ok(&line);
}

// A copy of a template: the project's name and why a copy failed
typedef struct
{
    const char *from;
    const char *to;
    const char *name;
    VerbaLinea *err_msg;
    int failed;
} CliCopy;

static int cli_copied(const char *from, const char *to, const char *name, VerbaLinea *err_msg);

// that a path could not be copied, reported
static int cli_uncopied(const char *path, VerbaLinea *err_msg)
{
    mmgr_line_put(err_msg, path);
    mmgr_line_put(err_msg, ": it could not be copied");
    return 0;
}

// a file copied with `{{name}}` written as the name in its text where it is UTF-8
static int cli_file_copied(const char *from, const char *to, const char *name, VerbaLinea *err_msg)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_read(from, &size);
    if (bytes == NULL)
    {
        return cli_uncopied(from, err_msg);
    }
    int text = 1;
    for (unsigned long long at = 0ull; text && (at < size);)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter(bytes, size, at, &letter);
        text = !((step == 1ull) && (bytes[at] >= 0x80u));
        at += step;
    }
    // the times the text names the project, each a piece before it and the name, and the piece after the last
    unsigned long long named = 0ull;
    for (unsigned long long at = 0ull; text && ((at + 8ull) <= size);)
    {
        const int names = memor.cmp(&(MemoriaCfg){.src = bytes + at, .other = "{{name}}", .bytes = 8u}) == 0;
        named += names ? 1ull : 0ull;
        at += names ? 8ull : 1ull;
    }
    CliFilePiece *const piece =
        g_cli_cells->persistent_buf_alloc(((2ull * named) + 1ull) * sizeof(*piece), &g_cli_templates_cells);
    if (piece == NULL)
    {
        cli_file_let_go(bytes, size);
        return cli_uncopied(from, err_msg);
    }
    unsigned int pieces = 0u;
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; text && ((at + 8ull) <= size);)
    {
        if (memor.cmp(&(MemoriaCfg){.src = bytes + at, .other = "{{name}}", .bytes = 8u}) != 0)
        {
            at += 1ull;
            continue;
        }
        piece[pieces] = (CliFilePiece){bytes + start, at - start};
        piece[pieces + 1u] = (CliFilePiece){name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL})};
        pieces += 2u;
        at += 8ull;
        start = at;
    }
    piece[pieces] = (CliFilePiece){bytes + start, size - start};
    pieces += 1u;
    // a piece of no bytes contains no zeros either
    const int written = cli_file_pieces_written(to, piece, pieces);
    g_cli_cells->persistent_buf_release(piece, &g_cli_templates_cells);
    cli_file_let_go(bytes, size);
    return written || cli_uncopied(to, err_msg);
}

static void cli_copy_each(void *context, const char *entry, int is_folder)
{
    CliCopy *const copy = (CliCopy *)context;
    if (copy->failed)
    {
        return;
    }
    char named[CLI_PATH_MAX_LEN];
    char from[CLI_PATH_MAX_LEN];
    char to[CLI_PATH_MAX_LEN];
    if (!cli_named(entry, copy->name, named, sizeof(named)))
    {
        copy->failed = !cli_uncopied(entry, copy->err_msg);
        return;
    }
    cli_full(copy->from, entry, from);
    cli_full(copy->to, named, to);
    copy->failed = is_folder ? !cli_copied(from, to, copy->name, copy->err_msg)
                             : !cli_file_copied(from, to, copy->name, copy->err_msg);
}

// `from` copied into `to`, made, `{{name}}` written as `name` in each file's name and in each file's text where it is
// text: 1, or 0 with why given
static int cli_copied(const char *from, const char *to, const char *name, VerbaLinea *err_msg)
{
    if (!query_folder_made(to))
    {
        return cli_uncopied(to, err_msg);
    }
    CliCopy copy = {from, to, name, err_msg, 0};
    if (!query_folder_each(from, cli_copy_each, &copy))
    {
        return cli_uncopied(from, err_msg);
    }
    return !copy.failed;
}

int cli_templates_create(const char *template_name, const char *parent, const char *name, char *made,
                         VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_templates_folder(folder))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own to store templates in");
        return 0;
    }
    if (!cli_plain(name))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is no name a folder may take");
        return 0;
    }
    char source[CLI_PATH_MAX_LEN];
    cli_full(folder, template_name, source);
    if (!cli_plain(template_name) || !query_is_folder(source))
    {
        mmgr_line_put(err_msg, template_name);
        mmgr_line_put(err_msg, " is no template of yours: they are the folders in ");
        mmgr_line_put(err_msg, folder);
        return 0;
    }
    cli_full(parent, name, made);
    if (query_is_file(made) || query_is_folder(made))
    {
        mmgr_line_put(err_msg, made);
        mmgr_line_put(err_msg, " is there already");
        return 0;
    }
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    const int copied = cli_copied(source, made, name, err_msg);
    cli_lock_give(&s_lock);
    return copied;
}

int cli_templates_save(const char *root, const char *name, char *made, VerbaLinea *err_msg)
{
    if (!cli_plain(name))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is no name a template may take");
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_templates_folder(folder))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own to store templates in");
        return 0;
    }
    cli_full(folder, name, made);
    if (query_is_file(made) || query_is_folder(made))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is a template already");
        return 0;
    }
    const CliFiles *const files = cli_files_view(root);
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const file = cli_files_at(files, index);
        if ((cellul.eq(&(CatenaFinitaCfg){.src = file, .other = ".git", .cap = CLI_CELLS_POOL})) ||
            (cellul.starts(&(CatenaFinitaCfg){.src = file, .other = ".git/", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        char from[CLI_PATH_MAX_LEN];
        char to[CLI_PATH_MAX_LEN];
        char parent[CLI_PATH_MAX_LEN];
        cli_full(root, file, from);
        cli_full(made, file, to);
        if (cli_path_parent(to, parent) && !query_folder_made(parent))
        {
            cli_files_let(files);
            return cli_uncopied(parent, err_msg);
        }
        if (!query_file_copied(from, to))
        {
            const int uncopied = cli_uncopied(file, err_msg);
            cli_files_let(files);
            return uncopied;
        }
    }
    cli_files_let(files);
    return 1;
}
