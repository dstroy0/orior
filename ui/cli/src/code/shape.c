// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape.c: a text read as tokens, past spaces and comments: names, numbers, strings and operators, an operator of more
// than one letter read whole, and in Python a line's end outside brackets as a statement's end. A string's prefix
// letters are read up to its quote, a backtick's string from its backtick. Tokens past what their storage holds are
// not read
#include "shape_internal.h"

// the operators read as one token, each before any it starts with that follows it
static const char *const s_operators[40] = {">>>=", "...", "**=", "//=", ">>=", "<<=", "===", "!==",  "->", "=>",
                                            "::",   "==",  "!=",  "<=",  ">=",  "&&",  "||",  "??",   "?.", "+=",
                                            "-=",   "*=",  "/=",  "%=",  "&=",  "|=",  "^=",  "**",   "//", "<<",
                                            ">>",   "++",  "--",  ":=",  "@=",  "&&=", "||=", "?\?=", "..", "#!"};

// the languages that comment with `#`
static const char *const s_hash_comments[10] = {"python", "shell", "bash", "powershell", "r",
                                                "ruby",   "yaml",  "toml", "makefile",   "cmake"};

// the letter at `at` of the text and the bytes it takes, 0 past its end
static unsigned int shape_letter(const char *src, unsigned long long size, unsigned long long at,
                                 unsigned long long *length)
{
    if (at >= size)
    {
        *length = 0ull;
        return 0u;
    }
    return cli_text_letter(src, size, at, length);
}

// the letter `ahead` letters after the one at `at`, 0 past the text's end
static unsigned int shape_ahead(const char *src, unsigned long long size, unsigned long long at, unsigned int ahead)
{
    unsigned long long length = 0ull;
    unsigned int letter = shape_letter(src, size, at, &length);
    for (unsigned int step = 0u; (step < ahead) && (length > 0u); step += 1u)
    {
        at += length;
        letter = shape_letter(src, size, at, &length);
    }
    return (length > 0u) ? letter : 0u;
}

// `at` moved past one letter
static void shape_step(const char *src, unsigned long long size, unsigned long long *at)
{
    unsigned long long length = 0ull;
    (void)shape_letter(src, size, *at, &length);
    *at += (length > 0u) ? length : 1u;
}

// a token put on the tokens where it fits
static void shape_put(CliShapeTokens *tokens, unsigned char kind, unsigned long long start, unsigned long long end)
{
    if (tokens->count == tokens->most)
    {
        tokens->full = 1;
        return;
    }
    tokens->kind[tokens->count] = kind;
    tokens->start[tokens->count] = (unsigned int)start;
    tokens->end[tokens->count] = (unsigned int)end;
    tokens->count += 1u;
}

// whether the name at `start` is a Python string's prefix: up to two of r, b, f, u and t before a quote
static int shape_string_prefix(const char *src, unsigned long long size, unsigned long long start)
{
    unsigned long long at = start;
    unsigned int letters = 0u;
    while (at < size)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter(src, size, at, &length);
        if (!cli_text_alphabetic(letter))
        {
            break;
        }
        if ((letter >= 0x80u) ||
            (cellul.chr(&(CatenaFinitaCfg){.src = "rRbBfFuUtT", .cap = CLI_CELLS_POOL, .byte = (int)letter}) == NULL))
        {
            return 0;
        }
        letters += 1u;
        at += length;
    }
    return (letters <= 2u) && (at < size) && ((src[at] == '"') || (src[at] == '\''));
}

// a placeholder at `start`, `$name$` or `$name:kind$`, put as a name token, `*at` moved past it: 1, or 0 where none is
// there
static int shape_hole(const char *src, unsigned long long size, unsigned long long start, unsigned long long *at,
                      CliShapeTokens *tokens, CliShapeHoles *holes)
{
    unsigned long long close = 0ull;
    if (!cli_text_find(src + start + 1u, size - start - 1u, "$", 1u, &close))
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
    unsigned int hole = CLI_SHAPE_HOLE_EXPR;
    if ((kind_size == 4u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "name", .bytes = 4u}) == 0))
    {
        hole = CLI_SHAPE_HOLE_NAME;
    }
    else if (((kind_size == 4u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "stmt", .bytes = 4u}) == 0)) ||
             ((kind_size == 5u) && (memor.cmp(&(MemoriaCfg){.src = kind, .other = "stmts", .bytes = 5u}) == 0)))
    {
        hole = CLI_SHAPE_HOLE_STMT;
    }
    const unsigned long long end = start + 1u + close + 1u;
    *at = end;
    if (holes->count < CLI_SHAPE_HOLES)
    {
        holes->hole[holes->count] = (CliShapeHole){tokens->count, inside, name_size, hole};
        holes->count += 1u;
    }
    shape_put(tokens, CLI_SHAPE_NAME, start, end);
    return 1;
}

