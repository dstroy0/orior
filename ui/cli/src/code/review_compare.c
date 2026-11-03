// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_compare.c: the files a change touched compared by their declarations: each declaration added, taken out,
// changed or moved a piece, and each piece with the pieces it calls
#include "../command_line/callback_cfg.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "review_internal.h"
#include "symbols.h"

// A declaration of one side of a file: its name and kind and text as places in the pieces' texts, and its line
typedef struct
{
    unsigned int name;
    unsigned int kind;
    unsigned int line;
    unsigned int text;
} CliDecl;
// One side of a file, before or after the change: its lines, each where it starts and its bytes, its declarations,
// and its lines before the first of them; the lines and declarations are parts of a cell sized to the file's two
// sides by their newlines, taken as the file is compared and given back after
typedef struct
{
    const char *text;
    unsigned int *start;
    unsigned int *length;
    unsigned int lines;
    unsigned int lines_max;
    CliDecl *decl;
    unsigned int decls;
    unsigned int decls_max;
    unsigned int head;
    unsigned char *matched;
} CliSide;
// the least share of a piece's lines a move retains to count as one, in hundredths, and the most declarations of a
// file's text
#define CLI_MOVE_MIN_SHARE 60ULL
#define CLI_REVIEW_DECLS 16384u

// The comparison: the two sides of the file compared, the pieces added and taken out across the files, and capacity to
// sort the pieces
typedef struct
{
    CliSide was;
    CliSide now;
    CliReviewPiece added[CLI_REVIEW_PIECES];
    unsigned int addeds;
    unsigned char reserved[CLI_REVIEW_PIECES];
    CliReviewPiece removed[CLI_REVIEW_PIECES];
    unsigned int removeds;
    CliReviewPiece reserve[CLI_REVIEW_PIECES];
} CliCompare;

// the comparison, in the review's capacity for it
static CliCompare *s_compare;

unsigned long long cli_review_compared_bytes(void)
{
    return sizeof(CliCompare);
}

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned char **at;
    unsigned long long bytes;
    CliSide *side;
    unsigned int lines;
    const CliSide *side_read;
    unsigned int from;
    unsigned int to;
    unsigned int line;
    const char *path_text;
    unsigned int at_u32;
    const char *one;
    const char *other;
    unsigned int path;
    const CliDecl *decl;
    const char *how;
    unsigned int was;
    unsigned int now;
    CliReviewPiece *list;
    unsigned int *count;
    CliReviewPiece piece;
    const CliReviewFile *file;
    const CliReviewPiece *one_review_piece_pointer;
    const CliReviewPiece *other_review_piece_pointer;
} ReviewCompareCtx;
// the lines of `size` bytes of `text`: one more than its newlines
EMBED_INLINE unsigned int cli_lines_count(const ReviewCompareCtx *args)
{
    unsigned int lines = 1u;
    for (unsigned long long at = 0ULL; at < args->size; lines += 1u)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = args->text + at, .val = '\n', .bytes = args->size - at});
        if (newline == NULL)
        {
            break;
        }
        at = (unsigned long long)(newline - args->text) + 1ULL;
    }
    return lines;
}

// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
EMBED_INLINE void *cli_side_part(const ReviewCompareCtx *args)
{
    unsigned char **at = args->at;
    void *const part = *at;
    *at += (args->bytes + 7ULL) & ~7ULL;
    return part;
}

// a side's parts set into a cell from `*at` for `lines` lines, its declarations at most CLI_REVIEW_DECLS of them
EMBED_INLINE void cli_side_set(const ReviewCompareCtx *args)
{
    const unsigned int decls = (args->lines < CLI_REVIEW_DECLS) ? args->lines : CLI_REVIEW_DECLS;
    args->side->start = EMBED_CALL(cli_side_part, ReviewCompareCtx, .at = args->at,
                                   .bytes = (unsigned long long)args->lines * sizeof(unsigned int));
    args->side->length = EMBED_CALL(cli_side_part, ReviewCompareCtx, .at = args->at,
                                    .bytes = (unsigned long long)args->lines * sizeof(unsigned int));
    args->side->lines_max = args->lines;
    args->side->decl = EMBED_CALL(cli_side_part, ReviewCompareCtx, .at = args->at,
                                  .bytes = (unsigned long long)decls * sizeof(CliDecl));
    args->side->decls_max = decls;
    args->side->matched = EMBED_CALL(cli_side_part, ReviewCompareCtx, .at = args->at, .bytes = decls);
}

