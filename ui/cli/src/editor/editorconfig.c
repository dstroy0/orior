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
// the lock acquired while the files' bytes and a pattern's letters are stored
static CliLock s_lock = CLI_LOCK_INIT;

// the most bytes one `.editorconfig` contains, the most folders above a file, and the longest value of a key stored
#define CLI_EDITORCONFIG_BYTES (1u << 20u)

#define CLI_EDITORCONFIG_FOLDERS 256u

static unsigned char s_text[CLI_EDITORCONFIG_BYTES];

typedef struct
{
    const char **from_pointer;
    const char **to_pointer;
    const char *from;
    const char *to;
    const char *word;
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
    char letter;
    const char *path;
    CliIndent *indent;
} EditorconfigCtx;
// The bytes from `from` up to `to` with the ASCII spaces at each end left off
EMBED_INLINE void cli_trim(const EditorconfigCtx *args)
{
    const char **from = args->from_pointer;
    const char **to = args->to_pointer;
    *from += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = *from, .at = 0ull,
                        .size = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
    *to = *from + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = *from,
                             .at = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
}

// whether the bytes from `from` up to `to` are `word`, the ASCII letters of either case the same
EMBED_INLINE int cli_folded_is(const EditorconfigCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_CELLS_POOL});
    if ((unsigned long long)(args->to - args->from) != length)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char letter =
            ((args->from[at] >= 'A') && (args->from[at] <= 'Z')) ? (char)(args->from[at] + 32) : args->from[at];
        if (letter != args->word[at])
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
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ull,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *from = line;
        const char *to = stop;
        EMBED_CALL(cli_trim, EditorconfigCtx, .from_pointer = &from, .to_pointer = &to);
        if ((from < to) && (from[0] == '['))
        {
            return 0;
        }
        const char *equals = line;
        equals += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = equals, .at = 0ull,
                             .size = (unsigned long long)(stop - equals), .set = "=");
        const char *key = line;
        const char *key_end = equals;
        const char *value = equals + 1;
        const char *value_end = stop;
        EMBED_CALL(cli_trim, EditorconfigCtx, .from_pointer = &key, .to_pointer = &key_end);
        EMBED_CALL(cli_trim, EditorconfigCtx, .from_pointer = &value, .to_pointer = &value_end);
        if ((equals < stop) && EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = key, .to = key_end, .word = "root") &&
            EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = value, .to = value_end, .word = "true"))
        {
            return 1;
        }
        line = stop + 1;
    }
    return 0;
}

EMBED_INLINE void cli_value_store(const EditorconfigCtx *args)
{
    VerbaLinea line = {args->value->text, sizeof(args->value->text), 0ull};
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
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ull,
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
        equals += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = equals, .at = 0ull,
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
        CliValue *const target =
            EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = key, .to = key_end, .word = "indent_style") ? args->style
            : EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = key, .to = key_end, .word = "indent_size")
                ? args->size_value
            : EMBED_CALL(cli_folded_is, EditorconfigCtx, .from = key, .to = key_end, .word = "tab_width") ? args->width
                                                                                                          : NULL;
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
    unsigned long long size = 0ull;
    const unsigned long long digits = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL});
    if (EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = at, .at = 0ull, .size = digits,
                   .classes = CLI_TEXT_DIGITS, .set = "") != digits)
    {
        return 0;
    }
    size = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = at, .at = 0ull, .size = digits, .end = NULL);
    if (size > 0xffffffffull)
    {
        return 0;
    }
    at += digits;
    // checked against the most 32 bits represent above
    *number = (unsigned int)size;
    return 1;
}

EMBED_INLINE int cli_is_separator(const EditorconfigCtx *args)
{
    return (args->letter == '/') || (args->letter == '\\');
}

