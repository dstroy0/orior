// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_slice.c: the slice of the tree clangd is given for lines of a branch: a walk of the branch to its first line,
// the bytes and leaves of each branch it reaches in the walk's cells until the slice is written, then the names the
// lines hold followed to what the walk met that defines them, and the names those hold, each written as the lines it
// stands on with the branch and line it came from. A name, a bracket, a region's bracket and a statement's end are
// known by the roles its grammar's tokens name, and a bracket left open is closed by the text of the one that closes it
// in its branch
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "symbols_internal.h"

// One name a slice follows: its text, among a branch's bytes in the walk's cells, and its length
typedef struct
{
    const char *text;
    unsigned long long length;
} CliSliceName;
// One run of a branch's leaves written as the lines they stand on: the branch, a place among those the walk read
// whole, and its first leaf and the leaf after its last
typedef struct
{
    unsigned int branch;
    unsigned int first;
    unsigned int end;
} CliSlicePart;
// A slice being cut: the walk, the branch its lines stand in among those the walk read whole, the names followed and
// the lines' own parts, in one cell, and for each thing the walk met whether the slice writes it, in a cell of its own
typedef struct
{
    CliEffect *effect;
    unsigned int start;
    CliSliceName *name;
    unsigned int names;
    unsigned int name_capacity;
    CliSlicePart *part;
    unsigned int parts;
    unsigned int part_capacity;
    unsigned char *written;
} CliSlice;
// the most lines an opener's head runs across to the bracket that opens its region, the most openers read, and the
// most brackets the slice closes after its lines
#define CLI_SLICE_HEAD_LINES 64u
#define CLI_SLICE_OPENERS 64u
#define CLI_SLICE_OPEN 256u

typedef struct
{
    const CliSlice *slice_read;
    unsigned int branch;
    const CliLeafRead *read;
    unsigned int line;
    unsigned int leaf;
    CliSlice *slice;
    const char *text;
    unsigned long long length;
    unsigned int first;
    unsigned int end;
    int function;
    int *defined;
    unsigned int *first_pointer;
    unsigned int *last;
    unsigned int seen;
    CliSliceName name;
    unsigned int from;
    unsigned int view;
    const unsigned int *head_first;
    const unsigned int *head_end;
    unsigned int heads;
    CliSliceLine each;
    void *context;
    unsigned int open;
    CliEffect *effect;
    const char *path;
    unsigned int last_value;
    const unsigned int *opener;
    unsigned int openers;
} SymbolsSliceCtx;
// the leaves of the branch the walk read whole at `branch`
EMBED_INLINE const CliLeafRead *cli_slice_read(const SymbolsSliceCtx *args)
{
    return &args->slice_read->effect->branch_read[args->branch].read;
}

// the first leaf of `read` on line `line` or past it, `read->count` where none is
EMBED_INLINE unsigned int cli_slice_line_first(const SymbolsSliceCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->read->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (args->read->leaf[middle].line < args->line)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// whether the leaf `leaf` is a bracket its grammar names one that opens, 1, one that closes, -1, or neither, 0
EMBED_INLINE int cli_slice_bracket(const SymbolsSliceCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    return EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                      .roles = CLI_LEAF_OPEN)
               ? 1
               : (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                             .roles = CLI_LEAF_CLOSE)
                      ? -1
                      : 0);
}

// the name `text`, `length` bytes, put among those the slice follows where it is new
EMBED_INLINE void cli_slice_name_append(const SymbolsSliceCtx *args)
{
    for (unsigned int at = 0u; at < args->slice->names; at += 1u)
    {
        if ((args->slice->name[at].length == args->length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->slice->name[at].text,
                       .other_pointer = args->text, .size = args->length))
        {
            return;
        }
    }
    if (args->slice->names >= args->slice->name_capacity)
    {
        args->slice->effect->refused = 1;
        return;
    }
    args->slice->name[args->slice->names] = (CliSliceName){args->text, args->length};
    args->slice->names += 1u;
}

