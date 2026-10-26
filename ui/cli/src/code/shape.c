// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape.c: a text read as tokens, the leaves its language's grammar reads it into, past blanks and comments: a name
// where its leaf is a name, a keyword or a type, a number, a string, the leaves of one string one token, else a mark;
// each with the roles its grammar names, a bracket that opens or closes a pair, a region's bracket, and a statement's
// end; and, in a language whose plugin says a line's end ends a statement, a line's end outside brackets a statement's
// end. A pattern's placeholders, `$name$` or `$name:kind$`, are the search's own and read off its text first, its
// grammar reading it with their bytes blank, each a name token at its bytes among the leaves'. Tokens past what their
// storage fits are not read
#include "shape_internal.h"
#include "symbols_internal.h"

// A placeholder's bytes in a pattern and the line it stands on
typedef struct
{
    unsigned long long start;
    unsigned long long end;
    unsigned int line;
} CliShapeHolePlace;

// A text being read as tokens: its text, the tokens, whether a line's end outside brackets ends a statement, the depth
// of the brackets open, and the line of the token put last
typedef struct
{
    const char *src;
    unsigned long long size;
    CliShapeTokens *tokens;
    int line_ends_statement;
    unsigned int depth;
    unsigned int line;
} CliShapeReading;

// a token put on the tokens where it fits: its place among them, or the most they hold where it does not fit
static unsigned int shape_put(CliShapeTokens *tokens, unsigned char kind, unsigned char role, unsigned long long start,
                              unsigned long long end)
{
    if (tokens->count == tokens->most)
    {
        tokens->full = 1;
        return tokens->most;
    }
    tokens->kind[tokens->count] = kind;
    tokens->role[tokens->count] = role;
    tokens->start[tokens->count] = (unsigned int)start;
    tokens->end[tokens->count] = (unsigned int)end;
    tokens->count += 1u;
    return tokens->count - 1u;
}

// the line's end at or past the byte `from` of the text, the text's end where none is
static unsigned long long shape_line_end(const CliShapeReading *reading, unsigned long long from)
{
    while ((from < reading->size) && (reading->src[from] != '\n'))
    {
        from += 1ull;
    }
    return from;
}

// a token met on `line` put, a statement's end at the line's end before it where it stands on a later line than the
// last token, outside brackets, in a language whose line's end ends a statement, and the brackets' depth moved by its
// roles: its place among the tokens, or the most they hold where it does not fit
static unsigned int shape_met(CliShapeReading *reading, unsigned char kind, unsigned char role,
                              unsigned long long start, unsigned long long end, unsigned int line)
{
    CliShapeTokens *const tokens = reading->tokens;
    if (reading->line_ends_statement && (reading->depth == 0u) && (tokens->count > 0u) && (line > reading->line) &&
        (tokens->kind[tokens->count - 1u] != CLI_SHAPE_BREAK))
    {
        const unsigned long long ended = shape_line_end(reading, tokens->end[tokens->count - 1u]);
        (void)shape_put(tokens, CLI_SHAPE_BREAK, 0u, ended, ended + 1u);
    }
    const unsigned int at = shape_put(tokens, kind, role, start, end);
    reading->line = line;
    if ((role & CLI_SHAPE_OPENS) != 0u)
    {
        reading->depth += 1u;
    }
    else if (((role & CLI_SHAPE_CLOSES) != 0u) && (reading->depth > 0u))
    {
        reading->depth -= 1u;
    }
    return at;
}

