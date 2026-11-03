// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_edit.c: a paged text's edits written over its pieces, its lines written since the file was read, its matches,
// and the text saved to its file
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "files.h"
#include "pages_internal.h"

// The pieces a replace retains as it builds them: a cell of `most`, the text's pieces and the two a cut piece's parts
// about the lines written add
typedef struct
{
    CliPagePiece *piece;
    unsigned int most;
    unsigned int count;
} CliPagePieces;
// the most bytes of an edit's lines, the most bytes of the matches find writes, and the bytes a reading's cell takes
// for an edit, find and a save
#define CLI_PAGE_EDIT (1u << 25u)
#define CLI_PAGE_FOUND (1u << 24u)
// the bytes a save stages before each write to the file beside the text
#define CLI_PAGE_SAVE_STAGE (1ULL << 20u)

typedef struct
{
    const char *adjacent;
    VerbaLinea *block;
    VerbaLinea *err_msg;
    CliPagePieces *pieces;
    CliPagePiece piece;
    const CliPaged *one_read;
    const CliPagePiece *piece_pointer;
    unsigned long long a;
    unsigned long long b;
    unsigned long long count;
    CliPaged *one;
    unsigned long long from;
    unsigned long long to;
    CliPagePiece lines_page_piece;
    const char *line;
    unsigned long long length;
    unsigned long long col;
    const char *text;
    unsigned long long size;
    const CliEdit *edit;
    CliPageLines *lines_out;
    const CliPageLines *lines;
    unsigned char *storage;
} PagesEditCtx;
// the stage's bytes appended to the file at `adjacent` and the stage emptied: 1, or 0 with why given where the stage
// overflowed or the write failed
EMBED_INLINE int save_stage(const PagesEditCtx *args)
{
    const int written = mmgr_line_ok(args->block) &&
                        EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = args->adjacent,
                                   .bytes = args->block->out, .size = args->block->at, .fresh = 0);
    if (!written)
    {
        mmgr_line_put(args->err_msg, args->adjacent);
        mmgr_line_put(args->err_msg, mmgr_line_ok(args->block) ? ": the stage could not be written to it"
                                                               : ": the stage a save writes through overflowed");
    }
    args->block->at = 0ULL;
    return written;
}

// a piece put after the pieces retained, one of written lines left out where it contains none
EMBED_INLINE void cli_piece_append(const PagesEditCtx *args)
{
    if ((args->piece.written && (args->piece.lines == 0ULL)) || (args->pieces->count >= args->pieces->most))
    {
        return;
    }
    args->pieces->piece[args->pieces->count] = args->piece;
    args->pieces->count += 1u;
}

// the part of a piece of `one` from `a` to before `b` of its `count` lines
EMBED_INLINE CliPagePiece cli_cut(const PagesEditCtx *args)
{
    const CliPaged *one = args->one_read;
    const CliPagePiece *piece = args->piece_pointer;
    if (piece->written)
    {
        return (CliPagePiece){1,
                              EMBED_CALL(cli_pages_piece_internal.page_write_at, CliPagesPieceCtx, .one_read = one,
                                         .piece = piece, .line = args->a),
                              EMBED_CALL(cli_pages_piece_internal.page_write_at, CliPagesPieceCtx, .one_read = one,
                                         .piece = piece, .line = args->b),
                              0, args->b - args->a};
    }
    const int open = (args->b == args->count) && piece->open;
    return (CliPagePiece){0, piece->from + args->a, open ? 0ULL : (piece->from + args->b), open, 0ULL};
}

