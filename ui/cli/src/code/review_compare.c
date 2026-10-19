// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_compare.c: the files a change touched compared by their declarations: each declaration added, taken out,
// changed or moved a piece, and each piece with the pieces it calls
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "review_internal.h"
#include "symbols.h"

// the least share of a piece's lines a move keeps to count as one, in hundredths, and the most declarations of a
// file's text
#define CLI_MOVE_KEEPS 60ull
#define CLI_REVIEW_DECLS 16384u

// A declaration of one side of a file: its name and kind and text as places in the pieces' texts, and its line
typedef struct
{
    unsigned int name;
    unsigned int kind;
    unsigned int line;
    unsigned int text;
} CliDecl;

// One side of a file, before or after the change: its lines, each where it starts and its bytes, its declarations,
// and its lines before the first of them
typedef struct
{
    const char *text;
    unsigned int start[CLI_REVIEW_LINES];
    unsigned int length[CLI_REVIEW_LINES];
    unsigned int lines;
    CliDecl decl[CLI_REVIEW_DECLS];
    unsigned int decls;
    unsigned int head;
    unsigned char matched[CLI_REVIEW_DECLS];
} CliSide;

// The comparison: the two sides of the file compared, the pieces added and taken out across the files, and room to
// sort the pieces
typedef struct
{
    CliSide was;
    CliSide now;
    CliReviewPiece added[CLI_REVIEW_PIECES];
    unsigned int addeds;
    unsigned char taken[CLI_REVIEW_PIECES];
    CliReviewPiece removed[CLI_REVIEW_PIECES];
    unsigned int removeds;
    CliReviewPiece spare[CLI_REVIEW_PIECES];
} CliCompare;

// the comparison, in the review's room for it
static CliCompare *s_compare;

unsigned long long cli_review_compared_bytes(void)
{
    return sizeof(CliCompare);
}

unsigned int cli_review_kept(const char *text, unsigned long long length)
{
    if ((g_cli_review.kept_used + length + 1ull) > CLI_REVIEW_KEPT)
    {
        return 0u;
    }
    const unsigned int at = g_cli_review.kept_used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_review.kept + at, .src = text, .bytes = length});
    g_cli_review.kept[at + length] = '\0';
    // the texts are below CLI_REVIEW_KEPT bytes, which an unsigned int holds
    g_cli_review.kept_used += (unsigned int)length + 1u;
    return at;
}

// a text's lines read into a side, each less a carriage return before its newline
static void cli_lines_of(const char *text, unsigned long long size, CliSide *side)
{
    side->text = text;
    side->lines = 0u;
    for (unsigned long long at = 0ull; (at < size) && (side->lines < CLI_REVIEW_LINES);)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        unsigned long long length = end - at;
        length -= ((newline != NULL) && (length > 0ull) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
        // a text is below CLI_REVIEW_TEXT bytes, which an unsigned int holds
        side->start[side->lines] = (unsigned int)at;
        side->length[side->lines] = (unsigned int)length;
        side->lines += 1u;
        at = end + 1ull;
    }
}

// the side's lines from `from` to `to` joined by newlines, put in the pieces' texts: their place
static unsigned int cli_joined(const CliSide *side, unsigned int from, unsigned int to)
{
    const unsigned int at = g_cli_review.kept_used;
    unsigned long long size = 0ull;
    for (unsigned int line = from; line < to; line += 1u)
    {
        size += side->length[line] + ((line != from) ? 1ull : 0ull);
    }
    if ((at + size + 1ull) > CLI_REVIEW_KEPT)
    {
        return 0u;
    }
    char *write = g_cli_review.kept + at;
    for (unsigned int line = from; line < to; line += 1u)
    {
        if (line != from)
        {
            write[0] = '\n';
            write += 1;
        }
        memor.cpy(&(MemoriaCfg){.dst = write, .src = side->text + side->start[line], .bytes = side->length[line]});
        write += side->length[line];
    }
    write[0] = '\0';
    // the texts are below CLI_REVIEW_KEPT bytes, which an unsigned int holds
    g_cli_review.kept_used += (unsigned int)size + 1u;
    return at;
}

// whether a line of a side holds nothing but whitespace
static int cli_blank_line(const CliSide *side, unsigned int line)
{
    const char *text = side->text + side->start[line];
    unsigned long long length = side->length[line];
    cli_review_trimmed(&text, &length);
    return length == 0ull;
}

