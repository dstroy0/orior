// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_leaves.c: a text's leaves kept for a walk over them, in a cell whose room doubles as they come: each leaf's
// line, its bytes in the text and what it is, read off its token's name's segments alone. No language is read here
// but by its plugin's grammar
#include "../command_line/cells.h"
#include "../text/text.h"
#include "symbols_internal.h"

// the room the first cell of leaves holds
#define CLI_LEAVES_FIRST 256u

// whether the segment of `name` from `from`, `length` bytes, is `segment`
static int cli_leaf_segment_is(const char *name, unsigned long long from, unsigned long long length,
                               const char *segment)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = segment, .cap = CLI_CELLS_POOL});
    return (length == size) && cli_text_same(name + from, segment, size);
}

// the operations a segment of an operator's name names, each at the place its CLI_DOES_ number counts from 1
static const char *const s_operations[CLI_DOES_COUNT] = {
    "not",    "complement", "multiply", "divide", "remainder", "add", "subtract", "shiftleft", "shiftright", "less",
    "atmost", "greater",    "atleast",  "equal",  "unequal",   "and", "or",       "bitand",    "bitxor",     "bitor"};

// the operation the segment of `name` from `from`, `length` bytes, names, a CLI_DOES_ number, 0 where it names none
static unsigned char cli_leaf_operation(const char *name, unsigned long long from, unsigned long long length)
{
    for (unsigned int at = 0u; at < CLI_DOES_COUNT; at += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_operations[at], .cap = CLI_CELLS_POOL});
        if ((length == size) && cli_text_same(name + from, s_operations[at], size))
        {
            // CLI_DOES_COUNT is below 256
            return (unsigned char)(at + 1u);
        }
    }
    return 0u;
}

// the number a segment of `size` bytes at `text` writes in decimal digits, 0 where it writes none or more than a byte
// holds
static unsigned char cli_leaf_segment_number(const char *text, unsigned long long size)
{
    unsigned int value = 0u;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9') || (value > 25u))
        {
            return 0u;
        }
        value = (value * 10u) + (unsigned int)(text[at] - '0');
    }
    // a value checked below 256 above
    return (value < 256u) ? (unsigned char)value : 0u;
}