// the bytes a side's parts take for `lines` lines
EMBED_INLINE unsigned long long cli_side_bytes(const ReviewCompareCtx *args)
{
    const unsigned int decls = (args->lines < CLI_REVIEW_DECLS) ? args->lines : CLI_REVIEW_DECLS;
    return ((unsigned long long)args->lines * 2ULL * sizeof(unsigned int)) +
           ((unsigned long long)decls * sizeof(CliDecl)) + decls + (4ULL * 8ULL);
}

unsigned int cli_review_piece_text_append(const CliReviewTextLengthCtx *args)
{
    if ((args->length + 1ULL) > (g_cli_review.texts_capacity - g_cli_review.texts_used))
    {
        return 0u;
    }
    const unsigned int at = g_cli_review.texts_used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_review.texts + at, .src = args->text, .bytes = args->length});
    g_cli_review.texts[at + args->length] = '\0';
    // the texts are below CLI_REVIEW_TEXTS_MAX bytes, which an unsigned int stores
    g_cli_review.texts_used += (unsigned int)args->length + 1u;
    return at;
}

// a text's lines read into a side, each less a carriage return before its newline
EMBED_INLINE void cli_lines_get(const ReviewCompareCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    args->side->text = text;
    args->side->lines = 0u;
    for (unsigned long long at = 0ULL; (at < size) && (args->side->lines < args->side->lines_max);)
    {
        const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - at;
        length -= bounds.stop - bounds.cut;
        // a text is below CLI_REVIEW_TEXT bytes, which an unsigned int stores
        args->side->start[args->side->lines] = (unsigned int)at;
        args->side->length[args->side->lines] = (unsigned int)length;
        args->side->lines += 1u;
        at = end + 1ULL;
    }
}

// the side's lines from `from` to `to` joined by newlines, put in the pieces' texts: their place
EMBED_INLINE unsigned int cli_join(const ReviewCompareCtx *args)
{
    const unsigned int at = g_cli_review.texts_used;
    unsigned long long size = 0ULL;
    for (unsigned int line = args->from; line < args->to; line += 1u)
    {
        size += args->side_read->length[line] + ((line != args->from) ? 1ULL : 0ULL);
    }
    if ((size + 1ULL) > (g_cli_review.texts_capacity - at))
    {
        return 0u;
    }
    char *write = g_cli_review.texts + at;
    for (unsigned int line = args->from; line < args->to; line += 1u)
    {
        if (line != args->from)
        {
            write[0] = '\n';
            write += 1;
        }
        memor.cpy(&(MemoriaCfg){.dst = write,
                                .src = args->side_read->text + args->side_read->start[line],
                                .bytes = args->side_read->length[line]});
        write += args->side_read->length[line];
    }
    write[0] = '\0';
    // the texts are below CLI_REVIEW_TEXTS_MAX bytes, which an unsigned int stores
    g_cli_review.texts_used += (unsigned int)size + 1u;
    return at;
}

// whether a line of a side contains nothing but whitespace
EMBED_INLINE int cli_blank_line(const ReviewCompareCtx *args)
{
    const char *text = args->side_read->text + args->side_read->start[args->line];
    unsigned long long length = args->side_read->length[args->line];
    EMBED_CALL(cli_review_text_length_kind_internal.trim, CliReviewTextLengthKindCtx, .text_pointer = &text,
               .length_pointer = &length);
    return length == 0ULL;
}

static void cli_declare(const CliDeclaredCfg *args)
{
    unsigned long long length = args->length;
    CliSide *const side = (CliSide *)args->context;
    if (side->decls < side->decls_max)
    {
        side->decl[side->decls] = (CliDecl){
            EMBED_CALL(cli_review_text_length_internal.piece_text_append, CliReviewTextLengthCtx, .text = args->name,
                       .length = length),
            EMBED_CALL(cli_review_text_length_internal.piece_text_append, CliReviewTextLengthCtx, .text = args->kind,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->kind, .cap = CLI_CELLS_POOL})),
            args->line, 0u};
        side->decls += 1u;
    }
}

