// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_slice.c: the slice of the tree clangd is given for lines of a branch: a walk of the branch to its first line,
// the bytes and leaves of each branch it reaches in the walk's cells until the slice is written, then the names the
// lines hold followed to what the walk met that defines them, and the names those hold, each written as the lines it
// stands on with the branch and line it came from. A name, a bracket, a region's bracket and a statement's end are
// known by the roles its grammar's tokens name, and a bracket left open is closed by the text of the one that closes it
// in its branch
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "symbols_internal.h"

// the most lines an opener's head runs across to the bracket that opens its region, the most openers read, and the
// most brackets the slice closes after its lines
#define CLI_SLICE_HEAD_LINES 64u
#define CLI_SLICE_OPENERS 64u
#define CLI_SLICE_OPEN 256u

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
    unsigned int name_room;
    CliSlicePart *part;
    unsigned int parts;
    unsigned int part_room;
    unsigned char *written;
} CliSlice;

// the leaves of the branch the walk read whole at `branch`
static const CliLeafRead *cli_slice_read(const CliSlice *slice, unsigned int branch)
{
    return &slice->effect->branch_read[branch].read;
}

// the first leaf of `read` on line `line` or past it, `read->count` where none is
static unsigned int cli_slice_line_first(const CliLeafRead *read, unsigned int line)
{
    unsigned int low = 0u;
    unsigned int high = read->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (read->leaf[middle].line < line)
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
static int cli_slice_bracket(const CliLeafRead *read, unsigned int leaf)
{
    return cli_leaf_has(read, leaf, CLI_LEAF_OPEN) ? 1 : (cli_leaf_has(read, leaf, CLI_LEAF_CLOSE) ? -1 : 0);
}

// the name `text`, `length` bytes, put among those the slice follows where it is new
static void cli_slice_name_put(CliSlice *slice, const char *text, unsigned long long length)
{
    for (unsigned int at = 0u; at < slice->names; at += 1u)
    {
        if ((slice->name[at].length == length) && cli_text_same(slice->name[at].text, text, length))
        {
            return;
        }
    }
    if (slice->names >= slice->name_room)
    {
        slice->effect->refused = 1;
        return;
    }
    slice->name[slice->names] = (CliSliceName){text, length};
    slice->names += 1u;
}

// the names the leaves from `first` to before `end` of the branch at `branch` hold put among those followed: each leaf
// its grammar names a name or a type, but one that declares
static void cli_slice_names_of(CliSlice *slice, unsigned int branch, unsigned int first, unsigned int end)
{
    const CliLeafRead *const read = cli_slice_read(slice, branch);
    const unsigned long long named = CLI_LEAF_NAME | CLI_LEAF_TYPE;
    const unsigned long long passed = CLI_LEAF_DECLARES | CLI_LEAF_STRING | CLI_LEAF_COMMENT;
    for (unsigned int leaf = first; leaf < end; leaf += 1u)
    {
        const unsigned long long is = read->leaf[leaf].is;
        if (((is & named) != 0u) && ((is & passed) == 0u))
        {
            unsigned long long length = 0ull;
            const char *const text = cli_leaf_text(read, leaf, &length);
            cli_slice_name_put(slice, text, length);
        }
    }
}

// the leaves from `first` to before `end` of the branch at `branch` put among the lines' own parts, in the order of
// their leaves
static void cli_slice_part_put(CliSlice *slice, unsigned int branch, unsigned int first, unsigned int end)
{
    if (slice->parts >= slice->part_room)
    {
        slice->effect->refused = 1;
        return;
    }
    unsigned int at = slice->parts;
    while ((at > 0u) && (slice->part[at - 1u].first > first))
    {
        slice->part[at] = slice->part[at - 1u];
        at -= 1u;
    }
    slice->part[at] = (CliSlicePart){branch, first, end};
    slice->parts += 1u;
}

// the leaf after the last of the statement whose first leaf is `first`: its end, as its grammar names it, at the depth
// it starts at, or for a function, `function` 1, whose head a body follows, the bracket that closes the body's region,
// `*defined` 1 then; a directive's line at that depth ends a statement that runs into it
static unsigned int cli_slice_statement(const CliLeafRead *read, unsigned int first, int function, int *defined)
{
    *defined = 0;
    unsigned int depth = 0u;
    for (unsigned int leaf = first; leaf < read->count; leaf += 1u)
    {
        if ((depth == 0u) && ((read->leaf[leaf].is & CLI_LEAF_DIRECTIVE) != 0u) &&
            (read->leaf[leaf].line != read->leaf[first].line))
        {
            return cli_slice_line_first(read, read->leaf[leaf].line);
        }
        *defined |= function && (depth == 0u) && cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_OPEN);
        const int bracket = cli_slice_bracket(read, leaf);
        depth = (bracket > 0) ? (depth + 1u) : (((bracket < 0) && (depth > 0u)) ? (depth - 1u) : depth);
        const int closed = *defined && (bracket < 0);
        if ((depth == 0u) && (closed || (!*defined && cli_leaf_has(read, leaf, CLI_LEAF_STATEMENT))))
        {
            return leaf + 1u;
        }
    }
    return read->count;
}