// the names the leaves from `first` to before `end` of the branch at `branch` hold put among those followed: each leaf
// its grammar names a name or a type, but one that declares
EMBED_INLINE void cli_slice_names_get(const SymbolsSliceCtx *args)
{
    const CliLeafRead *const read =
        EMBED_CALL(cli_slice_read, SymbolsSliceCtx, .slice_read = args->slice, .branch = args->branch);
    const unsigned long long named = CLI_LEAF_NAME | CLI_LEAF_TYPE;
    const unsigned long long passed = CLI_LEAF_DECLARES | CLI_LEAF_STRING | CLI_LEAF_COMMENT;
    for (unsigned int leaf = args->first; leaf < args->end; leaf += 1u)
    {
        const unsigned long long is = read->leaf[leaf].is;
        if (((is & named) != 0u) && ((is & passed) == 0u))
        {
            unsigned long long length = 0ull;
            const char *const text = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                                .read_read = read, .leaf = leaf, .length_pointer = &length);
            EMBED_CALL(cli_slice_name_append, SymbolsSliceCtx, .slice = args->slice, .text = text, .length = length);
        }
    }
}

// the leaves from `first` to before `end` of the branch at `branch` put among the lines' own parts, in the order of
// their leaves
EMBED_INLINE void cli_slice_part_append(const SymbolsSliceCtx *args)
{
    if (args->slice->parts >= args->slice->part_capacity)
    {
        args->slice->effect->refused = 1;
        return;
    }
    unsigned int at = args->slice->parts;
    while ((at > 0u) && (args->slice->part[at - 1u].first > args->first))
    {
        args->slice->part[at] = args->slice->part[at - 1u];
        at -= 1u;
    }
    args->slice->part[at] = (CliSlicePart){args->branch, args->first, args->end};
    args->slice->parts += 1u;
}

// the leaf after the last of the statement whose first leaf is `first`: its end, as its grammar names it, at the depth
// it starts at, or for a function, `function` 1, whose head a body follows, the bracket that closes the body's region,
// `*defined` 1 then; a directive's line at that depth ends a statement that runs into it
EMBED_INLINE unsigned int cli_slice_statement(const SymbolsSliceCtx *args)
{
    const CliLeafRead *read = args->read;
    int *defined = args->defined;
    *defined = 0;
    unsigned int depth = 0u;
    for (unsigned int leaf = args->first; leaf < read->count; leaf += 1u)
    {
        if ((depth == 0u) && ((read->leaf[leaf].is & CLI_LEAF_DIRECTIVE) != 0u) &&
            (read->leaf[leaf].line != read->leaf[args->first].line))
        {
            return EMBED_CALL(cli_slice_line_first, SymbolsSliceCtx, .read = read, .line = read->leaf[leaf].line);
        }
        *defined |= args->function && (depth == 0u) &&
                    EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                               .roles = CLI_LEAF_REGION | CLI_LEAF_OPEN);
        const int bracket = EMBED_CALL(cli_slice_bracket, SymbolsSliceCtx, .read = read, .leaf = leaf);
        depth = (bracket > 0) ? (depth + 1u) : (((bracket < 0) && (depth > 0u)) ? (depth - 1u) : depth);
        const int closed = *defined && (bracket < 0);
        if ((depth == 0u) &&
            (closed || (!*defined && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx,
                                                .read_read = read, .leaf = leaf, .roles = CLI_LEAF_STATEMENT))))
        {
            return leaf + 1u;
        }
    }
    return read->count;
}

// whether the line `line` of `read` is a directive's
EMBED_INLINE int cli_slice_directive_line(const SymbolsSliceCtx *args)
{
    for (unsigned int leaf = cli_slice_line_first(args);
         (leaf < args->read->count) && (args->read->leaf[leaf].line == args->line); leaf += 1u)
    {
        if ((args->read->leaf[leaf].is & CLI_LEAF_DIRECTIVE) != 0u)
        {
            return 1;
        }
    }
    return 0;
}