// what a leaf whose token is `name` is, by its name's segments, into `leaf`: the first says what the leaf is, the
// others what it declares, what a type it names is or what role it plays, the segment after `binds` or `base` the
// number it binds with or its digits' base, and in an operator's name the operation it does
static void cli_leaf_of(const char *name, CliLeaf *leaf)
{
    leaf->is = 0ull;
    leaf->binds = 0u;
    leaf->base = 10u;
    leaf->does = 0u;
    int operating = 0;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (length == 0ull)
    {
        leaf->is = CLI_LEAF_BLANK;
        return;
    }
    unsigned long long is = 0ull;
    unsigned long long from = 0ull;
    unsigned int segment = 0u;
    int directive = 0;
    // the segment before this one names `binds` or `base`, and this one is a number
    unsigned int counts = 0u;
    while (from <= length)
    {
        unsigned long long to = from;
        while ((to < length) && (name[to] != '.'))
        {
            to += 1ull;
        }
        const unsigned long long size = to - from;
        if (counts != 0u)
        {
            const unsigned char number = cli_leaf_segment_number(name + from, size);
            leaf->binds = (counts == 1u) ? number : leaf->binds;
            leaf->base = ((counts == 2u) && (number > 1u)) ? number : leaf->base;
            counts = 0u;
        }
        else if (segment == 0u)
        {
            operating = cli_leaf_segment_is(name, from, size, "operator");
            is |= cli_leaf_segment_is(name, from, size, "string")       ? CLI_LEAF_STRING
                  : cli_leaf_segment_is(name, from, size, "comment")    ? CLI_LEAF_COMMENT
                  : cli_leaf_segment_is(name, from, size, "number")     ? CLI_LEAF_NUMBER
                  : cli_leaf_segment_is(name, from, size, "identifier") ? CLI_LEAF_NAME
                  : cli_leaf_segment_is(name, from, size, "predefined") ? CLI_LEAF_NAME
                  : cli_leaf_segment_is(name, from, size, "keyword")    ? CLI_LEAF_KEYWORD
                  : cli_leaf_segment_is(name, from, size, "type")       ? CLI_LEAF_TYPE
                                                                        : 0u;
        }
        else if (directive)
        {
            is |= cli_leaf_segment_is(name, from, size, "include")     ? CLI_LEAF_INCLUDE
                  : cli_leaf_segment_is(name, from, size, "define")    ? CLI_LEAF_DEFINE
                  : cli_leaf_segment_is(name, from, size, "undef")     ? CLI_LEAF_UNDEF
                  : cli_leaf_segment_is(name, from, size, "if")        ? CLI_LEAF_IF
                  : cli_leaf_segment_is(name, from, size, "ifdef")     ? CLI_LEAF_IFDEF
                  : cli_leaf_segment_is(name, from, size, "ifndef")    ? CLI_LEAF_IFNDEF
                  : cli_leaf_segment_is(name, from, size, "elif")      ? CLI_LEAF_ELIF
                  : cli_leaf_segment_is(name, from, size, "else")      ? CLI_LEAF_ELSE
                  : cli_leaf_segment_is(name, from, size, "endif")     ? CLI_LEAF_ENDIF
                  : cli_leaf_segment_is(name, from, size, "defined")   ? CLI_LEAF_DEFINED
                  : cli_leaf_segment_is(name, from, size, "continued") ? CLI_LEAF_CONTINUED
                                                                       : 0u;
        }
        else
        {
            const int typed = (is & CLI_LEAF_TYPE) != 0u;
            const unsigned char operation = operating ? cli_leaf_operation(name, from, size) : 0u;
            leaf->does = (operation != 0u) ? operation : leaf->does;
            directive = (segment == 1u) && cli_leaf_segment_is(name, from, size, "directive");
            counts = cli_leaf_segment_is(name, from, size, "binds")  ? 1u
                     : cli_leaf_segment_is(name, from, size, "base") ? 2u
                                                                     : 0u;
            is |= cli_leaf_segment_is(name, from, size, "parameter")           ? CLI_LEAF_PARAMETER
                  : cli_leaf_segment_is(name, from, size, "memory")            ? CLI_LEAF_MEMORY
                  : cli_leaf_segment_is(name, from, size, "local")             ? CLI_LEAF_LOCAL
                  : cli_leaf_segment_is(name, from, size, "declaration")       ? CLI_LEAF_DECLARES
                  : cli_leaf_segment_is(name, from, size, "rest")              ? CLI_LEAF_REST
                  : cli_leaf_segment_is(name, from, size, "join")              ? CLI_LEAF_JOIN
                  : cli_leaf_segment_is(name, from, size, "quote")             ? CLI_LEAF_QUOTE
                  : cli_leaf_segment_is(name, from, size, "unary")             ? CLI_LEAF_UNARY
                  : cli_leaf_segment_is(name, from, size, "choice")            ? CLI_LEAF_CHOICE
                  : cli_leaf_segment_is(name, from, size, "otherwise")         ? CLI_LEAF_OTHERWISE
                  : cli_leaf_segment_is(name, from, size, "prefix")            ? CLI_LEAF_PREFIX
                  : cli_leaf_segment_is(name, from, size, "include")           ? CLI_LEAF_INCLUDED
                  : cli_leaf_segment_is(name, from, size, "beside")            ? CLI_LEAF_BESIDE
                  : (typed && cli_leaf_segment_is(name, from, size, "number")) ? CLI_LEAF_OF_NUMBER
                  : (typed && cli_leaf_segment_is(name, from, size, "string")) ? CLI_LEAF_OF_STRING
                  : cli_leaf_segment_is(name, from, size, "open")              ? CLI_LEAF_OPEN
                  : cli_leaf_segment_is(name, from, size, "close")             ? CLI_LEAF_CLOSE
                  : cli_leaf_segment_is(name, from, size, "region")            ? CLI_LEAF_REGION
                  : cli_leaf_segment_is(name, from, size, "statement")         ? CLI_LEAF_STATEMENT
                  : cli_leaf_segment_is(name, from, size, "separator")         ? CLI_LEAF_SEPARATOR
                  : cli_leaf_segment_is(name, from, size, "call")              ? CLI_LEAF_CALL
                  : cli_leaf_segment_is(name, from, size, "assign")            ? CLI_LEAF_ASSIGN
                  : cli_leaf_segment_is(name, from, size, "function")          ? CLI_LEAF_FUNCTION
                  : cli_leaf_segment_is(name, from, size, "depth")             ? CLI_LEAF_DEPTH
                                                                               : 0u;
        }
        segment += 1u;
        from = to + 1ull;
    }
    leaf->is = is;
}

