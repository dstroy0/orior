// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editorconfig.c: a file's indentation from the `.editorconfig` files above it
#include "editorconfig.h"

// the lock held while the files' bytes and a pattern's letters are kept
static CliLock s_lock = CLI_LOCK_INIT;

// the most bytes one `.editorconfig` holds, the most folders above a file, and the longest value of a key kept
#define CLI_EDITORCONFIG_BYTES (1u << 20u)

#define CLI_EDITORCONFIG_FOLDERS 256u

#define CLI_VALUE_LONGEST 64u

static unsigned char s_text[CLI_EDITORCONFIG_BYTES];

// The bytes from `from` up to `to` with the ASCII spaces at each end left off
static void cli_trimmed(const char **from, const char **to)
{
    while ((*from < *to) && (((*from)[0] == ' ') || (((*from)[0] >= '\t') && ((*from)[0] <= '\r'))))
    {
        *from += 1;
    }
    while ((*to > *from) && (((*to)[-1] == ' ') || (((*to)[-1] >= '\t') && ((*to)[-1] <= '\r'))))
    {
        *to -= 1;
    }
}

// whether the bytes from `from` up to `to` are `word`, the ASCII letters of either case the same
static int cli_folded_is(const char *from, const char *to, const char *word)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL});
    if ((unsigned long long)(to - from) != length)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char letter = ((from[at] >= 'A') && (from[at] <= 'Z')) ? (char)(from[at] + 32) : from[at];
        if (letter != word[at])
        {
            return 0;
        }
    }
    return 1;
}

// whether the file's lines before its first section hold `root = true`
static int cli_is_root(const char *text, unsigned long long size)
{
    const char *line = text;
    const char *const end = text + size;
    while (line < end)
    {
        const char *stop = line;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *from = line;
        const char *to = stop;
        cli_trimmed(&from, &to);
        if ((from < to) && (from[0] == '['))
        {
            return 0;
        }
        const char *equals = line;
        while ((equals < stop) && (equals[0] != '='))
        {
            equals += 1;
        }
        const char *key = line;
        const char *key_end = equals;
        const char *value = equals + 1;
        const char *value_end = stop;
        cli_trimmed(&key, &key_end);
        cli_trimmed(&value, &value_end);
        if ((equals < stop) && cli_folded_is(key, key_end, "root") && cli_folded_is(value, value_end, "true"))
        {
            return 1;
        }
        line = stop + 1;
    }
    return 0;
}

// A key's value as the last section that names the file sets it, lower case: 1 in `set` where one did
typedef struct
{
    char text[CLI_VALUE_LONGEST];
    int set;
} CliValue;

static void cli_value_kept(CliValue *value, const char *from, const char *to)
{
    CliLine line = {value->text, sizeof(value->text), 0ull};
    for (const char *at = from; at < to; at += 1)
    {
        cli_line_character(&line, ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0]);
    }
    // a value too long to keep is kept as none of the words or numbers it is read for
    value->set = cli_line_fits(&line) ? 1 : 2;
    cli_line_closed(&line);
}

// the lines of one `.editorconfig`, each key of a section that names the file over what was kept before
static void cli_sections_read(const char *text, unsigned long long size, const char *relative, const char *name,
                              CliValue *style, CliValue *size_value, CliValue *width)
{
    const char *line = text;
    const char *const end = text + size;
    int inside = 0;
    while (line < end)
    {
        const char *stop = line;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *from = line;
        const char *to = stop;
        line = stop + 1;
        cli_trimmed(&from, &to);
        if ((from == to) || (from[0] == '#') || (from[0] == ';'))
        {
            continue;
        }
        if ((from[0] == '[') && ((to - from) >= 2) && (to[-1] == ']'))
        {
            inside = cli_editorconfig_names(from + 1, (unsigned long long)(to - from - 2), relative, name);
            continue;
        }
        const char *equals = from;
        while ((equals < to) && (equals[0] != '='))
        {
            equals += 1;
        }
        if ((equals == to) || !inside)
        {
            continue;
        }
        const char *key = from;
        const char *key_end = equals;
        const char *value = equals + 1;
        const char *value_end = to;
        cli_trimmed(&key, &key_end);
        cli_trimmed(&value, &value_end);
        CliValue *const kept = cli_folded_is(key, key_end, "indent_style")  ? style
                               : cli_folded_is(key, key_end, "indent_size") ? size_value
                               : cli_folded_is(key, key_end, "tab_width")   ? width
                                                                            : NULL;
        if (kept != NULL)
        {
            cli_value_kept(kept, value, value_end);
        }
    }
}