// the last leaf before the leaf `leaf` none of whose blank or comment it is, `read->count` where none is
EMBED_INLINE unsigned int cli_slice_last_before(const SymbolsSliceCtx *args)
{
    unsigned int leaf = args->leaf;
    while ((leaf > 0u) && ((args->read->leaf[leaf - 1u].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
    {
        leaf -= 1u;
    }
    return (leaf > 0u) ? (leaf - 1u) : args->read->count;
}

// whether a statement or a region's head ends at the leaf `leaf`: a statement's end, a region's bracket, or the last
// leaf of a directive's line
EMBED_INLINE int cli_slice_ends(const SymbolsSliceCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    return EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                      .roles = CLI_LEAF_STATEMENT) ||
           EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                      .roles = CLI_LEAF_REGION) ||
           EMBED_CALL(cli_slice_directive_line, SymbolsSliceCtx, .read = read, .line = read->leaf[leaf].line);
}

// the lines `*first` to `*last` widened to whole statements, CLI_SLICE_HEAD_LINES lines at most each way: the first
// back to the line after one that ends a statement, the last on to one that ends one
EMBED_INLINE void cli_slice_widened(const SymbolsSliceCtx *args)
{
    unsigned int *first = args->first_pointer;
    unsigned int *last = args->last;
    for (unsigned int lines = 0u; lines < CLI_SLICE_HEAD_LINES; lines += 1u)
    {
        const unsigned int before =
            EMBED_CALL(cli_slice_last_before, SymbolsSliceCtx, .read = args->read,
                       .leaf = EMBED_CALL(cli_slice_line_first, SymbolsSliceCtx, .read = args->read, .line = *first));
        if ((before == args->read->count) ||
            EMBED_CALL(cli_slice_ends, SymbolsSliceCtx, .read = args->read, .leaf = before))
        {
            break;
        }
        *first = args->read->leaf[before].line;
    }
    for (unsigned int lines = 0u; lines < CLI_SLICE_HEAD_LINES; lines += 1u)
    {
        const unsigned int after =
            EMBED_CALL(cli_slice_line_first, SymbolsSliceCtx, .read = args->read, .line = *last + 1u);
        const unsigned int ending =
            EMBED_CALL(cli_slice_last_before, SymbolsSliceCtx, .read = args->read, .leaf = after);
        if ((after == args->read->count) || (ending == args->read->count) ||
            EMBED_CALL(cli_slice_ends, SymbolsSliceCtx, .read = args->read, .leaf = ending))
        {
            break;
        }
        *last = args->read->leaf[after].line;
    }
}

// the leaf after the bracket that opens the region the line `line` opens: the first region's opening at the depth the
// line starts at within CLI_SLICE_HEAD_LINES lines, else the line's end
EMBED_INLINE unsigned int cli_slice_head(const SymbolsSliceCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int depth = 0u;
    for (unsigned int leaf = EMBED_CALL(cli_slice_line_first, SymbolsSliceCtx, .read = read, .line = args->line);
         (leaf < read->count) && (read->leaf[leaf].line < (args->line + CLI_SLICE_HEAD_LINES)); leaf += 1u)
    {
        if ((depth == 0u) && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                        .leaf = leaf, .roles = CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            return leaf + 1u;
        }
        const int bracket = EMBED_CALL(cli_slice_bracket, SymbolsSliceCtx, .read = read, .leaf = leaf);
        depth = (bracket > 0) ? (depth + 1u) : (((bracket < 0) && (depth > 0u)) ? (depth - 1u) : depth);
    }
    return EMBED_CALL(cli_slice_line_first, SymbolsSliceCtx, .read = read, .line = args->line + 1u);
}

// the thing the walk met at `seen`, its leaves running to before its `end`, written with the slice, and the names its
// leaves hold followed
EMBED_INLINE void cli_slice_pick(const SymbolsSliceCtx *args)
{
    const CliEffectSeen *const one = &args->slice->effect->seen[args->seen];
    if ((args->slice->written[args->seen] != 0u) || (one->branch == CLI_JSON_NONE))
    {
        return;
    }
    args->slice->written[args->seen] = 1u;
    EMBED_CALL(cli_slice_names_get, SymbolsSliceCtx, .slice = args->slice, .branch = one->branch, .first = one->first,
               .end = one->end);
}

// what the walk met that defines the name `name`: the define in effect where a macro is in effect under it, else the
// last declaration of it the walk met in an active region that no body follows, else the last whose body follows it,
// with the body
EMBED_INLINE void cli_slice_follow(const SymbolsSliceCtx *args)
{
    CliSliceName name = args->name;
    CliEffect *const effect = args->slice->effect;
    const unsigned int macro = EMBED_CALL(cli_symbols_name_internal.effect_macro, CliSymbolsNameCtx,
                                          .effect_read = effect, .name = name.text, .length = name.length);
    if (macro != CLI_JSON_NONE)
    {
        if (effect->macro[macro].seen != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_slice_pick, SymbolsSliceCtx, .slice = args->slice, .seen = effect->macro[macro].seen);
        }
        return;
    }
    unsigned int chosen = CLI_JSON_NONE;
    for (unsigned int at = effect->seens; at > 0u; at -= 1u)
    {
        CliEffectSeen *const one = &effect->seen[at - 1u];
        if ((one->is != CLI_SEEN_DECLARED) || (one->branch == CLI_JSON_NONE) || (one->length != name.length) ||
            !EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg,
                        .one = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                          .at_value = one->name),
                        .other_pointer = name.text, .size = name.length))
        {
            continue;
        }
        const int function =
            cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx,
                                                           .effect_read = effect, .at_value = one->kind),
                                         .other = "function",
                                         .cap = CLI_CELLS_POOL});
        int defined = 0;
        one->end = EMBED_CALL(
            cli_slice_statement, SymbolsSliceCtx,
            .read = EMBED_CALL(cli_slice_read, SymbolsSliceCtx, .slice_read = args->slice, .branch = one->branch),
            .first = one->first, .function = function, .defined = &defined);
        chosen = ((chosen == CLI_JSON_NONE) || !defined) ? (at - 1u) : chosen;
        if (!defined)
        {
            break;
        }
    }
    if (chosen != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_slice_pick, SymbolsSliceCtx, .slice = args->slice, .seen = chosen);
    }
}

