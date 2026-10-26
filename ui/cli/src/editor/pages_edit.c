// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_edit.c: a paged text's edits written over its pieces, its lines written since the file was read, its matches,
// and the text saved to its file
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "files.h"
#include "pages_internal.h"

// the most bytes of an edit's lines, the most bytes of the matches find writes, and the bytes a reading's cell begins
// with for an edit, find and a save, which grows to the lines read
#define CLI_PAGE_EDIT (1u << 25u)
#define CLI_PAGE_FOUND (1u << 24u)
#define CLI_PAGE_LINES_FIRST (64ull * 1024ull)

// The pieces a replace retains as it builds them: a cell of `most`, the text's pieces and the two a cut piece's parts
// about the lines written add
typedef struct
{
    CliPagePiece *piece;
    unsigned int most;
    unsigned int count;
} CliPagePieces;

// a piece put after the pieces retained, one of written lines left out where it contains none
static void cli_piece_put(CliPagePieces *pieces, CliPagePiece piece)
{
    if ((piece.written && (piece.lines == 0ull)) || (pieces->count >= pieces->most))
    {
        return;
    }
    pieces->piece[pieces->count] = piece;
    pieces->count += 1u;
}

// the part of a piece of `one` from `a` to before `b` of its `count` lines
static CliPagePiece cli_cut(const CliPaged *one, const CliPagePiece *piece, unsigned long long a, unsigned long long b,
                            unsigned long long count)
{
    if (piece->written)
    {
        return (CliPagePiece){1, cli_page_written_at(one, piece, a), cli_page_written_at(one, piece, b), 0, b - a};
    }
    const int open = (b == count) && piece->open;
    return (CliPagePiece){0, piece->from + a, open ? 0ull : (piece->from + b), open, 0ull};
}

// the written lines `lines` put in place of the text's lines from `from` to before `to`, the pieces built in a cell
// sized to the text's pieces and the two more a cut adds: 1, or 0 with why given where the cell is refused
static int cli_replaced(CliPaged *one, unsigned long long from, unsigned long long to, CliPagePiece lines,
                        VerbaLinea *err_msg)
{
    const unsigned int most = ((one->pieces + 2u) < CLI_PAGE_PIECES) ? (one->pieces + 2u) : CLI_PAGE_PIECES;
    CliPagePieces pieces = {g_cli_cells->persistent_buf_alloc((unsigned long long)most * sizeof(CliPagePiece), NULL),
                            most, 0u};
    if (pieces.piece == NULL)
    {
        mmgr_line_put(err_msg, "the pieces a replace retains could not be taken");
        return 0;
    }
    const unsigned long long indexed = cli_page_indexed(one);
    unsigned long long at = 0ull;
    int placed = 0;
    for (unsigned int index = 0u; index < one->pieces; index += 1u)
    {
        const CliPagePiece *const piece = &one->piece[index];
        const unsigned long long count = cli_page_length(piece, indexed);
        const unsigned long long end = at + count;
        if ((end <= from) || (at >= to))
        {
            if ((at >= to) && !placed)
            {
                cli_piece_put(&pieces, lines);
                placed = 1;
            }
            cli_piece_put(&pieces, *piece);
        }
        else
        {
            // the piece's part before `from` stays, then the lines written, then its part from `to`
            if (from > at)
            {
                cli_piece_put(&pieces, cli_cut(one, piece, 0ull, from - at, count));
            }
            if (!placed)
            {
                cli_piece_put(&pieces, lines);
                placed = 1;
            }
            if (to < end)
            {
                cli_piece_put(&pieces, cli_cut(one, piece, to - at, count, count));
            }
        }
        at = end;
    }
    if (!placed)
    {
        cli_piece_put(&pieces, lines);
    }
    memor.cpy(&(MemoriaCfg){.dst = one->piece, .src = pieces.piece, .bytes = pieces.count * sizeof(CliPagePiece)});
    one->pieces = pieces.count;
    g_cli_cells->persistent_buf_release(pieces.piece, NULL);
    return 1;
}

// the byte a col counted in UTF-16 units falls at in `length` bytes of `line`, past its end for a col past it
static unsigned long long cli_byte_at(const char *line, unsigned long long length, unsigned long long col)
{
    unsigned long long units = 0ull;
    for (unsigned long long at = 0ull; at < length;)
    {
        if (units >= col)
        {
            return at;
        }
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)line, length, at, &letter);
        units += (letter > 0xffffu) ? 2ull : 1ull;
    }
    return length;
}

