// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_leaves.c: a text's leaves saved for a walk over them, in a cell at their most: each leaf's line, its bytes in
// the text and what it is, read off its token's name's segments alone. No language is read here but by its plugin's
// grammar
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "symbols_internal.h"

// the room the first cell of leaves holds
#define CLI_LEAVES_MARKS 256ull
#define CLI_LEAVES_MOST (1u << 19u)

typedef struct
{
    const char *name;
    unsigned long long from;
    unsigned long long length;
    const char *segment;
    const char *text;
    unsigned long long size;
    CliLeaf *leaf;
    CliLeafRead *read;
} SymbolsLeavesCtx;
// whether the segment of `name` from `from`, `length` bytes, is `segment`
EMBED_INLINE int cli_leaf_segment_is(const SymbolsLeavesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->segment, .cap = CLI_CELLS_POOL});
    return (args->length == size) &&
           EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->name + args->from,
                      .other_pointer = args->segment, .size = size);
}

// the operations a segment of an operator's name names, each at the place its CLI_DOES_ number counts from 1
static const char *const s_operations[CLI_DOES_COUNT] = {
    "not",    "complement", "multiply", "divide", "remainder", "add", "subtract", "shiftleft", "shiftright", "less",
    "atmost", "greater",    "atleast",  "equal",  "unequal",   "and", "or",       "bitand",    "bitxor",     "bitor"};

// the operation the segment of `name` from `from`, `length` bytes, names, a CLI_DOES_ number, 0 where it names none
EMBED_INLINE unsigned char cli_leaf_operation(const SymbolsLeavesCtx *args)
{
    for (unsigned int at = 0u; at < CLI_DOES_COUNT; at += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_operations[at], .cap = CLI_CELLS_POOL});
        if ((args->length == size) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->name + args->from,
                       .other_pointer = s_operations[at], .size = size))
        {
            // CLI_DOES_COUNT is below 256
            return (unsigned char)(at + 1u);
        }
    }
    return 0u;
}

// the number a segment of `size` bytes at `text` writes in decimal digits, 0 where it writes none or more than a byte
// holds
EMBED_INLINE unsigned char cli_leaf_segment_number(const SymbolsLeavesCtx *args)
{
    unsigned int value = 0u;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        if ((args->text[at] < '0') || (args->text[at] > '9') || (value > 25u))
        {
            return 0u;
        }
        value = (value * 10u) + (unsigned int)(args->text[at] - '0');
    }
    // a value checked below 256 above
    return (value < 256u) ? (unsigned char)value : 0u;
}