// a string from `*at`, its prefix letters and its quotes, `*at` moved past it
static void shape_string(const char *src, unsigned long long size, int python, unsigned long long *at)
{
    while ((*at < size) && (src[*at] != '"') && (src[*at] != '\'') && (src[*at] != '`'))
    {
        shape_step(src, size, at);
    }
    if (*at >= size)
    {
        return;
    }
    const char quote = src[*at];
    const int triple = python && (*at + 2u < size) && (src[*at + 1u] == quote) && (src[*at + 2u] == quote);
    *at += triple ? 3u : 1u;
    while (*at < size)
    {
        const char here = src[*at];
        if (here == '\\')
        {
            shape_step(src, size, at);
            if (*at < size)
            {
                shape_step(src, size, at);
            }
            continue;
        }
        if ((here == quote) && (!triple || ((*at + 2u < size) && (src[*at + 1u] == quote) && (src[*at + 2u] == quote))))
        {
            *at += triple ? 3u : 1u;
            return;
        }
        if ((here == '\n') && !triple && (quote != '`'))
        {
            return;
        }
        shape_step(src, size, at);
    }
}

// a comment from `*at`, `*at` moved past it: 1, or 0 where none starts there
static int shape_comment(const char *src, unsigned long long size, int hashes, unsigned int letter,
                         unsigned long long *at)
{
    const unsigned int next = shape_ahead(src, size, *at, 1u);
    if ((hashes && (letter == '#')) || (!hashes && (letter == '/') && (next == '/')))
    {
        while ((*at < size) && (src[*at] != '\n'))
        {
            shape_step(src, size, at);
        }
        return 1;
    }
    if (hashes || (letter != '/') || (next != '*'))
    {
        return 0;
    }
    *at += 2u;
    while ((*at < size) && !((src[*at] == '*') && (*at + 1u < size) && (src[*at + 1u] == '/')))
    {
        shape_step(src, size, at);
    }
    for (unsigned int step = 0u; (step < 2u) && (*at < size); step += 1u)
    {
        shape_step(src, size, at);
    }
    return 1;
}

void cli_shape_tokens(const char *language, const char *src, unsigned long long size, CliShapeTokens *tokens,
                      CliShapeHoles *holes)
{
    const int python = (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL}));
    int hashes = 0;
    for (unsigned int at = 0u; at < 10u; at += 1u)
    {
        hashes |= (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = s_hash_comments[at], .cap = CLI_CELLS_POOL}));
    }
    tokens->count = 0u;
    tokens->full = 0;
    long long depth = 0;
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned long long start = at;
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter(src, size, at, &length);
        if (letter == '\n')
        {
            if (python && (depth == 0) && (tokens->count > 0u) && (tokens->kind[tokens->count - 1u] != CLI_SHAPE_BREAK))
            {
                shape_put(tokens, CLI_SHAPE_BREAK, start, start + 1u);
            }
            at += 1u;
            continue;
        }
        if (cli_text_white(letter) ||
            (python && (letter == '\\') &&
             ((shape_ahead(src, size, at, 1u) == '\n') || (shape_ahead(src, size, at, 1u) == '\r'))))
        {
            at += length;
            continue;
        }
        if (shape_comment(src, size, hashes, letter, &at))
        {
            continue;
        }
        if ((letter == '$') && (holes != NULL) && shape_hole(src, size, start, &at, tokens, holes))
        {
            continue;
        }
        unsigned char kind = CLI_SHAPE_PUNCT;
        if ((letter == '"') || (letter == '\'') || (letter == '`') ||
            (python && cli_text_alphabetic(letter) && shape_string_prefix(src, size, start)))
        {
            shape_string(src, size, python, &at);
            kind = CLI_SHAPE_STR;
        }
        else if (cli_text_alphabetic(letter) || (letter == '_') || (letter == '$'))
        {
            unsigned int one = letter;
            while ((at < size) && (cli_text_alphanumeric(one) || (one == '_') || (one == '$')))
            {
                at += length;
                one = shape_letter(src, size, at, &length);
            }
            kind = CLI_SHAPE_NAME;
        }
        else if ((letter >= '0') && (letter <= '9'))
        {
            unsigned int one = letter;
            while ((at < size) && (cli_text_alphanumeric(one) || (one == '_') || (one == '.')))
            {
                at += length;
                one = shape_letter(src, size, at, &length);
            }
            kind = CLI_SHAPE_NUMBER;
        }
        else
        {
            unsigned long long taken = length;
            for (unsigned int op = 0u; op < 40u; op += 1u)
            {
                if (cli_text_starts(src + start, size - start, s_operators[op]))
                {
                    taken = cellul.len(&(CatenaFinitaCfg){.src = s_operators[op], .cap = CLI_CELLS_POOL});
                    break;
                }
            }
            at += taken;
            depth += ((letter == '(') || (letter == '[') || (letter == '{')) ? 1 : 0;
            depth -= (((letter == ')') || (letter == ']') || (letter == '}')) && (depth > 0)) ? 1 : 0;
        }
        shape_put(tokens, kind, start, (at < size) ? at : size);
    }
}