// the written lines `lines` put in place of the text's lines from `from` to before `to`, the pieces built in a cell
// sized to the text's pieces and the two more a cut adds: 1, or 0 with why given where the cell is refused
EMBED_INLINE int cli_on_replace(const PagesEditCtx *args)
{
    CliPaged *one = args->one;
    const unsigned int most = ((one->pieces + 2u) < CLI_PAGE_PIECES) ? (one->pieces + 2u) : CLI_PAGE_PIECES;
    CliPagePieces pieces = {
        g_cli_cells->persistent_buf_alloc((unsigned long long)most * sizeof(CliPagePiece), &g_cli_editor_cells), most,
        0u};
    if (pieces.piece == NULL)
    {
        mmgr_line_put(args->err_msg, "the pieces a replace retains could not be taken");
        return 0;
    }
    const unsigned long long indexed = EMBED_CALL(cli_pages_one_internal.page_index, CliPagesOneCtx, .one = one);
    unsigned long long at = 0ULL;
    int positioned = 0;
    for (unsigned int index = 0u; index < one->pieces; index += 1u)
    {
        const CliPagePiece *const piece = &one->piece[index];
        const unsigned long long count =
            EMBED_CALL(cli_pages_internal.page_length, CliPagesCtx, .piece = piece, .indexed = indexed);
        const unsigned long long end = at + count;
        if ((end <= args->from) || (at >= args->to))
        {
            if ((at >= args->to) && !positioned)
            {
                EMBED_CALL(cli_piece_append, PagesEditCtx, .pieces = &pieces, .piece = args->lines_page_piece);
                positioned = 1;
            }
            EMBED_CALL(cli_piece_append, PagesEditCtx, .pieces = &pieces, .piece = *piece);
        }
        else
        {
            // the piece's part before `from` stays, then the lines written, then its part from `to`
            if (args->from > at)
            {
                EMBED_CALL(cli_piece_append, PagesEditCtx, .pieces = &pieces,
                           .piece = EMBED_CALL(cli_cut, PagesEditCtx, .one_read = one, .piece_pointer = piece,
                                               .a = 0ULL, .b = args->from - at, .count = count));
            }
            if (!positioned)
            {
                EMBED_CALL(cli_piece_append, PagesEditCtx, .pieces = &pieces, .piece = args->lines_page_piece);
                positioned = 1;
            }
            if (args->to < end)
            {
                EMBED_CALL(cli_piece_append, PagesEditCtx, .pieces = &pieces,
                           .piece = EMBED_CALL(cli_cut, PagesEditCtx, .one_read = one, .piece_pointer = piece,
                                               .a = args->to - at, .b = count, .count = count));
            }
        }
        at = end;
    }
    if (!positioned)
    {
        EMBED_CALL(cli_piece_append, PagesEditCtx, .pieces = &pieces, .piece = args->lines_page_piece);
    }
    memor.cpy(&(MemoriaCfg){.dst = one->piece, .src = pieces.piece, .bytes = pieces.count * sizeof(CliPagePiece)});
    one->pieces = pieces.count;
    g_cli_cells->persistent_buf_release(pieces.piece, &g_cli_editor_cells);
    return 1;
}

// the byte a col counted in UTF-16 units falls at in `length` bytes of `line`, past its end for a col past it
EMBED_INLINE unsigned long long cli_byte_at(const PagesEditCtx *args)
{
    unsigned long long length = args->length;
    unsigned long long units = 0ULL;
    for (unsigned long long at = 0ULL; at < length;)
    {
        if (units >= args->col)
        {
            return at;
        }
        unsigned int letter = 0u;
        at +=
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)args->line, .length = length, .at = at, .letter = &letter);
        units += (letter > 0xffffu) ? 2ULL : 1ULL;
    }
    return length;
}

// `size` bytes of an edit's lines, each ended by "\n", put after the shadow's, the shadow started where it contains
// none: the piece of them, or one of no lines with why given where the shadow could not be written
EMBED_INLINE CliPagePiece cli_write_append(const PagesEditCtx *args)
{
    unsigned long long size = args->size;
    const unsigned long long from = args->one->shadow_used;
    if (!EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = args->one->shadow,
                    .bytes = args->text, .size = size, .fresh = from == 0ULL))
    {
        // a write cut short leaves the shadow its bytes, and the next lines go after them
        (void)EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = args->one->shadow,
                         .size_pointer = &args->one->shadow_used);
        mmgr_line_put(args->err_msg, args->one->shadow);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return (CliPagePiece){1, from, from, 0, 0ULL};
    }
    unsigned long long lines = 0ULL;
    for (const char *at = memor.chr(&(MemoriaCfg){.src = args->text, .val = '\n', .bytes = size}); at != NULL;
         at = memor.chr(
             &(MemoriaCfg){.src = at + 1, .val = '\n', .bytes = size - (unsigned long long)(at + 1 - args->text)}))
    {
        lines += 1ULL;
    }
    args->one->shadow_used = from + size;
    return (CliPagePiece){1, from, args->one->shadow_used, 0, lines};
}