// what a leaf whose token is `name` is, by its name's segments, into `leaf`: the first says what the leaf is, the
// others what it declares, what a type it names is or what role it plays, the segment after `binds` or `base` the
// number it binds with or its digits' base, and in an operator's name the operation it does
static void cli_leaf_get(const SymbolsLeavesCtx *args)
{
    args->leaf->is = 0ull;
    args->leaf->binds = 0u;
    args->leaf->base = 10u;
    args->leaf->does = 0u;
    int operating = 0;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    if (length == 0ull)
    {
        args->leaf->is = CLI_LEAF_BLANK;
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
        to = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->name, .at = to,
                        .size = length, .set = ".");
        const unsigned long long size = to - from;
        if (counts != 0u)
        {
            const unsigned char number =
                EMBED_CALL(cli_leaf_segment_number, SymbolsLeavesCtx, .text = args->name + from, .size = size);
            args->leaf->binds = (counts == 1u) ? number : args->leaf->binds;
            args->leaf->base = ((counts == 2u) && (number > 1u)) ? number : args->leaf->base;
            counts = 0u;
        }
        else if (segment == 0u)
        {
            operating = EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from,
                                   .length = size, .segment = "operator");
            is |= EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                             .segment = "string")
                      ? CLI_LEAF_STRING
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "comment")
                      ? CLI_LEAF_COMMENT
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "number")
                      ? CLI_LEAF_NUMBER
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "identifier")
                      ? CLI_LEAF_NAME
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "predefined")
                      ? CLI_LEAF_NAME
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "keyword")
                      ? CLI_LEAF_KEYWORD
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "type")
                      ? CLI_LEAF_TYPE
                      : 0u;
        }
        else if (directive)
        {
            is |= EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                             .segment = "include")
                      ? CLI_LEAF_INCLUDE
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "define")
                      ? CLI_LEAF_DEFINE
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "undef")
                      ? CLI_LEAF_UNDEF
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "if")
                      ? CLI_LEAF_IF
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "ifdef")
                      ? CLI_LEAF_IFDEF
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "ifndef")
                      ? CLI_LEAF_IFNDEF
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "elif")
                      ? CLI_LEAF_ELIF
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "else")
                      ? CLI_LEAF_ELSE
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "endif")
                      ? CLI_LEAF_ENDIF
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "defined")
                      ? CLI_LEAF_DEFINED
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "continued")
                      ? CLI_LEAF_CONTINUED
                      : 0u;
        }
        else
        {
            const int typed = (is & CLI_LEAF_TYPE) != 0u;
            const unsigned char operation = operating ? EMBED_CALL(cli_leaf_operation, SymbolsLeavesCtx,
                                                                   .name = args->name, .from = from, .length = size)
                                                      : 0u;
            args->leaf->does = (operation != 0u) ? operation : args->leaf->does;
            directive = (segment == 1u) && EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name,
                                                      .from = from, .length = size, .segment = "directive");
            counts = EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                                .segment = "binds")
                         ? 1u
                     : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from,
                                  .length = size, .segment = "base")
                         ? 2u
                         : 0u;
            is |= EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                             .segment = "parameter")
                      ? CLI_LEAF_PARAMETER
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "memory")
                      ? CLI_LEAF_MEMORY
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "local")
                      ? CLI_LEAF_LOCAL
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "declaration")
                      ? CLI_LEAF_DECLARES
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "rest")
                      ? CLI_LEAF_REST
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "join")
                      ? CLI_LEAF_JOIN
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "quote")
                      ? CLI_LEAF_QUOTE
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "unary")
                      ? CLI_LEAF_UNARY
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "choice")
                      ? CLI_LEAF_CHOICE
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "otherwise")
                      ? CLI_LEAF_OTHERWISE
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "prefix")
                      ? CLI_LEAF_PREFIX
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "include")
                      ? CLI_LEAF_INCLUDED
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "beside")
                      ? CLI_LEAF_BESIDE
                  : (typed && EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from,
                                         .length = size, .segment = "number"))
                      ? CLI_LEAF_OF_NUMBER
                  : (typed && EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from,
                                         .length = size, .segment = "string"))
                      ? CLI_LEAF_OF_STRING
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "open")
                      ? CLI_LEAF_OPEN
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "close")
                      ? CLI_LEAF_CLOSE
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "region")
                      ? CLI_LEAF_REGION
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "statement")
                      ? CLI_LEAF_STATEMENT
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "separator")
                      ? CLI_LEAF_SEPARATOR
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "call")
                      ? CLI_LEAF_CALL
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "assign")
                      ? CLI_LEAF_ASSIGN
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "function")
                      ? CLI_LEAF_FUNCTION
                  : EMBED_CALL(cli_leaf_segment_is, SymbolsLeavesCtx, .name = args->name, .from = from, .length = size,
                               .segment = "depth")
                      ? CLI_LEAF_DEPTH
                      : 0u;
        }
        segment += 1u;
        from = to + 1ull;
    }
    args->leaf->is = is;
}

unsigned int cli_symbols_name_roles(const CliSymbolsQueryCfg *args)
{
    CliLeaf leaf;
    EMBED_CALL(cli_leaf_get, SymbolsLeavesCtx, .name = args->name, .leaf = &leaf);
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

// the leaves' one cell taken for the text's bytes, a leaf a byte at the most and a mark past those, at most
// CLI_LEAVES_MOST: 1, or 0 where it stands full or the cells cannot meet it
EMBED_INLINE int cli_leaves_take(const SymbolsLeavesCtx *args)
{
    if (args->read->room != 0u)
    {
        return 0;
    }
    const unsigned long long wanted = args->read->size + CLI_LEAVES_MARKS;
    const unsigned int room = (wanted < CLI_LEAVES_MOST) ? (unsigned int)wanted : CLI_LEAVES_MOST;
    args->read->leaf =
        g_cli_cells->temporary_buf_alloc((unsigned long long)room * sizeof(CliLeaf), &g_cli_leaves_cells);
    args->read->room = (args->read->leaf != NULL) ? room : 0u;
    return args->read->leaf != NULL;
}

// One line's leaves saved after those before, each at its bytes in the text, a mark a leaf of no bytes: the copy the
// colorer reads holds each line at the text's own bytes, its first line at the copy's first byte
static void cli_leaves_line(const CliLeavesEachCfg *args)
{
    CliLeafRead *const read = (CliLeafRead *)args->context;
    const char *const line = args->leaves->line;
    if (read->first == NULL)
    {
        read->first = line;
    }
    // the line stands in the copy at or past its first byte
    const unsigned long long start = (unsigned long long)(line - read->first);
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
    unsigned long long byte = 0ull;
    unsigned int at = 0u;
    for (unsigned int run = 0u; !read->refused && (run < args->leaves->runs); run += 1u)
    {
        const unsigned long long from =
            EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = line, .size = size,
                       .col = args->leaves->col[run], .byte = &byte, .at = &at);
        const unsigned long long to =
            ((run + 1u) < args->leaves->runs)
                ? EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = line, .size = size,
                             .col = args->leaves->col[run + 1u], .byte = &byte, .at = &at)
                : size;
        // a run of no letters is a leaf where it is a mark, a run its grammar names
        if ((to < from) || ((to == from) && (args->leaves->name[run] == 0u)))
        {
            continue;
        }
        if ((read->count == read->room) && !EMBED_CALL(cli_leaves_take, SymbolsLeavesCtx, .read = read))
        {
            read->refused = 1;
            break;
        }
        CliLeaf *const leaf = &read->leaf[read->count];
        leaf->line = read->lines;
        leaf->from = start + from;
        leaf->to = start + to;
        const char *const name = args->leaves->text + args->leaves->name_at[args->leaves->name[run]];
        EMBED_CALL(cli_leaf_get, SymbolsLeavesCtx, .name = name, .leaf = leaf);
        read->count += 1u;
        const char *const kind = ((leaf->is & CLI_LEAF_DECLARES) != 0u)
                                     ? EMBED_CALL(cli_symbols_name_internal.declares, CliSymbolsNameCtx, .name = name)
                                     : NULL;
        if ((read->declared != NULL) && (kind != NULL) && (kind[0] != '\0'))
        {
            EMBED_CALL(read->declared, CliDeclaredCfg, .context = read->declared_context, .name = line + from,
                       .length = to - from, .kind = kind, .line = read->lines);
        }
    }
    read->lines += 1u;
}

