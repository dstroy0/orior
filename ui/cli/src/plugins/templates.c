// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// templates.c: the reader's templates listed, a project begun from one with its name written in, and a tree kept as
// one
#include "templates.h"
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the most templates listed and the bytes of their names, and the most times a file's text names the project
#define CLI_TEMPLATES_MOST 4096u
#define CLI_TEMPLATES_TEXT (1u << 20u)
#define CLI_TEMPLATE_PIECES 65536u

int cli_templates_folder(char *folder)
{
    char home[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        return 0;
    }
    cli_full(home, "templates", folder);
    return 1;
}

// The names listed as a folder is read
typedef struct
{
    char text[CLI_TEMPLATES_TEXT];
    unsigned int used;
    const char *name[CLI_TEMPLATES_MOST];
    unsigned int count;
} CliTemplateNames;

static CliTemplateNames s_names;

static void cli_name_each(void *context, const char *name, int is_folder)
{
    (void)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1ull;
    if (!is_folder || (s_names.count >= CLI_TEMPLATES_MOST) || ((s_names.used + length) > CLI_TEMPLATES_TEXT))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_names.text + s_names.used, .src = name, .bytes = length});
    s_names.name[s_names.count] = s_names.text + s_names.used;
    s_names.count += 1u;
    // the names are below CLI_TEMPLATES_TEXT bytes, which an unsigned int holds
    s_names.used += (unsigned int)length;
}

void cli_templates_list(CliLine *out)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    s_names.used = 0u;
    s_names.count = 0u;
    char folder[CLI_PATH_LONGEST];
    if (cli_templates_folder(folder))
    {
        query_folder_each(folder, cli_name_each, NULL);
    }
    for (unsigned int at = 1u; at < s_names.count; at += 1u)
    {
        const char *const held = s_names.name[at];
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(s_names.name[place - 1u], held) > 0))
        {
            s_names.name[place] = s_names.name[place - 1u];
            place -= 1u;
        }
        s_names.name[place] = held;
    }
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < s_names.count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, s_names.name[at],
                        cellul.len(&(CatenaFinitaCfg){.src = s_names.name[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
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

// `text` with every `{{name}}` in it written as `name`, into `out`, which holds `room`: 1, or 0 where it does not fit
static int cli_named(const char *text, const char *name, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    for (const char *at = text; at[0] != '\0';)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = at, .other = "{{name}}", .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line, name);
            at += 8;
            continue;
        }
        cli_line_character(&line, at[0]);
        at += 1;
    }
    cli_line_closed(&line);
    return cli_line_fits(&line);
}

// A copy of a template: the project's name and why a copy failed
typedef struct
{
    const char *from;
    const char *to;
    const char *name;
    CliLine *said;
    int failed;
} CliCopy;

static int cli_copied(const char *from, const char *to, const char *name, CliLine *said);

// that a path could not be copied, said
static int cli_uncopied(const char *path, CliLine *said)
{
    cli_line_text(said, path);
    cli_line_text(said, ": it could not be copied");
    return 0;
}

// a file copied with `{{name}}` written as the name in its text where it is UTF-8
static int cli_file_copied(const char *from, const char *to, const char *name, CliLine *said)
{
    static CliFilePiece s_pieces[CLI_TEMPLATE_PIECES];
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(from, &size);
    if (bytes == NULL)
    {
        return cli_uncopied(from, said);
    }
    int text = 1;
    for (unsigned long long at = 0ull; text && (at < size);)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter(bytes, size, at, &letter);
        text = !((step == 1ull) && (bytes[at] >= 0x80u));
        at += step;
    }
    unsigned int pieces = 0u;
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; text && ((at + 8ull) <= size) && (pieces < (CLI_TEMPLATE_PIECES - 2u));)
    {
        if (memor.cmp(&(MemoriaCfg){.src = bytes + at, .other = "{{name}}", .bytes = 8u}) != 0)
        {
            at += 1ull;
            continue;
        }
        s_pieces[pieces] = (CliFilePiece){bytes + start, at - start};
        s_pieces[pieces + 1u] =
            (CliFilePiece){name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL})};
        pieces += 2u;
        at += 8ull;
        start = at;
    }
    s_pieces[pieces] = (CliFilePiece){bytes + start, size - start};
    pieces += 1u;
    // a piece of no bytes holds no zeros either
    const int written = cli_file_pieces_written(to, s_pieces, pieces);
    qry_member_file_unmapped(bytes, size);
    return written || cli_uncopied(to, said);
}