static void cli_declared(void *context, const char *name, unsigned long long length, const char *kind,
                         unsigned int line)
{
    CliSide *const side = (CliSide *)context;
    if (side->decls < CLI_REVIEW_DECLS)
    {
        side->decl[side->decls] = (CliDecl){
            cli_review_kept(name, length),
            cli_review_kept(kind, cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL})), line, 0u};
        side->decls += 1u;
    }
}

// the declarations of a side's text, read in the language of `path`, each with its text, and its lines before the
// first of them
static void cli_spans(const char *path, const char *text, unsigned long long size, CliSide *side)
{
    cli_lines_of(text, size, side);
    side->decls = 0u;
    cli_symbols_declared(path, text, size, cli_declared, side);
    const unsigned int lines = side->lines;
    for (unsigned int at = 0u; at < side->decls; at += 1u)
    {
        const unsigned int start = side->decl[at].line;
        unsigned int end = ((at + 1u) < side->decls) ? side->decl[at + 1u].line : lines;
        end = (end < lines) ? end : lines;
        while ((end > (start + 1u)) && cli_blank_line(side, end - 1u))
        {
            end -= 1u;
        }
        const unsigned int from = (start < lines) ? start : lines;
        const unsigned int last = (end > start) ? end : start;
        const unsigned int to = (last < lines) ? last : lines;
        side->decl[at].text = cli_joined(side, from, to);
        side->matched[at] = 0u;
    }
    const unsigned int first = (side->decls > 0u) ? side->decl[0].line : lines;
    side->head = cli_joined(side, 0u, (first < lines) ? first : lines);
}

// how many declarations of a side before `at` share its name and kind
static unsigned int cli_occurrence(const CliSide *side, unsigned int at)
{
    unsigned int count = 0u;
    for (unsigned int before = 0u; before < at; before += 1u)
    {
        count += ((cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.kept + side->decl[before].name,
                                                .other = g_cli_review.kept + side->decl[at].name,
                                                .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.kept + side->decl[before].kind,
                                                .other = g_cli_review.kept + side->decl[at].kind,
                                                .cap = CLI_CELLS_POOL})))
                     ? 1u
                     : 0u;
    }
    return count;
}

// whether two texts are the same once the whitespace at either end of each is left off
static int cli_same_trimmed(const char *one, const char *other)
{
    unsigned long long one_length = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL});
    unsigned long long other_length = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_CELLS_POOL});
    cli_review_trimmed(&one, &one_length);
    cli_review_trimmed(&other, &other_length);
    return (one_length == other_length) &&
           (memor.cmp(&(MemoriaCfg){.src = one, .other = other, .bytes = one_length}) == 0);
}

// a file named apart
static void cli_other_put(unsigned int path)
{
    if (g_cli_review.others < CLI_REVIEW_FILES)
    {
        g_cli_review.other[g_cli_review.others] = path;
        g_cli_review.others += 1u;
    }
}

// a piece made: its file, its declaration and how the change touched it, its line and its texts
static CliReviewPiece cli_piece(unsigned int path, const CliDecl *decl, const char *how, unsigned int was,
                                unsigned int now)
{
    return (CliReviewPiece){
        cli_review_kept(g_cli_review.text + path,
                        cellul.len(&(CatenaFinitaCfg){.src = g_cli_review.text + path, .cap = CLI_CELLS_POOL})),
        decl->name,
        decl->kind,
        how,
        0,
        0u,
        decl->line,
        was,
        now,
        0u,
        0u};
}

// a piece put after `list`, which holds CLI_REVIEW_PIECES
static void cli_piece_put(CliReviewPiece *list, unsigned int *count, CliReviewPiece piece)
{
    if (*count < CLI_REVIEW_PIECES)
    {
        list[*count] = piece;
        *count += 1u;
    }
}

