// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_read.c: a version catalog read: each `[section]` and the entries under it, a key, bare or quoted,
// its parts joined by dots, `=` and a value, one entry a line, comments and blank lines passed; and what prevented it
// from reading, each with its span. A line whose entry could not be read is passed whole, and the line after it read as
// its own. Parts of the catalog past what its storage contains are not stored, and it is marked full
#include "../text/text.h"
#include "version_catalog_internal.h"

// A reader of a catalog: its text and the byte it stands at
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
} CliVersionsReader;
CliVersionsParsed g_cli_versions;

// no letter, past the text's end
#define CLI_VERSIONS_END 0xffffffffu

typedef struct
{
    const CliVersionsReader *read_read;
    unsigned long long *length;
    const char *bytes;
    unsigned long long size;
    unsigned long long from;
    unsigned long long to;
    const char *message;
    const char *stops;
    CliVersionsReader *read;
    char quote;
    VerbaLinea *out;
    unsigned long long *from_pointer;
    unsigned long long *to_pointer;
    unsigned int letter;
    unsigned int *first;
    unsigned int *last;
    unsigned int *count;
    unsigned int field_u32;
    CliVersionsField *field;
} VersionCatalogReadCtx;
// the letter the reader stands at and the bytes it takes, CLI_VERSIONS_END past the text's end
EMBED_INLINE unsigned int read_peek(const VersionCatalogReadCtx *args)
{
    unsigned long long *length = args->length;
    if (args->read_read->at >= args->read_read->size)
    {
        *length = 0ull;
        return CLI_VERSIONS_END;
    }
    return EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->read_read->text, .size = args->read_read->size,
                      .at = args->read_read->at, .length = length);
}

// `size` bytes of `bytes` stored in the pool: where they start, CLI_VERSIONS_POOL where they do not fit
EMBED_INLINE unsigned int read_text_store(const VersionCatalogReadCtx *args)
{
    const char *bytes = args->bytes;
    CliVersionsParsed *const parsed = &g_cli_versions;
    if (args->size + 1u > parsed->pool_capacity - parsed->used)
    {
        parsed->full = 1;
        return CLI_VERSIONS_POOL;
    }
    const unsigned int at = parsed->used;
    memor.cpy(&(MemoriaCfg){.dst = parsed->pool + at, .src = bytes, .bytes = (size_t)args->size});
    parsed->pool[at + args->size] = '\0';
    parsed->used += (unsigned int)args->size + 1u;
    return at;
}

// what prevented the catalog from reading put with its span
EMBED_INLINE void read_error(const VersionCatalogReadCtx *args)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    const unsigned int at = read_text_store(args);
    if ((parsed->errors == parsed->errors_capacity) || (at == CLI_VERSIONS_POOL))
    {
        parsed->full = 1;
        return;
    }
    parsed->error[parsed->errors] = (CliVersionsError){args->from, args->to, at, (unsigned int)args->size};
    parsed->errors += 1u;
}

// what prevented the catalog from reading, a text, put with its span
EMBED_INLINE void read_error_text(const VersionCatalogReadCtx *args)
{
    EMBED_CALL(read_error, VersionCatalogReadCtx, .from = args->from, .to = args->to, .bytes = args->message,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->message, .cap = CLI_CELLS_POOL}));
}

// the byte of the first of `stops` from the reader's place, the text's end where there is none
EMBED_INLINE unsigned long long read_find(const VersionCatalogReadCtx *args)
{
    for (unsigned long long at = args->read_read->at; at < args->read_read->size; at += 1u)
    {
        if (cellul.chr(&(CatenaFinitaCfg){
                .src = args->stops, .cap = CLI_CELLS_POOL, .byte = args->read_read->text[at]}) != NULL)
        {
            return at;
        }
    }
    return args->read_read->size;
}