// `size` bytes of an edit's lines, each ended by "\n", put after the shadow's, the shadow started where it contains
// none: the piece of them, or one of no lines with why given where the shadow could not be written
static CliPagePiece cli_written_put(CliPaged *one, const char *text, unsigned long long size, VerbaLinea *err_msg)
{
    const unsigned long long from = one->shadow_used;
    if (!cli_file_appended(one->shadow, text, size, from == 0ull))
    {
        // a write cut short leaves the shadow its bytes, so the next lines go after them
        (void)cli_file_size(one->shadow, &one->shadow_used);
        mmgr_line_put(err_msg, one->shadow);
        mmgr_line_put(err_msg, ": it could not be written");
        return (CliPagePiece){1, from, from, 0, 0ull};
    }
    unsigned long long lines = 0ull;
    for (const char *at = memor.chr(&(MemoriaCfg){.src = text, .val = '\n', .bytes = size}); at != NULL;
         at = memor.chr(&(MemoriaCfg){.src = at + 1, .val = '\n', .bytes = size - (unsigned long long)(at + 1 - text)}))
    {
        lines += 1ull;
    }
    one->shadow_used = from + size;
    return (CliPagePiece){1, from, one->shadow_used, 0, lines};
}

// one edit written over the text: the lines it reaches read into `lines`, the head of the first and the tail of the
// last retained about its text in a cell of their size, and those lines put in their place: 1, or 0 with why given
static int cli_edit_written(CliPaged *one, const CliEdit *edit, CliPageLines *lines, VerbaLinea *err_msg)
{
    const unsigned long long reach = edit->to.line + 1ull;
    if (!cli_page_lines_of(one, edit->from.line, reach, lines, err_msg))
    {
        return 0;
    }
    if (lines->full)
    {
        mmgr_line_put(err_msg, "the lines an edit reaches are more than a reading of the text contains");
        return 0;
    }
    if ((lines->count == 0ull) || (lines->count != (reach - edit->from.line)))
    {
        mmgr_line_put(err_msg, "an edit reaches past the text, to line ");
        mmgr_line_decimal(err_msg, edit->to.line, 0u);
        return 0;
    }
    const char *const first = lines->text + lines->start[0];
    const unsigned long long first_length = lines->length[0];
    const char *const last = lines->text + lines->start[lines->count - 1ull];
    const unsigned long long last_length = lines->length[lines->count - 1ull];
    const unsigned long long head = cli_byte_at(first, first_length, edit->from.col);
    const unsigned long long tail = cli_byte_at(last, last_length, edit->to.col);
    // the head, the edit's text, the tail, the newline that ends the last line, and the byte a line reserves back for
    // its terminator
    const unsigned long long size = head + edit->length + (last_length - tail) + 2ull;
    if (size > CLI_PAGE_EDIT)
    {
        mmgr_line_put(err_msg, "the lines written over ");
        mmgr_line_put(err_msg, one->key);
        mmgr_line_put(err_msg, " are more than are retained");
        return 0;
    }
    char *const text = g_cli_cells->persistent_buf_alloc(size, NULL);
    if (text == NULL)
    {
        mmgr_line_put(err_msg, "an edit's lines as written could not be taken");
        return 0;
    }
    VerbaLinea written = {text, size, 0ull};
    mmgr_line_put_n(&written, first, head);
    mmgr_line_put_n(&written, edit->text, edit->length);
    mmgr_line_put_n(&written, last + tail, last_length - tail);
    mmgr_line_ch(&written, '\n');
    if (!mmgr_line_ok(&written))
    {
        g_cli_cells->persistent_buf_release(text, NULL);
        mmgr_line_put(err_msg, "an edit's lines as written do not fit the cell sized to them");
        return 0;
    }
    const CliPagePiece put = cli_written_put(one, text, written.at, err_msg);
    g_cli_cells->persistent_buf_release(text, NULL);
    return (put.lines != 0ull) && cli_replaced(one, edit->from.line, reach, put, err_msg);
}

int cli_pages_edit(const char *key, const CliEdit *const *changes, const unsigned int *edits, unsigned int count,
                   VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, err_msg);
    CliPageLines lines;
    int written = (one != NULL) && cli_page_lines_reserved(&lines, 2ull, CLI_PAGE_LINES_FIRST, err_msg);
    const int done = written;
    for (unsigned int change = 0u; written && (change < count); change += 1u)
    {
        for (unsigned int at = edits[change]; written && (at > 0u); at -= 1u)
        {
            written = cli_edit_written(one, &changes[change][at - 1u], &lines, err_msg);
        }
    }
    if (written)
    {
        cli_page_shape_written(one, out);
    }
    if (done)
    {
        cli_page_lines_given(&lines);
    }
    cli_lock_give(&g_cli_pages_lock);
    return written;
}