// one file's declarations compared: those changed put among the pieces, those added and taken out kept for the moves
static void cli_file_compared(const CliReviewFile *file)
{
    const char *const path = g_cli_review.text + file->path;
    if (!cli_symbols_reads(path))
    {
        cli_other_put(file->path);
        return;
    }
    CliSide *const was = &s_compare->was;
    CliSide *const now = &s_compare->now;
    const unsigned int empty = cli_review_kept("", 0ull);
    cli_spans(g_cli_review.text + file->was_path, file->has_was ? (g_cli_review.text + file->was) : "",
              file->has_was ? file->was_size : 0ull, was);
    cli_spans(path, file->has_now ? (g_cli_review.text + file->now) : "", file->has_now ? file->now_size : 0ull, now);
    if (!cli_same_trimmed(g_cli_review.kept + was->head, g_cli_review.kept + now->head) && file->has_was &&
        file->has_now)
    {
        cli_other_put(file->path);
    }
    for (unsigned int at = 0u; at < now->decls; at += 1u)
    {
        const CliDecl *const one = &now->decl[at];
        const unsigned int occurrence = cli_occurrence(now, at);
        unsigned int match = was->decls;
        for (unsigned int old = 0u; (match == was->decls) && (old < was->decls); old += 1u)
        {
            const int same = (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.kept + was->decl[old].name,
                                                           .other = g_cli_review.kept + one->name,
                                                           .cap = CLI_CELLS_POOL})) &&
                             (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.kept + was->decl[old].kind,
                                                           .other = g_cli_review.kept + one->kind,
                                                           .cap = CLI_CELLS_POOL})) &&
                             (cli_occurrence(was, old) == occurrence);
            match = same ? old : match;
        }
        if (match == was->decls)
        {
            cli_piece_put(s_compare->added, &s_compare->addeds, cli_piece(file->path, one, "added", empty, one->text));
            continue;
        }
        was->matched[match] = 1u;
        if (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.kept + was->decl[match].text,
                                          .other = g_cli_review.kept + one->text,
                                          .cap = CLI_CELLS_POOL}))
        {
            cli_piece_put(g_cli_review.piece, &g_cli_review.pieces,
                          cli_piece(file->path, one, "changed", was->decl[match].text, one->text));
        }
    }
    // those taken out, by their lines
    for (unsigned int line_order = 0u; line_order < was->decls; line_order += 1u)
    {
        unsigned int least = was->decls;
        for (unsigned int old = 0u; old < was->decls; old += 1u)
        {
            const int open = !was->matched[old];
            least = (open && ((least == was->decls) || (was->decl[old].line < was->decl[least].line))) ? old : least;
        }
        if (least == was->decls)
        {
            break;
        }
        was->matched[least] = 1u;
        cli_piece_put(s_compare->removed, &s_compare->removeds,
                      cli_piece(file->was_path, &was->decl[least], "removed", was->decl[least].text, empty));
    }
}

// whether a piece comes before another: by path, then line
static int cli_piece_before(const CliReviewPiece *one, const CliReviewPiece *other)
{
    const int order = cli_text_order(g_cli_review.kept + one->path, g_cli_review.kept + other->path);
    return (order < 0) || ((order == 0) && (one->line < other->line));
}

// the pieces sorted by path, then line, those alike kept in their order
static void cli_pieces_sorted(void)
{
    CliReviewPiece *from = g_cli_review.piece;
    CliReviewPiece *into = s_compare->spare;
    const unsigned int count = g_cli_review.pieces;
    for (unsigned int width = 1u; width < count; width *= 2u)
    {
        for (unsigned int start = 0u; start < count; start += 2u * width)
        {
            const unsigned int middle = ((start + width) < count) ? (start + width) : count;
            const unsigned int end = ((middle + width) < count) ? (middle + width) : count;
            unsigned int left = start;
            unsigned int right = middle;
            for (unsigned int at = start; at < end; at += 1u)
            {
                const int take_left =
                    (left < middle) && ((right >= end) || !cli_piece_before(&from[right], &from[left]));
                into[at] = take_left ? from[left] : from[right];
                left += take_left ? 1u : 0u;
                right += take_left ? 0u : 1u;
            }
        }
        CliReviewPiece *const turned = from;
        from = into;
        into = turned;
    }
    if (from != g_cli_review.piece)
    {
        memor.cpy(&(MemoriaCfg){.dst = g_cli_review.piece, .src = from, .bytes = count * sizeof(CliReviewPiece)});
    }
}