// the declarations of a side's text, read in the language of `path`, each with its text, and its lines before the
// first of them
EMBED_INLINE void cli_spans(const ReviewCompareCtx *args)
{
    const char *path = args->path_text;
    const char *text = args->text;
    unsigned long long size = args->size;
    EMBED_CALL(cli_lines_get, ReviewCompareCtx, .text = text, .size = size, .side = args->side);
    args->side->decls = 0u;
    EMBED_CALL(cli_symbols_path_text_size.declare, CliSymbolsPathTextSizeCfg, .path = path, .text = text, .size = size,
               .each_declared = cli_declare, .context = args->side);
    const unsigned int lines = args->side->lines;
    for (unsigned int at = 0u; at < args->side->decls; at += 1u)
    {
        const unsigned int start = args->side->decl[at].line;
        unsigned int end = ((at + 1u) < args->side->decls) ? args->side->decl[at + 1u].line : lines;
        end = (end < lines) ? end : lines;
        while ((end > (start + 1u)) &&
               EMBED_CALL(cli_blank_line, ReviewCompareCtx, .side_read = args->side, .line = end - 1u))
        {
            end -= 1u;
        }
        const unsigned int from = (start < lines) ? start : lines;
        const unsigned int last = (end > start) ? end : start;
        const unsigned int to = (last < lines) ? last : lines;
        args->side->decl[at].text =
            EMBED_CALL(cli_join, ReviewCompareCtx, .side_read = args->side, .from = from, .to = to);
        args->side->matched[at] = 0u;
    }
    const unsigned int first = (args->side->decls > 0u) ? args->side->decl[0].line : lines;
    args->side->head = EMBED_CALL(cli_join, ReviewCompareCtx, .side_read = args->side, .from = 0u,
                                  .to = (first < lines) ? first : lines);
}

// how many declarations of a side before `at` share its name and kind
EMBED_INLINE unsigned int cli_occurrence(const ReviewCompareCtx *args)
{
    unsigned int count = 0u;
    for (unsigned int before = 0u; before < args->at_u32; before += 1u)
    {
        count += ((cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.texts + args->side_read->decl[before].name,
                                                .other = g_cli_review.texts + args->side_read->decl[args->at_u32].name,
                                                .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.texts + args->side_read->decl[before].kind,
                                                .other = g_cli_review.texts + args->side_read->decl[args->at_u32].kind,
                                                .cap = CLI_CELLS_POOL})))
                     ? 1u
                     : 0u;
    }
    return count;
}

// whether two texts are the same once the whitespace at either end of each is left off
EMBED_INLINE int cli_same_trim(const ReviewCompareCtx *args)
{
    const char *one = args->one;
    const char *other = args->other;
    unsigned long long one_length = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL});
    unsigned long long other_length = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_CELLS_POOL});
    EMBED_CALL(cli_review_text_length_kind_internal.trim, CliReviewTextLengthKindCtx, .text_pointer = &one,
               .length_pointer = &one_length);
    EMBED_CALL(cli_review_text_length_kind_internal.trim, CliReviewTextLengthKindCtx, .text_pointer = &other,
               .length_pointer = &other_length);
    return (one_length == other_length) &&
           (memor.cmp(&(MemoriaCfg){.src = one, .other = other, .bytes = one_length}) == 0);
}

// a file named apart
EMBED_INLINE void cli_other_append(const ReviewCompareCtx *args)
{
    if (g_cli_review.others < CLI_REVIEW_FILES)
    {
        g_cli_review.other[g_cli_review.others] = args->path;
        g_cli_review.others += 1u;
    }
}

// a piece made: its file, its declaration and how the change touched it, its line and its texts
EMBED_INLINE CliReviewPiece cli_piece(const ReviewCompareCtx *args)
{
    return (CliReviewPiece){EMBED_CALL(cli_review_text_length_internal.piece_text_append, CliReviewTextLengthCtx,
                                       .text = g_cli_review.text + args->path,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = g_cli_review.text + args->path,
                                                                               .cap = CLI_CELLS_POOL})),
                            args->decl->name,
                            args->decl->kind,
                            args->how,
                            0,
                            0u,
                            args->decl->line,
                            args->was,
                            args->now,
                            0u,
                            0u};
}