// spaces passed, and blank lines and comments with them
EMBED_INLINE void read_blank(const VersionCatalogReadCtx *args)
{
    for (;;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter =
            EMBED_CALL(read_peek, VersionCatalogReadCtx, .read_read = args->read, .length = &length);
        if (letter == '#')
        {
            const unsigned long long end =
                EMBED_CALL(read_find, VersionCatalogReadCtx, .read_read = args->read, .stops = "\n");
            args->read->at = (end < args->read->size) ? end + 1u : end;
        }
        else if ((letter != CLI_VERSIONS_END) && EMBED_CALL(cli_text_table.white, CliTextCfg, .letter = letter))
        {
            args->read->at += length;
        }
        else
        {
            return;
        }
    }
}

// spaces and tabs on the line passed
EMBED_INLINE void read_spaces(const VersionCatalogReadCtx *args)
{
    args->read->at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->read->text,
                                .at = args->read->at, .size = args->read->size, .set = " \t");
}

// the rest of the line passed
EMBED_INLINE void read_skip_line(const VersionCatalogReadCtx *args)
{
    const unsigned long long end = EMBED_CALL(read_find, VersionCatalogReadCtx, .read_read = args->read, .stops = "\n");
    args->read->at = (end < args->read->size) ? end + 1u : end;
}

// what follows a value on its line, a comment or nothing, passed with the line
EMBED_INLINE void read_remainder_of_line(const VersionCatalogReadCtx *args)
{
    read_spaces(args);
    if (args->read->at < args->read->size)
    {
        const char here = args->read->text[args->read->at];
        if ((here != '\n') && (here != '\r') && (here != '#'))
        {
            EMBED_CALL(read_error_text, VersionCatalogReadCtx, .from = args->read->at,
                       .to = EMBED_CALL(read_find, VersionCatalogReadCtx, .read_read = args->read, .stops = "\n"),
                       .message = "A line has one entry");
        }
    }
    read_skip_line(args);
}

// a string in `quote`s, its text written into `out` and the span inside its quotes into `from` and `to`: 1, or 0 where
// it is not closed on its line
EMBED_INLINE int read_string(const VersionCatalogReadCtx *args)
{
    unsigned long long *from = args->from_pointer;
    unsigned long long *to = args->to_pointer;
    const unsigned long long start = args->read->at;
    args->read->at += 1u;
    *from = args->read->at;
    for (;;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter =
            EMBED_CALL(read_peek, VersionCatalogReadCtx, .read_read = args->read, .length = &length);
        if ((letter == CLI_VERSIONS_END) || (letter == '\n'))
        {
            break;
        }
        if (letter == (unsigned char)args->quote)
        {
            *to = args->read->at;
            args->read->at += 1u;
            return 1;
        }
        if ((letter == '\\') && (args->quote == '"'))
        {
            args->read->at += 1u;
            if (EMBED_CALL(read_peek, VersionCatalogReadCtx, .read_read = args->read, .length = &length) !=
                CLI_VERSIONS_END)
            {
                mmgr_line_put_n(args->out, args->read->text + args->read->at, length);
                args->read->at += length;
            }
            continue;
        }
        mmgr_line_put_n(args->out, args->read->text + args->read->at, length);
        args->read->at += length;
    }
    EMBED_CALL(read_error_text, VersionCatalogReadCtx, .from = start, .to = args->read->at,
               .message = "A string is closed on its line");
    return 0;
}

// whether a letter goes in a bare key
EMBED_INLINE int read_bare_letter(const VersionCatalogReadCtx *args)
{
    unsigned int letter = args->letter;
    return (letter != CLI_VERSIONS_END) && (EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = letter) ||
                                            (letter == '_') || (letter == '-'));
}

