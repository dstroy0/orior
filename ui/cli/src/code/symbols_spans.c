// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_spans.c: the spans Expand Selection steps through, read off a text's leaves by the roles its grammar names.
// From the leaf a place stands in outward: each bracket pair around it, inside its brackets and with them; where the
// pair encloses a region, first the statement the place stands in among the region's, bounded at the region's depth by
// a statement's end, a region's close, a directive's line and the region's own brackets, and after the region the
// region with its head, the statement before its opening bracket; and last the whole text. A span is handed only where
// it holds the one before it and is more than it
#include "../text/edits.h"
#include "../text/text.h"
#include "symbols.h"
#include "symbols_internal.h"

// The spans being handed: the leaves, the text's size, where the last one handed stands, and to whom
typedef struct
{
    const CliLeafRead *read;
    unsigned long long size;
    unsigned long long from;
    unsigned long long to;
    CliSpanned each;
    void *context;
} CliSpans;

// whether the leaf `leaf` of `read` is a blank or a comment
static int spans_blank(const CliLeafRead *read, unsigned int leaf)
{
    return (read->leaf[leaf].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u;
}

// the byte of `size` bytes of `text` the place `place` falls at, the text's end where it is past it
static unsigned long long spans_byte_of(const char *text, unsigned long long size, CliSymbolPlace place)
{
    unsigned long long start = 0ull;
    for (unsigned int line = 0u; (line < place.line) && (start < size); start += 1ull)
    {
        line += (text[start] == '\n') ? 1u : 0u;
    }
    unsigned long long end = start;
    while ((end < size) && (text[end] != '\n'))
    {
        end += 1ull;
    }
    return start + cli_text_byte_of(text + start, end - start, place.col);
}

// the place of the byte `byte` of the text, the text's first byte or one on the line of the leaf `leaf` or past it by
// the line ends between
static CliSymbolPlace spans_place(const CliSpans *spans, unsigned int leaf, unsigned long long byte)
{
    if (byte == 0ull)
    {
        return (CliSymbolPlace){0u, 0u};
    }
    const char *const text = spans->read->text;
    unsigned int line = spans->read->leaf[leaf].line;
    unsigned long long start = spans->read->leaf[leaf].from;
    while ((start > 0ull) && (text[start - 1ull] != '\n'))
    {
        start -= 1ull;
    }
    for (unsigned long long at = spans->read->leaf[leaf].from; at < byte; at += 1ull)
    {
        if (text[at] == '\n')
        {
            line += 1u;
            start = at + 1ull;
        }
    }
    // a line's units are below 2^32
    return (CliSymbolPlace){line, (unsigned int)cli_text_units(text + start, byte - start)};
}

// the bytes `from` to `to` handed as a span, the leaves `first` and `last` on their lines, where they hold the last
// span handed and are more than it
static void spans_handed(CliSpans *spans, unsigned int first, unsigned long long from, unsigned int last,
                         unsigned long long to)
{
    if ((from > spans->from) || (to < spans->to) || ((from == spans->from) && (to == spans->to)))
    {
        return;
    }
    spans->each(spans->context, spans_place(spans, first, from), spans_place(spans, last, to));
    spans->from = from;
    spans->to = to;
}

// the leaves `first` to before `end` handed as a span, the blanks and comments at either end left out
static void spans_leaves_handed(CliSpans *spans, unsigned int first, unsigned int end)
{
    const CliLeafRead *const read = spans->read;
    while ((first < end) && spans_blank(read, first))
    {
        first += 1u;
    }
    while ((end > first) && spans_blank(read, end - 1u))
    {
        end -= 1u;
    }
    if (end > first)
    {
        spans_handed(spans, first, read->leaf[first].from, end - 1u, read->leaf[end - 1u].to);
    }
}

// the bracket that closes the pair the leaf `open` of `read` opens, `read->count` where none does
static unsigned int spans_closing(const CliLeafRead *read, unsigned int open)
{
    unsigned int depth = 0u;
    for (unsigned int leaf = open + 1u; leaf < read->count; leaf += 1u)
    {
        if (cli_leaf_has(read, leaf, CLI_LEAF_OPEN))
        {
            depth += 1u;
        }
        else if (cli_leaf_has(read, leaf, CLI_LEAF_CLOSE))
        {
            if (depth == 0u)
            {
                return leaf;
            }
            depth -= 1u;
        }
    }
    return read->count;
}

// the bracket that opens the pair the leaves before the leaf `leaf` of `read` stand in, CLI_JSON_NONE where none does
static unsigned int spans_opening(const CliLeafRead *read, unsigned int leaf)
{
    unsigned int depth = 0u;
    for (unsigned int back = leaf; back > 0u; back -= 1u)
    {
        if (cli_leaf_has(read, back - 1u, CLI_LEAF_CLOSE))
        {
            depth += 1u;
        }
        else if (cli_leaf_has(read, back - 1u, CLI_LEAF_OPEN))
        {
            if (depth == 0u)
            {
                return back - 1u;
            }
            depth -= 1u;
        }
    }
    return CLI_JSON_NONE;
}

// the first leaf of the statement the leaf `leaf` of `read` stands in among those from `first` on at their depth: past
// the last statement's end, region's close or directive's line at that depth before it, or the first of the directive's
// line it stands on
static unsigned int spans_statement_first(const CliLeafRead *read, unsigned int first, unsigned int leaf)
{
    unsigned int depth = 0u;
    unsigned int start = first;
    unsigned int line_first = first;
    unsigned int directive_line = CLI_SLICE_NOWHERE;
    for (unsigned int one = first; one < leaf; one += 1u)
    {
        line_first = ((one > first) && (read->leaf[one].line != read->leaf[one - 1u].line)) ? one : line_first;
        const int opens = cli_leaf_has(read, one, CLI_LEAF_OPEN);
        const int closes = cli_leaf_has(read, one, CLI_LEAF_CLOSE);
        depth = opens ? (depth + 1u) : ((closes && (depth > 0u)) ? (depth - 1u) : depth);
        if ((depth > 0u) || opens)
        {
            continue;
        }
        if ((read->leaf[one].is & CLI_LEAF_DIRECTIVE) != 0u)
        {
            directive_line = read->leaf[one].line;
            start = line_first;
        }
        else if (((read->leaf[one].line == directive_line) && (directive_line != read->leaf[leaf].line)) ||
                 cli_leaf_has(read, one, CLI_LEAF_STATEMENT) || (closes && cli_leaf_has(read, one, CLI_LEAF_REGION)))
        {
            start = one + 1u;
        }
    }
    return start;
}

// the leaf past the statement whose first leaf is `start`, the leaf `leaf` among it: past its end at its depth, at the
// opening bracket of the region its head is before, at the bracket that closes the pair it stands in, or past the
// directive's line it is
static unsigned int spans_statement_end(const CliLeafRead *read, unsigned int start, unsigned int leaf)
{
    int directive = 0;
    for (unsigned int one = start; (one < read->count) && (read->leaf[one].line == read->leaf[start].line); one += 1u)
    {
        directive |= (read->leaf[one].is & CLI_LEAF_DIRECTIVE) != 0u;
    }
    unsigned int depth = 0u;
    for (unsigned int one = start; one < read->count; one += 1u)
    {
        if (directive && (read->leaf[one].line != read->leaf[start].line))
        {
            return one;
        }
        if (cli_leaf_has(read, one, CLI_LEAF_OPEN))
        {
            if ((depth == 0u) && (one >= leaf) && cli_leaf_has(read, one, CLI_LEAF_REGION))
            {
                return one;
            }
            depth += 1u;
        }
        else if (cli_leaf_has(read, one, CLI_LEAF_CLOSE))
        {
            if (depth == 0u)
            {
                return one;
            }
            depth -= 1u;
        }
        else if ((depth == 0u) && (one >= leaf) && cli_leaf_has(read, one, CLI_LEAF_STATEMENT))
        {
            return one + 1u;
        }
    }
    return read->count;
}

// the pair the leaf `open` opens and `close` closes handed, inside its brackets and with them, the text's end where no
// bracket closes it
static void spans_pair_handed(CliSpans *spans, unsigned int open, unsigned int close)
{
    const CliLeafRead *const read = spans->read;
    const unsigned int last = (close < read->count) ? close : (read->count - 1u);
    const unsigned long long inside = (close < read->count) ? read->leaf[close].from : spans->size;
    const unsigned long long whole = (close < read->count) ? read->leaf[close].to : spans->size;
    spans_handed(spans, open, read->leaf[open].to, last, inside);
    spans_handed(spans, open, read->leaf[open].from, last, whole);
}

// the statement the leaf `leaf` stands in among those from `first` on handed, and where it is the head of a region,
// the region with it
static void spans_statement_handed(CliSpans *spans, unsigned int first, unsigned int leaf)
{
    const CliLeafRead *const read = spans->read;
    const unsigned int start = spans_statement_first(read, first, leaf);
    const unsigned int end = spans_statement_end(read, start, leaf);
    spans_leaves_handed(spans, start, end);
    if ((end < read->count) && cli_leaf_has(read, end, CLI_LEAF_REGION | CLI_LEAF_OPEN))
    {
        spans_leaves_handed(spans, start, spans_closing(read, end) + 1u);
    }
}

// the leaf a place at the byte `byte` stands in: the one that holds it, else the one that ends at it where that one is
// a name and the one that holds it is not, or is a blank; the last that starts at or before it
static unsigned int spans_leaf_at(const CliLeafRead *read, unsigned long long byte)
{
    unsigned int low = 0u;
    unsigned int high = read->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (read->leaf[middle].from <= byte)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    const unsigned int at = (low > 0u) ? (low - 1u) : 0u;
    const int before =
        (at > 0u) && (read->leaf[at].from == byte) && (read->leaf[at - 1u].to == byte) && !spans_blank(read, at - 1u);
    const int named_before =
        before && cli_leaf_has(read, at - 1u, CLI_LEAF_NAME) && !cli_leaf_has(read, at, CLI_LEAF_NAME);
    return (before && (spans_blank(read, at) || named_before)) ? (at - 1u) : at;
}

int cli_symbols_spans(const char *path, const char *text, unsigned long long size, CliSymbolPlace from,
                      CliSymbolPlace to, CliSpanned each, void *context, VerbaLinea *err_msg)
{
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    CliLeafRead read = {text, NULL, 0u, 0u, 0u, NULL, 0, NULL, NULL};
    const int leaved = (languages != NULL) && cli_symbols_leaves_kept(languages, path, "root", text, size, &read);
    if (leaved && (read.count > 0u))
    {
        CliSpans spans = {&read, size, spans_byte_of(text, size, from), spans_byte_of(text, size, to), each, context};
        const unsigned int word = spans_leaf_at(&read, spans.from);
        if (!spans_blank(&read, word))
        {
            spans_handed(&spans, word, read.leaf[word].from, word, read.leaf[word].to);
        }
        // each pair around the place from the innermost out, the statement of a region's before its brackets
        unsigned int inner = word;
        unsigned int open = cli_leaf_has(&read, word, CLI_LEAF_OPEN) ? word : spans_opening(&read, word);
        while (open != CLI_JSON_NONE)
        {
            const unsigned int outer = spans_opening(&read, open);
            const int region = cli_leaf_has(&read, open, CLI_LEAF_REGION);
            if (region && (inner != open))
            {
                spans_statement_handed(&spans, open + 1u, inner);
            }
            const unsigned int close = spans_closing(&read, open);
            spans_pair_handed(&spans, open, close);
            if (region)
            {
                const unsigned int head =
                    spans_statement_first(&read, (outer != CLI_JSON_NONE) ? (outer + 1u) : 0u, open);
                spans_leaves_handed(&spans, head, (close < read.count) ? (close + 1u) : read.count);
            }
            inner = open;
            open = outer;
        }
        spans_statement_handed(&spans, 0u, inner);
        spans_handed(&spans, 0u, 0ull, read.count - 1u, size);
    }
    const int refused = (languages != NULL) && read.refused;
    cli_symbols_leaves_given(&read);
    cli_symbols_languages_given(languages);
    if (refused)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the text's leaves");
    }
    return !refused;
}