int cli_pages_changed(const char *key, VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, err_msg);
    if (one != NULL)
    {
        const unsigned long long indexed = cli_page_indexed(one);
        unsigned long long at = 0ull;
        unsigned int runs = 0u;
        mmgr_line_ch(out, '[');
        for (unsigned int index = 0u; index < one->pieces; index += 1u)
        {
            const unsigned long long count = cli_page_length(&one->piece[index], indexed);
            if (one->piece[index].written)
            {
                mmgr_line_put(out, (runs != 0u) ? ",[" : "[");
                mmgr_line_decimal(out, at, 0u);
                mmgr_line_ch(out, ',');
                mmgr_line_decimal(out, at + count, 0u);
                mmgr_line_ch(out, ']');
                runs += 1u;
            }
            at += count;
        }
        mmgr_line_ch(out, ']');
    }
    cli_lock_give(&g_cli_pages_lock);
    return one != NULL;
}

// the bytes a block of UTF-16 lines for find takes over the lines read: a unit for each byte at the most, since a
// letter of one to three bytes is one unit and one of four bytes two, and each line's start and length
static unsigned long long cli_block_bytes(const CliPageLines *lines)
{
    const unsigned long long count = (lines->count < CLI_PAGE_BLOCK) ? lines->count : CLI_PAGE_BLOCK;
    return (lines->used * sizeof(unsigned short)) + (2ull * count * sizeof(unsigned long long));
}