// one edit written over the text: the lines it reaches read into `lines`, the head of the first and the tail of the
// last retained about its text in a cell of their size, and those lines put in their place: 1, or 0 with why given
EMBED_INLINE int cli_edit_write(const PagesEditCtx *args)
{
    CliPaged *one = args->one;
    CliPageLines *lines = args->lines_out;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned long long reach = args->edit->to.line + 1ULL;
    if (!EMBED_CALL(cli_pages_lines_err_msg_internal.page_lines_get, CliPagesLinesErrMsgCtx, .one = one,
                    .from = args->edit->from.line, .to_value = reach, .lines = lines, .err_msg = err_msg))
    {
        return 0;
    }
    if (lines->full)
    {
        mmgr_line_put(err_msg, "the lines an edit reaches are more than a reading of the text contains");
        return 0;
    }
    if ((lines->count == 0ULL) || (lines->count != (reach - args->edit->from.line)))
    {
        mmgr_line_put(err_msg, "an edit reaches past the text, to line ");
        mmgr_line_decimal(err_msg, args->edit->to.line, 0u);
        return 0;
    }
    const char *const first = lines->text + lines->start[0];
    const unsigned long long first_length = lines->length[0];
    const char *const last = lines->text + lines->start[lines->count - 1ULL];
    const unsigned long long last_length = lines->length[lines->count - 1ULL];
    const unsigned long long head =
        EMBED_CALL(cli_byte_at, PagesEditCtx, .line = first, .length = first_length, .col = args->edit->from.col);
    const unsigned long long tail =
        EMBED_CALL(cli_byte_at, PagesEditCtx, .line = last, .length = last_length, .col = args->edit->to.col);
    // the head, the edit's text, the tail, the newline that ends the last line, and the byte a line reserves back for
    // its terminator
    const unsigned long long size = head + args->edit->length + (last_length - tail) + 2ULL;
    if (size > CLI_PAGE_EDIT)
    {
        mmgr_line_put(err_msg, "the lines written over ");
        mmgr_line_put(err_msg, one->key);
        mmgr_line_put(err_msg, " are more than are retained");
        return 0;
    }
    char *const text = g_cli_cells->temporary_buf_alloc(size, &g_cli_editor_cells);
    if (text == NULL)
    {
        mmgr_line_put(err_msg, "an edit's lines as written could not be taken");
        return 0;
    }
    VerbaLinea written = {text, size, 0ULL};
    mmgr_line_put_n(&written, first, head);
    mmgr_line_put_n(&written, args->edit->text, args->edit->length);
    mmgr_line_put_n(&written, last + tail, last_length - tail);
    mmgr_line_ch(&written, '\n');
    if (!mmgr_line_ok(&written))
    {
        g_cli_cells->persistent_buf_release(text, &g_cli_editor_cells);
        mmgr_line_put(err_msg, "an edit's lines as written do not fit the cell sized to them");
        return 0;
    }
    const CliPagePiece put =
        EMBED_CALL(cli_write_append, PagesEditCtx, .one = one, .text = text, .size = written.at, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(text, &g_cli_editor_cells);
    return (put.lines != 0ULL) && EMBED_CALL(cli_on_replace, PagesEditCtx, .one = one, .from = args->edit->from.line,
                                             .to = reach, .lines_page_piece = put, .err_msg = err_msg);
}

int cli_pages_edit(const CliPagesOutErrMsgCfg *args)
{
    CliPaged *const one =
        EMBED_CALL(cli_pages_err_msg_internal.paged_get, CliPagesErrMsgCtx, .key = args->key, .err_msg = args->err_msg);
    CliPageLines lines = {0};
    int written = one != NULL;
    const int done = written;
    for (unsigned int change = 0u; written && (change < args->count_u32); change += 1u)
    {
        unsigned int at = args->edits[change];
        while (written && (at > 0u))
        {
            written = EMBED_CALL(cli_edit_write, PagesEditCtx, .one = one, .edit = &args->changes[change][at - 1u],
                                 .lines_out = &lines, .err_msg = args->err_msg);
            at -= 1u;
        }
    }
    if (written)
    {
        EMBED_CALL(cli_pages_one_internal.page_shape_write, CliPagesOneCtx, .one = one, .out = args->out);
    }
    if (done)
    {
        EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_free, CliPagesLinesLinesFirstCtx, .lines = &lines);
    }
    return written;
}