// the locals declared from the leaf `from` to before the leaf `view` the slice's names name, each a statement whose
// line holds a leaf its grammar names a local, at the depth of the regions the openers' heads `head_first` to
// `head_end`, `heads` of them, open above it, put among the lines' own parts with the names they hold followed
EMBED_INLINE void cli_slice_locals(const SymbolsSliceCtx *args)
{
    const CliLeafRead *const read =
        EMBED_CALL(cli_slice_read, SymbolsSliceCtx, .slice_read = args->slice, .branch = args->slice->start);
    unsigned int depth = 0u;
    unsigned int leaf = args->from;
    while (leaf < args->view)
    {
        // an opener's head is no local's: its leaves passed over, the bracket that opens its region counted
        unsigned int opened = 0u;
        unsigned int head = args->heads;
        for (unsigned int at = 0u; at < args->heads; at += 1u)
        {
            opened += (args->head_end[at] <= leaf) ? 1u : 0u;
            head = (args->head_first[at] == leaf) ? at : head;
        }
        if (head != args->heads)
        {
            depth += 1u;
            leaf = args->head_end[head];
            continue;
        }
        const unsigned int line = read->leaf[leaf].line;
        const unsigned int line_end =
            EMBED_CALL(cli_slice_line_first, SymbolsSliceCtx, .read = read, .line = line + 1u);
        unsigned int named = args->slice->names;
        for (unsigned int each = leaf; (depth == opened) && (each < line_end) && (each < args->view); each += 1u)
        {
            if ((read->leaf[each].is & CLI_LEAF_LOCAL) == 0u)
            {
                continue;
            }
            unsigned long long length = 0ull;
            const char *const text = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                                .read_read = read, .leaf = each, .length_pointer = &length);
            for (unsigned int at = 0u; at < args->slice->names; at += 1u)
            {
                named = ((args->slice->name[at].length == length) &&
                         EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->slice->name[at].text,
                                    .other_pointer = text, .size = length))
                            ? at
                            : named;
            }
        }
        if (named != args->slice->names)
        {
            int defined = 0;
            const unsigned int end = EMBED_CALL(cli_slice_statement, SymbolsSliceCtx, .read = read, .first = leaf,
                                                .function = 0, .defined = &defined);
            EMBED_CALL(cli_slice_part_append, SymbolsSliceCtx, .slice = args->slice, .branch = args->slice->start,
                       .first = leaf, .end = end);
            EMBED_CALL(cli_slice_names_get, SymbolsSliceCtx, .slice = args->slice, .branch = args->slice->start,
                       .first = leaf, .end = end);
            leaf = (end > leaf) ? end : line_end;
            continue;
        }
        for (; (leaf < line_end) && (leaf < args->view); leaf += 1u)
        {
            depth += EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                                .roles = CLI_LEAF_REGION | CLI_LEAF_OPEN)
                         ? 1u
                         : 0u;
            depth -= (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                                 .roles = CLI_LEAF_REGION | CLI_LEAF_CLOSE) &&
                      (depth > 0u))
                         ? 1u
                         : 0u;
        }
    }
}

