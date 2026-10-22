// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_parse.c: an expression read from a script's text as the Python paths reading takes it: names,
// strings, whole numbers, tuples and lists, attributes, calls, indexes and slices, `/` and `+`; a keyword argument's
// name is passed over
#include "../editor/files_internal.h"
#include "../text/regexp.h"
#include "python_paths_internal.h"

unsigned int cli_py_text_put(const char *text, unsigned long long length)
{
    if ((g_cli_py.used + length + 1ull) > g_cli_py.text_most)
    {
        return CLI_PY_NONE;
    }
    const unsigned int at = g_cli_py.used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_py.text + at, .src = text, .bytes = length});
    g_cli_py.text[at + length] = '\0';
    // the texts are below their most, CLI_PY_TEXT bytes at the most, which an unsigned int holds
    g_cli_py.used += (unsigned int)length + 1u;
    return at;
}

const char *cli_py_text(unsigned int at)
{
    return g_cli_py.text + at;
}

// the room the texts have left, as a line a text is written in where it is put
static CliLine cli_text_line(void)
{
    return (CliLine){g_cli_py.text + g_cli_py.used, g_cli_py.text_most - g_cli_py.used, 0ull};
}

// the text written in `line`, which cli_text_line gave, put: its place, or CLI_PY_NONE where it did not fit
static unsigned int cli_text_line_put(const CliLine *line)
{
    if (!cli_line_fits(line))
    {
        return CLI_PY_NONE;
    }
    const unsigned int at = g_cli_py.used;
    g_cli_py.text[at + line->at] = '\0';
    // the line fit in the room the texts have left, below text_most, which an unsigned int holds
    g_cli_py.used += (unsigned int)line->at + 1u;
    return at;
}

// the letters from `from` to `to` put as a text: its place, or CLI_PY_NONE
static unsigned int cli_letters_put(unsigned int from, unsigned int to)
{
    CliLine line = cli_text_line();
    for (unsigned int at = from; at < to; at += 1u)
    {
        cli_utf8_put(&line, g_cli_py.letter[at]);
    }
    return cli_text_line_put(&line);
}

// an expression made: its place, or CLI_PY_NONE where the reading holds no more
static unsigned int cli_expr_made(unsigned int kind, unsigned int text, long long number, unsigned int left,
                                  unsigned int right, unsigned int first, unsigned int count)
{
    if ((g_cli_py.exprs >= g_cli_py.exprs_most) || ((kind <= CLI_EXPR_TEXT) && (text == CLI_PY_NONE)))
    {
        return CLI_PY_NONE;
    }
    g_cli_py.expr[g_cli_py.exprs] = (CliExpr){kind, text, number, left, right, first, count};
    g_cli_py.exprs += 1u;
    return g_cli_py.exprs - 1u;
}

// whether the letter at `at` is `letter`
static int cli_letter_is(unsigned int at, unsigned int letter)
{
    return (at < g_cli_py.letters) && (g_cli_py.letter[at] == letter);
}

// whitespace and line joins passed over
static void cli_blank(void)
{
    while ((g_cli_py.at < g_cli_py.letters) &&
           (cli_fuzzy_white(g_cli_py.letter[g_cli_py.at]) || (g_cli_py.letter[g_cli_py.at] == '\\')))
    {
        g_cli_py.at += 1u;
    }
}

// whether `letter` comes next, past whitespace, taken where it does
static int cli_eat(unsigned int letter)
{
    cli_blank();
    if (cli_letter_is(g_cli_py.at, letter))
    {
        g_cli_py.at += 1u;
        return 1;
    }
    return 0;
}

// a word of letters, numbers and `_` taken: 1 with where it starts into `start`, or 0 where none starts here
static int cli_word(unsigned int *start)
{
    *start = g_cli_py.at;
    while ((g_cli_py.at < g_cli_py.letters) && cli_hit_wordy(g_cli_py.letter[g_cli_py.at]))
    {
        g_cli_py.at += 1u;
    }
    return g_cli_py.at > *start;
}

static unsigned int cli_expr(void);

// the items of the list being read, from `mark` on the stack, put in the items: 1 with their place and count, or 0
static int cli_list_ended(unsigned int mark, unsigned int *first, unsigned int *count)
{
    const unsigned int length = g_cli_py.stacked - mark;
    if ((g_cli_py.items + length) > g_cli_py.items_most)
    {
        g_cli_py.stacked = mark;
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = g_cli_py.item + g_cli_py.items, .src = g_cli_py.stack + mark, .bytes = length * sizeof(unsigned int)});
    *first = g_cli_py.items;
    *count = length;
    g_cli_py.items += length;
    g_cli_py.stacked = mark;
    return 1;
}

// expressions parted by `,` up to `close`, a keyword argument's name passed over: 1 with their place and count, or 0
static int cli_list(unsigned int close, unsigned int *first, unsigned int *count)
{
    const unsigned int mark = g_cli_py.stacked;
    for (;;)
    {
        if (cli_eat(close))
        {
            return cli_list_ended(mark, first, count);
        }
        const unsigned int start = g_cli_py.at;
        unsigned int word = 0u;
        if (cli_word(&word))
        {
            cli_blank();
            const int keyword = cli_letter_is(g_cli_py.at, '=') && !cli_letter_is(g_cli_py.at + 1u, '=');
            g_cli_py.at = keyword ? (g_cli_py.at + 1u) : start;
        }
        const unsigned int item = cli_expr();
        if ((item == CLI_PY_NONE) || (g_cli_py.stacked >= g_cli_py.items_most))
        {
            g_cli_py.stacked = mark;
            return 0;
        }
        g_cli_py.stack[g_cli_py.stacked] = item;
        g_cli_py.stacked += 1u;
        if (!cli_eat(','))
        {
            if (cli_eat(close))
            {
                return cli_list_ended(mark, first, count);
            }
            g_cli_py.stacked = mark;
            return 0;
        }
    }
}

