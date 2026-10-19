// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_read.c: a version catalog read: each `[section]` and the entries under it, a key, bare or quoted,
// its parts joined by dots, `=` and a value, one entry a line, comments and blank lines passed; and what kept it from
// reading, each with its span. A line whose entry could not be read is passed whole, and the line after it read as
// its own. Parts of the catalog past what its storage holds are not kept, and it is marked full
#include "version_catalog_internal.h"

CliVersionsParsed g_cli_versions;
CliLock g_cli_versions_lock = CLI_LOCK_INIT;

// no letter, past the text's end
#define CLI_VERSIONS_END 0xffffffffu

// A reader of a catalog: its text and the byte it stands at
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
} CliVersionsReader;

// the letter the reader stands at and the bytes it takes, CLI_VERSIONS_END past the text's end
static unsigned int read_peek(const CliVersionsReader *read, unsigned long long *length)
{
    if (read->at >= read->size)
    {
        *length = 0ull;
        return CLI_VERSIONS_END;
    }
    return cli_text_letter(read->text, read->size, read->at, length);
}

// `size` bytes of `bytes` kept in the pool: where they start, CLI_VERSIONS_POOL where they do not fit
static unsigned int read_kept(const char *bytes, unsigned long long size)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    if (size + 1u > parsed->pool_room - parsed->used)
    {
        parsed->full = 1;
        return CLI_VERSIONS_POOL;
    }
    const unsigned int at = parsed->used;
    memor.cpy(&(MemoriaCfg){.dst = parsed->pool + at, .src = bytes, .bytes = (size_t)size});
    parsed->pool[at + size] = '\0';
    parsed->used += (unsigned int)size + 1u;
    return at;
}

// what kept the catalog from reading put with its span
static void read_error(unsigned long long from, unsigned long long to, const char *bytes, unsigned long long size)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    const unsigned int at = read_kept(bytes, size);
    if ((parsed->errors == parsed->errors_room) || (at == CLI_VERSIONS_POOL))
    {
        parsed->full = 1;
        return;
    }
    parsed->error[parsed->errors] = (CliVersionsError){from, to, at, (unsigned int)size};
    parsed->errors += 1u;
}

// what kept the catalog from reading, a text, put with its span
static void read_said(unsigned long long from, unsigned long long to, const char *message)
{
    read_error(from, to, message, cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}));
}

// the byte of the first of `stops` from the reader's place, the text's end where there is none
static unsigned long long read_find(const CliVersionsReader *read, const char *stops)
{
    for (unsigned long long at = read->at; at < read->size; at += 1u)
    {
        if (cellul.chr(&(CatenaFinitaCfg){.src = stops, .cap = CLI_CELLS_POOL, .byte = read->text[at]}) != NULL)
        {
            return at;
        }
    }
    return read->size;
}

// spaces passed, and blank lines and comments with them
static void read_blank(CliVersionsReader *read)
{
    for (;;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = read_peek(read, &length);
        if (letter == '#')
        {
            const unsigned long long end = read_find(read, "\n");
            read->at = (end < read->size) ? end + 1u : end;
        }
        else if ((letter != CLI_VERSIONS_END) && cli_text_white(letter))
        {
            read->at += length;
        }
        else
        {
            return;
        }
    }
}

// spaces and tabs on the line passed
static void read_spaces(CliVersionsReader *read)
{
    while ((read->at < read->size) && ((read->text[read->at] == ' ') || (read->text[read->at] == '\t')))
    {
        read->at += 1u;
    }
}

// the rest of the line passed
static void read_skip_line(CliVersionsReader *read)
{
    const unsigned long long end = read_find(read, "\n");
    read->at = (end < read->size) ? end + 1u : end;
}

// what follows a value on its line, a comment or nothing, passed with the line
static void read_rest_of_line(CliVersionsReader *read)
{
    read_spaces(read);
    if (read->at < read->size)
    {
        const char here = read->text[read->at];
        if ((here != '\n') && (here != '\r') && (here != '#'))
        {
            read_said(read->at, read_find(read, "\n"), "A line holds one entry");
        }
    }
    read_skip_line(read);
}

// a string in `quote`s, its text written into `out` and the span inside its quotes into `from` and `to`: 1, or 0 where
// it is not closed on its line
static int read_string(CliVersionsReader *read, char quote, CliLine *out, unsigned long long *from,
                       unsigned long long *to)
{
    const unsigned long long start = read->at;
    read->at += 1u;
    *from = read->at;
    for (;;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = read_peek(read, &length);
        if ((letter == CLI_VERSIONS_END) || (letter == '\n'))
        {
            break;
        }
        if (letter == (unsigned char)quote)
        {
            *to = read->at;
            read->at += 1u;
            return 1;
        }
        if ((letter == '\\') && (quote == '"'))
        {
            read->at += 1u;
            if (read_peek(read, &length) != CLI_VERSIONS_END)
            {
                cli_line_counted(out, read->text + read->at, length);
                read->at += length;
            }
            continue;
        }
        cli_line_counted(out, read->text + read->at, length);
        read->at += length;
    }
    read_said(start, read->at, "A string is closed on its line");
    return 0;
}