int cli_symbols_leaves_save(const CliSymbolsPathCtx *args)
{
    return EMBED_CALL(cli_symbols_path_internal.leaves_declare, CliSymbolsPathCtx, .languages = args->languages,
                      .path = args->path, .state = args->state, .text = args->text, .size = args->size,
                      .read = args->read, .each = NULL, .context = NULL);
}

int cli_symbols_leaves_declare(const CliSymbolsPathCtx *args)
{
    unsigned long long size = args->size;
    CliLeafRead *read = args->read;
    *read = (CliLeafRead){args->text, NULL, 0u, 0u, 0u, NULL, 0, args->each, args->context, 0};
    read->size = size;
    return EMBED_CALL(cli_symbols_path_read_languages_text_internal.leaves_in, CliSymbolsPathReadLanguagesTextCtx,
                      .read_symbols_languages_pointer_other = args->languages, .path = args->path, .state = args->state,
                      .text = args->text, .size = size, .each_leaves_each = cli_leaves_line, .context = read) &&
           !read->refused;
}

int cli_symbols_leaves_temporary(const CliSymbolsPathCtx *args)
{
    unsigned long long size = args->size;
    CliLeafRead *read = args->read;
    *read = (CliLeafRead){args->text, NULL, 0u, 0u, 0u, NULL, 0, NULL, NULL, 1};
    read->size = size;
    return EMBED_CALL(cli_symbols_path_read_languages_text_internal.leaves_in, CliSymbolsPathReadLanguagesTextCtx,
                      .read_symbols_languages_pointer_other = args->languages, .path = args->path, .state = args->state,
                      .text = args->text, .size = size, .each_leaves_each = cli_leaves_line, .context = read) &&
           !read->refused;
}

int cli_symbols_language_leaves_save(const CliSymbolsTextCtx *args)
{
    unsigned long long size = args->size;
    CliLeafRead *read = args->read;
    *read = (CliLeafRead){args->text, NULL, 0u, 0u, 0u, NULL, 0, NULL, NULL, 0};
    read->size = size;
    return EMBED_CALL(cli_symbols_read_languages_text_internal.language_leaves_in, CliSymbolsReadLanguagesTextCtx,
                      .read_symbols_languages_pointer_other = args->languages, .id = args->id, .state = "root",
                      .text = args->text, .size = size, .each_leaves_each = cli_leaves_line, .context = read) &&
           !read->refused;
}

const char *cli_leaf_text(const CliSymbolsReadLeafCtx *args)
{
    unsigned long long *length = args->length_pointer;
    *length = args->read_read->leaf[args->leaf].to - args->read_read->leaf[args->leaf].from;
    return args->read_read->text + args->read_read->leaf[args->leaf].from;
}

int cli_leaf_has(const CliSymbolsReadCtx *args)
{
    unsigned long long roles = args->roles;
    const CliLeaf *const one = &args->read_read->leaf[args->leaf];
    return ((one->is & roles) == roles) && ((one->is & (CLI_LEAF_STRING | CLI_LEAF_COMMENT)) == 0u);
}

void cli_symbols_leaves_free(const CliSymbolsTextCtx *args)
{
    g_cli_cells->persistent_buf_release(args->read->leaf, &g_cli_leaves_cells);
    args->read->leaf = NULL;
    args->read->count = 0u;
    args->read->room = 0u;
}
