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

typedef struct
{
    const CliLeafRead *read;
    unsigned int leaf;
    const char *text;
    unsigned long long size;
    CliSymbolPosition place;
    const CliSpans *spans_read;
    unsigned long long byte;
    CliSpans *spans;
    unsigned int first;
    unsigned long long from;
    unsigned int last;
    unsigned long long to;
    unsigned int end;
    unsigned int open;
    unsigned int start;
    unsigned int close;
} SymbolsSpansCtx;
// whether the leaf `leaf` of `read` is a blank, a comment or a mark with no bytes, a statement's end or a block's
// close the leaves' reading put
EMBED_INLINE int spans_blank(const SymbolsSpansCtx *args)
{
    return ((args->read->leaf[args->leaf].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u) ||
           (args->read->leaf[args->leaf].from == args->read->leaf[args->leaf].to);
}

// the byte of `size` bytes of `text` the place `place` falls at, the text's end where it is past it
EMBED_INLINE unsigned long long spans_byte_get(const SymbolsSpansCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long start = 0ULL;
    for (unsigned int line = 0u; (line < args->place.line) && (start < size); start += 1ULL)
    {
        line += (text[start] == '\n') ? 1u : 0u;
    }
    unsigned long long end = start;
    end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = end, .size = size,
                     .set = "\n");
    return start + EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = text + start, .size = end - start,
                              .col = args->place.col);
}

// the place of the byte `byte` of the text, the text's first byte or one on the line of the leaf `leaf` or past it by
// the line ends between
EMBED_INLINE CliSymbolPosition spans_position(const SymbolsSpansCtx *args)
{
    if (args->byte == 0ULL)
    {
        return (CliSymbolPosition){0u, 0u};
    }
    const char *const text = args->spans_read->read->text;
    unsigned int line = args->spans_read->read->leaf[args->leaf].line;
    unsigned long long start = args->spans_read->read->leaf[args->leaf].from;
    start =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text, .at = start, .set = "\n");
    for (unsigned long long at = args->spans_read->read->leaf[args->leaf].from; at < args->byte; at += 1ULL)
    {
        if (text[at] == '\n')
        {
            line += 1u;
            start = at + 1ULL;
        }
    }
    // a line's units are below 2^32
    return (CliSymbolPosition){line, (unsigned int)EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg,
                                                              .text = text + start, .size = args->byte - start)};
}

// the bytes `from` to `to` handed as a span, the leaves `first` and `last` on their lines, where they hold the last
// span handed and are more than it
EMBED_INLINE void spans_dispatch(const SymbolsSpansCtx *args)
{
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    if ((from > args->spans->from) || (to < args->spans->to) ||
        ((from == args->spans->from) && (to == args->spans->to)))
    {
        return;
    }
    EMBED_CALL(
        args->spans->each, CliSpannedCfg, .context = args->spans->context,
        .from =
            EMBED_CALL(spans_position, SymbolsSpansCtx, .spans_read = args->spans, .leaf = args->first, .byte = from),
        .to = EMBED_CALL(spans_position, SymbolsSpansCtx, .spans_read = args->spans, .leaf = args->last, .byte = to));
    args->spans->from = from;
    args->spans->to = to;
}

// the leaves `first` to before `end` handed as a span, the blanks and comments at either end left out
EMBED_INLINE void spans_leaves_dispatch(const SymbolsSpansCtx *args)
{
    unsigned int first = args->first;
    unsigned int end = args->end;
    const CliLeafRead *const read = args->spans->read;
    while ((first < end) && EMBED_CALL(spans_blank, SymbolsSpansCtx, .read = read, .leaf = first))
    {
        first += 1u;
    }
    while ((end > first) && EMBED_CALL(spans_blank, SymbolsSpansCtx, .read = read, .leaf = end - 1u))
    {
        end -= 1u;
    }
    if (end > first)
    {
        EMBED_CALL(spans_dispatch, SymbolsSpansCtx, .spans = args->spans, .first = first,
                   .from = read->leaf[first].from, .last = end - 1u, .to = read->leaf[end - 1u].to);
    }
}

