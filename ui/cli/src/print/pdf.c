// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pdf.c: the pages laid out as a PDF of orior's own writing: PDF 1.4, its text in the Courier faces every PDF reader
// contains, regular, bold, oblique and bold oblique, encoded as WinAnsi, a letter that has no place there written as a
// question mark. Only the pages the layout names are written
#include "../text/regexp.h"
#include "print.h"

#include <math.h>

// the bytes a page's contents are taken with
#define CLI_PDF_CONTENT_CAPACITY (4ull << 20u)

// the color of the line numbers
static const unsigned char s_number_color[3] = {0x88u, 0x88u, 0x88u};

// The letters past Latin-1's printing ones WinAnsi writes from 0x80 to 0x9F, each its byte
static const unsigned int s_win_ansi[27][2] = {
    {0x20acu, 0x80u}, {0x201au, 0x82u}, {0x0192u, 0x83u}, {0x201eu, 0x84u}, {0x2026u, 0x85u}, {0x2020u, 0x86u},
    {0x2021u, 0x87u}, {0x02c6u, 0x88u}, {0x2030u, 0x89u}, {0x0160u, 0x8au}, {0x2039u, 0x8bu}, {0x0152u, 0x8cu},
    {0x017du, 0x8eu}, {0x2018u, 0x91u}, {0x2019u, 0x92u}, {0x201cu, 0x93u}, {0x201du, 0x94u}, {0x2022u, 0x95u},
    {0x2013u, 0x96u}, {0x2014u, 0x97u}, {0x02dcu, 0x98u}, {0x2122u, 0x99u}, {0x0161u, 0x9au}, {0x203au, 0x9bu},
    {0x0153u, 0x9cu}, {0x017eu, 0x9eu}, {0x0178u, 0x9fu}};

typedef struct
{
    unsigned int letter;
    VerbaLinea *out;
    const char *text;
    unsigned long long length;
    unsigned int value;
    float value_f32;
    unsigned int positions;
    const unsigned char *rgb;
    const char *face;
    float x;
    float baseline;
    const CliPrintLaid *laid;
    unsigned int page;
    float height;
    int gray;
    unsigned long long *offsets;
    unsigned int number;
} PdfCtx;
// A letter as WinAnsi writes it, '?' where it has no place there
EMBED_INLINE unsigned char cli_win_ansi(const PdfCtx *args)
{
    if (((args->letter >= 0x20u) && (args->letter <= 0x7eu)) || ((args->letter >= 0xa0u) && (args->letter <= 0xffu)))
    {
        // a letter of Latin-1 is its one byte
        return (unsigned char)args->letter;
    }
    for (unsigned int at = 0u; at < 27u; at += 1u)
    {
        if (s_win_ansi[at][0] == args->letter)
        {
            // each of the table's bytes is below 0x100
            return (unsigned char)s_win_ansi[at][1];
        }
    }
    return '?';
}

// `length` bytes of `text` as a PDF string of WinAnsi bytes, its brackets and backslashes escaped
EMBED_INLINE void cli_string(const PdfCtx *args)
{
    const char *text = args->text;
    static unsigned int s_letters[CLI_PATH_MAX_LEN * 4u];
    mmgr_line_ch(args->out, '(');
    unsigned long long done = 0ull;
    while (done < args->length)
    {
        // the text read a piece at a time, each piece cut where a letter starts
        unsigned long long piece = args->length - done;
        piece = (piece < CLI_PATH_MAX_LEN) ? piece : CLI_PATH_MAX_LEN;
        while (((done + piece) < args->length) && ((((unsigned char)text[done + piece]) & 0xc0u) == 0x80u))
        {
            piece -= 1ull;
        }
        const unsigned long long count =
            EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text + done, .size = piece,
                       .letters = s_letters, .capacity_u64 = CLI_PATH_MAX_LEN * 4u);
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            const unsigned char byte = EMBED_CALL(cli_win_ansi, PdfCtx, .letter = s_letters[at]);
            if ((byte == '(') || (byte == ')') || (byte == '\\'))
            {
                mmgr_line_ch(args->out, '\\');
            }
            // a byte of WinAnsi written as the char it is
            mmgr_line_ch(args->out, (char)byte);
        }
        done += piece;
    }
    mmgr_line_ch(args->out, ')');
}