unsigned int cli_symbols_name_roles(const char *name)
{
    CliLeaf leaf;
    cli_leaf_of(name, &leaf);
    if ((leaf.is & (CLI_LEAF_STRING | CLI_LEAF_COMMENT)) != 0u)
    {
        return 0u;
    }
    const int region = (leaf.is & CLI_LEAF_REGION) != 0u;
    const int opens = (leaf.is & CLI_LEAF_OPEN) != 0u;
    const int closes = (leaf.is & CLI_LEAF_CLOSE) != 0u;
    const int parts = (leaf.is & (CLI_LEAF_STATEMENT | CLI_LEAF_SEPARATOR)) != 0u;
    const int named = (leaf.is & CLI_LEAF_NAME) != 0u;
    const int ends = (leaf.is & (CLI_LEAF_STATEMENT | CLI_LEAF_DIRECTIVE)) != 0u;
    const int depth = (leaf.is & CLI_LEAF_DEPTH) != 0u;
    return ((region && opens) ? CLI_ROLE_REGION_OPENS : 0u) | ((region && closes) ? CLI_ROLE_REGION_CLOSES : 0u) |
           ((depth && opens) ? CLI_ROLE_DEPTH_OPENS : 0u) | ((depth && closes) ? CLI_ROLE_DEPTH_CLOSES : 0u) |
           ((closes || parts) ? CLI_ROLE_CLOSING : 0u) | ((leaf.is == CLI_LEAF_NAME) ? CLI_ROLE_NAME : 0u) |
           ((named && ((leaf.is & CLI_LEAF_LOCAL) != 0u)) ? CLI_ROLE_LOCAL : 0u) |
           ((named && ((leaf.is & CLI_LEAF_PARAMETER) != 0u)) ? CLI_ROLE_PARAMETER : 0u) | (ends ? CLI_ROLE_ENDS : 0u) |
           ((((leaf.is & CLI_LEAF_MEMORY) != 0u) && (leaf.binds == 0u) && ((leaf.is & CLI_LEAF_UNARY) == 0u))
                ? CLI_ROLE_REACHES
                : 0u);
}

// the leaves' room twice what it was, or CLI_LEAVES_FIRST where it has none: 1, or 0 where the cells cannot meet it
static int cli_leaves_grown(CliLeafRead *read)
{
    const unsigned int room = (read->room == 0u) ? CLI_LEAVES_FIRST : (2u * read->room);
    const size_t bytes = (size_t)room * sizeof(CliLeaf);
    CliLeaf *const grown = cli_cell_regrown(read->leaf, (size_t)read->count * sizeof(CliLeaf), bytes, &g_cli_leaves_cells);
    if (grown == NULL)
    {
        return 0;
    }
    read->leaf = grown;
    read->room = room;
    return 1;
}