// whether a letter goes in a bare key
static int read_bare_letter(unsigned int letter)
{
    return (letter != CLI_VERSIONS_END) && (cli_text_alphanumeric(letter) || (letter == '_') || (letter == '-'));
}

// a key, bare or quoted, its parts joined by dots, into `out`: 1, or 0 where none is there
static int read_key(CliVersionsReader *read, CliLine *out)
{
    for (;;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = read_peek(read, &length);
        if ((letter == '"') || (letter == '\''))
        {
            unsigned long long from = 0ull;
            unsigned long long to = 0ull;
            if (!read_string(read, (char)letter, out, &from, &to))
            {
                return 0;
            }
        }
        else if (read_bare_letter(letter))
        {
            const unsigned long long start = read->at;
            while (read_bare_letter(read_peek(read, &length)))
            {
                read->at += length;
            }
            cli_line_counted(out, read->text + start, read->at - start);
        }
        else
        {
            const unsigned long long end = read_find(read, "\n");
            const unsigned long long past = (end > read->at + 1u) ? end : read->at + 1u;
            read_said(read->at, (past < read->size) ? past : read->size, "An entry starts with its name");
            return 0;
        }
        read_spaces(read);
        if ((read->at >= read->size) || (read->text[read->at] != '.'))
        {
            return 1;
        }
        read->at += 1u;
        read_spaces(read);
        cli_line_character(out, '.');
    }
}

static unsigned int read_field(CliVersionsReader *read);

// a field linked after the last of a table's or a section's fields
static void read_linked(unsigned int *first, unsigned int *last, unsigned int *count, unsigned int field)
{
    if (*first == CLI_VERSIONS_NONE)
    {
        *first = field;
    }
    else
    {
        g_cli_versions.field[*last].next = field;
    }
    *last = field;
    *count += 1u;
}

// a table of fields read into the field at `field`: 1, or 0 where a field of it could not be read
static int read_table(CliVersionsReader *read, CliVersionsField *field)
{
    read->at += 1u;
    field->shape = CLI_VERSIONS_TABLE;
    unsigned int last = CLI_VERSIONS_NONE;
    for (;;)
    {
        read_spaces(read);
        const char here = (read->at < read->size) ? read->text[read->at] : '\n';
        if (here == '}')
        {
            read->at += 1u;
            return 1;
        }
        if (here == ',')
        {
            read->at += 1u;
            continue;
        }
        if (here == '\n')
        {
            read_said((read->at > 0u) ? read->at - 1u : 0u, read->at, "A table is closed with `}` on its line");
            return 1;
        }
        const unsigned int child = read_field(read);
        if (child == CLI_VERSIONS_NONE)
        {
            return 0;
        }
        read_linked(&field->first, &last, &field->count, child);
    }
}

// a list of strings read into the field at `field`: 1, or 0 where a string of it is not closed
static int read_list(CliVersionsReader *read, CliVersionsField *field)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    const unsigned long long start = read->at;
    read->at += 1u;
    field->shape = CLI_VERSIONS_LIST;
    field->first = parsed->items;
    for (;;)
    {
        read_blank(read);
        if (read->at >= read->size)
        {
            read_said(start, start + 1u, "A list is closed with `]`");
            return 1;
        }
        const char here = read->text[read->at];
        if (here == ']')
        {
            read->at += 1u;
            return 1;
        }
        if (here == ',')
        {
            read->at += 1u;
            continue;
        }
        if ((here == '"') || (here == '\''))
        {
            CliLine text = {parsed->line, parsed->longest + 1u, 0ull};
            unsigned long long from = 0ull;
            unsigned long long to = 0ull;
            if (!read_string(read, here, &text, &from, &to))
            {
                return 0;
            }
            const unsigned int at = read_kept(parsed->line, text.at);
            if ((parsed->items < parsed->items_room) && (at != CLI_VERSIONS_POOL) &&
                (parsed->items == field->first + field->count))
            {
                parsed->item[parsed->items] = (CliVersionsItem){at, (unsigned int)text.at, from, to};
                parsed->items += 1u;
                field->count += 1u;
            }
            continue;
        }
        const unsigned long long end = read_find(read, ",]\n");
        read_said(read->at, end, "A bundle lists its libraries' aliases as strings");
        read->at = end;
    }
}