int cli_pages_change(const CliPagesPathCfg *args)
{
    CliPaged *const one =
        EMBED_CALL(cli_pages_err_msg_internal.paged_get, CliPagesErrMsgCtx, .key = args->key, .err_msg = args->err_msg);
    if (one != NULL)
    {
        const unsigned long long indexed = EMBED_CALL(cli_pages_one_internal.page_index, CliPagesOneCtx, .one = one);
        unsigned long long at = 0ULL;
        unsigned int runs = 0u;
        mmgr_line_ch(args->out, '[');
        for (unsigned int index = 0u; index < one->pieces; index += 1u)
        {
            const unsigned long long count = EMBED_CALL(cli_pages_internal.page_length, CliPagesCtx,
                                                        .piece = &one->piece[index], .indexed = indexed);
            if (one->piece[index].written)
            {
                mmgr_line_put(args->out, (runs != 0u) ? ",[" : "[");
                mmgr_line_decimal(args->out, at, 0u);
                mmgr_line_ch(args->out, ',');
                mmgr_line_decimal(args->out, at + count, 0u);
                mmgr_line_ch(args->out, ']');
                runs += 1u;
            }
            at += count;
        }
        mmgr_line_ch(args->out, ']');
    }
    return one != NULL;
}

// the bytes a block of UTF-16 lines for find takes over the lines read: a unit for each byte at the most, since a
// letter of one to three bytes is one unit and one of four bytes two, and each line's start and length
EMBED_INLINE unsigned long long cli_block_bytes(const PagesEditCtx *args)
{
    const unsigned long long count = (args->lines->count < CLI_PAGE_BLOCK) ? args->lines->count : CLI_PAGE_BLOCK;
    return (args->lines->used * sizeof(unsigned short)) + (2ULL * count * sizeof(unsigned long long));
}

// the lines read made a block of UTF-16 lines for find, in `storage` of cli_block_bytes
EMBED_INLINE CliLines cli_block_get(const PagesEditCtx *args)
{
    const CliPageLines *lines = args->lines;
    const unsigned long long count = (lines->count < CLI_PAGE_BLOCK) ? lines->count : CLI_PAGE_BLOCK;
    unsigned long long *const start = (unsigned long long *)(args->storage + (lines->used * sizeof(unsigned short)));
    CliLines block = {
        (unsigned short *)args->storage, lines->used, 0ULL, start, start + count, (unsigned int)count, 0u, 0};
    for (unsigned long long line = 0ULL; line < count; line += 1ULL)
    {
        const char *const text = lines->text + lines->start[line];
        const unsigned long long size = lines->length[line];
        block.start[block.lines] = block.used;
        for (unsigned long long at = 0ULL; at < size;)
        {
            unsigned int letter = 0u;
            at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                             .text_bytes = (const unsigned char *)text, .length = size, .at = at, .letter = &letter);
            const unsigned long long need = (letter > 0xffffu) ? 2ULL : 1ULL;
            if ((block.used + need) > block.units_max)
            {
                block.full = 1;
                break;
            }
            // each unit of a letter is below 2^16
            block.unit[block.used] =
                (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
            EMBED_CALL(cli_lines_histogram_append, CliLinesHistogramAppendCfg, .lines = &block,
                       .unit = block.unit[block.used]);
            if (need == 2ULL)
            {
                block.unit[block.used + 1ULL] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
                EMBED_CALL(cli_lines_histogram_append, CliLinesHistogramAppendCfg, .lines = &block,
                           .unit = block.unit[block.used + 1ULL]);
            }
            block.used += need;
        }
        block.length[block.lines] = block.used - block.start[block.lines];
        block.lines += 1u;
    }
    return block;
}

// The matches found so far: where they are written, the first line of the block, and how many
typedef struct
{
    VerbaLinea *out;
    unsigned long long from;
    unsigned long long found;
} CliPageFound;

static void cli_page_match(const CliFound3Cfg *args)
{
    CliPageFound *const found = (CliPageFound *)args->context;
    mmgr_line_put(found->out, (found->found != 0ULL) ? ",[" : "[");
    mmgr_line_decimal(found->out, found->from + args->line, 0u);
    mmgr_line_ch(found->out, ',');
    mmgr_line_decimal(found->out, args->from, 0u);
    mmgr_line_ch(found->out, ',');
    mmgr_line_decimal(found->out, args->to, 0u);
    mmgr_line_ch(found->out, ']');
    found->found += 1ULL;
}

// the text's lines in all, as far as the index has read
EMBED_INLINE unsigned long long cli_total(const PagesEditCtx *args)
{
    CliPaged *one = args->one;
    const unsigned long long indexed = EMBED_CALL(cli_pages_one_internal.page_index, CliPagesOneCtx, .one = one);
    unsigned long long total = 0ULL;
    for (unsigned int at = 0u; at < one->pieces; at += 1u)
    {
        total += EMBED_CALL(cli_pages_internal.page_length, CliPagesCtx, .piece = &one->piece[at], .indexed = indexed);
    }
    return total;
}

