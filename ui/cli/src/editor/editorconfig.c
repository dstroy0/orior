// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editorconfig.c: a file's indentation from the `.editorconfig` files above it
#include "editorconfig.h"
#include "../text/text.h"

#define CLI_VALUE_MAX_LEN 64u
// A key's value as the last section that names the file sets it, lower case: 1 in `set` where one did
typedef struct
{
    char text[CLI_VALUE_MAX_LEN];
    int set;
} CliValue;
// the lock acquired while the files' bytes and a pattern's characters are stored

// the max bytes one `.editorconfig` contains, the max folders above a file, and the longest value of a key stored
#define CLI_EDITORCONFIG_BYTES (1u << 20u)

#define CLI_EDITORCONFIG_FOLDERS 256u

static unsigned char s_text[CLI_EDITORCONFIG_BYTES];

typedef struct
{
    const char **from_pointer;
    const char **to_pointer;
    const char *from;
    const char *to;
    const char *word_text;
    const char *text;
    unsigned long long size;
    CliValue *value;
    const char *relative;
    const char *name;
    CliValue *style;
    CliValue *size_value;
    CliValue *width;
    const CliValue *value_read;
    unsigned int *number;
    char character;
    const char *path;
    CliIndent *indent;
    unsigned long long *ends;
    unsigned int folders;
    unsigned long long folder;
    const char *leaf;
} EditorconfigCtx;
// The bytes from `from` up to `to` with the ASCII spaces at each end left off
EMBED_INLINE void cli_trim(const EditorconfigCtx *args)
{
    const char **from = args->from_pointer;
    const char **to = args->to_pointer;
    *from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = *from, .at = 0ULL,
                        .size = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
    *to = *from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = *from,
                             .at = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
}

// whether the bytes from `from` up to `to` are `word`, the ASCII characters of either case the same
EMBED_INLINE int cli_folded_is(const EditorconfigCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_POOL_BYTES});
    if ((unsigned long long)(args->to - args->from) != length)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        const char character =
            ((args->from[at] >= 'A') && (args->from[at] <= 'Z')) ? (char)(args->from[at] + 32) : args->from[at];
        if (character != args->word_text[at])
        {
            return 0;
        }
    }
    return 1;
}

// whether the file's lines before its first section contain `root = true`
EMBED_INLINE int cli_is_root(const EditorconfigCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const char *line = text;
    const char *const end = text + size;
    while (line < end)
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *from = line;
        const char *to = stop;
        EMBED_CALL(cli_trim, EditorconfigCtx, .from_pointer = &from, .to_pointer = &to);
        if ((from < to) && (from[0] == '['))
        {
            return 0;
        }
        const char *equals = line;
        equals += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = equals, .at = 0ULL,
                             .size = (unsigned long long)(stop - equals), .set = "=");
        const char *key = line;
        const char *key_end = equals;
        const char *value = equals + 1;
        const char *value_end = stop;
        EMBED_CALL(cli_trim, EditorconfigCtx, .from_pointer = &key, .to_pointer = &key_end);
        EMBED_CALL(cli_trim, EditorconfigCtx, .from_pointer = &value, .to_pointer = &value_end);
        if ((equals < stop) &&
            EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = key, .to = key_end, .word_text = "root") &&
            EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = value, .to = value_end, .word_text = "true"))
        {
            return 1;
        }
        line = stop + 1;
    }
    return 0;
}

EMBED_INLINE void cli_value_store(const EditorconfigCtx *args)
{
    VerbaLinea line = {args->value->text, sizeof(args->value->text), 0ULL};
    for (const char *at = args->from; at < args->to; at += 1)
    {
        mmgr_line_ch(&line, ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0]);
    }
    // a value too long to store is stored as none of the words or numbers it is read for
    args->value->set = mmgr_line_ok(&line) ? 1 : 2;
    mmgr_line_finish(&line);
}