// a key, bare or quoted, its parts joined by dots, into `out`: 1, or 0 where none is there
EMBED_INLINE int read_key(const VersionCatalogReadCtx *args)
{
    for (;;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter =
            EMBED_CALL(read_peek, VersionCatalogReadCtx, .read_read = args->read, .length = &length);
        if ((letter == '"') || (letter == '\''))
        {
            unsigned long long from = 0ull;
            unsigned long long to = 0ull;
            if (!EMBED_CALL(read_string, VersionCatalogReadCtx, .read = args->read, .quote = (char)letter,
                            .out = args->out, .from_pointer = &from, .to_pointer = &to))
            {
                return 0;
            }
        }
        else if (EMBED_CALL(read_bare_letter, VersionCatalogReadCtx, .letter = letter))
        {
            const unsigned long long start = args->read->at;
            while (EMBED_CALL(
                read_bare_letter, VersionCatalogReadCtx,
                .letter = EMBED_CALL(read_peek, VersionCatalogReadCtx, .read_read = args->read, .length = &length)))
            {
                args->read->at += length;
            }
            mmgr_line_put_n(args->out, args->read->text + start, args->read->at - start);
        }
        else
        {
            const unsigned long long line_end =
                EMBED_CALL(read_find, VersionCatalogReadCtx, .read_read = args->read, .stops = "\n");
            const unsigned long long end = (line_end > args->read->at + 1u) ? line_end : args->read->at + 1u;
            EMBED_CALL(read_error_text, VersionCatalogReadCtx, .from = args->read->at,
                       .to = (end < args->read->size) ? end : args->read->size,
                       .message = "An entry starts with its name");
            return 0;
        }
        read_spaces(args);
        if ((args->read->at >= args->read->size) || (args->read->text[args->read->at] != '.'))
        {
            return 1;
        }
        args->read->at += 1u;
        read_spaces(args);
        mmgr_line_ch(args->out, '.');
    }
}

static unsigned int read_field(const VersionCatalogReadCtx *args);

// a field linked after the last of a table's or a section's fields
EMBED_INLINE void read_link(const VersionCatalogReadCtx *args)
{
    unsigned int *first = args->first;
    unsigned int *last = args->last;
    unsigned int *count = args->count;
    if (*first == CLI_VERSIONS_NONE)
    {
        *first = args->field_u32;
    }
    else
    {
        g_cli_versions.field[*last].next = args->field_u32;
    }
    *last = args->field_u32;
    *count += 1u;
}

// a table of fields read into the field at `field`: 1, or 0 where a field of it could not be read
static int read_table(const VersionCatalogReadCtx *args)
{
    args->read->at += 1u;
    args->field->shape = CLI_VERSIONS_TABLE;
    unsigned int last = CLI_VERSIONS_NONE;
    for (;;)
    {
        read_spaces(args);
        const char here = (args->read->at < args->read->size) ? args->read->text[args->read->at] : '\n';
        if (here == '}')
        {
            args->read->at += 1u;
            return 1;
        }
        if (here == ',')
        {
            args->read->at += 1u;
            continue;
        }
        if (here == '\n')
        {
            EMBED_CALL(read_error_text, VersionCatalogReadCtx, .from = (args->read->at > 0u) ? args->read->at - 1u : 0u,
                       .to = args->read->at, .message = "A table is closed with `}` on its line");
            return 1;
        }
        const unsigned int child = read_field(args);
        if (child == CLI_VERSIONS_NONE)
        {
            return 0;
        }
        EMBED_CALL(read_link, VersionCatalogReadCtx, .first = &args->field->first, .last = &last,
                   .count = &args->field->count, .field_u32 = child);
    }
}

// a list of strings read into the field at `field`: 1, or 0 where a string of it is not closed
EMBED_INLINE int read_list(const VersionCatalogReadCtx *args)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    const unsigned long long start = args->read->at;
    args->read->at += 1u;
    args->field->shape = CLI_VERSIONS_LIST;
    args->field->first = parsed->items;
    for (;;)
    {
        read_blank(args);
        if (args->read->at >= args->read->size)
        {
            EMBED_CALL(read_error_text, VersionCatalogReadCtx, .from = start, .to = start + 1u,
                       .message = "A list is closed with `]`");
            return 1;
        }
        const char here = args->read->text[args->read->at];
        if (here == ']')
        {
            args->read->at += 1u;
            return 1;
        }
        if (here == ',')
        {
            args->read->at += 1u;
            continue;
        }
        if ((here == '"') || (here == '\''))
        {
            VerbaLinea text = {parsed->line, parsed->longest + 1u, 0ull};
            unsigned long long from = 0ull;
            unsigned long long to = 0ull;
            if (!EMBED_CALL(read_string, VersionCatalogReadCtx, .read = args->read, .quote = here, .out = &text,
                            .from_pointer = &from, .to_pointer = &to))
            {
                return 0;
            }
            const unsigned int at =
                EMBED_CALL(read_text_store, VersionCatalogReadCtx, .bytes = parsed->line, .size = text.at);
            if ((parsed->items < parsed->items_capacity) && (at != CLI_VERSIONS_POOL) &&
                (parsed->items == args->field->first + args->field->count))
            {
                parsed->item[parsed->items] = (CliVersionsItem){at, (unsigned int)text.at, from, to};
                parsed->items += 1u;
                args->field->count += 1u;
            }
            continue;
        }
        const unsigned long long end =
            EMBED_CALL(read_find, VersionCatalogReadCtx, .read_read = args->read, .stops = ",]\n");
        EMBED_CALL(read_error_text, VersionCatalogReadCtx, .from = args->read->at, .to = end,
                   .message = "A bundle lists its libraries' aliases as strings");
        args->read->at = end;
    }
}

