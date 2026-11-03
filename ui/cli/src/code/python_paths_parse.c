// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_parse.c: an expression read from a script's text as the Python paths reading takes it: names,
// strings, whole numbers, tuples and lists, attributes, calls, indexes and slices, `/` and `+`; a keyword argument's
// name is passed over
#include "../editor/files_internal.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "python_paths_internal.h"

unsigned int cli_py_text_append(const CliPythonPathsTextCtx *args)
{
    if ((g_cli_py.used + args->length + 1ULL) > g_cli_py.text_max)
    {
        return CLI_PY_NONE;
    }
    const unsigned int at = g_cli_py.used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_py.text + at, .src = args->text, .bytes = args->length});
    g_cli_py.text[at + args->length] = '\0';
    // the texts are below their most, CLI_PY_TEXT bytes at the most, which an unsigned int stores
    g_cli_py.used += (unsigned int)args->length + 1u;
    return at;
}

const char *cli_py_text(const CliPythonPathsAtCtx *args)
{
    return g_cli_py.text + args->at;
}

// the capacity the texts have left, as a line a text is written in where it is put
static VerbaLinea cli_text_line(void)
{
    return (VerbaLinea){g_cli_py.text + g_cli_py.used, g_cli_py.text_max - g_cli_py.used, 0ULL};
}

typedef struct
{
    const VerbaLinea *line;
    unsigned int from;
    unsigned int to;
    unsigned int kind;
    unsigned int text;
    long long number;
    unsigned int left;
    unsigned int right;
    unsigned int first_value;
    unsigned int count_value;
    unsigned int at;
    unsigned int letter;
    unsigned int *start;
    unsigned int mark;
    unsigned int *first;
    unsigned int *count;
    unsigned int close;
    unsigned int quote;
    int raw;
} PythonPathsParseCtx;
// the text written in `line`, which cli_text_line gave, put: its place, or CLI_PY_NONE where it did not fit
EMBED_INLINE unsigned int cli_text_line_append(const PythonPathsParseCtx *args)
{
    if (!mmgr_line_ok(args->line))
    {
        return CLI_PY_NONE;
    }
    const unsigned int at = g_cli_py.used;
    g_cli_py.text[at + args->line->at] = '\0';
    // the line fit in the capacity the texts have left, below text_max, which an unsigned int stores
    g_cli_py.used += (unsigned int)args->line->at + 1u;
    return at;
}

// the letters from `from` to `to` put as a text: its place, or CLI_PY_NONE
EMBED_INLINE unsigned int cli_letters_append(const PythonPathsParseCtx *args)
{
    VerbaLinea line = cli_text_line();
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line,
                   .letter = g_cli_py.letter[at]);
    }
    return EMBED_CALL(cli_text_line_append, PythonPathsParseCtx, .line = &line);
}

// an expression made: its place, or CLI_PY_NONE where the reading has no capacity for more
EMBED_INLINE unsigned int cli_expr_init(const PythonPathsParseCtx *args)
{
    if ((g_cli_py.exprs >= g_cli_py.exprs_max) || ((args->kind <= CLI_EXPR_TEXT) && (args->text == CLI_PY_NONE)))
    {
        return CLI_PY_NONE;
    }
    g_cli_py.expr[g_cli_py.exprs] =
        (CliExpr){args->kind, args->text, args->number, args->left, args->right, args->first_value, args->count_value};
    g_cli_py.exprs += 1u;
    return g_cli_py.exprs - 1u;
}

// whether the letter at `at` is `letter`
EMBED_INLINE int cli_letter_is(const PythonPathsParseCtx *args)
{
    return (args->at < g_cli_py.letters) && (g_cli_py.letter[args->at] == args->letter);
}

// whitespace and line joins passed over
static void cli_blank(void)
{
    while ((g_cli_py.at < g_cli_py.letters) && (EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx,
                                                           .letter = g_cli_py.letter[g_cli_py.at]) ||
                                                (g_cli_py.letter[g_cli_py.at] == '\\')))
    {
        g_cli_py.at += 1u;
    }
}