// a piece put after `list`, sized for CLI_REVIEW_PIECES
EMBED_INLINE void cli_piece_append(const ReviewCompareCtx *args)
{
    unsigned int *count = args->count;
    if (*count < CLI_REVIEW_PIECES)
    {
        args->list[*count] = args->piece;
        *count += 1u;
    }
}

// one file's declarations compared: those changed put among the pieces, those added and taken out retained for the
// moves
EMBED_INLINE void cli_file_compare(const ReviewCompareCtx *args)
{
    const char *const path = g_cli_review.text + args->file->path;
    if (!EMBED_CALL(cli_symbols_path_name.reads, CliSymbolsPathNameCfg, .path = path))
    {
        EMBED_CALL(cli_other_append, ReviewCompareCtx, .path = args->file->path);
        return;
    }
    CliSide *const was = &s_compare->was;
    CliSide *const now = &s_compare->now;
    const char *const was_text = args->file->has_was ? (g_cli_review.text + args->file->was) : "";
    const char *const now_text = args->file->has_now ? (g_cli_review.text + args->file->now) : "";
    const unsigned long long was_size = args->file->has_was ? args->file->was_size : 0ULL;
    const unsigned long long now_size = args->file->has_now ? args->file->now_size : 0ULL;
    // the two sides' lines and declarations in one cell sized to the file's newlines
    const unsigned int was_lines = EMBED_CALL(cli_lines_count, ReviewCompareCtx, .text = was_text, .size = was_size);
    const unsigned int now_lines = EMBED_CALL(cli_lines_count, ReviewCompareCtx, .text = now_text, .size = now_size);
    unsigned char *const cell =
        g_cli_cells->persistent_buf_alloc(EMBED_CALL(cli_side_bytes, ReviewCompareCtx, .lines = was_lines) +
                                              EMBED_CALL(cli_side_bytes, ReviewCompareCtx, .lines = now_lines),
                                          &g_cli_code_cells);
    if (cell == NULL)
    {
        EMBED_CALL(cli_other_append, ReviewCompareCtx, .path = args->file->path);
        return;
    }
    unsigned char *at_cell = cell;
    EMBED_CALL(cli_side_set, ReviewCompareCtx, .side = was, .at = &at_cell, .lines = was_lines);
    EMBED_CALL(cli_side_set, ReviewCompareCtx, .side = now, .at = &at_cell, .lines = now_lines);
    const unsigned int empty = EMBED_CALL(cli_review_text_length_internal.piece_text_append, CliReviewTextLengthCtx,
                                          .text = "", .length = 0ULL);
    EMBED_CALL(cli_spans, ReviewCompareCtx, .path_text = g_cli_review.text + args->file->was_path, .text = was_text,
               .size = was_size, .side = was);
    EMBED_CALL(cli_spans, ReviewCompareCtx, .path_text = path, .text = now_text, .size = now_size, .side = now);
    if (!EMBED_CALL(cli_same_trim, ReviewCompareCtx, .one = g_cli_review.texts + was->head,
                    .other = g_cli_review.texts + now->head) &&
        args->file->has_was && args->file->has_now)
    {
        EMBED_CALL(cli_other_append, ReviewCompareCtx, .path = args->file->path);
    }
    for (unsigned int at = 0u; at < now->decls; at += 1u)
    {
        const CliDecl *const one = &now->decl[at];
        const unsigned int occurrence = EMBED_CALL(cli_occurrence, ReviewCompareCtx, .side_read = now, .at_u32 = at);
        unsigned int match = was->decls;
        for (unsigned int old = 0u; (match == was->decls) && (old < was->decls); old += 1u)
        {
            const int same =
                (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.texts + was->decl[old].name,
                                              .other = g_cli_review.texts + one->name,
                                              .cap = CLI_CELLS_POOL})) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.texts + was->decl[old].kind,
                                              .other = g_cli_review.texts + one->kind,
                                              .cap = CLI_CELLS_POOL})) &&
                (EMBED_CALL(cli_occurrence, ReviewCompareCtx, .side_read = was, .at_u32 = old) == occurrence);
            match = same ? old : match;
        }
        if (match == was->decls)
        {
            EMBED_CALL(cli_piece_append, ReviewCompareCtx, .list = s_compare->added, .count = &s_compare->addeds,
                       .piece = EMBED_CALL(cli_piece, ReviewCompareCtx, .path = args->file->path, .decl = one,
                                           .how = "added", .was = empty, .now = one->text));
            continue;
        }
        was->matched[match] = 1u;
        if (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.texts + was->decl[match].text,
                                          .other = g_cli_review.texts + one->text,
                                          .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_piece_append, ReviewCompareCtx, .list = g_cli_review.piece, .count = &g_cli_review.pieces,
                       .piece = EMBED_CALL(cli_piece, ReviewCompareCtx, .path = args->file->path, .decl = one,
                                           .how = "changed", .was = was->decl[match].text, .now = one->text));
        }
    }
    // those taken out, by their lines
    for (unsigned int line_order = 0u; line_order < was->decls; line_order += 1u)
    {
        unsigned int min = was->decls;
        for (unsigned int old = 0u; old < was->decls; old += 1u)
        {
            const int open = !was->matched[old];
            min = (open && ((min == was->decls) || (was->decl[old].line < was->decl[min].line))) ? old : min;
        }
        if (min == was->decls)
        {
            break;
        }
        was->matched[min] = 1u;
        EMBED_CALL(cli_piece_append, ReviewCompareCtx, .list = s_compare->removed, .count = &s_compare->removeds,
                   .piece =
                       EMBED_CALL(cli_piece, ReviewCompareCtx, .path = args->file->was_path, .decl = &was->decl[min],
                                  .how = "removed", .was = was->decl[min].text, .now = empty));
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_code_cells);
}