// a hex digit in upper case
EMBED_INLINE char cli_upper_hex(const PdfCtx *args)
{
    static const char s_digits[17] = "0123456789ABCDEF";
    return s_digits[args->value & 0x0fu];
}

// `text` as a PDF text string in UTF-16, for the document's title
EMBED_INLINE void cli_text_string(const PdfCtx *args)
{
    const char *text = args->text;
    static unsigned int s_letters[CLI_PATH_MAX_LEN];
    mmgr_line_put(args->out, "<FEFF");
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), .letters = s_letters,
                   .capacity_u64 = CLI_PATH_MAX_LEN);
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned int letter = s_letters[at];
        unsigned int units[2] = {letter, 0u};
        unsigned int unit_count = 1u;
        if (letter >= 0x10000u)
        {
            units[0] = 0xd800u + ((letter - 0x10000u) >> 10u);
            units[1] = 0xdc00u + ((letter - 0x10000u) & 0x3ffu);
            unit_count = 2u;
        }
        for (unsigned int unit = 0u; unit < unit_count; unit += 1u)
        {
            for (unsigned int shift = 16u; shift > 0u; shift -= 4u)
            {
                mmgr_line_ch(args->out, EMBED_CALL(cli_upper_hex, PdfCtx, .value = units[unit] >> (shift - 4u)));
            }
        }
    }
    mmgr_line_ch(args->out, '>');
}

// a number written with `positions` digits after the point, rounded
EMBED_INLINE void cli_fix(const PdfCtx *args)
{
    const double scale = (args->positions == 3u) ? 1000.0 : 100.0;
    const long long whole = llround((double)args->value_f32 * scale);
    // the value's size, never negative
    const unsigned long long size = (unsigned long long)((whole < 0ll) ? -whole : whole);
    // a scale of 100 or 1000, which an unsigned long long represents whole
    const unsigned long long divisor = (unsigned long long)scale;
    if (whole < 0ll)
    {
        mmgr_line_ch(args->out, '-');
    }
    mmgr_line_decimal(args->out, size / divisor, 1u);
    mmgr_line_ch(args->out, '.');
    mmgr_line_decimal(args->out, size % divisor, args->positions);
}

EMBED_INLINE void cli_shade(const PdfCtx *args)
{
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_fix, PdfCtx, .out = args->out, .value_f32 = (float)args->rgb[at] / 255.0f, .positions = 3u);
        mmgr_line_ch(args->out, ' ');
    }
    mmgr_line_put(args->out, "rg");
}

// one row's number or piece written at `x` on its baseline
EMBED_INLINE void cli_show(const PdfCtx *args)
{
    // the face at the text's size, 9.5 points
    mmgr_line_ch(args->out, '/');
    mmgr_line_put(args->out, args->face);
    mmgr_line_put(args->out, " 9.5 Tf 1 0 0 1 ");
    EMBED_CALL(cli_fix, PdfCtx, .out = args->out, .value_f32 = args->x, .positions = 2u);
    mmgr_line_ch(args->out, ' ');
    EMBED_CALL(cli_fix, PdfCtx, .out = args->out, .value_f32 = args->baseline, .positions = 2u);
    mmgr_line_put(args->out, " Tm ");
    cli_shade(args);
    mmgr_line_ch(args->out, ' ');
    cli_string(args);
    mmgr_line_put(args->out, " Tj\n");
}