// whether `letter` comes next, past whitespace, taken where it does
EMBED_INLINE int cli_eat(const PythonPathsParseCtx *args)
{
    cli_blank();
    if (EMBED_CALL(cli_letter_is, PythonPathsParseCtx, .at = g_cli_py.at, .letter = args->letter))
    {
        g_cli_py.at += 1u;
        return 1;
    }
    return 0;
}

// a word of letters, numbers and `_` taken: 1 with where it starts into `start`, or 0 where none starts here
EMBED_INLINE int cli_word(const PythonPathsParseCtx *args)
{
    unsigned int *start = args->start;
    *start = g_cli_py.at;
    while ((g_cli_py.at < g_cli_py.letters) &&
           EMBED_CALL(cli_files_letter_internal.hit_wordy, CliFilesLetterCtx, .letter = g_cli_py.letter[g_cli_py.at]))
    {
        g_cli_py.at += 1u;
    }
    return g_cli_py.at > *start;
}

static unsigned int cli_expr(void);

// the items of the list being read, from `mark` on the stack, put in the items: 1 with their place and count, or 0
EMBED_INLINE int cli_list_end(const PythonPathsParseCtx *args)
{
    unsigned int *first = args->first;
    unsigned int *count = args->count;
    const unsigned int length = g_cli_py.stacked - args->mark;
    if ((g_cli_py.items + length) > g_cli_py.items_max)
    {
        g_cli_py.stacked = args->mark;
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = g_cli_py.item + g_cli_py.items,
                            .src = g_cli_py.stack + args->mark,
                            .bytes = length * sizeof(unsigned int)});
    *first = g_cli_py.items;
    *count = length;
    g_cli_py.items += length;
    g_cli_py.stacked = args->mark;
    return 1;
}

// expressions parted by `,` up to `close`, a keyword argument's name passed over: 1 with their place and count, or 0
EMBED_INLINE int cli_list(const PythonPathsParseCtx *args)
{
    const unsigned int mark = g_cli_py.stacked;
    for (;;)
    {
        if (EMBED_CALL(cli_eat, PythonPathsParseCtx, .letter = args->close))
        {
            return EMBED_CALL(cli_list_end, PythonPathsParseCtx, .mark = mark, .first = args->first,
                              .count = args->count);
        }
        const unsigned int start = g_cli_py.at;
        unsigned int word_text = 0u;
        if (EMBED_CALL(cli_word, PythonPathsParseCtx, .start = &word_text))
        {
            cli_blank();
            const int keyword = EMBED_CALL(cli_letter_is, PythonPathsParseCtx, .at = g_cli_py.at, .letter = '=') &&
                                !EMBED_CALL(cli_letter_is, PythonPathsParseCtx, .at = g_cli_py.at + 1u, .letter = '=');
            g_cli_py.at = keyword ? (g_cli_py.at + 1u) : start;
        }
        const unsigned int item = cli_expr();
        if ((item == CLI_PY_NONE) || (g_cli_py.stacked >= g_cli_py.items_max))
        {
            g_cli_py.stacked = mark;
            return 0;
        }
        g_cli_py.stack[g_cli_py.stacked] = item;
        g_cli_py.stacked += 1u;
        if (!EMBED_CALL(cli_eat, PythonPathsParseCtx, .letter = ','))
        {
            if (EMBED_CALL(cli_eat, PythonPathsParseCtx, .letter = args->close))
            {
                return EMBED_CALL(cli_list_end, PythonPathsParseCtx, .mark = mark, .first = args->first,
                                  .count = args->count);
            }
            g_cli_py.stacked = mark;
            return 0;
        }
    }
}

// a string read from its opening quote at `quote`, escapes read where it is not raw: its expression, or CLI_PY_NONE
// where it does not end
EMBED_INLINE unsigned int cli_string(const PythonPathsParseCtx *args)
{
    VerbaLinea line = cli_text_line();
    const unsigned int mark = g_cli_py.letter[args->quote];
    g_cli_py.at = args->quote + 1u;
    while (g_cli_py.at < g_cli_py.letters)
    {
        const unsigned int next = g_cli_py.letter[g_cli_py.at];
        g_cli_py.at += 1u;
        if (next == mark)
        {
            return EMBED_CALL(cli_expr_init, PythonPathsParseCtx, .kind = CLI_EXPR_TEXT,
                              .text = EMBED_CALL(cli_text_line_append, PythonPathsParseCtx, .line = &line),
                              .number = 0LL, .left = 0u, .right = 0u, .first_value = 0u, .count_value = 0u);
        }
        if ((next == '\\') && !args->raw)
        {
            if (g_cli_py.at < g_cli_py.letters)
            {
                const unsigned int escaped = g_cli_py.letter[g_cli_py.at];
                g_cli_py.at += 1u;
                const int unescaped = (escaped == 't') ? '\t' : escaped;
                EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line,
                           .letter = (escaped == 'n') ? '\n' : unescaped);
            }
            continue;
        }
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line, .letter = next);
    }
    return CLI_PY_NONE;
}