// the bracket that closes the pair the leaf `open` of `read` opens, `read->count` where none does
EMBED_INLINE unsigned int spans_closing(const SymbolsSpansCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int depth = 0u;
    for (unsigned int leaf = args->open + 1u; leaf < read->count; leaf += 1u)
    {
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_OPEN))
        {
            depth += 1u;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                            .roles = CLI_LEAF_CLOSE))
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
EMBED_INLINE unsigned int spans_opening(const SymbolsSpansCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    unsigned int depth = 0u;
    for (unsigned int back = leaf; back > 0u; back -= 1u)
    {
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = back - 1u,
                       .roles = CLI_LEAF_CLOSE))
        {
            depth += 1u;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = back - 1u,
                            .roles = CLI_LEAF_OPEN))
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
EMBED_INLINE unsigned int spans_statement_first(const SymbolsSpansCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    unsigned int depth = 0u;
    unsigned int start = args->first;
    unsigned int line_first = args->first;
    unsigned int directive_line = CLI_SLICE_NOWHERE;
    for (unsigned int one = args->first; one < leaf; one += 1u)
    {
        line_first = ((one > args->first) && (read->leaf[one].line != read->leaf[one - 1u].line)) ? one : line_first;
        const int opens = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                     .leaf = one, .roles = CLI_LEAF_OPEN);
        const int closes = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                      .leaf = one, .roles = CLI_LEAF_CLOSE);
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
                 EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = one,
                            .roles = CLI_LEAF_STATEMENT) ||
                 (closes && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                       .leaf = one, .roles = CLI_LEAF_REGION)))
        {
            start = one + 1u;
        }
    }
    return start;
}

// the leaf past the statement whose first leaf is `start`, the leaf `leaf` among it: past its end at its depth, at the
// opening bracket of the region its head is before, at the bracket that closes the pair it stands in, or past the
// directive's line it is
EMBED_INLINE unsigned int spans_statement_end(const SymbolsSpansCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    int directive = 0;
    for (unsigned int one = args->start; (one < read->count) && (read->leaf[one].line == read->leaf[args->start].line);
         one += 1u)
    {
        directive |= (read->leaf[one].is & CLI_LEAF_DIRECTIVE) != 0u;
    }
    unsigned int depth = 0u;
    for (unsigned int one = args->start; one < read->count; one += 1u)
    {
        if (directive && (read->leaf[one].line != read->leaf[args->start].line))
        {
            return one;
        }
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = one,
                       .roles = CLI_LEAF_OPEN))
        {
            if ((depth == 0u) && (one >= leaf) &&
                EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = one,
                           .roles = CLI_LEAF_REGION))
            {
                return one;
            }
            depth += 1u;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = one,
                            .roles = CLI_LEAF_CLOSE))
        {
            if (depth == 0u)
            {
                return one;
            }
            depth -= 1u;
        }
        else if ((depth == 0u) && (one >= leaf) &&
                 EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = one,
                            .roles = CLI_LEAF_STATEMENT))
        {
            return one + 1u;
        }
    }
    return read->count;
}

// the pair the leaf `open` opens and `close` closes handed, inside its brackets and with them, the text's end where no
// bracket closes it; a pair a mark of no bytes closes ending, both ways, where the last leaf before the mark that is
// no blank ends
EMBED_INLINE void spans_pair_dispatch(const SymbolsSpansCtx *args)
{
    const CliLeafRead *const read = args->spans->read;
    unsigned int last = (args->close < read->count) ? args->close : (read->count - 1u);
    unsigned long long inside = (args->close < read->count) ? read->leaf[args->close].from : args->spans->size;
    unsigned long long whole = (args->close < read->count) ? read->leaf[args->close].to : args->spans->size;
    if ((args->close < read->count) && (read->leaf[args->close].from == read->leaf[args->close].to))
    {
        last = args->close;
        while ((last > (args->open + 1u)) && EMBED_CALL(spans_blank, SymbolsSpansCtx, .read = read, .leaf = last - 1u))
        {
            last -= 1u;
        }
        last = (last > (args->open + 1u)) ? (last - 1u) : args->open;
        inside = read->leaf[last].to;
        whole = inside;
    }
    EMBED_CALL(spans_dispatch, SymbolsSpansCtx, .spans = args->spans, .first = args->open,
               .from = read->leaf[args->open].to, .last = last, .to = inside);
    EMBED_CALL(spans_dispatch, SymbolsSpansCtx, .spans = args->spans, .first = args->open,
               .from = read->leaf[args->open].from, .last = last, .to = whole);
}