// One line's leaves kept after those before, each at its bytes in the text, a mark a leaf of no bytes: the copy the
// colorer reads holds each line at the text's own bytes, its first line at the copy's first byte
static void cli_leaves_line(void *context, const CliLeaves *leaves)
{
    CliLeafRead *const read = (CliLeafRead *)context;
    const char *const line = leaves->line;
    if (read->first == NULL)
    {
        read->first = line;
    }
    // the line stands in the copy at or past its first byte
    const unsigned long long start = (unsigned long long)(line - read->first);
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
    unsigned long long byte = 0ull;
    unsigned int at = 0u;
    for (unsigned int run = 0u; !read->refused && (run < leaves->runs); run += 1u)
    {
        const unsigned long long from = cli_symbols_byte_at(line, size, leaves->col[run], &byte, &at);
        const unsigned long long to =
            ((run + 1u) < leaves->runs) ? cli_symbols_byte_at(line, size, leaves->col[run + 1u], &byte, &at) : size;
        // a run of no letters is a leaf where it is a mark, a run its grammar names
        if ((to < from) || ((to == from) && (leaves->name[run] == 0u)))
        {
            continue;
        }
        if ((read->count == read->room) && !cli_leaves_grown(read))
        {
            read->refused = 1;
            break;
        }
        CliLeaf *const leaf = &read->leaf[read->count];
        leaf->line = read->lines;
        leaf->from = start + from;
        leaf->to = start + to;
        const char *const name = leaves->text + leaves->name_at[leaves->name[run]];
        cli_leaf_of(name, leaf);
        read->count += 1u;
        const char *const kind = ((leaf->is & CLI_LEAF_DECLARES) != 0u) ? cli_symbols_declares(name) : NULL;
        if ((read->declared != NULL) && (kind != NULL) && (kind[0] != '\0'))
        {
            read->declared(read->declared_context, line + from, to - from, kind, read->lines);
        }
    }
    read->lines += 1u;
}

int cli_symbols_leaves_kept(CliSymbolsLanguages *languages, const char *path, const char *state, const char *text,
                            unsigned long long size, CliLeafRead *read)
{
    return cli_symbols_leaves_declared(languages, path, state, text, size, read, NULL, NULL);
}

int cli_symbols_leaves_declared(CliSymbolsLanguages *languages, const char *path, const char *state, const char *text,
                                unsigned long long size, CliLeafRead *read, CliDeclared each, void *context)
{
    *read = (CliLeafRead){text, NULL, 0u, 0u, 0u, NULL, 0, each, context, 0};
    return cli_symbols_leaves_in(languages, path, state, text, size, cli_leaves_line, read) && !read->refused;
}

int cli_symbols_leaves_temporary(CliSymbolsLanguages *languages, const char *path, const char *state, const char *text,
                             unsigned long long size, CliLeafRead *read)
{
    *read = (CliLeafRead){text, NULL, 0u, 0u, 0u, NULL, 0, NULL, NULL, 1};
    return cli_symbols_leaves_in(languages, path, state, text, size, cli_leaves_line, read) && !read->refused;
}

int cli_symbols_language_leaves_kept(CliSymbolsLanguages *languages, const char *id, const char *text,
                                     unsigned long long size, CliLeafRead *read)
{
    *read = (CliLeafRead){text, NULL, 0u, 0u, 0u, NULL, 0, NULL, NULL, 0};
    return cli_symbols_language_leaves_in(languages, id, "root", text, size, cli_leaves_line, read) && !read->refused;
}

const char *cli_leaf_text(const CliLeafRead *read, unsigned int leaf, unsigned long long *length)
{
    *length = read->leaf[leaf].to - read->leaf[leaf].from;
    return read->text + read->leaf[leaf].from;
}

int cli_leaf_has(const CliLeafRead *read, unsigned int leaf, unsigned long long roles)
{
    const CliLeaf *const one = &read->leaf[leaf];
    return ((one->is & roles) == roles) && ((one->is & (CLI_LEAF_STRING | CLI_LEAF_COMMENT)) == 0u);
}

void cli_symbols_leaves_given(CliLeafRead *read)
{
    g_cli_cells->persistent_buf_release(read->leaf, &g_cli_leaves_cells);
    read->leaf = NULL;
    read->count = 0u;
    read->room = 0u;
}