// a string read from its opening quote at `quote`, escapes read where it is not raw: its expression, or CLI_PY_NONE
// where it does not end
static unsigned int cli_string(unsigned int quote, int raw)
{
    CliLine line = cli_text_line();
    const unsigned int mark = g_cli_py.letter[quote];
    g_cli_py.at = quote + 1u;
    while (g_cli_py.at < g_cli_py.letters)
    {
        const unsigned int next = g_cli_py.letter[g_cli_py.at];
        g_cli_py.at += 1u;
        if (next == mark)
        {
            return cli_expr_made(CLI_EXPR_TEXT, cli_text_line_put(&line), 0ll, 0u, 0u, 0u, 0u);
        }
        if ((next == '\\') && !raw)
        {
            if (g_cli_py.at < g_cli_py.letters)
            {
                const unsigned int escaped = g_cli_py.letter[g_cli_py.at];
                g_cli_py.at += 1u;
                cli_utf8_put(&line, (escaped == 'n') ? '\n' : ((escaped == 't') ? '\t' : escaped));
            }
            continue;
        }
        cli_utf8_put(&line, next);
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
        if (!cli_list((letter == '(') ? ')' : ']', &first, &count))
        {
            return CLI_PY_NONE;
        }
        return ((letter == '(') && (count == 1u)) ? g_cli_py.item[first]
                                                  : cli_expr_made(CLI_EXPR_MANY, 0u, 0ll, 0u, 0u, first, count);
    }
    unsigned int start = 0u;
    if ((letter >= '0') && (letter <= '9'))
    {
        cli_word(&start);
        long long number = 0ll;
        for (unsigned int at = start; at < g_cli_py.at; at += 1u)
        {
            const unsigned int digit = g_cli_py.letter[at] - '0';
            if ((digit > 9u) || (number > ((0x7fffffffffffffffll - (long long)digit) / 10ll)))
            {
                return CLI_PY_NONE;
            }
            number = (number * 10ll) + (long long)digit;
        }
        return cli_expr_made(CLI_EXPR_NUMBER, 0u, number, 0u, 0u, 0u, 0u);
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
    if (((quote - mark) <= 2u) && (cli_letter_is(quote, '"') || cli_letter_is(quote, '\'')))
    {
        return formatted ? CLI_PY_NONE : cli_string(quote, raw);
    }
    g_cli_py.at = mark;
    if (!cli_word(&start))
    {
        return CLI_PY_NONE;
    }
    return cli_expr_made(CLI_EXPR_NAME, cli_letters_put(start, g_cli_py.at), 0ll, 0u, 0u, 0u, 0u);
}

// a primary with its attributes, calls and indexes after it: its expression, or CLI_PY_NONE
static unsigned int cli_postfix(void)
{
    unsigned int value = cli_primary();
    while (value != CLI_PY_NONE)
    {
        unsigned int first = 0u;
        unsigned int count = 0u;
        if (cli_eat('.'))
        {
            cli_blank();
            unsigned int start = 0u;
            if (!cli_word(&start))
            {
                return CLI_PY_NONE;
            }
            value = cli_expr_made(CLI_EXPR_ATTR, cli_letters_put(start, g_cli_py.at), 0ll, value, 0u, 0u, 0u);
        }
        else if (cli_eat('('))
        {
            if (!cli_list(')', &first, &count))
            {
                return CLI_PY_NONE;
            }
            value = cli_expr_made(CLI_EXPR_CALL, 0u, 0ll, value, 0u, first, count);
        }
        else if (cli_eat('['))
        {
            cli_blank();
            unsigned int index = CLI_PY_NONE;
            if (cli_letter_is(g_cli_py.at, ':'))
            {
                // a slice reads as its start, which the reading takes as 0
                while ((g_cli_py.at < g_cli_py.letters) && (g_cli_py.letter[g_cli_py.at] != ']'))
                {
                    g_cli_py.at += 1u;
                }
                index = cli_expr_made(CLI_EXPR_NUMBER, 0u, 0ll, 0u, 0u, 0u, 0u);
            }
            else
            {
                index = cli_expr();
            }
            if ((index == CLI_PY_NONE) || !cli_eat(']'))
            {
                return CLI_PY_NONE;
            }
            value = cli_expr_made(CLI_EXPR_INDEX, 0u, 0ll, value, index, 0u, 0u);
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
        const int join = cli_eat('/');
        if (!join && !cli_eat('+'))
        {
            return left;
        }
        const unsigned int right = cli_postfix();
        if (right == CLI_PY_NONE)
        {
            return CLI_PY_NONE;
        }
        left = cli_expr_made(join ? CLI_EXPR_JOIN : CLI_EXPR_ADD, 0u, 0ll, left, right, 0u, 0u);
    }
    return CLI_PY_NONE;
}

unsigned int cli_py_parse(const char *text, unsigned long long length)
{
    // a letter count is below the reading's most, which an unsigned int holds
    g_cli_py.letters = (unsigned int)cli_regexp_letters(text, length, g_cli_py.letter, g_cli_py.letters_most);
    g_cli_py.at = 0u;
    g_cli_py.stacked = 0u;
    const unsigned int expr = cli_expr();
    cli_blank();
    return (g_cli_py.at >= g_cli_py.letters) ? expr : CLI_PY_NONE;
}