// whether the line `line` of `read` is a directive's
static int cli_slice_directive_line(const CliLeafRead *read, unsigned int line)
{
    for (unsigned int leaf = cli_slice_line_first(read, line); (leaf < read->count) && (read->leaf[leaf].line == line);
         leaf += 1u)
    {
        if ((read->leaf[leaf].is & CLI_LEAF_DIRECTIVE) != 0u)
        {
            return 1;
        }
    }
    return 0;
}

// the last leaf before the leaf `leaf` none of whose blank or comment it is, `read->count` where none is
static unsigned int cli_slice_last_before(const CliLeafRead *read, unsigned int leaf)
{
    while ((leaf > 0u) && ((read->leaf[leaf - 1u].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
    {
        leaf -= 1u;
    }
    return (leaf > 0u) ? (leaf - 1u) : read->count;
}

// whether a statement or a region's head ends at the leaf `leaf`: a statement's end, a region's bracket, or the last
// leaf of a directive's line
static int cli_slice_ends(const CliLeafRead *read, unsigned int leaf)
{
    return cli_leaf_has(read, leaf, CLI_LEAF_STATEMENT) || cli_leaf_has(read, leaf, CLI_LEAF_REGION) ||
           cli_slice_directive_line(read, read->leaf[leaf].line);
}

// the lines `*first` to `*last` widened to whole statements, CLI_SLICE_HEAD_LINES lines at most each way: the first
// back to the line after one that ends a statement, the last on to one that ends one
static void cli_slice_widened(const CliLeafRead *read, unsigned int *first, unsigned int *last)
{
    for (unsigned int lines = 0u; lines < CLI_SLICE_HEAD_LINES; lines += 1u)
    {
        const unsigned int before = cli_slice_last_before(read, cli_slice_line_first(read, *first));
        if ((before == read->count) || cli_slice_ends(read, before))
        {
            break;
        }
        *first = read->leaf[before].line;
    }
    for (unsigned int lines = 0u; lines < CLI_SLICE_HEAD_LINES; lines += 1u)
    {
        const unsigned int after = cli_slice_line_first(read, *last + 1u);
        const unsigned int ending = cli_slice_last_before(read, after);
        if ((after == read->count) || (ending == read->count) || cli_slice_ends(read, ending))
        {
            break;
        }
        *last = read->leaf[after].line;
    }
}

// the leaf after the bracket that opens the region the line `line` opens: the first region's opening at the depth the
// line starts at within CLI_SLICE_HEAD_LINES lines, else the line's end
static unsigned int cli_slice_head(const CliLeafRead *read, unsigned int line)
{
    unsigned int depth = 0u;
    for (unsigned int leaf = cli_slice_line_first(read, line);
         (leaf < read->count) && (read->leaf[leaf].line < (line + CLI_SLICE_HEAD_LINES)); leaf += 1u)
    {
        if ((depth == 0u) && cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            return leaf + 1u;
        }
        const int bracket = cli_slice_bracket(read, leaf);
        depth = (bracket > 0) ? (depth + 1u) : (((bracket < 0) && (depth > 0u)) ? (depth - 1u) : depth);
    }
    return cli_slice_line_first(read, line + 1u);
}

// the thing the walk met at `seen`, its leaves running to before its `end`, written with the slice, and the names its
// leaves hold followed
static void cli_slice_picked(CliSlice *slice, unsigned int seen)
{
    const CliEffectSeen *const one = &slice->effect->seen[seen];
    if ((slice->written[seen] != 0u) || (one->branch == CLI_JSON_NONE))
    {
        return;
    }
    slice->written[seen] = 1u;
    cli_slice_names_of(slice, one->branch, one->first, one->end);
}

// what the walk met that defines the name `name`: the define in effect where a macro is in effect under it, else the
// last declaration of it the walk met in an active region that no body follows, else the last whose body follows it,
// with the body
static void cli_slice_followed(CliSlice *slice, CliSliceName name)
{
    CliEffect *const effect = slice->effect;
    const unsigned int macro = cli_effect_macro(effect, name.text, name.length);
    if (macro != CLI_JSON_NONE)
    {
        if (effect->macro[macro].seen != CLI_JSON_NONE)
        {
            cli_slice_picked(slice, effect->macro[macro].seen);
        }
        return;
    }
    unsigned int chosen = CLI_JSON_NONE;
    for (unsigned int at = effect->seens; at > 0u; at -= 1u)
    {
        CliEffectSeen *const one = &effect->seen[at - 1u];
        if ((one->is != CLI_SEEN_DECLARED) || (one->branch == CLI_JSON_NONE) || (one->length != name.length) ||
            !cli_text_same(cli_effect_text(effect, one->name), name.text, name.length))
        {
            continue;
        }
        const int function = cellul.eq(
            &(CatenaFinitaCfg){.src = cli_effect_text(effect, one->kind), .other = "function", .cap = CLI_CELLS_POOL});
        int defined = 0;
        one->end = cli_slice_statement(cli_slice_read(slice, one->branch), one->first, function, &defined);
        chosen = ((chosen == CLI_JSON_NONE) || !defined) ? (at - 1u) : chosen;
        if (!defined)
        {
            break;
        }
    }
    if (chosen != CLI_JSON_NONE)
    {
        cli_slice_picked(slice, chosen);
    }
}

// the locals declared from the leaf `from` to before the leaf `view` the slice's names name, each a statement whose
// line holds a leaf its grammar names a local, at the depth of the regions the openers' heads `head_first` to
// `head_end`, `heads` of them, open above it, put among the lines' own parts with the names they hold followed
static void cli_slice_locals(CliSlice *slice, unsigned int from, unsigned int view, const unsigned int *head_first,
                             const unsigned int *head_end, unsigned int heads)
{
    const CliLeafRead *const read = cli_slice_read(slice, slice->start);
    unsigned int depth = 0u;
    unsigned int leaf = from;
    while (leaf < view)
    {
        // an opener's head is no local's: its leaves passed over, the bracket that opens its region counted
        unsigned int opened = 0u;
        unsigned int head = heads;
        for (unsigned int at = 0u; at < heads; at += 1u)
        {
            opened += (head_end[at] <= leaf) ? 1u : 0u;
            head = (head_first[at] == leaf) ? at : head;
        }
        if (head != heads)
        {
            depth += 1u;
            leaf = head_end[head];
            continue;
        }
        const unsigned int line = read->leaf[leaf].line;
        const unsigned int line_end = cli_slice_line_first(read, line + 1u);
        unsigned int named = slice->names;
        for (unsigned int each = leaf; (depth == opened) && (each < line_end) && (each < view); each += 1u)
        {
            if ((read->leaf[each].is & CLI_LEAF_LOCAL) == 0u)
            {
                continue;
            }
            unsigned long long length = 0ull;
            const char *const text = cli_leaf_text(read, each, &length);
            for (unsigned int at = 0u; at < slice->names; at += 1u)
            {
                named = ((slice->name[at].length == length) && cli_text_same(slice->name[at].text, text, length))
                            ? at
                            : named;
            }
        }
        if (named != slice->names)
        {
            int defined = 0;
            const unsigned int end = cli_slice_statement(read, leaf, 0, &defined);
            cli_slice_part_put(slice, slice->start, leaf, end);
            cli_slice_names_of(slice, slice->start, leaf, end);
            leaf = (end > leaf) ? end : line_end;
            continue;
        }
        for (; (leaf < line_end) && (leaf < view); leaf += 1u)
        {
            depth += cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_OPEN) ? 1u : 0u;
            depth -= (cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_CLOSE) && (depth > 0u)) ? 1u : 0u;
        }
    }
}

// the lines the leaves from `first` to before `end` of the branch at `branch` stand on handed to `each`, each with the
// branch and line it came from, the blanks and comments at its end left off
static void cli_slice_lines_handed(const CliSlice *slice, unsigned int branch, unsigned int first, unsigned int end,
                                   CliSliceLine each, void *context)
{
    const CliEffectBranchRead *const whole = &slice->effect->branch_read[branch];
    const CliLeafRead *const read = &whole->read;
    while ((end > first) && ((read->leaf[end - 1u].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
    {
        end -= 1u;
    }
    if (end <= first)
    {
        return;
    }
    const char *const path = cli_effect_text(slice->effect, whole->path);
    const unsigned int last = read->leaf[end - 1u].line;
    unsigned int leaf = first;
    for (unsigned int line = read->leaf[first].line; line <= last; line += 1u)
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
        each(context, path, line, (const char *)whole->bytes + from, to - from);
    }
}

// the leaf of `read` that closes the bracket that opens at the leaf `open`, at its depth, `read->count` where none does
static unsigned int cli_slice_closer(const CliLeafRead *read, unsigned int open)
{
    unsigned int depth = 0u;
    for (unsigned int leaf = open + 1u; leaf < read->count; leaf += 1u)
    {
        const int bracket = cli_slice_bracket(read, leaf);
        if ((bracket < 0) && (depth == 0u))
        {
            return leaf;
        }
        depth = (bracket > 0) ? (depth + 1u) : (((bracket < 0) && (depth > 0u)) ? (depth - 1u) : depth);
    }
    return read->count;
}

// the slice handed to `each` a line at a time: each include of no file of the tree and each thing it writes the walk
// met, in the order the walk met them, then the lines' own parts, then a line closing each bracket they leave open,
// innermost first, from no branch, each the text of the bracket that closes it in its branch
static void cli_slice_handed(const CliSlice *slice, CliSliceLine each, void *context)
{
    const CliEffect *const effect = slice->effect;
    for (unsigned int at = 0u; at < effect->seens; at += 1u)
    {
        const CliEffectSeen *const one = &effect->seen[at];
        const int writes = (one->is == CLI_SEEN_INCLUDE) || (slice->written[at] != 0u);
        if (writes && (one->branch != CLI_JSON_NONE))
        {
            cli_slice_lines_handed(slice, one->branch, one->first, one->end, each, context);
        }
    }
    const CliLeafRead *open_read[CLI_SLICE_OPEN];
    unsigned int open_leaf[CLI_SLICE_OPEN];
    unsigned int opens = 0u;
    for (unsigned int at = 0u; at < slice->parts; at += 1u)
    {
        const CliSlicePart *const part = &slice->part[at];
        const CliLeafRead *const read = cli_slice_read(slice, part->branch);
        cli_slice_lines_handed(slice, part->branch, part->first, part->end, each, context);
        for (unsigned int leaf = part->first; leaf < part->end; leaf += 1u)
        {
            const int bracket = cli_slice_bracket(read, leaf);
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
        const unsigned int closer = cli_slice_closer(read, open_leaf[opens - 1u]);
        unsigned long long length = 0ull;
        const char *const text = (closer < read->count) ? cli_leaf_text(read, closer, &length) : "";
        if (length > 0ull)
        {
            each(context, NULL, CLI_SLICE_NOWHERE, text, length);
        }
    }
}

// The slice written as JSON: where it is written, and whether a line is written yet
typedef struct
{
    VerbaLinea *out;
    int any;
} CliSliceJson;

// one line of the slice written as `{"path", "line", "text"}`, after the lines before it
static void cli_slice_json_line(void *context, const char *path, unsigned int line, const char *text,
                                unsigned long long length)
{
    CliSliceJson *const json = (CliSliceJson *)context;
    mmgr_line_put(json->out, json->any ? ",{\"path\":" : "{\"path\":");
    json->any = 1;
    if (path == NULL)
    {
        mmgr_line_put(json->out, "null,\"line\":null");
    }
    else
    {
        cli_json_quoted(json->out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(json->out, ",\"line\":");
        mmgr_line_decimal(json->out, line, 1u);
    }
    mmgr_line_put(json->out, ",\"text\":");
    cli_json_quoted_lossy(json->out, (const unsigned char *)text, length);
    mmgr_line_ch(json->out, '}');
}

int cli_symbols_slice(const char *path, const char *text, unsigned long long size, unsigned int first,
                      unsigned int last, const unsigned int *opener, unsigned int openers, VerbaLinea *out,
                      VerbaLinea *err_msg)
{
    CliSliceJson json = {out, 0};
    mmgr_line_put(out, "{\"lines\":[");
    const int cut =
        cli_symbols_slice_each(path, text, size, first, last, opener, openers, cli_slice_json_line, &json, err_msg);
    mmgr_line_put(out, "]}");
    return cut;
}

// the first leaf of the line `line` of `read`, or where the line begins with brackets that close regions the first past
// them and the blanks about them: the head of the region a line that closes one region and opens another opens
static unsigned int cli_slice_head_first(const CliLeafRead *read, unsigned int line)
{
    const unsigned int first = cli_slice_line_first(read, line);
    unsigned int leaf = first;
    int closed = 0;
    while ((leaf < read->count) && (read->leaf[leaf].line == line) &&
           (cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_CLOSE) ||
            ((read->leaf[leaf].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u)))
    {
        closed |= cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_CLOSE);
        leaf += 1u;
    }
    return closed ? leaf : first;
}

// the slice of the lines `first` to `last` of the branch at `path` cut from the walk `effect` as far as it has gone,
// the lines opening the regions `first` stands in `opener`, `openers` of them, handed to `each` a line at a time: 1, or
// 0 with the walk refused where the cells cannot meet it
static int cli_slice_cut(CliEffect *effect, const char *path, unsigned int first, unsigned int last,
                         const unsigned int *opener, unsigned int openers, CliSliceLine each, void *context)
{
    const unsigned long long name_bytes = CLI_EFFECT_LIST_MOST * sizeof(CliSliceName);
    char *const lists =
        g_cli_cells->temporary_buf_alloc(name_bytes + (CLI_EFFECT_LIST_MOST * sizeof(CliSlicePart)), &g_cli_code_cells);
    CliSlice slice = {effect, CLI_JSON_NONE, NULL, 0u, 0u, NULL, 0u, 0u, NULL};
    if (lists != NULL)
    {
        slice.name = (CliSliceName *)(void *)lists;
        slice.name_room = CLI_EFFECT_LIST_MOST;
        slice.part = (CliSlicePart *)(void *)(lists + name_bytes);
        slice.part_room = CLI_EFFECT_LIST_MOST;
    }
    else
    {
        effect->refused = 1;
    }
    // the branch the lines stand in, the last read whole of its path
    for (unsigned int at = 0u; at < effect->branches_read; at += 1u)
    {
        slice.start =
            cellul.eq(&(CatenaFinitaCfg){
                .src = cli_effect_text(effect, effect->branch_read[at].path), .other = path, .cap = CLI_CELLS_POOL})
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
    const unsigned int heads = (openers < CLI_SLICE_OPENERS) ? openers : CLI_SLICE_OPENERS;
    if (!effect->refused && (slice.start != CLI_JSON_NONE))
    {
        const CliLeafRead *const read = cli_slice_read(&slice, slice.start);
        // the openers' heads and the lines themselves, then the locals they name, then what the names follow to
        for (unsigned int at = 0u; at < heads; at += 1u)
        {
            head_first[at] = cli_slice_head_first(read, opener[at]);
            head_end[at] = cli_slice_head(read, opener[at]);
            cli_slice_part_put(&slice, slice.start, head_first[at], head_end[at]);
            cli_slice_names_of(&slice, slice.start, head_first[at], head_end[at]);
        }
        unsigned int view_first = first;
        unsigned int view_last = last;
        cli_slice_widened(read, &view_first, &view_last);
        const unsigned int view = cli_slice_line_first(read, view_first);
        const unsigned int view_end = cli_slice_line_first(read, view_last + 1u);
        cli_slice_part_put(&slice, slice.start, view, view_end);
        cli_slice_names_of(&slice, slice.start, view, view_end);
        if (heads != 0u)
        {
            cli_slice_locals(&slice, head_end[0], view, head_first + 1, head_end + 1, heads - 1u);
        }
        for (unsigned int at = 0u; !effect->refused && (at < slice.names); at += 1u)
        {
            cli_slice_followed(&slice, slice.name[at]);
        }
    }
    if (!effect->refused && (slice.start != CLI_JSON_NONE))
    {
        cli_slice_handed(&slice, each, context);
    }
    g_cli_cells->persistent_buf_release(lists, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(slice.written, &g_cli_code_cells);
    return !effect->refused;
}

int cli_symbols_slice_each(const char *path, const char *text, unsigned long long size, unsigned int first,
                           unsigned int last, const unsigned int *opener, unsigned int openers, CliSliceLine each,
                           void *context, VerbaLinea *err_msg)
{
    CliEffect *const effect = cli_effect_made(path, "", 1, err_msg);
    if (effect == NULL)
    {
        return 0;
    }
    effect->start_text = text;
    effect->start_size = size;
    cli_effect_taken(effect, path, first);
    const int cut = !effect->refused && cli_slice_cut(effect, path, first, last, opener, openers, each, context);
    cli_effect_given_back(effect);
    if (!cut)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the slice");
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
static unsigned int cli_slice_head_line(const CliLeafRead *read, unsigned int leaf)
{
    unsigned int first = leaf;
    int started = 0;
    while (!started)
    {
        const unsigned int before = cli_slice_last_before(read, first);
        started = (before == read->count) || cli_slice_ends(read, before) ||
                  ((read->leaf[leaf].line - read->leaf[before].line) >= CLI_SLICE_HEAD_LINES);
        first = started ? first : before;
    }
    return read->leaf[first].line;
}

// a line of the branch reached by the walk: the window it begins cut where it is the first of one to hold a leaf, the
// regions open above it its openers, and the brackets of an active line that is no directive's opening and closing
// regions for the windows after it
static int cli_slices_reached(void *context, const CliLeafRead *read, unsigned int leaf, int active)
{
    CliSlices *const slices = (CliSlices *)context;
    const unsigned int line = read->leaf[leaf].line;
    if (line >= slices->next)
    {
        const unsigned int first = line - (line % slices->height);
        const unsigned int last = first + (slices->height - 1u);
        slices->next = first + slices->height;
        if (!cli_slice_cut(slices->effect, slices->path, first, last, slices->opener, slices->openers, slices->each,
                           slices->context) ||
            !slices->window(slices->context, first, last, slices->opener, slices->openers))
        {
            return 0;
        }
    }
    if (!active || cli_slice_directive_line(read, line))
    {
        return 1;
    }
    for (unsigned int at = leaf; (at < read->count) && (read->leaf[at].line == line); at += 1u)
    {
        if (cli_leaf_has(read, at, CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            const int read_room = slices->openers < CLI_SLICE_OPENERS;
            if (read_room)
            {
                slices->opener[slices->openers] = cli_slice_head_line(read, at);
                slices->openers += 1u;
            }
            slices->beyond += read_room ? 0u : 1u;
        }
        else if (cli_leaf_has(read, at, CLI_LEAF_REGION | CLI_LEAF_CLOSE))
        {
            const int past = slices->beyond > 0u;
            slices->beyond -= past ? 1u : 0u;
            slices->openers -= (!past && (slices->openers > 0u)) ? 1u : 0u;
        }
    }
    return 1;
}

int cli_symbols_slices_each(const char *path, const char *text, unsigned long long size, unsigned int height,
                            CliSliceLine each, CliSliceWindow window, void *context, VerbaLinea *err_msg)
{
    CliEffect *const effect = cli_effect_made(path, "", 1, err_msg);
    if (effect == NULL)
    {
        return 0;
    }
    effect->start_text = text;
    effect->start_size = size;
    CliSlices slices = {effect, path, each, window, context, (height > 0u) ? height : 1u, 0u, {0u}, 0u, 0u};
    effect->reached = cli_slices_reached;
    effect->reached_context = &slices;
    cli_effect_taken(effect, path, CLI_EFFECT_GIVEN);
    const int refused = effect->refused;
    cli_effect_given_back(effect);
    if (refused)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the slices");
    }
    return !refused;
}