// a piece taken out matched to one added of its name and kind that keeps the most of its lines, a move
static void cli_moves(void)
{
    for (unsigned int index = 0u; index < s_compare->removeds; index += 1u)
    {
        const CliReviewPiece *const gone = &s_compare->removed[index];
        unsigned int best = s_compare->addeds;
        unsigned long long best_same = 0ull;
        unsigned long long best_longest = 1ull;
        for (unsigned int at = 0u; at < s_compare->addeds; at += 1u)
        {
            const CliReviewPiece *const one = &s_compare->added[at];
            if (s_compare->taken[at] ||
                (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.kept + one->name,
                                               .other = g_cli_review.kept + gone->name,
                                               .cap = CLI_CELLS_POOL})) ||
                (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.kept + one->kind,
                                               .other = g_cli_review.kept + gone->kind,
                                               .cap = CLI_CELLS_POOL})))
            {
                continue;
            }
            unsigned long long same = 0ull;
            unsigned long long longest = 1ull;
            cli_review_share(g_cli_review.kept + gone->was, g_cli_review.kept + one->now, &same, &longest);
            // a share as large as the best yet takes its place, the later winning a tie
            const int keeps = (same * 100ull) >= (CLI_MOVE_KEEPS * longest);
            if (keeps && ((best == s_compare->addeds) || ((same * best_longest) >= (best_same * longest))))
            {
                best = at;
                best_same = same;
                best_longest = longest;
            }
        }
        if (best == s_compare->addeds)
        {
            cli_piece_put(g_cli_review.piece, &g_cli_review.pieces, *gone);
            continue;
        }
        s_compare->taken[best] = 1u;
        CliReviewPiece moved = s_compare->added[best];
        moved.how = "moved";
        moved.was = gone->was;
        moved.has_from = !cellul.eq(&(CatenaFinitaCfg){
            .src = g_cli_review.kept + gone->path, .other = g_cli_review.kept + moved.path, .cap = CLI_CELLS_POOL});
        moved.from = gone->path;
        cli_piece_put(g_cli_review.piece, &g_cli_review.pieces, moved);
    }
    for (unsigned int at = 0u; at < s_compare->addeds; at += 1u)
    {
        if (!s_compare->taken[at])
        {
            cli_piece_put(g_cli_review.piece, &g_cli_review.pieces, s_compare->added[at]);
        }
    }
}

void cli_review_compare(void)
{
    s_compare = (CliCompare *)g_cli_review.compared;
    g_cli_review.pieces = 0u;
    g_cli_review.kept_used = 0u;
    g_cli_review.calls = 0u;
    g_cli_review.others = 0u;
    s_compare->addeds = 0u;
    s_compare->removeds = 0u;
    for (unsigned int at = 0u; at < g_cli_review.files; at += 1u)
    {
        cli_file_compared(&g_cli_review.file[at]);
    }
    memor.set(&(MemoriaCfg){.dst = s_compare->taken, .val = 0, .bytes = sizeof(s_compare->taken)});
    cli_moves();
    cli_pieces_sorted();
    for (unsigned int at = 0u; at < g_cli_review.pieces; at += 1u)
    {
        CliReviewPiece *const piece = &g_cli_review.piece[at];
        const char *const text =
            g_cli_review.kept + ((g_cli_review.kept[piece->now] != '\0') ? piece->now : piece->was);
        const char *const newline = cellul.chr(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .byte = '\n'});
        const char *const body = (newline != NULL) ? (newline + 1) : "";
        piece->first = g_cli_review.calls;
        for (unsigned int other = 0u; other < g_cli_review.pieces; other += 1u)
        {
            const char *const name = g_cli_review.kept + g_cli_review.piece[other].name;
            if ((other != at) &&
                (!cellul.eq(&(CatenaFinitaCfg){
                    .src = name, .other = g_cli_review.kept + piece->name, .cap = CLI_CELLS_POOL})) &&
                cli_review_calls(body, name) && (g_cli_review.calls < CLI_REVIEW_CALLS))
            {
                g_cli_review.call[g_cli_review.calls] = other;
                g_cli_review.calls += 1u;
            }
        }
        piece->count = g_cli_review.calls - piece->first;
    }
    // the files named apart sorted, each once
    for (unsigned int at = 1u; at < g_cli_review.others; at += 1u)
    {
        const unsigned int held = g_cli_review.other[at];
        unsigned int place = at;
        while ((place > 0u) &&
               (cli_text_order(g_cli_review.text + g_cli_review.other[place - 1u], g_cli_review.text + held) > 0))
        {
            g_cli_review.other[place] = g_cli_review.other[place - 1u];
            place -= 1u;
        }
        g_cli_review.other[place] = held;
    }
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < g_cli_review.others; at += 1u)
    {
        if ((kept == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.text + g_cli_review.other[kept - 1u],
                                                           .other = g_cli_review.text + g_cli_review.other[at],
                                                           .cap = CLI_CELLS_POOL})))
        {
            g_cli_review.other[kept] = g_cli_review.other[at];
            kept += 1u;
        }
    }
    g_cli_review.others = kept;
}