// The content of one page: each row's number and pieces at their columns
EMBED_INLINE void cli_content(const PdfCtx *args)
{
    int gray = args->gray;
    unsigned char number_tint[3];
    if (gray)
    {
        EMBED_CALL(cli_print_text.color, CliPrintTextCfg, .text = "#888888", .length = 7u, .gray = 1,
                   .rgb = number_tint);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = number_tint, .src = s_number_color, .bytes = 3u});
    }
    mmgr_line_put(args->out, "BT\n");
    const unsigned int first = args->page * args->laid->rows_per_page;
    const unsigned int last = ((first + args->laid->rows_per_page) < args->laid->rows)
                                  ? (first + args->laid->rows_per_page)
                                  : args->laid->rows;
    for (unsigned int index = first; index < last; index += 1u)
    {
        const CliPrintRow *const row = &args->laid->row[index];
        const float baseline = args->height - CLI_PRINT_MARGIN - ((float)(index - first) * CLI_PRINT_LEADING) -
                               (CLI_PRINT_LEADING * 0.75f);
        if (row->numbered)
        {
            char number[32];
            VerbaLinea line = {number, sizeof(number), 0ull};
            const unsigned long long width =
                (args->laid->number_columns > 2ull) ? (args->laid->number_columns - 2ull) : 0ull;
            mmgr_line_decimal_columns(&line, row->number, (unsigned int)width);
            const unsigned long long length = mmgr_line_finish(&line);
            EMBED_CALL(cli_show, PdfCtx, .out = args->out, .face = "F1", .x = CLI_PRINT_MARGIN, .baseline = baseline,
                       .rgb = number_tint, .text = number, .length = length);
        }
        for (unsigned int at = 0u; at < row->pieces; at += 1u)
        {
            const CliPrintPiece *const piece = &args->laid->piece[row->first + at];
            static const char *const s_faces[4] = {"F1", "F2", "F3", "F4"};
            const float x =
                CLI_PRINT_MARGIN + ((float)(args->laid->number_columns + piece->column) * CLI_PRINT_ADVANCE);
            EMBED_CALL(cli_show, PdfCtx, .out = args->out,
                       .face = s_faces[(piece->bold ? 1u : 0u) | (piece->italic ? 2u : 0u)], .x = x,
                       .baseline = baseline, .rgb = piece->color, .text = args->laid->text + piece->text,
                       .length = piece->length);
        }
    }
    mmgr_line_put(args->out, "ET\n");
}

// the object `number` begun at the out's place, recorded among the offsets
EMBED_INLINE void cli_object(const PdfCtx *args)
{
    args->offsets[args->number - 1u] = args->out->at;
    mmgr_line_decimal(args->out, args->number, 1u);
    mmgr_line_put(args->out, " 0 obj\n");
}