// a name, a string, a whole number, or a tuple or list: its expression, or CLI_PY_NONE
static unsigned int cli_primary(void)
{
    cli_blank();
    if (g_cli_py.at >= g_cli_py.letters)
    {
        return CLI_PY_NONE;
    }
    const unsigned int letter = g_cli_py.letter[g_cli_py.at];
    unsigned int first = 0u;
    unsigned int count = 0u;
    if ((letter == '(') || (letter == '['))
    {
        g_cli_py.at += 1u;
        if (!EMBED_CALL(cli_list, PythonPathsParseCtx, .close = (letter == '(') ? ')' : ']', .first = &first,
                        .count = &count))
        {
            return CLI_PY_NONE;
        }
        return ((letter == '(') && (count == 1u))
                   ? g_cli_py.item[first]
                   : EMBED_CALL(cli_expr_init, PythonPathsParseCtx, .kind = CLI_EXPR_MANY, .text = 0u, .number = 0LL,
                                .left = 0u, .right = 0u, .first_value = first, .count_value = count);
    }
    unsigned int start = 0u;
    if ((letter >= '0') && (letter <= '9'))
    {
        EMBED_CALL(cli_word, PythonPathsParseCtx, .start = &start);
        long long number = 0LL;
        for (unsigned int at = start; at < g_cli_py.at; at += 1u)
        {
            const unsigned int digit = g_cli_py.letter[at] - '0';
            if ((digit > 9u) || (number > ((0x7fffffffffffffffLL - (long long)digit) / 10LL)))
            {
                return CLI_PY_NONE;
            }
            number = (number * 10LL) + (long long)digit;
        }
        return EMBED_CALL(cli_expr_init, PythonPathsParseCtx, .kind = CLI_EXPR_NUMBER, .text = 0u, .number = number,
                          .left = 0u, .right = 0u, .first_value = 0u, .count_value = 0u);
    }
    const unsigned int mark = g_cli_py.at;
    unsigned int quote = mark;
    int raw = 0;
    int formatted = 0;
    while ((quote < g_cli_py.letters) &&
           (((g_cli_py.letter[quote] | 0x20u) >= 'a') && ((g_cli_py.letter[quote] | 0x20u) <= 'z')))
    {
        raw = raw || ((g_cli_py.letter[quote] | 0x20u) == 'r');
        formatted = formatted || ((g_cli_py.letter[quote] | 0x20u) == 'f');
        quote += 1u;
    }
    if (((quote - mark) <= 2u) && (EMBED_CALL(cli_letter_is, PythonPathsParseCtx, .at = quote, .letter = '"') ||
                                   EMBED_CALL(cli_letter_is, PythonPathsParseCtx, .at = quote, .letter = '\'')))
    {
        return formatted ? CLI_PY_NONE : EMBED_CALL(cli_string, PythonPathsParseCtx, .quote = quote, .raw = raw);
    }
    g_cli_py.at = mark;
    if (!EMBED_CALL(cli_word, PythonPathsParseCtx, .start = &start))
    {
        return CLI_PY_NONE;
    }
    return EMBED_CALL(cli_expr_init, PythonPathsParseCtx, .kind = CLI_EXPR_NAME,
                      .text = EMBED_CALL(cli_letters_append, PythonPathsParseCtx, .from = start, .to = g_cli_py.at),
                      .number = 0LL, .left = 0u, .right = 0u, .first_value = 0u, .count_value = 0u);
}

