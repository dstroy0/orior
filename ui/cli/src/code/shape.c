// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape.c: a text read as tokens, the leaves its language's grammar reads it into, past blanks and comments: a name
// where its leaf is a name, a keyword or a type, a number, a string, the leaves of one string one token, else a mark;
// each with the roles its grammar names, a bracket that opens or closes a pair, a region's bracket, and a statement's
// end. A statement's end the leaves' reading puts at a line's end is a statement's end token, and a block's close it
// puts where an indent returns a mark with its roles, each standing where the token before it ends. A pattern's
// placeholders, `$name$` or `$name:kind$`, are the search's own and read off its text first, its grammar reading it
// with their bytes blank, each a name token at its bytes among the leaves'. Tokens past what their storage fits are not
// read
#include "shape_internal.h"
#include "symbols_internal.h"

// A placeholder's bytes in a pattern
typedef struct
{
    unsigned long long start;
    unsigned long long end;
} CliShapeHolePosition;

typedef struct
{
    CliShapeTokens *tokens;
    unsigned char kind;
    unsigned char role;
    unsigned long long start;
    unsigned long long end;
    const char *src;
    unsigned long long size;
    unsigned long long *end_pointer;
    CliShapeHole *hole;
    CliShapeHoles *holes;
    CliShapeHolePosition *positions;
    char *blank;
    const CliLeafRead *read;
    unsigned int leaf;
    unsigned long long from;
    unsigned long long to;
    const CliShapeHolePosition *positions_read;
} ShapeCtx;
// a token put on the tokens where it fits: its place among them, or the most they hold where it does not fit
EMBED_INLINE unsigned int shape_append(const ShapeCtx *args)
{
    if (args->tokens->count == args->tokens->most)
    {
        args->tokens->full = 1;
        return args->tokens->most;
    }
    args->tokens->kind[args->tokens->count] = args->kind;
    args->tokens->role[args->tokens->count] = args->role;
    args->tokens->start[args->tokens->count] = (unsigned int)args->start;
    args->tokens->end[args->tokens->count] = (unsigned int)args->end;
    args->tokens->count += 1u;
    return args->tokens->count - 1u;
}

// a placeholder at `start` of the pattern, `$name$` or `$name:kind$`: 1 with it into `hole` and its end into `end`, or
// 0 where none is there
EMBED_INLINE int shape_hole_at(const ShapeCtx *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    unsigned long long *end = args->end_pointer;
    CliShapeHole *hole = args->hole;
    unsigned long long close = 0ULL;
    if ((args->start + 1u >= size) ||
        !EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = src + args->start + 1u,
                    .length_value = size - args->start - 1u, .required = "$", .size = 1u, .at_pointer = &close))
    {
        return 0;
    }
    const char *const inside = src + args->start + 1u;
    unsigned long long name_size = close;
    const char *kind = inside + close;
    unsigned long long kind_size = 0ULL;
    unsigned long long colon = 0ULL;
    if (EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = inside, .length_value = close, .required = ":",
                   .size = 1u, .at_pointer = &colon))
    {
        name_size = colon;
        kind = inside + colon + 1u;
        kind_size = close - colon - 1u;
    }
    if (name_size == 0u)
    {
        return 0;
    }
    for (unsigned long long walk = 0ULL; walk < name_size;)
    {
        unsigned long long length = 0ULL;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = inside, .size = name_size,
                                               .at = walk, .length = &length);
        if (!EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = letter) && (letter != '_'))
        {
            return 0;
        }
        walk += length;
    }
    unsigned int exists = CLI_SHAPE_HOLE_EXPR;
    if ((kind_size == 4u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "name", .bytes = 4u}) == 0))
    {
        exists = CLI_SHAPE_HOLE_NAME;
    }
    else if (((kind_size == 4u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "stmt", .bytes = 4u}) == 0)) ||
             ((kind_size == 5u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "stmts", .bytes = 5u}) == 0)))
    {
        exists = CLI_SHAPE_HOLE_STMT;
    }
    *end = args->start + 1u + close + 1u;
    *hole = (CliShapeHole){0u, inside, name_size, exists};
    return 1;
}