// the lines read made a block of UTF-16 lines for find, in `storage` of cli_block_bytes
static CliLines cli_block_of(const CliPageLines *lines, unsigned char *storage)
{
    const unsigned long long count = (lines->count < CLI_PAGE_BLOCK) ? lines->count : CLI_PAGE_BLOCK;
    unsigned long long *const start = (unsigned long long *)(storage + (lines->used * sizeof(unsigned short)));
    CliLines block = {(unsigned short *)storage, lines->used, 0ull, start, start + count, (unsigned int)count, 0u, 0};
    for (unsigned long long line = 0ull; line < count; line += 1ull)
    {
        const char *const text = lines->text + lines->start[line];
        const unsigned long long size = lines->length[line];
        block.start[block.lines] = block.used;
        for (unsigned long long at = 0ull; at < size;)
        {
            unsigned int letter = 0u;
            at += cli_lossy_letter((const unsigned char *)text, size, at, &letter);
            const unsigned long long need = (letter > 0xffffu) ? 2ull : 1ull;
            if ((block.used + need) > block.units_most)
            {
                block.full = 1;
                break;
            }
            // each unit of a letter is below 2^16
            block.unit[block.used] =
                (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
            cli_lines_census_put(&block, block.unit[block.used]);
            block.unit[block.used + 1ull] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
            if (need == 2ull)
            {
                cli_lines_census_put(&block, block.unit[block.used + 1ull]);
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

static void cli_page_match(void *context, unsigned long long line, unsigned long long from, unsigned long long to)
{
    CliPageFound *const found = (CliPageFound *)context;
    mmgr_line_put(found->out, (found->found != 0ull) ? ",[" : "[");
    mmgr_line_decimal(found->out, found->from + line, 0u);
    mmgr_line_ch(found->out, ',');
    mmgr_line_decimal(found->out, from, 0u);
    mmgr_line_ch(found->out, ',');
    mmgr_line_decimal(found->out, to, 0u);
    mmgr_line_ch(found->out, ']');
    found->found += 1ull;
}

// the text's lines in all, as far as the index has read
static unsigned long long cli_total(CliPaged *one)
{
    const unsigned long long indexed = cli_page_indexed(one);
    unsigned long long total = 0ull;
    for (unsigned int at = 0u; at < one->pieces; at += 1u)
    {
        total += cli_page_length(&one->piece[at], indexed);
    }
    return total;
}

int cli_pages_find(const char *key, const CliDocSought *sought, unsigned long long most, VerbaLinea *out,
                   VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, err_msg);
    char *const found_text = (one != NULL) ? g_cli_cells->persistent_buf_alloc(CLI_PAGE_FOUND, NULL) : NULL;
    if ((one != NULL) && (found_text == NULL))
    {
        mmgr_line_put(err_msg, "the matches find writes could not be taken");
    }
    CliPageLines lines;
    int read = (found_text != NULL) && cli_page_lines_reserved(&lines, CLI_PAGE_BLOCK, CLI_PAGE_LINES_FIRST, err_msg);
    const int done = read;
    const unsigned long long total = read ? cli_total(one) : 0ull;
    VerbaLinea written = {found_text, CLI_PAGE_FOUND, 0ull};
    CliPageFound found = {&written, 0ull, 0ull};
    unsigned long long block_most = CLI_PAGE_BLOCK;
    for (unsigned long long from = 0ull; read && (from < total) && (found.found < most);)
    {
        unsigned long long to = from;
        read = cli_page_block_read(one, from, total, &block_most, &to, &lines, err_msg);
        // a block of more than CLI_PAGE_FIND_TEXT bytes is read again as half as many lines
        if (read && (lines.used > CLI_PAGE_FIND_TEXT) && (block_most > 1ull))
        {
            block_most /= 2ull;
            continue;
        }
        // the block's units and lines a cell sized to the lines read, given back as the block is searched; a block
        // whose units the cells cannot meet is read again as half as many lines
        unsigned char *const storage = read ? g_cli_cells->persistent_buf_alloc(cli_block_bytes(&lines), NULL) : NULL;
        if (read && (storage == NULL) && (block_most > 1ull))
        {
            block_most /= 2ull;
            continue;
        }
        if (read && (storage == NULL))
        {
            mmgr_line_put(err_msg, "a block's units for find could not be taken");
            read = 0;
        }
        if (read)
        {
            const CliLines block = cli_block_of(&lines, storage);
            found.from = from;
            read = cli_document_find_each(&block, sought, most - found.found, cli_page_match, &found, err_msg);
        }
        g_cli_cells->persistent_buf_release(storage, NULL);
        from = to;
    }
    if (read)
    {
        mmgr_line_ch(out, '[');
        mmgr_line_put_n(out, found_text, written.at);
        mmgr_line_ch(out, ']');
    }
    if (done)
    {
        cli_page_lines_given(&lines);
    }
    g_cli_cells->persistent_buf_release(found_text, NULL);
    cli_lock_give(&g_cli_pages_lock);
    return read;
}

int cli_pages_save(const char *key, VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, err_msg);
    if (one == NULL)
    {
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    cli_lock_take(&g_cli_index_lock);
    const int done = one->index.done;
    cli_lock_give(&g_cli_index_lock);
    if (!done)
    {
        mmgr_line_put(err_msg, "the file is still being read: it saves once it is read whole");
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    CliPageLines lines;
    if (!cli_page_lines_reserved(&lines, CLI_PAGE_BLOCK, CLI_PAGE_LINES_FIRST, err_msg))
    {
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    // the text goes to the file as the tree writes every file: beside it, then moved over it once whole
    const unsigned long long total = cli_total(one);
    char beside[CLI_PATH_MAX_LEN];
    if (!cli_files_saving_beside(one->path, one->path, beside, err_msg))
    {
        cli_page_lines_given(&lines);
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    int written = cli_file_appended(beside, "", 0ull, 1);
    unsigned long long block_most = CLI_PAGE_BLOCK;
    for (unsigned long long from = 0ull; written && (from < total);)
    {
        unsigned long long to = from;
        written = cli_page_block_read(one, from, total, &block_most, &to, &lines, err_msg);
        // the block's text a cell sized to the lines read and an end of line, two bytes at the most, for each
        const unsigned long long size = lines.used + (2ull * lines.count) + 1ull;
        char *const text = written ? g_cli_cells->persistent_buf_alloc(size, NULL) : NULL;
        if (written && (text == NULL))
        {
            mmgr_line_put(err_msg, "a block of lines saved could not be taken");
            written = 0;
        }
        VerbaLinea block = {text, size, 0ull};
        for (unsigned long long at = 0ull; written && (at < lines.count); at += 1ull)
        {
            if ((from + at) > 0ull)
            {
                mmgr_line_put(&block, one->eol);
            }
            mmgr_line_put_n(&block, lines.text + lines.start[at], lines.length[at]);
        }
        written = written && mmgr_line_ok(&block) && cli_file_appended(beside, text, block.at, 0);
        g_cli_cells->persistent_buf_release(text, NULL);
        from = to;
    }
    cli_page_lines_given(&lines);
    if (!written)
    {
        query_removed(beside);
        if (err_msg->at == 0ull)
        {
            mmgr_line_put(err_msg, beside);
            mmgr_line_put(err_msg, ": it could not be written");
        }
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    if (!cli_files_saved(beside, one->path, one->path, err_msg))
    {
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = path,
                            .src = one->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->path, .cap = CLI_CELLS_POOL}) + 1u});
    const int opened = cli_page_open_reading(key, path, 0, out, err_msg);
    cli_lock_give(&g_cli_pages_lock);
    return opened;
}
