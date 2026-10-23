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

// The pieces a replace keeps as it builds them: a cell of `most`, the text's pieces and the two a cut piece's parts
// about the lines written add
typedef struct
{
    CliPagePiece *piece;
    unsigned int most;
    unsigned int count;
} CliPageKept;

// a piece put after the pieces kept, one of written lines left out where it holds none
static void cli_kept_put(CliPageKept *kept, CliPagePiece piece)
{
    if ((piece.written && (piece.lines == 0ull)) || (kept->count >= kept->most))
    {
        return;
    }
    kept->piece[kept->count] = piece;
    kept->count += 1u;
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
// sized to the text's pieces and the two more a cut adds: 1, or 0 with why said where the cell is refused
static int cli_replaced(CliPaged *one, unsigned long long from, unsigned long long to, CliPagePiece lines,
                        CliLine *said)
{
    const unsigned int most = ((one->pieces + 2u) < CLI_PAGE_PIECES) ? (one->pieces + 2u) : CLI_PAGE_PIECES;
    CliPageKept kept = {cli_cell_taken((unsigned long long)most * sizeof(CliPagePiece), "the pieces a replace keeps"),
                        most, 0u};
    if (kept.piece == NULL)
    {
        cli_line_text(said, "the pieces a replace keeps could not be taken");
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
                cli_kept_put(&kept, lines);
                placed = 1;
            }
            cli_kept_put(&kept, *piece);
        }
        else
        {
            // the piece's part before `from` stays, then the lines written, then its part from `to`
            if (from > at)
            {
                cli_kept_put(&kept, cli_cut(one, piece, 0ull, from - at, count));
            }
            if (!placed)
            {
                cli_kept_put(&kept, lines);
                placed = 1;
            }
            if (to < end)
            {
                cli_kept_put(&kept, cli_cut(one, piece, to - at, count, count));
            }
        }
        at = end;
    }
    if (!placed)
    {
        cli_kept_put(&kept, lines);
    }
    memor.cpy(&(MemoriaCfg){.dst = one->piece, .src = kept.piece, .bytes = kept.count * sizeof(CliPagePiece)});
    one->pieces = kept.count;
    cli_cell_given(kept.piece);
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