// the lines the leaves from `first` to before `end` of the branch at `branch` stand on handed to `each`, each with the
// branch and line it came from, the blanks and comments at its end left off
EMBED_INLINE void cli_slice_lines_dispatch(const SymbolsSliceCtx *args)
{
    unsigned int end = args->end;
    const CliEffectBranchRead *const whole = &args->slice_read->effect->branch_read[args->branch];
    const CliLeafRead *const read = &whole->read;
    while ((end > args->first) && ((read->leaf[end - 1u].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
    {
        end -= 1u;
    }
    if (end <= args->first)
    {
        return;
    }
    const char *const path = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx,
                                        .effect_read = args->slice_read->effect, .at_value = whole->path);
    const unsigned int last = read->leaf[end - 1u].line;
    unsigned int leaf = args->first;
    for (unsigned int line = read->leaf[args->first].line; line <= last; line += 1u)
    {
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        int any = 0;
        while ((leaf < end) && (read->leaf[leaf].line == line))
        {
            from = any ? from : read->leaf[leaf].from;
            to = read->leaf[leaf].to;
            any = 1;
            leaf += 1u;
        }
        EMBED_CALL(args->each, CliSliceLineCfg, .context = args->context, .path = path, .line = line,
                   .text = (const char *)whole->bytes + from, .length = to - from);
    }
}

// the leaf of `read` that closes the bracket that opens at the leaf `open`, at its depth, `read->count` where none does
EMBED_INLINE unsigned int cli_slice_closer(const SymbolsSliceCtx *args)
{
    unsigned int depth = 0u;
    for (unsigned int leaf = args->open + 1u; leaf < args->read->count; leaf += 1u)
    {
        const int bracket = EMBED_CALL(cli_slice_bracket, SymbolsSliceCtx, .read = args->read, .leaf = leaf);
        if ((bracket < 0) && (depth == 0u))
        {
            return leaf;
        }
        depth = (bracket > 0) ? (depth + 1u) : (((bracket < 0) && (depth > 0u)) ? (depth - 1u) : depth);
    }
    return args->read->count;
}

// the slice handed to `each` a line at a time: each include of no file of the tree and each thing it writes the walk
// met, in the order the walk met them, then the lines' own parts, then a line closing each bracket they leave open,
// innermost first, from no branch, each the text of the bracket that closes it in its branch
EMBED_INLINE void cli_slice_dispatch(const SymbolsSliceCtx *args)
{
    const CliEffect *const effect = args->slice_read->effect;
    for (unsigned int at = 0u; at < effect->seens; at += 1u)
    {
        const CliEffectSeen *const one = &effect->seen[at];
        const int writes = (one->is == CLI_SEEN_INCLUDE) || (args->slice_read->written[at] != 0u);
        if (writes && (one->branch != CLI_JSON_NONE))
        {
            EMBED_CALL(cli_slice_lines_dispatch, SymbolsSliceCtx, .slice_read = args->slice_read, .branch = one->branch,
                       .first = one->first, .end = one->end, .each = args->each, .context = args->context);
        }
    }
    const CliLeafRead *open_read[CLI_SLICE_OPEN];
    unsigned int open_leaf[CLI_SLICE_OPEN];
    unsigned int opens = 0u;
    for (unsigned int at = 0u; at < args->slice_read->parts; at += 1u)
    {
        const CliSlicePart *const part = &args->slice_read->part[at];
        const CliLeafRead *const read =
            EMBED_CALL(cli_slice_read, SymbolsSliceCtx, .slice_read = args->slice_read, .branch = part->branch);
        EMBED_CALL(cli_slice_lines_dispatch, SymbolsSliceCtx, .slice_read = args->slice_read, .branch = part->branch,
                   .first = part->first, .end = part->end, .each = args->each, .context = args->context);
        for (unsigned int leaf = part->first; leaf < part->end; leaf += 1u)
        {
            const int bracket = EMBED_CALL(cli_slice_bracket, SymbolsSliceCtx, .read = read, .leaf = leaf);
            if ((bracket > 0) && (opens < CLI_SLICE_OPEN))
            {
                open_read[opens] = read;
                open_leaf[opens] = leaf;
                opens += 1u;
            }
            opens -= ((bracket < 0) && (opens > 0u)) ? 1u : 0u;
        }
    }
    for (; opens > 0u; opens -= 1u)
    {
        const CliLeafRead *const read = open_read[opens - 1u];
        const unsigned int closer =
            EMBED_CALL(cli_slice_closer, SymbolsSliceCtx, .read = read, .open = open_leaf[opens - 1u]);
        unsigned long long length = 0ull;
        const char *const text = (closer < read->count)
                                     ? EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                                  .read_read = read, .leaf = closer, .length_pointer = &length)
                                     : "";
        if (length > 0ull)
        {
            EMBED_CALL(args->each, CliSliceLineCfg, .context = args->context, .path = NULL, .line = CLI_SLICE_NOWHERE,
                       .text = text, .length = length);
        }
    }
}

// The slice written as JSON: where it is written, and whether a line is written yet
typedef struct
{
    VerbaLinea *out;
    int any;
} CliSliceJson;

// the first leaf of the line `line` of `read`, or where the line begins with brackets that close regions the first past
// them and the blanks about them: the head of the region a line that closes one region and opens another opens
EMBED_INLINE unsigned int cli_slice_head_first(const SymbolsSliceCtx *args)
{
    const CliLeafRead *read = args->read;
    const unsigned int first = EMBED_CALL(cli_slice_line_first, SymbolsSliceCtx, .read = read, .line = args->line);
    unsigned int leaf = first;
    int closed = 0;
    while ((leaf < read->count) && (read->leaf[leaf].line == args->line) &&
           (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_REGION | CLI_LEAF_CLOSE) ||
            ((read->leaf[leaf].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u)))
    {
        closed |= EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                             .roles = CLI_LEAF_REGION | CLI_LEAF_CLOSE);
        leaf += 1u;
    }
    return closed ? leaf : first;
}