// the whole number a kept value writes, as an unsigned 32-bit number reads: 1, or 0 where it writes none
static int cli_value_number(const CliValue *value, unsigned int *number)
{
    if (value->set != 1)
    {
        return 0;
    }
    const char *at = value->text;
    at += (at[0] == '+') ? 1 : 0;
    if (at[0] == '\0')
    {
        return 0;
    }
    unsigned long long size = 0ull;
    for (; at[0] != '\0'; at += 1)
    {
        if ((at[0] < '0') || (at[0] > '9'))
        {
            return 0;
        }
        size = (size * 10ull) + (unsigned long long)(at[0] - '0');
        if (size > 0xffffffffull)
        {
            return 0;
        }
    }
    // checked against the most 32 bits hold above
    *number = (unsigned int)size;
    return 1;
}

static int cli_is_separator(char letter)
{
    return (letter == '/') || (letter == '\\');
}

static void cli_indent_held(const char *path, CliIndent *indent)
{
    unsigned long long ends[CLI_EDITORCONFIG_FOLDERS];
    unsigned int folders = 0u;
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    // each folder from the file's up, the length of `path` it is, until one says it is the root
    while (folders < CLI_EDITORCONFIG_FOLDERS)
    {
        unsigned long long cut = end;
        while ((cut > 0ull) && !cli_is_separator(path[cut - 1u]))
        {
            cut -= 1ull;
        }
        if (cut == 0ull)
        {
            break;
        }
        const int top = (cut == 1ull) || ((cut == 3ull) && (path[1] == ':'));
        const unsigned long long folder = top ? cut : (cut - 1ull);
        if (folder == end)
        {
            break;
        }
        char file[CLI_PATH_LONGEST];
        CliLine line = {file, sizeof(file), 0ull};
        cli_line_counted(&line, path, folder);
        cli_line_text(&line, top ? ".editorconfig" : "/.editorconfig");
        const unsigned long long size =
            (cli_line_closed(&line) != 0ull) ? qry_member_file_read(file, s_text, sizeof(s_text)) : QRY_MEMBER_UNREAD;
        if (size != QRY_MEMBER_UNREAD)
        {
            ends[folders] = folder;
            folders += 1u;
            // the file's bytes read as text: a char and an unsigned char share their width
            if (cli_is_root((const char *)s_text, size))
            {
                break;
            }
        }
        if (top)
        {
            break;
        }
        end = cut - 1ull;
    }
    CliValue style = {{0}, 0};
    CliValue size_value = {{0}, 0};
    CliValue width = {{0}, 0};
    const char *name = path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    while ((name > path) && !cli_is_separator(name[-1]))
    {
        name -= 1;
    }
    for (unsigned int at = folders; at > 0u; at -= 1u)
    {
        const unsigned long long folder = ends[at - 1u];
        char relative[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){
            .dst = relative,
            .src = path + folder + (cli_is_separator(path[folder]) ? 1u : 0u),
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path + folder + (cli_is_separator(path[folder]) ? 1u : 0u),
                                                   .cap = CLI_CELLS_POOL}) +
                     1u});
        for (char *letter = relative; letter[0] != '\0'; letter += 1)
        {
            letter[0] = (letter[0] == '\\') ? '/' : letter[0];
        }
        char file[CLI_PATH_LONGEST];
        CliLine line = {file, sizeof(file), 0ull};
        cli_line_counted(&line, path, folder);
        cli_line_text(&line,
                      ((folder != 0ull) && cli_is_separator(path[folder - 1u])) ? ".editorconfig" : "/.editorconfig");
        const unsigned long long size =
            (cli_line_closed(&line) != 0ull) ? qry_member_file_read(file, s_text, sizeof(s_text)) : QRY_MEMBER_UNREAD;
        if (size != QRY_MEMBER_UNREAD)
        {
            cli_sections_read((const char *)s_text, size, relative, name, &style, &size_value, &width);
        }
    }
    indent->tabs = (style.set != 1)                                                                              ? -1
                   : (cellul.eq(&(CatenaFinitaCfg){.src = style.text, .other = "tab", .cap = CLI_CELLS_POOL}))   ? 1
                   : (cellul.eq(&(CatenaFinitaCfg){.src = style.text, .other = "space", .cap = CLI_CELLS_POOL})) ? 0
                                                                                                                 : -1;
    unsigned int tab_width = 0u;
    const int has_width = cli_value_number(&width, &tab_width);
    unsigned int number = 0u;
    if (size_value.set == 0)
    {
        indent->sized = (indent->tabs == 1) && has_width;
        indent->size = tab_width;
    }
    else if ((size_value.set == 1) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = size_value.text, .other = "tab", .cap = CLI_CELLS_POOL})))
    {
        indent->sized = has_width;
        indent->size = tab_width;
    }
    else if (cli_value_number(&size_value, &number))
    {
        indent->sized = 1;
        indent->size = number;
    }
    else
    {
        indent->sized = has_width;
        indent->size = tab_width;
    }
}

void cli_indent(const char *path, CliIndent *indent)
{
    cli_lock_take(&s_lock);
    cli_indent_held(path, indent);
    cli_lock_give(&s_lock);
}