// a placeholder at `start` of the pattern, `$name$` or `$name:kind$`: 1 with it into `hole` and its end into `end`, or
// 0 where none is there
static int shape_hole_at(const char *src, unsigned long long size, unsigned long long start, unsigned long long *end,
                         CliShapeHole *hole)
{
    unsigned long long close = 0ull;
    if ((start + 1u >= size) || !cli_text_find(src + start + 1u, size - start - 1u, "$", 1u, &close))
    {
        return 0;
    }
    const char *const inside = src + start + 1u;
    unsigned long long name_size = close;
    const char *kind = inside + close;
    unsigned long long kind_size = 0ull;
    unsigned long long colon = 0ull;
    if (cli_text_find(inside, close, ":", 1u, &colon))
    {
        name_size = colon;
        kind = inside + colon + 1u;
        kind_size = close - colon - 1u;
    }
    if (name_size == 0u)
    {
        return 0;
    }
    for (unsigned long long walk = 0ull; walk < name_size;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter(inside, name_size, walk, &length);
        if (!cli_text_alphanumeric(letter) && (letter != '_'))
        {
            return 0;
        }
        walk += length;
    }
    unsigned int stands = CLI_SHAPE_HOLE_EXPR;
    if ((kind_size == 4u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "name", .bytes = 4u}) == 0))
    {
        stands = CLI_SHAPE_HOLE_NAME;
    }
    else if (((kind_size == 4u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "stmt", .bytes = 4u}) == 0)) ||
             ((kind_size == 5u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "stmts", .bytes = 5u}) == 0)))
    {
        stands = CLI_SHAPE_HOLE_STMT;
    }
    *end = start + 1u + close + 1u;
    *hole = (CliShapeHole){0u, inside, name_size, stands};
    return 1;
}

// the placeholders of the pattern, CLI_SHAPE_HOLES at the most, each put among the holes with its bytes and its line
// into `places` and its bytes made blank in `blank`, the pattern's copy
static void shape_holes_found(const char *src, unsigned long long size, CliShapeHoles *holes, CliShapeHolePlace *places,
                              char *blank)
{
    holes->count = 0u;
    unsigned int line = 0u;
    for (unsigned long long byte = 0ull; byte < size;)
    {
        CliShapeHole hole;
        unsigned long long end = 0ull;
        if ((src[byte] == '$') && (holes->count < CLI_SHAPE_HOLES) && shape_hole_at(src, size, byte, &end, &hole))
        {
            holes->hole[holes->count] = hole;
            places[holes->count] = (CliShapeHolePlace){byte, end, line};
            holes->count += 1u;
            // a placeholder's bytes are a pattern's, below its size, which a size_t holds
            memor.set(&(MemoriaCfg){.dst = blank + byte, .val = ' ', .bytes = (size_t)(end - byte)});
            byte = end;
            continue;
        }
        line += (src[byte] == '\n') ? 1u : 0u;
        byte += 1ull;
    }
}

// what the leaf `leaf` of `read` is as a token: a string, a number, a name where it is a name, a keyword or a type,
// else a mark
static unsigned char shape_kind_of(const CliLeafRead *read, unsigned int leaf)
{
    const unsigned long long is = read->leaf[leaf].is;
    return ((is & CLI_LEAF_STRING) != 0u)                                      ? CLI_SHAPE_STR
           : ((is & CLI_LEAF_NUMBER) != 0u)                                    ? CLI_SHAPE_NUMBER
           : ((is & (CLI_LEAF_NAME | CLI_LEAF_KEYWORD | CLI_LEAF_TYPE)) != 0u) ? CLI_SHAPE_NAME
                                                                               : CLI_SHAPE_PUNCT;
}

// the roles the leaf `leaf` of `read` plays as a token, as its grammar names them
static unsigned char shape_role_of(const CliLeafRead *read, unsigned int leaf)
{
    const unsigned int role = (cli_leaf_has(read, leaf, CLI_LEAF_OPEN) ? CLI_SHAPE_OPENS : 0u) |
                              (cli_leaf_has(read, leaf, CLI_LEAF_CLOSE) ? CLI_SHAPE_CLOSES : 0u) |
                              (cli_leaf_has(read, leaf, CLI_LEAF_STATEMENT) ? CLI_SHAPE_STATEMENT : 0u) |
                              (cli_leaf_has(read, leaf, CLI_LEAF_REGION) ? CLI_SHAPE_REGION : 0u);
    // the roles are the four low bits, which a byte holds
    return (unsigned char)role;
}