// the statement the leaf `leaf` stands in among those from `first` on handed, and where it is the head of a region,
// the region with it
EMBED_INLINE void spans_statement_dispatch(const SymbolsSpansCtx *args)
{
    unsigned int leaf = args->leaf;
    const CliLeafRead *const read = args->spans->read;
    const unsigned int start =
        EMBED_CALL(spans_statement_first, SymbolsSpansCtx, .read = read, .first = args->first, .leaf = leaf);
    const unsigned int end =
        EMBED_CALL(spans_statement_end, SymbolsSpansCtx, .read = read, .start = start, .leaf = leaf);
    EMBED_CALL(spans_leaves_dispatch, SymbolsSpansCtx, .spans = args->spans, .first = start, .end = end);
    if ((end < read->count) && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                          .leaf = end, .roles = CLI_LEAF_REGION | CLI_LEAF_OPEN))
    {
        EMBED_CALL(spans_leaves_dispatch, SymbolsSpansCtx, .spans = args->spans, .first = start,
                   .end = EMBED_CALL(spans_closing, SymbolsSpansCtx, .read = read, .open = end) + 1u);
    }
}

// the leaf a place at the byte `byte` stands in: the one that holds it, else the one that ends at it where that one is
// a name and the one that holds it is not, or is a blank; the last that starts at or before it
EMBED_INLINE unsigned int spans_leaf_at(const SymbolsSpansCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int low = 0u;
    unsigned int high = read->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (read->leaf[middle].from <= args->byte)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    const unsigned int at = (low > 0u) ? (low - 1u) : 0u;
    const int before = (at > 0u) && (read->leaf[at].from == args->byte) && (read->leaf[at - 1u].to == args->byte) &&
                       !EMBED_CALL(spans_blank, SymbolsSpansCtx, .read = read, .leaf = at - 1u);
    const int named_before = before &&
                             EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                        .leaf = at - 1u, .roles = CLI_LEAF_NAME) &&
                             !EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                         .leaf = at, .roles = CLI_LEAF_NAME);
    return (before && (EMBED_CALL(spans_blank, SymbolsSpansCtx, .read = read, .leaf = at) || named_before)) ? (at - 1u)
                                                                                                            : at;
}

int cli_symbols_spans(const CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionCfg *args)
{
    CliSymbolsLanguages *const languages = cli_symbols_languages_for_call();
    CliLeafRead read = {args->text, NULL, 0u, 0u, 0u, NULL, 0, NULL, NULL, 0};
    const int leaved = (languages != NULL) && EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx,
                                                         .languages = languages, .path = args->path, .state = "root",
                                                         .text = args->text, .size = args->size, .read = &read);
    if (leaved && (read.count > 0u))
    {
        CliSpans spans = {&read,
                          args->size,
                          EMBED_CALL(spans_byte_get, SymbolsSpansCtx, .text = args->text, .size = args->size,
                                     .place = args->from_symbol_position),
                          EMBED_CALL(spans_byte_get, SymbolsSpansCtx, .text = args->text, .size = args->size,
                                     .place = args->to_symbol_position),
                          args->each_spanned,
                          args->context};
        const unsigned int word = EMBED_CALL(spans_leaf_at, SymbolsSpansCtx, .read = &read, .byte = spans.from);
        if (!EMBED_CALL(spans_blank, SymbolsSpansCtx, .read = &read, .leaf = word))
        {
            EMBED_CALL(spans_dispatch, SymbolsSpansCtx, .spans = &spans, .first = word, .from = read.leaf[word].from,
                       .last = word, .to = read.leaf[word].to);
        }
        // each pair around the place from the innermost out, the statement of a region's before its brackets
        unsigned int inner = word;
        unsigned int open = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = &read,
                                       .leaf = word, .roles = CLI_LEAF_OPEN)
                                ? word
                                : EMBED_CALL(spans_opening, SymbolsSpansCtx, .read = &read, .leaf = word);
        while (open != CLI_JSON_NONE)
        {
            const unsigned int outer = EMBED_CALL(spans_opening, SymbolsSpansCtx, .read = &read, .leaf = open);
            const int region = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = &read,
                                          .leaf = open, .roles = CLI_LEAF_REGION);
            if (region && (inner != open))
            {
                EMBED_CALL(spans_statement_dispatch, SymbolsSpansCtx, .spans = &spans, .first = open + 1u,
                           .leaf = inner);
            }
            const unsigned int close = EMBED_CALL(spans_closing, SymbolsSpansCtx, .read = &read, .open = open);
            EMBED_CALL(spans_pair_dispatch, SymbolsSpansCtx, .spans = &spans, .open = open, .close = close);
            if (region)
            {
                const unsigned int head =
                    EMBED_CALL(spans_statement_first, SymbolsSpansCtx, .read = &read,
                               .first = (outer != CLI_JSON_NONE) ? (outer + 1u) : 0u, .leaf = open);
                EMBED_CALL(spans_leaves_dispatch, SymbolsSpansCtx, .spans = &spans, .first = head,
                           .end = (close < read.count) ? (close + 1u) : read.count);
            }
            inner = open;
            open = outer;
        }
        EMBED_CALL(spans_statement_dispatch, SymbolsSpansCtx, .spans = &spans, .first = 0u, .leaf = inner);
        EMBED_CALL(spans_dispatch, SymbolsSpansCtx, .spans = &spans, .first = 0u, .from = 0ULL, .last = read.count - 1u,
                   .to = args->size);
    }
    const int refused = (languages != NULL) && read.refused;
    EMBED_CALL(cli_symbols_text_internal.leaves_free, CliSymbolsTextCtx, .read = &read);
    EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = languages);
    if (refused)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the text's leaves");
    }
    return !refused;
}