// the lines of one `.editorconfig`, each key of a section that names the file over what was stored before
EMBED_INLINE void cli_sections_read(const EditorconfigCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const char *relative = args->relative;
    const char *name = args->name;
    const char *line = text;
    const char *const end = text + size;
    int inside = 0;
    while (line < end)
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *from = line;
        const char *to = stop;
        line = stop + 1;
        EMBED_CALL(cli_trim, EditorconfigCtx, .from_pointer = &from, .to_pointer = &to);
        if ((from == to) || (from[0] == '#') || (from[0] == ';'))
        {
            continue;
        }
        if ((from[0] == '[') && ((to - from) >= 2) && (to[-1] == ']'))
        {
            inside = EMBED_CALL(cli_editorconfig_pattern.names, CliEditorconfigPatternCfg, .pattern = from + 1,
                                .size = (unsigned long long)(to - from - 2), .relative = relative, .name = name);
            continue;
        }
        const char *equals = from;
        equals += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = equals, .at = 0ULL,
                             .size = (unsigned long long)(to - equals), .set = "=");
        if ((equals == to) || !inside)
        {
            continue;
        }
        const char *key = from;
        const char *key_end = equals;
        const char *value = equals + 1;
        const char *value_end = to;
        EMBED_CALL(cli_trim, EditorconfigCtx, .from_pointer = &key, .to_pointer = &key_end);
        EMBED_CALL(cli_trim, EditorconfigCtx, .from_pointer = &value, .to_pointer = &value_end);
        CliValue *target;
        if (EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = key, .to = key_end, .word_text = "indent_style"))
        {
            target = args->style;
        }
        else if (EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = key, .to = key_end, .word_text = "indent_size"))
        {
            target = args->size_value;
        }
        else if (EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = key, .to = key_end, .word_text = "tab_width"))
        {
            target = args->width;
        }
        else
        {
            target = NULL;
        }
        if (target != NULL)
        {
            EMBED_CALL(cli_value_store, EditorconfigCtx, .value = target, .from = value, .to = value_end);
        }
    }
}

// the whole number a stored value writes, as an unsigned 32-bit number reads: 1, or 0 where it writes none
EMBED_INLINE int cli_value_number(const EditorconfigCtx *args)
{
    unsigned int *number = args->number;
    if (args->value_read->set != 1)
    {
        return 0;
    }
    const char *at = args->value_read->text;
    at += (at[0] == '+') ? 1 : 0;
    if (at[0] == '\0')
    {
        return 0;
    }
    unsigned long long size = 0ULL;
    const unsigned long long digits = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_POOL_BYTES});
    if (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL, .size = digits,
                   .classes = CLI_TEXT_DIGITS, .set = "") != digits)
    {
        return 0;
    }
    size = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = at, .at = 0ULL, .size = digits, .end = NULL);
    if (size > 0xffffffffULL)
    {
        return 0;
    }
    // checked against the max 32 bits represent above
    *number = (unsigned int)size;
    return 1;
}

EMBED_INLINE int cli_is_separator(const EditorconfigCtx *args)
{
    return (args->character == '/') || (args->character == '\\');
}

// the `.editorconfig` of the folder `folder` characters of `path` long, `leaf` its name after them, read into s_text: its
// size, or QRY_MEMBER_UNREAD where it is not read
EMBED_INLINE unsigned long long cli_editorconfig_read(const EditorconfigCtx *args)
{
    char file[CLI_PATH_MAX_LEN];
    VerbaLinea line = {file, sizeof(file), 0ULL};
    mmgr_line_put_n(&line, args->path, args->folder);
    mmgr_line_put(&line, args->leaf);
    return (mmgr_line_finish(&line) != 0ULL) ? qry_member_file_read(file, s_text, sizeof(s_text)) : QRY_MEMBER_UNREAD;
}

// each folder from the file's up with an `.editorconfig` read, the length of `path` it is, into `ends`, until one
// declares it is the root, the top is reached or CLI_EDITORCONFIG_FOLDERS are: how many
EMBED_INLINE unsigned int cli_folders_found(const EditorconfigCtx *args)
{
    unsigned int folders = 0u;
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    int going = 1;
    while (going && (folders < CLI_EDITORCONFIG_FOLDERS))
    {
        unsigned long long cut = end;
        while ((cut > 0ULL) && !EMBED_CALL(cli_is_separator, EditorconfigCtx, .character = args->path[cut - 1u]))
        {
            cut -= 1ULL;
        }
        if (cut == 0ULL)
        {
            going = 0;
            continue;
        }
        const int top = (cut == 1ULL) || ((cut == 3ULL) && (args->path[1] == ':'));
        const unsigned long long folder = top ? cut : (cut - 1ULL);
        if (folder == end)
        {
            going = 0;
            continue;
        }
        const unsigned long long size = EMBED_CALL(cli_editorconfig_read, EditorconfigCtx, .path = args->path,
                                                   .folder = folder, .leaf = top ? ".editorconfig" : "/.editorconfig");
        if (size != QRY_MEMBER_UNREAD)
        {
            args->ends[folders] = folder;
            folders += 1u;
            // the file's bytes read as text: a char and an unsigned char share their width
            going = !EMBED_CALL(cli_is_root, EditorconfigCtx, .text = (const char *)s_text, .size = size);
        }
        going = going && !top;
        end = cut - 1ULL;
    }
    return folders;
}