static void cli_copy_each(void *context, const char *entry, int is_folder)
{
    CliCopy *const copy = (CliCopy *)context;
    if (copy->failed)
    {
        return;
    }
    char named[CLI_PATH_LONGEST];
    char from[CLI_PATH_LONGEST];
    char to[CLI_PATH_LONGEST];
    if (!cli_named(entry, copy->name, named, sizeof(named)))
    {
        copy->failed = !cli_uncopied(entry, copy->said);
        return;
    }
    cli_full(copy->from, entry, from);
    cli_full(copy->to, named, to);
    copy->failed =
        is_folder ? !cli_copied(from, to, copy->name, copy->said) : !cli_file_copied(from, to, copy->name, copy->said);
}

// `from` copied into `to`, made, `{{name}}` written as `name` in each file's name and in each file's text where it is
// text: 1, or 0 with why said
static int cli_copied(const char *from, const char *to, const char *name, CliLine *said)
{
    if (!query_folder_made(to))
    {
        return cli_uncopied(to, said);
    }
    CliCopy copy = {from, to, name, said, 0};
    if (!query_folder_each(from, cli_copy_each, &copy))
    {
        return cli_uncopied(from, said);
    }
    return !copy.failed;
}

int cli_templates_create(const char *template_name, const char *parent, const char *name, char *made, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_templates_folder(folder))
    {
        cli_line_text(said, "orior has no folder of its own to keep templates in");
        return 0;
    }
    if (!cli_plain(name))
    {
        cli_line_text(said, name);
        cli_line_text(said, " is no name a folder may take");
        return 0;
    }
    char source[CLI_PATH_LONGEST];
    cli_full(folder, template_name, source);
    if (!cli_plain(template_name) || !query_is_folder(source))
    {
        cli_line_text(said, template_name);
        cli_line_text(said, " is no template of yours: they are the folders in ");
        cli_line_text(said, folder);
        return 0;
    }
    cli_full(parent, name, made);
    if (query_is_file(made) || query_is_folder(made))
    {
        cli_line_text(said, made);
        cli_line_text(said, " is there already");
        return 0;
    }
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    const int copied = cli_copied(source, made, name, said);
    cli_lock_give(&s_lock);
    return copied;
}

int cli_templates_keep(const char *root, const char *name, char *made, CliLine *said)
{
    if (!cli_plain(name))
    {
        cli_line_text(said, name);
        cli_line_text(said, " is no name a template may take");
        return 0;
    }
    char folder[CLI_PATH_LONGEST];
    if (!cli_templates_folder(folder))
    {
        cli_line_text(said, "orior has no folder of its own to keep templates in");
        return 0;
    }
    cli_full(folder, name, made);
    if (query_is_file(made) || query_is_folder(made))
    {
        cli_line_text(said, name);
        cli_line_text(said, " is a template already");
        return 0;
    }
    const CliFiles *const files = cli_files_held(root);
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const file = cli_files_at(files, index);
        if ((cellul.eq(&(CatenaFinitaCfg){.src = file, .other = ".git", .cap = CLI_CELLS_POOL})) ||
            (cellul.starts(&(CatenaFinitaCfg){.src = file, .other = ".git/", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        char from[CLI_PATH_LONGEST];
        char to[CLI_PATH_LONGEST];
        char parent[CLI_PATH_LONGEST];
        cli_full(root, file, from);
        cli_full(made, file, to);
        if (cli_path_parent(to, parent) && !query_folder_made(parent))
        {
            cli_files_let(files);
            return cli_uncopied(parent, said);
        }
        if (!query_file_copied(from, to))
        {
            const int uncopied = cli_uncopied(file, said);
            cli_files_let(files);
            return uncopied;
        }
    }
    cli_files_let(files);
    return 1;
}