// whether a piece comes before another: by path, then line
EMBED_INLINE int cli_piece_before(const ReviewCompareCtx *args)
{
    const CliReviewPiece *one = args->one_review_piece_pointer;
    const CliReviewPiece *other = args->other_review_piece_pointer;
    const int order = EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = g_cli_review.texts + one->path,
                                 .other_pointer = g_cli_review.texts + other->path);
    return (order < 0) || ((order == 0) && (one->line < other->line));
}

// the pieces sorted by path, then line, those alike retained in their order
static void cli_pieces_sort(void)
{
    CliReviewPiece *from = g_cli_review.piece;
    CliReviewPiece *into = s_compare->reserve;
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
                    (left < middle) && ((right >= end) || !EMBED_CALL(cli_piece_before, ReviewCompareCtx,
                                                                      .one_review_piece_pointer = &from[right],
                                                                      .other_review_piece_pointer = &from[left]));
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

// a piece taken out matched to one added of its name and kind that retains the most of its lines, a move
static void cli_moves(void)
{
    for (unsigned int index = 0u; index < s_compare->removeds; index += 1u)
    {
        const CliReviewPiece *const deleted = &s_compare->removed[index];
        unsigned int best = s_compare->addeds;
        unsigned long long best_same = 0ULL;
        unsigned long long best_longest = 1ULL;
        for (unsigned int at = 0u; at < s_compare->addeds; at += 1u)
        {
            const CliReviewPiece *const one = &s_compare->added[at];
            if (s_compare->reserved[at] ||
                (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.texts + one->name,
                                               .other = g_cli_review.texts + deleted->name,
                                               .cap = CLI_CELLS_POOL})) ||
                (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.texts + one->kind,
                                               .other = g_cli_review.texts + deleted->kind,
                                               .cap = CLI_CELLS_POOL})))
            {
                continue;
            }
            unsigned long long same = 0ULL;
            unsigned long long longest = 1ULL;
            if (!EMBED_CALL(cli_review_one_internal.share, CliReviewOneCtx, .one = g_cli_review.texts + deleted->was,
                            .other = g_cli_review.texts + one->now, .same = &same, .longest = &longest))
            {
                continue;
            }
            // a share as large as the best yet takes its place, the later winning a tie
            const int is_move = (same * 100ULL) >= (CLI_MOVE_MIN_SHARE * longest);
            if (is_move && ((best == s_compare->addeds) || ((same * best_longest) >= (best_same * longest))))
            {
                best = at;
                best_same = same;
                best_longest = longest;
            }
        }
        if (best == s_compare->addeds)
        {
            EMBED_CALL(cli_piece_append, ReviewCompareCtx, .list = g_cli_review.piece, .count = &g_cli_review.pieces,
                       .piece = *deleted);
            continue;
        }
        s_compare->reserved[best] = 1u;
        CliReviewPiece moved = s_compare->added[best];
        moved.how = "moved";
        moved.was = deleted->was;
        moved.has_from = !cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.texts + deleted->path,
                                                       .other = g_cli_review.texts + moved.path,
                                                       .cap = CLI_CELLS_POOL});
        moved.from = deleted->path;
        EMBED_CALL(cli_piece_append, ReviewCompareCtx, .list = g_cli_review.piece, .count = &g_cli_review.pieces,
                   .piece = moved);
    }
    for (unsigned int at = 0u; at < s_compare->addeds; at += 1u)
    {
        if (!s_compare->reserved[at])
        {
            EMBED_CALL(cli_piece_append, ReviewCompareCtx, .list = g_cli_review.piece, .count = &g_cli_review.pieces,
                       .piece = s_compare->added[at]);
        }
    }
}