// a primary with its attributes, calls and indexes after it: its expression, or CLI_PY_NONE
static unsigned int cli_postfix(void)
{
    unsigned int value = cli_primary();
    while (value != CLI_PY_NONE)
    {
        unsigned int first = 0u;
        unsigned int count = 0u;
        if (EMBED_CALL(cli_eat, PythonPathsParseCtx, .letter = '.'))
        {
            cli_blank();
            unsigned int start = 0u;
            if (!EMBED_CALL(cli_word, PythonPathsParseCtx, .start = &start))
            {
                return CLI_PY_NONE;
            }
            value = EMBED_CALL(
                cli_expr_init, PythonPathsParseCtx, .kind = CLI_EXPR_ATTR,
                .text = EMBED_CALL(cli_letters_append, PythonPathsParseCtx, .from = start, .to = g_cli_py.at),
                .number = 0LL, .left = value, .right = 0u, .first_value = 0u, .count_value = 0u);
        }
        else if (EMBED_CALL(cli_eat, PythonPathsParseCtx, .letter = '('))
        {
            if (!EMBED_CALL(cli_list, PythonPathsParseCtx, .close = ')', .first = &first, .count = &count))
            {
                return CLI_PY_NONE;
            }
            value = EMBED_CALL(cli_expr_init, PythonPathsParseCtx, .kind = CLI_EXPR_CALL, .text = 0u, .number = 0LL,
                               .left = value, .right = 0u, .first_value = first, .count_value = count);
        }
        else if (EMBED_CALL(cli_eat, PythonPathsParseCtx, .letter = '['))
        {
            cli_blank();
            unsigned int index = CLI_PY_NONE;
            if (EMBED_CALL(cli_letter_is, PythonPathsParseCtx, .at = g_cli_py.at, .letter = ':'))
            {
                // a slice reads as its start, which the reading takes as 0
                while ((g_cli_py.at < g_cli_py.letters) && (g_cli_py.letter[g_cli_py.at] != ']'))
                {
                    g_cli_py.at += 1u;
                }
                index = EMBED_CALL(cli_expr_init, PythonPathsParseCtx, .kind = CLI_EXPR_NUMBER, .text = 0u,
                                   .number = 0LL, .left = 0u, .right = 0u, .first_value = 0u, .count_value = 0u);
            }
            else
            {
                index = cli_expr();
            }
            if ((index == CLI_PY_NONE) || !EMBED_CALL(cli_eat, PythonPathsParseCtx, .letter = ']'))
            {
                return CLI_PY_NONE;
            }
            value = EMBED_CALL(cli_expr_init, PythonPathsParseCtx, .kind = CLI_EXPR_INDEX, .text = 0u, .number = 0LL,
                               .left = value, .right = index, .first_value = 0u, .count_value = 0u);
        }
        else
        {
            return value;
        }
    }
    return CLI_PY_NONE;
}

// postfix expressions joined by `/` and added by `+`: the expression, or CLI_PY_NONE
static unsigned int cli_expr(void)
{
    unsigned int left = cli_postfix();
    while (left != CLI_PY_NONE)
    {
        const int join = EMBED_CALL(cli_eat, PythonPathsParseCtx, .letter = '/');
        if (!join && !EMBED_CALL(cli_eat, PythonPathsParseCtx, .letter = '+'))
        {
            return left;
        }
        const unsigned int right = cli_postfix();
        if (right == CLI_PY_NONE)
        {
            return CLI_PY_NONE;
        }
        left = EMBED_CALL(cli_expr_init, PythonPathsParseCtx, .kind = join ? CLI_EXPR_JOIN : CLI_EXPR_ADD, .text = 0u,
                          .number = 0LL, .left = left, .right = right, .first_value = 0u, .count_value = 0u);
    }
    return CLI_PY_NONE;
}

unsigned int cli_py_parse(const CliPythonPathsTextCtx *args)
{
    // a letter count is below the reading's most, which an unsigned int stores
    g_cli_py.letters =
        (unsigned int)EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->text, .size = args->length,
                                 .letters = g_cli_py.letter, .capacity_u64 = g_cli_py.letters_max);
    g_cli_py.at = 0u;
    g_cli_py.stacked = 0u;
    const unsigned int expr = cli_expr();
    cli_blank();
    return (g_cli_py.at >= g_cli_py.letters) ? expr : CLI_PY_NONE;
}