void cli_print_pdf(const CliPrintLayoutListingOutCfg *args)
{
    unsigned long long *pages = args->pages;
    static const char *const s_faces[4] = {"Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique"};
    const CliPrintLaid *const laid = cli_print_layout_listing_out.lay_out(args);
    float width = 0.0f;
    float height = 0.0f;
    EMBED_CALL(cli_print_layout_width.size, CliPrintLayoutWidthCfg, .layout = args->layout, .width = &width,
               .height = &height);
    // the pages printed, the objects' offsets and a page's contents, in cells sized to the pages laid out
    const unsigned int most = (laid->pages < CLI_PRINT_PAGES) ? laid->pages : CLI_PRINT_PAGES;
    unsigned int *const printed =
        g_cli_cells->temporary_buf_alloc(((unsigned long long)most + 1ull) * sizeof(unsigned int), &g_cli_print_cells);
    unsigned long long *const offsets =
        g_cli_cells->temporary_buf_alloc((7ull + (2ull * most)) * sizeof(unsigned long long), &g_cli_print_cells);
    char *const content_cell = g_cli_cells->temporary_buf_alloc(CLI_PDF_CONTENT_CAPACITY, &g_cli_print_cells);
    VerbaLinea content = mmgr_line_over(content_cell, CLI_PDF_CONTENT_CAPACITY);
    const int begun = content.out != NULL;
    *pages = 0ull;
    if ((printed == NULL) || (offsets == NULL) || !begun)
    {
        // with no capacity to write it in, nothing is written
        g_cli_cells->persistent_buf_release(printed, &g_cli_print_cells);
        g_cli_cells->persistent_buf_release(offsets, &g_cli_print_cells);
        g_cli_cells->persistent_buf_release(content.out, &g_cli_print_cells);
        return;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < laid->pages; at += 1u)
    {
        if (EMBED_CALL(cli_print_table.prints, CliPrintCfg, .layout = args->layout, .page_u64 = at + 1ull) &&
            (count < most))
        {
            printed[count] = at;
            count += 1u;
        }
    }
    // 1 the catalog, 2 the pages, 3 to 6 the faces, 7 the document's information; then each page and its contents,
    // two objects a page
    const unsigned int first_page = 8u;
    const unsigned int objects = 7u + (2u * count);
    mmgr_line_put(args->out, "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n");
    EMBED_CALL(cli_object, PdfCtx, .out = args->out, .offsets = offsets, .number = 1u);
    mmgr_line_put(args->out, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    EMBED_CALL(cli_object, PdfCtx, .out = args->out, .offsets = offsets, .number = 2u);
    mmgr_line_put(args->out, "<< /Type /Pages /Kids [");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(args->out, (at != 0u) ? " " : "");
        mmgr_line_decimal(args->out, first_page + (at * 2u), 1u);
        mmgr_line_put(args->out, " 0 R");
    }
    mmgr_line_put(args->out, "] /Count ");
    mmgr_line_decimal(args->out, count, 1u);
    mmgr_line_put(args->out, " >>\nendobj\n");
    for (unsigned int face = 0u; face < 4u; face += 1u)
    {
        EMBED_CALL(cli_object, PdfCtx, .out = args->out, .offsets = offsets, .number = 3u + face);
        mmgr_line_put(args->out, "<< /Type /Font /Subtype /Type1 /BaseFont /");
        mmgr_line_put(args->out, s_faces[face]);
        mmgr_line_put(args->out, " /Encoding /WinAnsiEncoding >>\nendobj\n");
    }
    EMBED_CALL(cli_object, PdfCtx, .out = args->out, .offsets = offsets, .number = 7u);
    mmgr_line_put(args->out, "<< /Title ");
    EMBED_CALL(cli_text_string, PdfCtx, .out = args->out, .text = args->listing->title);
    mmgr_line_put(args->out, " /Producer (orior) >>\nendobj\n");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int page = first_page + (at * 2u);
        EMBED_CALL(cli_object, PdfCtx, .out = args->out, .offsets = offsets, .number = page);
        mmgr_line_put(args->out, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ");
        EMBED_CALL(cli_fix, PdfCtx, .out = args->out, .value_f32 = width, .positions = 2u);
        mmgr_line_ch(args->out, ' ');
        EMBED_CALL(cli_fix, PdfCtx, .out = args->out, .value_f32 = height, .positions = 2u);
        mmgr_line_put(args->out, "] /Resources << /Font << /F1 3 0 R /F2 4 0 R /F3 5 0 R /F4 6 0 R >> >> /Contents ");
        mmgr_line_decimal(args->out, page + 1u, 1u);
        mmgr_line_put(args->out, " 0 R >>\nendobj\n");
        content.at = 0ull;
        EMBED_CALL(cli_content, PdfCtx, .laid = laid, .page = printed[at], .height = height, .gray = args->layout->gray,
                   .out = &content);
        const unsigned long long length = mmgr_line_ok(&content) ? content.at : 0ull;
        EMBED_CALL(cli_object, PdfCtx, .out = args->out, .offsets = offsets, .number = page + 1u);
        mmgr_line_put(args->out, "<< /Length ");
        mmgr_line_decimal(args->out, length, 1u);
        mmgr_line_put(args->out, " >>\nstream\n");
        mmgr_line_put_n(args->out, content.out, length);
        mmgr_line_put(args->out, "endstream\nendobj\n");
    }
    const unsigned long long xref = args->out->at;
    mmgr_line_put(args->out, "xref\n0 ");
    mmgr_line_decimal(args->out, objects + 1u, 1u);
    mmgr_line_put(args->out, "\n0000000000 65535 f \n");
    for (unsigned int at = 0u; at < objects; at += 1u)
    {
        mmgr_line_decimal(args->out, offsets[at], 10u);
        mmgr_line_put(args->out, " 00000 n \n");
    }
    mmgr_line_put(args->out, "trailer\n<< /Size ");
    mmgr_line_decimal(args->out, objects + 1u, 1u);
    mmgr_line_put(args->out, " /Root 1 0 R /Info 7 0 R >>\nstartxref\n");
    mmgr_line_decimal(args->out, xref, 1u);
    mmgr_line_put(args->out, "\n%%EOF\n");
    *pages = count;
    g_cli_cells->persistent_buf_release(printed, &g_cli_print_cells);
    g_cli_cells->persistent_buf_release(offsets, &g_cli_print_cells);
    g_cli_cells->persistent_buf_release(content.out, &g_cli_print_cells);
}