// whether the leaf `leaf` of `read` is a bracket of a pair: a leaf of one byte or more that opens or closes
EMBED_INLINE int spans_bracket(const SymbolsSpansCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    return (leaf < read->count) && (read->leaf[leaf].to > read->leaf[leaf].from) &&
           (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_OPEN) ||
            EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_CLOSE));
}

// the bracket of `read` that ends at the byte `byte`, else the one that starts at it, `read->count` where neither does
EMBED_INLINE unsigned int spans_bracket_at(const SymbolsSpansCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->read->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (args->read->leaf[middle].from < args->byte)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    if ((low > 0u) && (args->read->leaf[low - 1u].to == args->byte) &&
        EMBED_CALL(spans_bracket, SymbolsSpansCtx, .read = args->read, .leaf = low - 1u))
    {
        return low - 1u;
    }
    return ((low < args->read->count) && (args->read->leaf[low].from == args->byte) &&
            EMBED_CALL(spans_bracket, SymbolsSpansCtx, .read = args->read, .leaf = low))
               ? low
               : args->read->count;
}

int cli_symbols_pair(const CliSymbolsPathTextSizeStateCfg *args)
{
    CliSymbolsLanguages *const languages = cli_symbols_languages_for_call();
    CliLeafRead read = {args->text, NULL, 0u, 0u, 0u, NULL, 0, NULL, NULL, 1};
    const int leaved =
        (languages != NULL) &&
        EMBED_CALL(cli_symbols_path_internal.leaves_temporary, CliSymbolsPathCtx, .languages = languages,
                   .path = args->path, .state = args->state, .text = args->text, .size = args->size, .read = &read);
    const unsigned int bracket =
        leaved ? EMBED_CALL(spans_bracket_at, SymbolsSpansCtx, .read = &read,
                            .byte = EMBED_CALL(spans_byte_get, SymbolsSpansCtx, .text = args->text, .size = args->size,
                                               .place = args->at_symbol_position))
               : read.count;
    const int opens = (bracket < read.count) && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx,
                                                           .read_read = &read, .leaf = bracket, .roles = CLI_LEAF_OPEN);
    const unsigned int peer = (bracket >= read.count) ? read.count
                              : opens ? EMBED_CALL(spans_closing, SymbolsSpansCtx, .read = &read, .open = bracket)
                                      : EMBED_CALL(spans_opening, SymbolsSpansCtx, .read = &read, .leaf = bracket);
    const int paired = EMBED_CALL(spans_bracket, SymbolsSpansCtx, .read = &read, .leaf = peer);
    if (paired)
    {
        const CliSpans spans = {&read, args->size, 0ULL, 0ULL, NULL, NULL};
        args->pair[0] = EMBED_CALL(spans_position, SymbolsSpansCtx, .spans_read = &spans, .leaf = bracket,
                                   .byte = read.leaf[bracket].from);
        args->pair[1] = EMBED_CALL(spans_position, SymbolsSpansCtx, .spans_read = &spans, .leaf = peer,
                                   .byte = read.leaf[peer].from);
    }
    EMBED_CALL(cli_symbols_text_internal.leaves_free, CliSymbolsTextCtx, .read = &read);
    EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = languages);
    return paired;
}