// capacity made for one call more: 1, or 0 where the review contains the most or the cell is refused
static int compare_call_capacity(void)
{
    return g_cli_review.calls < CLI_REVIEW_CALLS;
}

void cli_review_compare(void)
{
    s_compare = (CliCompare *)g_cli_review.compared;
    g_cli_review.pieces = 0u;
    g_cli_review.texts_used = 0u;
    g_cli_review.calls = 0u;
    g_cli_review.others = 0u;
    s_compare->addeds = 0u;
    s_compare->removeds = 0u;
    for (unsigned int at = 0u; at < g_cli_review.files; at += 1u)
    {
        EMBED_CALL(cli_file_compare, ReviewCompareCtx, .file = &g_cli_review.file[at]);
    }
    memor.set(&(MemoriaCfg){.dst = s_compare->reserved, .val = 0, .bytes = sizeof(s_compare->reserved)});
    cli_moves();
    cli_pieces_sort();
    for (unsigned int at = 0u; at < g_cli_review.pieces; at += 1u)
    {
        CliReviewPiece *const piece = &g_cli_review.piece[at];
        const char *const text =
            g_cli_review.texts + ((g_cli_review.texts[piece->now] != '\0') ? piece->now : piece->was);
        const char *const newline = cellul.chr(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .byte = '\n'});
        const char *const body = (newline != NULL) ? (newline + 1) : "";
        piece->first = g_cli_review.calls;
        for (unsigned int other = 0u; other < g_cli_review.pieces; other += 1u)
        {
            const char *const name = g_cli_review.texts + g_cli_review.piece[other].name;
            if ((other != at) &&
                (!cellul.eq(&(CatenaFinitaCfg){
                    .src = name, .other = g_cli_review.texts + piece->name, .cap = CLI_CELLS_POOL})) &&
                EMBED_CALL(cli_review_internal.calls, CliReviewCtx, .text = body, .name = name) &&
                compare_call_capacity())
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
        const unsigned int pivot = g_cli_review.other[at];
        unsigned int place = at;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                           .one = g_cli_review.text + g_cli_review.other[place - 1u],
                                           .other_pointer = g_cli_review.text + pivot) > 0))
        {
            g_cli_review.other[place] = g_cli_review.other[place - 1u];
            place -= 1u;
        }
        g_cli_review.other[place] = pivot;
    }
    unsigned int remaining = 0u;
    for (unsigned int at = 0u; at < g_cli_review.others; at += 1u)
    {
        if ((remaining == 0u) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_review.text + g_cli_review.other[remaining - 1u],
                                           .other = g_cli_review.text + g_cli_review.other[at],
                                           .cap = CLI_CELLS_POOL})))
        {
            g_cli_review.other[remaining] = g_cli_review.other[at];
            remaining += 1u;
        }
    }
    g_cli_review.others = remaining;
}