// `size` bytes of an edit's lines, each ended by "\n", put after the shadow's, the shadow started where it holds none:
// the piece of them, or one of no lines with why said where the shadow could not be written
static CliPagePiece cli_written_put(CliPaged *one, const char *text, unsigned long long size, CliLine *said)
{
    const unsigned long long from = one->shadow_used;
    if (!cli_file_appended(one->shadow, text, size, from == 0ull))
    {
        // a write cut short leaves the shadow its bytes, so the next lines go after them
        (void)cli_file_size(one->shadow, &one->shadow_used);
        cli_line_text(said, one->shadow);
        cli_line_text(said, ": it could not be written");
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
// last kept about its text in a cell of their size, and those lines put in their place: 1, or 0 with why said
static int cli_edit_written(CliPaged *one, const CliEdit *edit, CliPageLines *lines, CliLine *said)
{
    const unsigned long long reach = edit->to.line + 1ull;
    if (!cli_page_lines_of(one, edit->from.line, reach, lines, said))
    {
        return 0;
    }
    if ((lines->count == 0ull) || (lines->count != (reach - edit->from.line)))
    {
        cli_line_text(said, "an edit reaches past the text, to line ");
        cli_line_decimal(said, edit->to.line, 0u);
        return 0;
    }
    const char *const first = lines->text + lines->start[0];
    const unsigned long long first_length = lines->length[0];
    const char *const last = lines->text + lines->start[lines->count - 1ull];
    const unsigned long long last_length = lines->length[lines->count - 1ull];
    const unsigned long long head = cli_byte_at(first, first_length, edit->from.col);
    const unsigned long long tail = cli_byte_at(last, last_length, edit->to.col);
    // the head, the edit's text, the tail, the newline that ends the last line, and the byte a line holds back for
    // its terminator
    const unsigned long long size = head + edit->length + (last_length - tail) + 2ull;
    if (size > CLI_PAGE_EDIT)
    {
        cli_line_text(said, "the lines written over ");
        cli_line_text(said, one->key);
        cli_line_text(said, " are more than are kept");
        return 0;
    }
    char *const text = cli_cell_taken(size, "an edit's lines as written");
    if (text == NULL)
    {
        cli_line_text(said, "an edit's lines as written could not be taken");
        return 0;
    }
    CliLine written = {text, size, 0ull};
    cli_line_counted(&written, first, head);
    cli_line_counted(&written, edit->text, edit->length);
    cli_line_counted(&written, last + tail, last_length - tail);
    cli_line_character(&written, '\n');
    if (!cli_line_fits(&written))
    {
        cli_cell_given(text);
        cli_line_text(said, "an edit's lines as written do not fit the cell sized to them");
        return 0;
    }
    const CliPagePiece put = cli_written_put(one, text, written.at, said);
    cli_cell_given(text);
    return (put.lines != 0ull) && cli_replaced(one, edit->from.line, reach, put, said);
}

int cli_pages_edit(const char *key, const CliEdit *const *changes, const unsigned int *edits, unsigned int count,
                   CliLine *out, CliLine *said)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, said);
    CliPageLines lines;
    int written = (one != NULL) && cli_page_lines_held(&lines, 2ull, CLI_PAGE_LINES_FIRST, said);
    const int held = written;
    for (unsigned int change = 0u; written && (change < count); change += 1u)
    {
        for (unsigned int at = edits[change]; written && (at > 0u); at -= 1u)
        {
            written = cli_edit_written(one, &changes[change][at - 1u], &lines, said);
        }
    }
    if (written)
    {
        cli_page_shape_written(one, out);
    }
    if (held)
    {
        cli_page_lines_given(&lines);
    }
    cli_lock_give(&g_cli_pages_lock);
    return written;
}

int cli_pages_changed(const char *key, CliLine *out, CliLine *said)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, said);
    if (one != NULL)
    {
        const unsigned long long indexed = cli_page_indexed(one);
        unsigned long long at = 0ull;
        unsigned int runs = 0u;
        cli_line_character(out, '[');
        for (unsigned int index = 0u; index < one->pieces; index += 1u)
        {
            const unsigned long long count = cli_page_length(&one->piece[index], indexed);
            if (one->piece[index].written)
            {
                cli_line_text(out, (runs != 0u) ? ",[" : "[");
                cli_line_decimal(out, at, 0u);
                cli_line_character(out, ',');
                cli_line_decimal(out, at + count, 0u);
                cli_line_character(out, ']');
                runs += 1u;
            }
            at += count;
        }
        cli_line_character(out, ']');
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
    CliLine *out;
    unsigned long long from;
    unsigned long long found;
} CliPageFound;