// the slice of the lines `first` to `last` of the branch at `path` cut from the walk `effect` as far as it has gone,
// the lines opening the regions `first` stands in `opener`, `openers` of them, handed to `each` a line at a time: 1, or
// 0 with the walk refused where the cells cannot meet it
static int cli_slice_cut(const SymbolsSliceCtx *args)
{
    CliEffect *effect = args->effect;
    const unsigned long long name_bytes = CLI_EFFECT_LIST_MAX * sizeof(CliSliceName);
    char *const lists =
        g_cli_cells->temporary_buf_alloc(name_bytes + (CLI_EFFECT_LIST_MAX * sizeof(CliSlicePart)), &g_cli_code_cells);
    CliSlice slice = {effect, CLI_JSON_NONE, NULL, 0u, 0u, NULL, 0u, 0u, NULL};
    if (lists != NULL)
    {
        slice.name = (CliSliceName *)(void *)lists;
        slice.name_capacity = CLI_EFFECT_LIST_MAX;
        slice.part = (CliSlicePart *)(void *)(lists + name_bytes);
        slice.part_capacity = CLI_EFFECT_LIST_MAX;
    }
    else
    {
        effect->refused = 1;
    }
    // the branch the lines stand in, the last read whole of its path
    for (unsigned int at = 0u; at < effect->branches_read; at += 1u)
    {
        slice.start = cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx,
                                                                     .effect_read = effect,
                                                                     .at_value = effect->branch_read[at].path),
                                                   .other = args->path,
                                                   .cap = CLI_CELLS_POOL})
                          ? at
                          : slice.start;
    }
    slice.written = (effect->seens != 0u)
                        ? g_cli_cells->persistent_buf_alloc((unsigned long long)effect->seens, &g_cli_code_cells)
                        : NULL;
    effect->refused |= (effect->seens != 0u) && (slice.written == NULL);
    if (slice.written != NULL)
    {
        mmgr_zero_buf(slice.written, (size_t)effect->seens);
    }
    unsigned int head_first[CLI_SLICE_OPENERS];
    unsigned int head_end[CLI_SLICE_OPENERS];
    const unsigned int heads = (args->openers < CLI_SLICE_OPENERS) ? args->openers : CLI_SLICE_OPENERS;
    if (!effect->refused && (slice.start != CLI_JSON_NONE))
    {
        const CliLeafRead *const read =
            EMBED_CALL(cli_slice_read, SymbolsSliceCtx, .slice_read = &slice, .branch = slice.start);
        // the openers' heads and the lines themselves, then the locals they name, then what the names follow to
        for (unsigned int at = 0u; at < heads; at += 1u)
        {
            head_first[at] = EMBED_CALL(cli_slice_head_first, SymbolsSliceCtx, .read = read, .line = args->opener[at]);
            head_end[at] = EMBED_CALL(cli_slice_head, SymbolsSliceCtx, .read = read, .line = args->opener[at]);
            EMBED_CALL(cli_slice_part_append, SymbolsSliceCtx, .slice = &slice, .branch = slice.start,
                       .first = head_first[at], .end = head_end[at]);
            EMBED_CALL(cli_slice_names_get, SymbolsSliceCtx, .slice = &slice, .branch = slice.start,
                       .first = head_first[at], .end = head_end[at]);
        }
        unsigned int view_first = args->first;
        unsigned int view_last = args->last_value;
        EMBED_CALL(cli_slice_widened, SymbolsSliceCtx, .read = read, .first_pointer = &view_first, .last = &view_last);
        const unsigned int view = EMBED_CALL(cli_slice_line_first, SymbolsSliceCtx, .read = read, .line = view_first);
        const unsigned int view_end =
            EMBED_CALL(cli_slice_line_first, SymbolsSliceCtx, .read = read, .line = view_last + 1u);
        EMBED_CALL(cli_slice_part_append, SymbolsSliceCtx, .slice = &slice, .branch = slice.start, .first = view,
                   .end = view_end);
        EMBED_CALL(cli_slice_names_get, SymbolsSliceCtx, .slice = &slice, .branch = slice.start, .first = view,
                   .end = view_end);
        if (heads != 0u)
        {
            EMBED_CALL(cli_slice_locals, SymbolsSliceCtx, .slice = &slice, .from = head_end[0], .view = view,
                       .head_first = head_first + 1, .head_end = head_end + 1, .heads = heads - 1u);
        }
        for (unsigned int at = 0u; !effect->refused && (at < slice.names); at += 1u)
        {
            EMBED_CALL(cli_slice_follow, SymbolsSliceCtx, .slice = &slice, .name = slice.name[at]);
        }
    }
    if (!effect->refused && (slice.start != CLI_JSON_NONE))
    {
        EMBED_CALL(cli_slice_dispatch, SymbolsSliceCtx, .slice_read = &slice, .each = args->each,
                   .context = args->context);
    }
    g_cli_cells->persistent_buf_release(lists, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(slice.written, &g_cli_code_cells);
    return !effect->refused;
}