// the placeholders of the pattern, CLI_SHAPE_HOLES at the most, each put among the holes with its bytes into
// `positions` and its bytes made blank in `blank`, the pattern's copy
EMBED_INLINE void shape_holes_find(const ShapeCtx *args)
{
    args->holes->count = 0u;
    for (unsigned long long byte = 0ULL; byte < args->size;)
    {
        CliShapeHole hole;
        unsigned long long end = 0ULL;
        if ((args->src[byte] == '$') && (args->holes->count < CLI_SHAPE_HOLES) &&
            EMBED_CALL(shape_hole_at, ShapeCtx, .src = args->src, .size = args->size, .start = byte,
                       .end_pointer = &end, .hole = &hole))
        {
            args->holes->hole[args->holes->count] = hole;
            args->positions[args->holes->count] = (CliShapeHolePosition){byte, end};
            args->holes->count += 1u;
            // a placeholder's bytes are a pattern's, below its size, which a size_t holds
            memor.set(&(MemoriaCfg){.dst = args->blank + byte, .val = ' ', .bytes = (size_t)(end - byte)});
            byte = end;
            continue;
        }
        byte += 1ULL;
    }
}

// what the leaf `leaf` of `read` is as a token: a string, a number, a name where it is a name, a keyword or a type,
// else a mark
EMBED_INLINE unsigned char shape_kind_get(const ShapeCtx *args)
{
    const unsigned long long is = args->read->leaf[args->leaf].is;
    if ((is & CLI_LEAF_STRING) != 0u)
    {
        return CLI_SHAPE_STR;
    }
    if ((is & CLI_LEAF_NUMBER) != 0u)
    {
        return CLI_SHAPE_NUMBER;
    }
    if ((is & (CLI_LEAF_NAME | CLI_LEAF_KEYWORD | CLI_LEAF_TYPE)) != 0u)
    {
        return CLI_SHAPE_NAME;
    }
    return CLI_SHAPE_PUNCT;
}

// the roles the leaf `leaf` of `read` plays as a token, as its grammar names them
EMBED_INLINE unsigned char shape_role_get(const ShapeCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    const unsigned int role = (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                          .leaf = leaf, .roles = CLI_LEAF_OPEN)
                                   ? CLI_SHAPE_OPENS
                                   : 0u) |
                              (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                          .leaf = leaf, .roles = CLI_LEAF_CLOSE)
                                   ? CLI_SHAPE_CLOSES
                                   : 0u) |
                              (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                          .leaf = leaf, .roles = CLI_LEAF_STATEMENT)
                                   ? CLI_SHAPE_STATEMENT
                                   : 0u) |
                              (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                          .leaf = leaf, .roles = CLI_LEAF_REGION)
                                   ? CLI_SHAPE_REGION
                                   : 0u);
    // the roles are the four low bits, which a byte holds
    return (unsigned char)role;
}

// whether the bytes `from` to `to` of the text are none but line ends, so a string's leaves on either side are one
EMBED_INLINE int shape_join(const ShapeCtx *args)
{
    for (unsigned long long at = args->from; at < args->to; at += 1ULL)
    {
        if ((args->src[at] != '\n') && (args->src[at] != '\r'))
        {
            return 0;
        }
    }
    return 1;
}