// a value read into the field at `field`: 1, or 0 where none could be read
static int read_value(const VersionCatalogReadCtx *args)
{
    if (args->read->at >= args->read->size)
    {
        return 0;
    }
    const char here = args->read->text[args->read->at];
    if ((here == '"') || (here == '\''))
    {
        VerbaLinea text = {g_cli_versions.line, g_cli_versions.longest + 1u, 0ull};
        if (!EMBED_CALL(read_string, VersionCatalogReadCtx, .read = args->read, .quote = here, .out = &text,
                        .from_pointer = &args->field->value_from, .to_pointer = &args->field->value_to))
        {
            return 0;
        }
        args->field->shape = CLI_VERSIONS_TEXT;
        args->field->text_at =
            EMBED_CALL(read_text_store, VersionCatalogReadCtx, .bytes = g_cli_versions.line, .size = text.at);
        args->field->text_size = (unsigned int)text.at;
        return 1;
    }
    if (here == '{')
    {
        return read_table(args);
    }
    if (here == '[')
    {
        return read_list(args);
    }
    const unsigned long long start = args->read->at;
    const unsigned long long end =
        EMBED_CALL(read_find, VersionCatalogReadCtx, .read_read = args->read, .stops = ",}]\n#");
    args->read->at = end;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = args->read->text + start,
               .size = end - start, .from_u64s = &from, .to_u64s = &to);
    args->field->shape = CLI_VERSIONS_BARE;
    args->field->text_at =
        EMBED_CALL(read_text_store, VersionCatalogReadCtx, .bytes = args->read->text + start + from, .size = to - from);
    args->field->text_size = (unsigned int)(to - from);
    return 1;
}

// a key, `=`, and a value: the field's place, CLI_VERSIONS_NONE where it could not be read
static unsigned int read_field(const VersionCatalogReadCtx *args)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    const unsigned long long from = args->read->at;
    // the key in the line's cell, and a message naming it after
    VerbaLinea key = {parsed->line, parsed->longest + 1u, 0ull};
    if (!EMBED_CALL(read_key, VersionCatalogReadCtx, .read = args->read, .out = &key))
    {
        return CLI_VERSIONS_NONE;
    }
    const unsigned long long to = args->read->at;
    read_spaces(args);
    if ((args->read->at >= args->read->size) || (args->read->text[args->read->at] != '='))
    {
        const unsigned long long end =
            EMBED_CALL(read_find, VersionCatalogReadCtx, .read_read = args->read, .stops = "\n");
        VerbaLinea msg = {parsed->line + parsed->longest + 1u, parsed->longest + CLI_VERSIONS_MSG_MAX_LEN, 0ull};
        mmgr_line_ch(&msg, '`');
        mmgr_line_put_n(&msg, parsed->line, key.at);
        mmgr_line_put(&msg, "` is given its value with `=`");
        EMBED_CALL(read_error, VersionCatalogReadCtx, .from = from, .to = (end > to) ? end : to, .bytes = msg.out,
                   .size = msg.at);
        return CLI_VERSIONS_NONE;
    }
    args->read->at += 1u;
    read_spaces(args);
    if (parsed->fields == parsed->fields_capacity)
    {
        parsed->full = 1;
        return CLI_VERSIONS_NONE;
    }
    const unsigned int at = parsed->fields;
    parsed->fields += 1u;
    CliVersionsField *const field = &parsed->field[at];
    memor.set(&(MemoriaCfg){.dst = field, .val = 0, .bytes = sizeof(*field)});
    field->key_at = EMBED_CALL(read_text_store, VersionCatalogReadCtx, .bytes = parsed->line, .size = key.at);
    field->key_size = (unsigned int)key.at;
    field->from = from;
    field->to = to;
    field->first = CLI_VERSIONS_NONE;
    field->next = CLI_VERSIONS_NONE;
    return EMBED_CALL(read_value, VersionCatalogReadCtx, .read = args->read, .field = field) ? at : CLI_VERSIONS_NONE;
}