static void cli_page_match(void *context, unsigned long long line, unsigned long long from, unsigned long long to)
{
    CliPageFound *const found = (CliPageFound *)context;
    cli_line_text(found->out, (found->found != 0ull) ? ",[" : "[");
    cli_line_decimal(found->out, found->from + line, 0u);
    cli_line_character(found->out, ',');
    cli_line_decimal(found->out, from, 0u);
    cli_line_character(found->out, ',');
    cli_line_decimal(found->out, to, 0u);
    cli_line_character(found->out, ']');
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

int cli_pages_find(const char *key, const CliDocSought *sought, unsigned long long most, CliLine *out, CliLine *said)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, said);
    char *const found_text = (one != NULL) ? cli_cell_taken(CLI_PAGE_FOUND, "the matches find writes") : NULL;
    if ((one != NULL) && (found_text == NULL))
    {
        cli_line_text(said, "the matches find writes could not be taken");
    }
    CliPageLines lines;
    int read = (found_text != NULL) && cli_page_lines_held(&lines, CLI_PAGE_BLOCK, CLI_PAGE_LINES_FIRST, said);
    const int held = read;
    const unsigned long long total = read ? cli_total(one) : 0ull;
    CliLine written = {found_text, CLI_PAGE_FOUND, 0ull};
    CliPageFound found = {&written, 0ull, 0ull};
    for (unsigned long long from = 0ull; read && (from < total) && (found.found < most);)
    {
        const unsigned long long to = ((from + CLI_PAGE_BLOCK) < total) ? (from + CLI_PAGE_BLOCK) : total;
        read = cli_page_lines_of(one, from, to, &lines, said);
        // the block's units and lines a cell sized to the lines read, given back as the block is searched
        unsigned char *const storage =
            read ? cli_cell_taken(cli_block_bytes(&lines), "a block's units for find") : NULL;
        if (read && (storage == NULL))
        {
            cli_line_text(said, "a block's units for find could not be taken");
            read = 0;
        }
        if (read)
        {
            const CliLines block = cli_block_of(&lines, storage);
            found.from = from;
            read = cli_document_find_each(&block, sought, most - found.found, cli_page_match, &found, said);
        }
        cli_cell_given(storage);
        from = to;
    }
    if (read)
    {
        cli_line_character(out, '[');
        cli_line_counted(out, found_text, written.at);
        cli_line_character(out, ']');
    }
    if (held)
    {
        cli_page_lines_given(&lines);
    }
    cli_cell_given(found_text);
    cli_lock_give(&g_cli_pages_lock);
    return read;
}

int cli_pages_save(const char *key, CliLine *out, CliLine *said)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, said);
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
        cli_line_text(said, "the file is still being read: it saves once it is read whole");
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    CliPageLines lines;
    if (!cli_page_lines_held(&lines, CLI_PAGE_BLOCK, CLI_PAGE_LINES_FIRST, said))
    {
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    // the text goes to the file as the tree writes every file: beside it, then moved over it once whole
    const unsigned long long total = cli_total(one);
    char beside[CLI_PATH_LONGEST];
    if (!cli_files_saving_beside(one->path, one->path, beside, said))
    {
        cli_page_lines_given(&lines);
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    int written = cli_file_appended(beside, "", 0ull, 1);
    for (unsigned long long from = 0ull; written && (from < total);)
    {
        const unsigned long long to = ((from + CLI_PAGE_BLOCK) < total) ? (from + CLI_PAGE_BLOCK) : total;
        written = cli_page_lines_of(one, from, to, &lines, said);
        // the block's text a cell sized to the lines read and an end of line, two bytes at the most, for each
        const unsigned long long size = lines.used + (2ull * lines.count) + 1ull;
        char *const text = written ? cli_cell_taken(size, "a block of lines saved") : NULL;
        if (written && (text == NULL))
        {
            cli_line_text(said, "a block of lines saved could not be taken");
            written = 0;
        }
        CliLine block = {text, size, 0ull};
        for (unsigned long long at = 0ull; written && (at < lines.count); at += 1ull)
        {
            if ((from + at) > 0ull)
            {
                cli_line_text(&block, one->eol);
            }
            cli_line_counted(&block, lines.text + lines.start[at], lines.length[at]);
        }
        written = written && cli_line_fits(&block) && cli_file_appended(beside, text, block.at, 0);
        cli_cell_given(text);
        from = to;
    }
    cli_page_lines_given(&lines);
    if (!written)
    {
        query_removed(beside);
        if (said->at == 0ull)
        {
            cli_line_text(said, beside);
            cli_line_text(said, ": it could not be written");
        }
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    if (!cli_files_saved(beside, one->path, one->path, said))
    {
        cli_lock_give(&g_cli_pages_lock);
        return 0;
    }
    char path[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = path,
                            .src = one->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->path, .cap = CLI_CELLS_POOL}) + 1u});
    const int opened = cli_page_open_reading(key, path, 0, out, said);
    cli_lock_give(&g_cli_pages_lock);
    return opened;
}