// each leaf of `read`, the leaves of `size` bytes of `src`, past blanks and comments put as a token, a string's leaves
// one token and a leaf with no bytes standing where the token before it ends, a statement's end a statement's end
// token; and before each the placeholders that start before it, or at it where it has bytes, each hole given its
// token's place; and those left after the last
EMBED_INLINE void shape_leaves_append(const ShapeCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int hole = 0u;
    for (unsigned int leaf = 0u; leaf <= read->count; leaf += 1u)
    {
        const unsigned long long from = (leaf < read->count) ? read->leaf[leaf].from : args->size + 1ULL;
        const unsigned long long to = (leaf < read->count) ? read->leaf[leaf].to : args->size + 1ULL;
        for (;
             (args->holes != NULL) && (hole < args->holes->count) &&
             ((args->positions_read[hole].start < from) || ((args->positions_read[hole].start == from) && (to > from)));
             hole += 1u)
        {
            args->holes->hole[hole].token =
                EMBED_CALL(shape_append, ShapeCtx, .tokens = args->tokens, .kind = CLI_SHAPE_NAME, .role = 0u,
                           .start = args->positions_read[hole].start, .end = args->positions_read[hole].end);
        }
        if ((leaf == read->count) || ((read->leaf[leaf].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
        {
            continue;
        }
        const unsigned int last = args->tokens->count - 1u;
        if (to == from)
        {
            const unsigned long long at = (args->tokens->count > 0u) ? args->tokens->end[last] : 0ULL;
            const int ends = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                        .leaf = leaf, .roles = CLI_LEAF_STATEMENT);
            if (!ends || (args->tokens->count == 0u) || (args->tokens->kind[last] != CLI_SHAPE_BREAK))
            {
                (void)EMBED_CALL(shape_append, ShapeCtx, .tokens = args->tokens,
                                 .kind = ends ? CLI_SHAPE_BREAK : CLI_SHAPE_PUNCT,
                                 .role = ends ? 0u : EMBED_CALL(shape_role_get, ShapeCtx, .read = read, .leaf = leaf),
                                 .start = at, .end = at);
            }
            continue;
        }
        const unsigned char kind = EMBED_CALL(shape_kind_get, ShapeCtx, .read = read, .leaf = leaf);
        if ((kind == CLI_SHAPE_STR) && (args->tokens->count > 0u) && (args->tokens->kind[last] == CLI_SHAPE_STR) &&
            EMBED_CALL(shape_join, ShapeCtx, .src = args->src, .from = args->tokens->end[last], .to = from))
        {
            args->tokens->end[last] = (unsigned int)to;
            continue;
        }
        (void)EMBED_CALL(shape_append, ShapeCtx, .tokens = args->tokens, .kind = kind,
                         .role = EMBED_CALL(shape_role_get, ShapeCtx, .read = read, .leaf = leaf), .start = from,
                         .end = to);
    }
}

int cli_shape_tokens(const CliShapeLanguagesCtx *args)
{
    const char *src = args->src;
    args->tokens->count = 0u;
    args->tokens->full = 0;
    CliShapeHolePosition positions[CLI_SHAPE_HOLES];
    // the pattern's copy, its placeholders blank, which its grammar reads
    char *const blank =
        (args->holes != NULL) ? g_cli_guard->temporary_buf_alloc(args->size + 1ULL, g_cli_shape_owner) : NULL;
    if ((args->holes != NULL) && (blank == NULL))
    {
        return 0;
    }
    if (blank != NULL)
    {
        // a pattern's bytes fit a size_t
        memor.cpy(&(MemoriaCfg){.dst = blank, .src = src, .bytes = (size_t)args->size});
        blank[args->size] = '\0';
        EMBED_CALL(shape_holes_find, ShapeCtx, .src = src, .size = args->size, .holes = args->holes,
                   .positions = positions, .blank = blank);
    }
    CliLeafRead read;
    const int leaved =
        EMBED_CALL(cli_symbols_text_internal.language_leaves_save, CliSymbolsTextCtx, .languages = args->languages,
                   .id = args->language, .text = (blank != NULL) ? blank : src, .size = args->size, .read = &read);
    if (leaved)
    {
        EMBED_CALL(shape_leaves_append, ShapeCtx, .tokens = args->tokens, .src = src, .size = args->size, .read = &read,
                   .holes = args->holes, .positions_read = positions);
    }
    g_cli_guard->persistent_buf_release(read.leaf, g_cli_leaves_owner);
    read.leaf = NULL;
    read.count = 0u;
    read.capacity = 0u;
    g_cli_guard->persistent_buf_release(blank, g_cli_shape_owner);
    return leaved;
}