int cli_pages_find(const CliPagesOutErrMsgNeedleCfg *args)
{
    const CliDocNeedle *needle = args->needle;
    unsigned long long most = args->most;
    VerbaLinea *err_msg = args->err_msg;
    CliPaged *const one =
        EMBED_CALL(cli_pages_err_msg_internal.paged_get, CliPagesErrMsgCtx, .key = args->key, .err_msg = err_msg);
    char *const found_text =
        (one != NULL) ? g_cli_cells->temporary_buf_alloc(CLI_PAGE_FOUND, &g_cli_editor_cells) : NULL;
    if ((one != NULL) && (found_text == NULL))
    {
        mmgr_line_put(err_msg, "the matches find writes could not be taken");
    }
    CliPageLines lines = {0};
    int read = found_text != NULL;
    const int done = read;
    const unsigned long long total = read ? EMBED_CALL(cli_total, PagesEditCtx, .one = one) : 0ULL;
    VerbaLinea written = {found_text, CLI_PAGE_FOUND, 0ULL};
    CliPageFound found = {&written, 0ULL, 0ULL};
    unsigned long long block_max = CLI_PAGE_BLOCK;
    for (unsigned long long from = 0ULL; read && (from < total) && (found.found < most);)
    {
        unsigned long long to = from;
        read = EMBED_CALL(cli_pages_lines_internal.page_block_read, CliPagesLinesCtx, .one = one, .from = from,
                          .total = total, .block_max = &block_max, .to = &to, .lines = &lines, .err_msg = err_msg);
        // a block of more than CLI_PAGE_FIND_TEXT bytes is read again as half as many lines
        if (read && (lines.used > CLI_PAGE_FIND_TEXT) && (block_max > 1ULL))
        {
            block_max /= 2ULL;
            continue;
        }
        // the block's units and lines a cell sized to the lines read, given back as the block is searched; a block
        // whose units the cells cannot meet is read again as half as many lines
        unsigned char *const storage =
            read ? g_cli_cells->temporary_buf_alloc(EMBED_CALL(cli_block_bytes, PagesEditCtx, .lines = &lines),
                                                    &g_cli_editor_cells)
                 : NULL;
        if (read && (storage == NULL))
        {
            if (block_max > 1ULL)
            {
                block_max /= 2ULL;
                continue;
            }
            mmgr_line_put(err_msg, "a block's units for find could not be taken");
            read = 0;
        }
        if (read)
        {
            const CliLines block = EMBED_CALL(cli_block_get, PagesEditCtx, .lines = &lines, .storage = storage);
            found.from = from;
            read =
                EMBED_CALL(cli_document_lines.find_each, CliDocumentLinesCfg, .lines = &block, .needle = needle,
                           .most = most - found.found, .each = cli_page_match, .context = &found, .err_msg = err_msg);
        }
        g_cli_cells->persistent_buf_release(storage, &g_cli_editor_cells);
        from = to;
    }
    if (read)
    {
        mmgr_line_ch(args->out, '[');
        mmgr_line_put_n(args->out, found_text, written.at);
        mmgr_line_ch(args->out, ']');
    }
    if (done)
    {
        EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_free, CliPagesLinesLinesFirstCtx, .lines = &lines);
    }
    g_cli_cells->persistent_buf_release(found_text, &g_cli_editor_cells);
    return read;
}