// whether the bytes `from` to `to` of the text are none but line ends, so a string's leaves on either side are one
static int shape_joined(const char *src, unsigned long long from, unsigned long long to)
{
    for (unsigned long long at = from; at < to; at += 1ull)
    {
        if ((src[at] != '\n') && (src[at] != '\r'))
        {
            return 0;
        }
    }
    return 1;
}

// each leaf of `read` past blanks and comments put as a token, a string's leaves one token, and before each the
// placeholders that start at or before it, each hole given its token's place; and those left after the last
static void shape_leaves_put(CliShapeReading *reading, const CliLeafRead *read, CliShapeHoles *holes,
                             const CliShapeHolePlace *places)
{
    CliShapeTokens *const tokens = reading->tokens;
    unsigned int hole = 0u;
    for (unsigned int leaf = 0u; leaf <= read->count; leaf += 1u)
    {
        const unsigned long long from = (leaf < read->count) ? read->leaf[leaf].from : reading->size + 1ull;
        for (; (holes != NULL) && (hole < holes->count) && (places[hole].start <= from); hole += 1u)
        {
            holes->hole[hole].token =
                shape_met(reading, CLI_SHAPE_NAME, 0u, places[hole].start, places[hole].end, places[hole].line);
        }
        if ((leaf == read->count) || ((read->leaf[leaf].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
        {
            continue;
        }
        const unsigned char kind = shape_kind_of(read, leaf);
        const unsigned int last = tokens->count - 1u;
        if ((kind == CLI_SHAPE_STR) && (tokens->count > 0u) && (tokens->kind[last] == CLI_SHAPE_STR) &&
            shape_joined(reading->src, tokens->end[last], from))
        {
            tokens->end[last] = (unsigned int)read->leaf[leaf].to;
            reading->line = read->leaf[leaf].line;
            continue;
        }
        (void)shape_met(reading, kind, shape_role_of(read, leaf), from, read->leaf[leaf].to, read->leaf[leaf].line);
    }
}

int cli_shape_tokens(CliSymbolsLanguages *languages, const char *language, const char *src, unsigned long long size,
                     CliShapeTokens *tokens, CliShapeHoles *holes)
{
    tokens->count = 0u;
    tokens->full = 0;
    CliShapeHolePlace places[CLI_SHAPE_HOLES];
    // the pattern's copy, its placeholders blank, which its grammar reads
    char *const blank = (holes != NULL) ? g_cli_cells->persistent_buf_alloc(size + 1ull, &g_cli_shape_cells) : NULL;
    if ((holes != NULL) && (blank == NULL))
    {
        return 0;
    }
    if (blank != NULL)
    {
        // a pattern's bytes fit a size_t
        memor.cpy(&(MemoriaCfg){.dst = blank, .src = src, .bytes = (size_t)size});
        blank[size] = '\0';
        shape_holes_found(src, size, holes, places, blank);
    }
    CliLeafRead read;
    const int leaved =
        cli_symbols_language_leaves_kept(languages, language, (blank != NULL) ? blank : src, size, &read);
    if (leaved)
    {
        CliShapeReading reading = {src, size, tokens, cli_symbols_line_ends_statement(languages, language), 0u, 0u};
        shape_leaves_put(&reading, &read, holes, places);
        const unsigned long long ended =
            (tokens->count > 0u) ? shape_line_end(&reading, tokens->end[tokens->count - 1u]) : size;
        if (reading.line_ends_statement && (reading.depth == 0u) && (ended < size) &&
            (tokens->kind[tokens->count - 1u] != CLI_SHAPE_BREAK))
        {
            (void)shape_put(tokens, CLI_SHAPE_BREAK, 0u, ended, ended + 1u);
        }
    }
    cli_symbols_leaves_given(&read);
    g_cli_cells->persistent_buf_release(blank, &g_cli_shape_cells);
    return leaved;
}