// a value read into the field at `field`: 1, or 0 where none could be read
static int read_value(CliVersionsReader *read, CliVersionsField *field)
{
    if (read->at >= read->size)
    {
        return 0;
    }
    const char here = read->text[read->at];
    if ((here == '"') || (here == '\''))
    {
        CliLine text = {g_cli_versions.line, g_cli_versions.longest + 1u, 0ull};
        if (!read_string(read, here, &text, &field->value_from, &field->value_to))
        {
            return 0;
        }
        field->shape = CLI_VERSIONS_TEXT;
        field->text_at = read_kept(g_cli_versions.line, text.at);
        field->text_size = (unsigned int)text.at;
        return 1;
    }
    if (here == '{')
    {
        return read_table(read, field);
    }
    if (here == '[')
    {
        return read_list(read, field);
    }
    const unsigned long long start = read->at;
    const unsigned long long end = read_find(read, ",}]\n#");
    read->at = end;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(read->text + start, end - start, &from, &to);
    field->shape = CLI_VERSIONS_BARE;
    field->text_at = read_kept(read->text + start + from, to - from);
    field->text_size = (unsigned int)(to - from);
    return 1;
}

// a key, `=`, and a value: the field's place, CLI_VERSIONS_NONE where it could not be read
static unsigned int read_field(CliVersionsReader *read)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    const unsigned long long from = read->at;
    // the key in the line's cell, and a message naming it after
    CliLine key = {parsed->line, parsed->longest + 1u, 0ull};
    if (!read_key(read, &key))
    {
        return CLI_VERSIONS_NONE;
    }
    const unsigned long long to = read->at;
    read_spaces(read);
    if ((read->at >= read->size) || (read->text[read->at] != '='))
    {
        const unsigned long long end = read_find(read, "\n");
        CliLine said = {parsed->line + parsed->longest + 1u, parsed->longest + CLI_VERSIONS_SAID, 0ull};
        cli_line_character(&said, '`');
        cli_line_counted(&said, parsed->line, key.at);
        cli_line_text(&said, "` is given its value with `=`");
        read_error(from, (end > to) ? end : to, said.out, said.at);
        return CLI_VERSIONS_NONE;
    }
    read->at += 1u;
    read_spaces(read);
    if (parsed->fields == parsed->fields_room)
    {
        parsed->full = 1;
        return CLI_VERSIONS_NONE;
    }
    const unsigned int at = parsed->fields;
    parsed->fields += 1u;
    CliVersionsField *const field = &parsed->field[at];
    memor.set(&(MemoriaCfg){.dst = field, .val = 0, .bytes = sizeof(*field)});
    field->key_at = read_kept(parsed->line, key.at);
    field->key_size = (unsigned int)key.at;
    field->from = from;
    field->to = to;
    field->first = CLI_VERSIONS_NONE;
    field->next = CLI_VERSIONS_NONE;
    return read_value(read, field) ? at : CLI_VERSIONS_NONE;
}

// a section's name read from `[`
static void read_section(CliVersionsReader *read)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    const unsigned long long from = read->at + 1u;
    const unsigned long long end = read_find(read, "\n]");
    if ((end < read->size) && (read->text[end] == ']'))
    {
        unsigned long long name_from = 0ull;
        unsigned long long name_to = 0ull;
        cli_text_trimmed(read->text + from, end - from, &name_from, &name_to);
        if (parsed->sections < parsed->sections_room)
        {
            const unsigned int name = read_kept(read->text + from + name_from, name_to - name_from);
            parsed->section[parsed->sections] = (CliVersionsSection){
                name, (unsigned int)(name_to - name_from), from, end, CLI_VERSIONS_NONE, CLI_VERSIONS_NONE, 0u};
            parsed->sections += 1u;
        }
        else
        {
            parsed->full = 1;
        }
        read->at = end + 1u;
        return;
    }
    read_said(read->at, end, "A section's name is closed with `]`");
    read->at = end;
}

int cli_versions_parse(const char *text, unsigned long long size)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    parsed->sections = 0u;
    parsed->fields = 0u;
    parsed->items = 0u;
    parsed->errors = 0u;
    parsed->used = 0u;
    parsed->full = 0;
    if (!cli_versions_taken(text, size))
    {
        parsed->full = 1;
        return 0;
    }
    CliVersionsReader read = {text, size, 0ull};
    for (;;)
    {
        read_blank(&read);
        if (read.at >= size)
        {
            return 1;
        }
        if (text[read.at] == '[')
        {
            read_section(&read);
            read_rest_of_line(&read);
            continue;
        }
        const unsigned long long start = read.at;
        const unsigned int field = read_field(&read);
        if (field == CLI_VERSIONS_NONE)
        {
            read_skip_line(&read);
            continue;
        }
        if (parsed->sections == 0u)
        {
            read_said(start, read.at,
                      "An entry stands under a section: `[versions]`, `[libraries]`, `[bundles]` or `[plugins]`");
        }
        else
        {
            CliVersionsSection *const section = &parsed->section[parsed->sections - 1u];
            read_linked(&section->first, &section->last, &section->count, field);
        }
        read_rest_of_line(&read);
    }
}