// the `.editorconfig` of each of the `folders` folders at `ends`, from the topmost down, its sections that name the
// file by its `name` or by its path from the folder read into `style`, `size_value` and `width`
EMBED_INLINE void cli_folders_read(const EditorconfigCtx *args)
{
    for (unsigned int at = args->folders; at > 0u; at -= 1u)
    {
        const unsigned long long folder = args->ends[at - 1u];
        const char *const inside =
            args->path + folder +
            (EMBED_CALL(cli_is_separator, EditorconfigCtx, .character = args->path[folder]) ? 1u : 0u);
        char relative[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = relative,
                                .src = inside,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = inside, .cap = CLI_POOL_BYTES}) + 1u});
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = relative,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_POOL_BYTES}), .from = '\\',
                   .to = '/');
        const int separated =
            (folder != 0ULL) && EMBED_CALL(cli_is_separator, EditorconfigCtx, .character = args->path[folder - 1u]);
        const unsigned long long size =
            EMBED_CALL(cli_editorconfig_read, EditorconfigCtx, .path = args->path, .folder = folder,
                       .leaf = separated ? ".editorconfig" : "/.editorconfig");
        if (size != QRY_MEMBER_UNREAD)
        {
            EMBED_CALL(cli_sections_read, EditorconfigCtx, .text = (const char *)s_text, .size = size,
                       .relative = relative, .name = args->name, .style = args->style, .size_value = args->size_value,
                       .width = args->width);
        }
    }
}

// the file's indentation into `indent` from the values read for it: its style from `style`, and its size from
// `size_value`, or from `width` where the size is unset or "tab"
EMBED_INLINE void cli_indent_set(const EditorconfigCtx *args)
{
    if (args->style->set != 1)
    {
        args->indent->tabs = -1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->style->text, .other = "tab", .cap = CLI_POOL_BYTES}))
    {
        args->indent->tabs = 1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->style->text, .other = "space", .cap = CLI_POOL_BYTES}))
    {
        args->indent->tabs = 0;
    }
    else
    {
        args->indent->tabs = -1;
    }
    unsigned int tab_width = 0u;
    const int has_width =
        EMBED_CALL(cli_value_number, EditorconfigCtx, .value_read = args->width, .number = &tab_width);
    unsigned int number = 0u;
    if (args->size_value->set == 0)
    {
        args->indent->sized = (args->indent->tabs == 1) && has_width;
        args->indent->size = tab_width;
    }
    else if (((args->size_value->set != 1) ||
              !cellul.eq(&(CatenaFinitaCfg){.src = args->size_value->text, .other = "tab", .cap = CLI_POOL_BYTES})) &&
             EMBED_CALL(cli_value_number, EditorconfigCtx, .value_read = args->size_value, .number = &number))
    {
        args->indent->sized = 1;
        args->indent->size = number;
    }
    else
    {
        args->indent->sized = has_width;
        args->indent->size = tab_width;
    }
}

EMBED_INLINE void cli_indent_resolve(const EditorconfigCtx *args)
{
    unsigned long long ends[CLI_EDITORCONFIG_FOLDERS];
    const unsigned int folders = EMBED_CALL(cli_folders_found, EditorconfigCtx, .path = args->path, .ends = ends);
    CliValue style = {{0}, 0};
    CliValue size_value = {{0}, 0};
    CliValue width = {{0}, 0};
    const char *name = args->path + cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    while ((name > args->path) && !EMBED_CALL(cli_is_separator, EditorconfigCtx, .character = name[-1]))
    {
        name -= 1;
    }
    EMBED_CALL(cli_folders_read, EditorconfigCtx, .path = args->path, .ends = ends, .folders = folders, .name = name,
               .style = &style, .size_value = &size_value, .width = &width);
    EMBED_CALL(cli_indent_set, EditorconfigCtx, .style = &style, .size_value = &size_value, .width = &width,
               .indent = args->indent);
}

void cli_indent(const CliEditorconfigCfg *args)
{
    EMBED_CALL(cli_indent_resolve, EditorconfigCtx, .path = args->path, .indent = args->indent);
}