int cli_symbols_slice_each(const CliSymbolsPathCfg *args)
{
    CliEffect *const effect = EMBED_CALL(cli_symbols_path_name_internal.effect_init, CliSymbolsPathNameCtx,
                                         .path = args->path, .name = "", .slicing = 1, .err_msg = args->err_msg);
    if (effect == NULL)
    {
        return 0;
    }
    effect->start_text = args->text;
    effect->start_size = args->size;
    EMBED_CALL(cli_symbols_path_effect_internal.effect_alloc, CliSymbolsPathEffectCtx, .effect = effect,
               .path = args->path, .line_u32 = args->first);
    const int cut = !effect->refused &&
                    EMBED_CALL(cli_slice_cut, SymbolsSliceCtx, .effect = effect, .path = args->path,
                               .first = args->first, .last_value = args->last, .opener = args->opener,
                               .openers = args->openers, .each = args->each_slice_line, .context = args->context);
    EMBED_CALL(cli_symbols_text_length_internal.effect_input_back, CliSymbolsTextLengthCtx, .effect = effect);
    if (!cut)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the slice");
    }
    return cut;
}

// The slices of a branch cut in one walk of it: the walk and the branch's path; where each slice's lines and each
// window go; the lines a window holds and the first of the next; and the lines opening the regions the walk stands in,
// outermost first, and how many, and those open past the most read
typedef struct
{
    CliEffect *effect;
    const char *path;
    CliSliceLine each;
    CliSliceWindow window;
    void *context;
    unsigned int height;
    unsigned int next;
    unsigned int opener[CLI_SLICE_OPENERS];
    unsigned int openers;
    unsigned int beyond;
} CliSlices;