// a section's name read from `[`
EMBED_INLINE void read_section(const VersionCatalogReadCtx *args)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    const unsigned long long from = args->read->at + 1u;
    const unsigned long long end =
        EMBED_CALL(read_find, VersionCatalogReadCtx, .read_read = args->read, .stops = "\n]");
    if ((end < args->read->size) && (args->read->text[end] == ']'))
    {
        unsigned long long name_from = 0ull;
        unsigned long long name_to = 0ull;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = args->read->text + from,
                   .size = end - from, .from_u64s = &name_from, .to_u64s = &name_to);
        if (parsed->sections < parsed->sections_capacity)
        {
            const unsigned int name =
                EMBED_CALL(read_text_store, VersionCatalogReadCtx, .bytes = args->read->text + from + name_from,
                           .size = name_to - name_from);
            parsed->section[parsed->sections] = (CliVersionsSection){
                name, (unsigned int)(name_to - name_from), from, end, CLI_VERSIONS_NONE, CLI_VERSIONS_NONE, 0u};
            parsed->sections += 1u;
        }
        else
        {
            parsed->full = 1;
        }
        args->read->at = end + 1u;
        return;
    }
    EMBED_CALL(read_error_text, VersionCatalogReadCtx, .from = args->read->at, .to = end,
               .message = "A section's name is closed with `]`");
    args->read->at = end;
}

int cli_versions_parse(const CliVersionCatalogTextCtx *args)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    parsed->sections = 0u;
    parsed->fields = 0u;
    parsed->items = 0u;
    parsed->errors = 0u;
    parsed->used = 0u;
    parsed->full = 0;
    if (!cli_version_catalog_text_internal.versions_alloc(args))
    {
        parsed->full = 1;
        return 0;
    }
    CliVersionsReader read = {args->text, args->size, 0ull};
    for (;;)
    {
        EMBED_CALL(read_blank, VersionCatalogReadCtx, .read = &read);
        if (read.at >= args->size)
        {
            return 1;
        }
        if (args->text[read.at] == '[')
        {
            EMBED_CALL(read_section, VersionCatalogReadCtx, .read = &read);
            EMBED_CALL(read_remainder_of_line, VersionCatalogReadCtx, .read = &read);
            continue;
        }
        const unsigned long long start = read.at;
        const unsigned int field = EMBED_CALL(read_field, VersionCatalogReadCtx, .read = &read);
        if (field == CLI_VERSIONS_NONE)
        {
            EMBED_CALL(read_skip_line, VersionCatalogReadCtx, .read = &read);
            continue;
        }
        if (parsed->sections == 0u)
        {
            EMBED_CALL(read_error_text, VersionCatalogReadCtx, .from = start, .to = read.at,
                       .message =
                           "An entry stands under a section: `[versions]`, `[libraries]`, `[bundles]` or `[plugins]`");
        }
        else
        {
            CliVersionsSection *const section = &parsed->section[parsed->sections - 1u];
            EMBED_CALL(read_link, VersionCatalogReadCtx, .first = &section->first, .last = &section->last,
                       .count = &section->count, .field_u32 = field);
        }
        EMBED_CALL(read_remainder_of_line, VersionCatalogReadCtx, .read = &read);
    }
}