EMBED_INLINE void cli_indent_resolve(const EditorconfigCtx *args)
{
    unsigned long long ends[CLI_EDITORCONFIG_FOLDERS];
    unsigned int folders = 0u;
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    // each folder from the file's up, the length of `path` it is, until one declares it is the root
    while (folders < CLI_EDITORCONFIG_FOLDERS)
    {
        unsigned long long cut = end;
        while ((cut > 0ull) && !EMBED_CALL(cli_is_separator, EditorconfigCtx, .letter = args->path[cut - 1u]))
        {
            cut -= 1ull;
        }
        if (cut == 0ull)
        {
            break;
        }
        const int top = (cut == 1ull) || ((cut == 3ull) && (args->path[1] == ':'));
        const unsigned long long folder = top ? cut : (cut - 1ull);
        if (folder == end)
        {
            break;
        }
        char file[CLI_PATH_MAX_LEN];
        VerbaLinea line = {file, sizeof(file), 0ull};
        mmgr_line_put_n(&line, args->path, folder);
        mmgr_line_put(&line, top ? ".editorconfig" : "/.editorconfig");
        const unsigned long long size =
            (mmgr_line_finish(&line) != 0ull) ? qry_member_file_read(file, s_text, sizeof(s_text)) : QRY_MEMBER_UNREAD;
        if (size != QRY_MEMBER_UNREAD)
        {
            ends[folders] = folder;
            folders += 1u;
            // the file's bytes read as text: a char and an unsigned char share their width
            if (EMBED_CALL(cli_is_root, EditorconfigCtx, .text = (const char *)s_text, .size = size))
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
    const char *name = args->path + cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    while ((name > args->path) && !EMBED_CALL(cli_is_separator, EditorconfigCtx, .letter = name[-1]))
    {
        name -= 1;
    }
    for (unsigned int at = folders; at > 0u; at -= 1u)
    {
        const unsigned long long folder = ends[at - 1u];
        char relative[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){
            .dst = relative,
            .src = args->path + folder +
                   (EMBED_CALL(cli_is_separator, EditorconfigCtx, .letter = args->path[folder]) ? 1u : 0u),
            .bytes = cellul.len(&(CatenaFinitaCfg){
                         .src = args->path + folder +
                                (EMBED_CALL(cli_is_separator, EditorconfigCtx, .letter = args->path[folder]) ? 1u : 0u),
                         .cap = CLI_CELLS_POOL}) +
                     1u});
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = relative,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_CELLS_POOL}), .from = '\\',
                   .to = '/');
        char file[CLI_PATH_MAX_LEN];
        VerbaLinea line = {file, sizeof(file), 0ull};
        mmgr_line_put_n(&line, args->path, folder);
        mmgr_line_put(&line, ((folder != 0ull) &&
                              EMBED_CALL(cli_is_separator, EditorconfigCtx, .letter = args->path[folder - 1u]))
                                 ? ".editorconfig"
                                 : "/.editorconfig");
        const unsigned long long size =
            (mmgr_line_finish(&line) != 0ull) ? qry_member_file_read(file, s_text, sizeof(s_text)) : QRY_MEMBER_UNREAD;
        if (size != QRY_MEMBER_UNREAD)
        {
            EMBED_CALL(cli_sections_read, EditorconfigCtx, .text = (const char *)s_text, .size = size,
                       .relative = relative, .name = name, .style = &style, .size_value = &size_value, .width = &width);
        }
    }
    args->indent->tabs = (style.set != 1) ? -1
                         : (cellul.eq(&(CatenaFinitaCfg){.src = style.text, .other = "tab", .cap = CLI_CELLS_POOL})) ? 1
                         : (cellul.eq(&(CatenaFinitaCfg){.src = style.text, .other = "space", .cap = CLI_CELLS_POOL}))
                             ? 0
                             : -1;
    unsigned int tab_width = 0u;
    const int has_width = EMBED_CALL(cli_value_number, EditorconfigCtx, .value_read = &width, .number = &tab_width);
    unsigned int number = 0u;
    if (size_value.set == 0)
    {
        args->indent->sized = (args->indent->tabs == 1) && has_width;
        args->indent->size = tab_width;
    }
    else if ((size_value.set == 1) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = size_value.text, .other = "tab", .cap = CLI_CELLS_POOL})))
    {
        args->indent->sized = has_width;
        args->indent->size = tab_width;
    }
    else if (EMBED_CALL(cli_value_number, EditorconfigCtx, .value_read = &size_value, .number = &number))
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

void cli_indent(const CliEditorconfigCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    EMBED_CALL(cli_indent_resolve, EditorconfigCtx, .path = args->path, .indent = args->indent);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}