// the line the head of the region the bracket at `leaf` opens starts on: back from it to the leaf after the last that
// ends a statement or a region's head, CLI_SLICE_HEAD_LINES lines at most
EMBED_INLINE unsigned int cli_slice_head_line(const SymbolsSliceCtx *args)
{
    unsigned int first = args->leaf;
    int started = 0;
    while (!started)
    {
        const unsigned int before =
            EMBED_CALL(cli_slice_last_before, SymbolsSliceCtx, .read = args->read, .leaf = first);
        started = (before == args->read->count) ||
                  EMBED_CALL(cli_slice_ends, SymbolsSliceCtx, .read = args->read, .leaf = before) ||
                  ((args->read->leaf[args->leaf].line - args->read->leaf[before].line) >= CLI_SLICE_HEAD_LINES);
        first = started ? first : before;
    }
    return args->read->leaf[first].line;
}

// a line of the branch reached by the walk: the window it begins cut where it is the first of one to hold a leaf, the
// regions open above it its openers, and the brackets of an active line that is no directive's opening and closing
// regions for the windows after it
static int cli_slices_reach(const CliEffectReachedCfg *args)
{
    void *context = args->context;
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    CliSlices *const slices = (CliSlices *)context;
    const unsigned int line = read->leaf[leaf].line;
    if (line >= slices->next)
    {
        const unsigned int first = line - (line % slices->height);
        const unsigned int last = first + (slices->height - 1u);
        slices->next = first + slices->height;
        if (!EMBED_CALL(cli_slice_cut, SymbolsSliceCtx, .effect = slices->effect, .path = slices->path, .first = first,
                        .last_value = last, .opener = slices->opener, .openers = slices->openers, .each = slices->each,
                        .context = slices->context) ||
            !EMBED_CALL(slices->window, CliSliceWindowCfg, .context = slices->context, .first = first, .last = last,
                        .opener = slices->opener, .openers = slices->openers))
        {
            return 0;
        }
    }
    if (!args->active || EMBED_CALL(cli_slice_directive_line, SymbolsSliceCtx, .read = read, .line = line))
    {
        return 1;
    }
    for (unsigned int at = leaf; (at < read->count) && (read->leaf[at].line == line); at += 1u)
    {
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = at,
                       .roles = CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            const int read_capacity = slices->openers < CLI_SLICE_OPENERS;
            if (read_capacity)
            {
                slices->opener[slices->openers] =
                    EMBED_CALL(cli_slice_head_line, SymbolsSliceCtx, .read = read, .leaf = at);
                slices->openers += 1u;
            }
            slices->beyond += read_capacity ? 0u : 1u;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = at,
                            .roles = CLI_LEAF_REGION | CLI_LEAF_CLOSE))
        {
            const int end = slices->beyond > 0u;
            slices->beyond -= end ? 1u : 0u;
            slices->openers -= (!end && (slices->openers > 0u)) ? 1u : 0u;
        }
    }
    return 1;
}

int cli_symbols_slices_each(const CliSymbolsPathTextSizeContextErrMsgCfg *args)
{
    CliEffect *const effect = EMBED_CALL(cli_symbols_path_name_internal.effect_init, CliSymbolsPathNameCtx,
                                         .path = args->path, .name = "", .slicing = 1, .err_msg = args->err_msg);
    if (effect == NULL)
    {
        return 0;
    }
    effect->start_text = args->text;
    effect->start_size = args->size;
    CliSlices slices = {effect,
                        args->path,
                        args->each_slice_line,
                        args->window,
                        args->context,
                        (args->height > 0u) ? args->height : 1u,
                        0u,
                        {0u},
                        0u,
                        0u};
    effect->reached = cli_slices_reach;
    effect->reached_context = &slices;
    EMBED_CALL(cli_symbols_path_effect_internal.effect_alloc, CliSymbolsPathEffectCtx, .effect = effect,
               .path = args->path, .line_u32 = CLI_EFFECT_INPUT);
    const int refused = effect->refused;
    EMBED_CALL(cli_symbols_text_length_internal.effect_input_back, CliSymbolsTextLengthCtx, .effect = effect);
    if (refused)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the slices");
    }
    return !refused;
}