CliPaged *cli_pages_save_beside_alloc(const CliPagesPathCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    CliPaged *const one =
        EMBED_CALL(cli_pages_err_msg_internal.paged_get, CliPagesErrMsgCtx, .key = args->key, .err_msg = err_msg);
    if (one == NULL)
    {
        return NULL;
    }
    const int done = one->index.done;
    if (!done)
    {
        mmgr_line_put(err_msg, "the file is still being read: it saves once it is read whole");
        return NULL;
    }
    CliPageLines lines = {0};
    // the text goes to the file as the tree writes every file: beside it, then moved over it once whole
    const unsigned long long total = EMBED_CALL(cli_total, PagesEditCtx, .one = one);
    char adjacent[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_files_path_err_msg.saving_adjacent, CliFilesPathErrMsgCfg, .path = one->path, .file = one->path,
                    .adjacent = adjacent, .err_msg = err_msg))
    {
        EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_free, CliPagesLinesLinesFirstCtx, .lines = &lines);
        return NULL;
    }
    // the lines go to the file through a stage of CLI_PAGE_SAVE_STAGE bytes, written whenever the next line would
    // not fit, and a line wider than the stage is written as it lies in the reading
    char *const stage = g_cli_cells->temporary_buf_alloc(CLI_PAGE_SAVE_STAGE, &g_cli_editor_cells);
    int written = (stage != NULL) && EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = adjacent,
                                                .bytes = "", .size = 0ULL, .fresh = 1);
    if (stage == NULL)
    {
        mmgr_line_put(err_msg, "the stage a save writes through could not be taken");
    }
    VerbaLinea block = mmgr_line_over(stage, CLI_PAGE_SAVE_STAGE);
    const unsigned long long eol_size = cellul.len(&(CatenaFinitaCfg){.src = one->eol, .cap = CLI_CELLS_POOL});
    unsigned long long block_max = CLI_PAGE_BLOCK;
    for (unsigned long long from = 0ULL; written && (from < total);)
    {
        unsigned long long to = from;
        written = EMBED_CALL(cli_pages_lines_internal.page_block_read, CliPagesLinesCtx, .one = one, .from = from,
                             .total = total, .block_max = &block_max, .to = &to, .lines = &lines, .err_msg = err_msg);
        for (unsigned long long at = 0ULL; written && (at < lines.count); at += 1ULL)
        {
            const unsigned long long ending = ((from + at) > 0ULL) ? eol_size : 0ULL;
            const char *const line = lines.text + lines.start[at];
            if ((block.at + ending + lines.length[at]) >= block.capacity)
            {
                written =
                    EMBED_CALL(save_stage, PagesEditCtx, .adjacent = adjacent, .block = &block, .err_msg = err_msg);
            }
            if ((ending + lines.length[at]) >= block.capacity)
            {
                written = written &&
                          EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = adjacent,
                                     .bytes = one->eol, .size = ending, .fresh = 0) &&
                          EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = adjacent,
                                     .bytes = line, .size = lines.length[at], .fresh = 0);
            }
            else
            {
                mmgr_line_put_n(&block, one->eol, ending);
                mmgr_line_put_n(&block, line, lines.length[at]);
            }
        }
        from = to;
    }
    written =
        written && EMBED_CALL(save_stage, PagesEditCtx, .adjacent = adjacent, .block = &block, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(stage, &g_cli_editor_cells);
    EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_free, CliPagesLinesLinesFirstCtx, .lines = &lines);
    // the file written beside read as the text it becomes once moved over the file, its index whole
    CliPaged *const read =
        written ? EMBED_CALL(cli_pages_err_msg_internal.page_read_alloc, CliPagesErrMsgCtx, .key = one->key,
                             .path = one->path, .index_path = adjacent, .err_msg = err_msg)
                : NULL;
    if (read == NULL)
    {
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = adjacent);
        if (err_msg->at == 0ULL)
        {
            mmgr_line_put(err_msg, adjacent);
            mmgr_line_put(err_msg, ": it could not be written");
        }
    }
    return read;
}

int cli_pages_save_place(const CliPagesPlaceCfg *args)
{
    CliPaged *const read = args->read;
    char adjacent[CLI_PATH_MAX_LEN];
    const int beside = EMBED_CALL(cli_files_path_err_msg.saving_adjacent, CliFilesPathErrMsgCfg, .path = read->path,
                                  .file = read->path, .adjacent = adjacent, .err_msg = args->err_msg);
    if (beside && !args->whole)
    {
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = adjacent);
    }
    const int placed = beside && args->whole &&
                       EMBED_CALL(cli_files_path.save, CliFilesPathCfg, .adjacent_read = adjacent, .path = read->path,
                                  .file = read->path, .err_msg = args->err_msg);
    if (placed)
    {
        EMBED_CALL(cli_pages_one_internal.page_store, CliPagesOneCtx, .one = read);
    }
    else
    {
        EMBED_CALL(cli_pages_one_internal.page_free, CliPagesOneCtx, .one = read);
    }
    return placed || (beside && !args->whole);
}

void cli_pages_read_free(const CliPagesPlaceCfg *args)
{
    EMBED_CALL(cli_pages_one_internal.page_free, CliPagesOneCtx, .one = args->read);
}
